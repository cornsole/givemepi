#include "progress/ProgressReportingRunner.hpp"

#include <exception>
#include <stdexcept>
#include <utility>


namespace pi::progress
{

ProgressReportingRunner::ProgressReportingRunner(
    ProgressTracker& tracker,
    IProgressReporter& reporter,
    std::chrono::milliseconds interval,
    ProgressMetricPolicy metricPolicy
)
    : tracker_(tracker),
      reporter_(reporter),
      interval_(interval),
      metricPolicy_(metricPolicy)
{
    if (interval_ <= std::chrono::milliseconds::zero())
    {
        throw std::invalid_argument("Reporting interval must be positive");
    }
    ProgressMetricsCalculator::validatePolicy(metricPolicy_);
}


ProgressReportingRunner::~ProgressReportingRunner()
{
    stop();
}


bool ProgressReportingRunner::start()
{
    std::lock_guard lifecycleLock(lifecycleMutex_);
    if (thread_.joinable())
    {
        return false;
    }

    {
        std::lock_guard controlLock(controlMutex_);
        stopRequested_ = false;
    }
    running_.store(true, std::memory_order_release);
    try
    {
        thread_ = std::thread(&ProgressReportingRunner::run, this);
    }
    catch (...)
    {
        running_.store(false, std::memory_order_release);
        throw;
    }
    return true;
}


void ProgressReportingRunner::requestStop() noexcept
{
    {
        std::lock_guard lock(controlMutex_);
        stopRequested_ = true;
    }
    controlChanged_.notify_all();
}


void ProgressReportingRunner::join()
{
    std::lock_guard lifecycleLock(lifecycleMutex_);
    if (thread_.joinable())
    {
        thread_.join();
    }
}


void ProgressReportingRunner::stop()
{
    requestStop();
    join();
}


bool ProgressReportingRunner::isRunning() const noexcept
{
    return running_.load(std::memory_order_acquire);
}


std::uint64_t ProgressReportingRunner::reportCount() const noexcept
{
    return reportCount_.load(std::memory_order_relaxed);
}


std::uint64_t ProgressReportingRunner::failureCount() const noexcept
{
    return failureCount_.load(std::memory_order_relaxed);
}


std::optional<std::string> ProgressReportingRunner::lastFailure() const
{
    std::lock_guard lock(failureMutex_);
    return lastFailure_;
}


bool ProgressReportingRunner::reportLatest() noexcept
{
    bool terminal = false;
    try
    {
        const ProgressSnapshot snapshot = tracker_.snapshot();
        terminal = snapshot.terminalState()
            != ProgressTerminalState::running;
        const ProgressMetrics metrics = ProgressMetricsCalculator::calculate(
            snapshot,
            metricPolicy_
        );
        reporter_.report(snapshot, metrics);
        reportCount_.fetch_add(1, std::memory_order_relaxed);
    }
    catch (const std::exception& error)
    {
        failureCount_.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard lock(failureMutex_);
        lastFailure_ = error.what();
    }
    catch (...)
    {
        failureCount_.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard lock(failureMutex_);
        lastFailure_ = "Unknown progress reporter failure";
    }
    return terminal;
}


void ProgressReportingRunner::run() noexcept
{
    while (true)
    {
        if (reportLatest())
        {
            break;
        }

        std::unique_lock lock(controlMutex_);
        const bool stopping = controlChanged_.wait_for(
            lock,
            interval_,
            [this]() { return stopRequested_; }
        );
        lock.unlock();

        if (stopping)
        {
            static_cast<void>(reportLatest());
            break;
        }
    }
    running_.store(false, std::memory_order_release);
}

} // namespace pi::progress
