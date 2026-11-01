#include "progress/ProgressReportingRunner.hpp"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>


namespace
{

class BlockingReporter final : public pi::progress::IProgressReporter
{
public:
    void report(
        const pi::progress::ProgressSnapshot& snapshot,
        const pi::progress::ProgressMetrics&
    ) override
    {
        std::unique_lock lock(mutex_);
        completedTerms_.push_back(snapshot.completedTerms());
        if (!entered_)
        {
            entered_ = true;
            changed_.notify_all();
            changed_.wait(lock, [this]() { return released_; });
        }
    }

    void waitUntilEntered()
    {
        std::unique_lock lock(mutex_);
        changed_.wait(lock, [this]() { return entered_; });
    }

    void release()
    {
        {
            std::lock_guard lock(mutex_);
            released_ = true;
        }
        changed_.notify_all();
    }

    std::vector<std::uint64_t> completedTerms() const
    {
        std::lock_guard lock(mutex_);
        return completedTerms_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    bool entered_ = false;
    bool released_ = false;
    std::vector<std::uint64_t> completedTerms_;
};


class SlowReporter final : public pi::progress::IProgressReporter
{
public:
    explicit SlowReporter(std::chrono::milliseconds delay)
        : delay_(delay)
    {
    }

    void report(
        const pi::progress::ProgressSnapshot& snapshot,
        const pi::progress::ProgressMetrics&
    ) override
    {
        {
            std::lock_guard lock(mutex_);
            entered_ = true;
            completedTerms_.push_back(snapshot.completedTerms());
        }
        changed_.notify_all();
        std::this_thread::sleep_for(delay_);
    }

    void waitUntilEntered()
    {
        std::unique_lock lock(mutex_);
        changed_.wait(lock, [this]() { return entered_; });
    }

    std::vector<std::uint64_t> completedTerms() const
    {
        std::lock_guard lock(mutex_);
        return completedTerms_;
    }

private:
    std::chrono::milliseconds delay_;
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    bool entered_ = false;
    std::vector<std::uint64_t> completedTerms_;
};


class ThrowingReporter final : public pi::progress::IProgressReporter
{
public:
    void report(
        const pi::progress::ProgressSnapshot&,
        const pi::progress::ProgressMetrics&
    ) override
    {
        throw std::runtime_error("reporter unavailable");
    }
};


class TerminalFailingReporter final : public pi::progress::IProgressReporter
{
public:
    void report(
        const pi::progress::ProgressSnapshot& snapshot,
        const pi::progress::ProgressMetrics&
    ) override
    {
        if (snapshot.terminalState()
            != pi::progress::ProgressTerminalState::running)
        {
            throw std::runtime_error("terminal record unavailable");
        }
    }
};

} // namespace


int main()
{
    using namespace std::chrono_literals;
    using pi::progress::ProgressPhase;
    using pi::progress::ProgressReportingRunner;
    using pi::progress::ProgressTracker;
    using pi::progress::ProgressWorkPlan;

    ProgressTracker tracker(ProgressWorkPlan{1'000, 10'000, 0});
    assert(tracker.transitionTo(ProgressPhase::splitting));
    BlockingReporter blocked;
    ProgressReportingRunner runner(tracker, blocked, 5ms);
    assert(runner.start());
    assert(!runner.start());
    blocked.waitUntilEntered();

    for (std::uint64_t term = 0; term < 10'000; ++term)
    {
        assert(tracker.addCompletedTerms(1));
    }
    assert(tracker.transitionTo(ProgressPhase::finalizing));
    assert(tracker.transitionTo(ProgressPhase::completed));
    blocked.release();
    runner.join();

    const auto samples = blocked.completedTerms();
    assert(samples.size() == 2);
    assert(samples.front() == 0);
    assert(samples.back() == 10'000);
    assert(runner.reportCount() == 2);
    assert(runner.failureCount() == 0);
    assert(!runner.isRunning());

    ThrowingReporter throwing;
    ProgressTracker failingTracker(ProgressWorkPlan{100, 8, 0});
    ProgressReportingRunner isolated(failingTracker, throwing, 5ms);
    assert(isolated.start());
    const auto failureDeadline = std::chrono::steady_clock::now() + 1s;
    while (isolated.failureCount() == 0
        && std::chrono::steady_clock::now() < failureDeadline)
    {
        std::this_thread::yield();
    }
    assert(isolated.failureCount() >= 1);
    assert(failingTracker.addCompletedTerms(1));
    assert(failingTracker.markFailed("calculation stopped"));
    isolated.join();
    assert(isolated.reportCount() == 0);
    assert(isolated.failureCount() >= 2);
    assert(isolated.lastFailure()
        == std::optional<std::string>{"reporter unavailable"});

    TerminalFailingReporter terminalFailing;
    ProgressTracker completedTracker(ProgressWorkPlan{100, 8, 0});
    assert(completedTracker.transitionTo(ProgressPhase::splitting));
    ProgressReportingRunner terminalRunner(
        completedTracker,
        terminalFailing,
        5ms
    );
    assert(terminalRunner.start());
    const auto reportDeadline = std::chrono::steady_clock::now() + 1s;
    while (terminalRunner.reportCount() == 0
        && std::chrono::steady_clock::now() < reportDeadline)
    {
        std::this_thread::yield();
    }
    assert(terminalRunner.reportCount() >= 1);
    assert(completedTracker.addCompletedTerms(8));
    assert(completedTracker.transitionTo(ProgressPhase::finalizing));
    assert(completedTracker.transitionTo(ProgressPhase::completed));
    bool terminalFailureSurfaced = false;
    try
    {
        terminalRunner.joinChecked();
    }
    catch (const std::runtime_error& error)
    {
        terminalFailureSurfaced = std::string(error.what())
            == "Progress reporter failed: terminal record unavailable";
    }
    assert(terminalFailureSurfaced);
    assert(!terminalRunner.isRunning());
    assert(terminalRunner.failureCount() == 1);

    SlowReporter checkedReporter(1ms);
    ProgressTracker checkedTracker(ProgressWorkPlan{100, 8, 0});
    assert(checkedTracker.transitionTo(ProgressPhase::splitting));
    ProgressReportingRunner checked(checkedTracker, checkedReporter, 1s);
    assert(checked.start());
    checkedReporter.waitUntilEntered();
    assert(checkedTracker.addCompletedTerms(8));
    assert(checkedTracker.transitionTo(ProgressPhase::finalizing));
    assert(checkedTracker.transitionTo(ProgressPhase::completed));
    checked.requestStop();
    checked.joinChecked();
    assert(checked.failureCount() == 0);

    SlowReporter stoppedReporter(1ms);
    ProgressTracker stoppedTracker(ProgressWorkPlan{100, 8, 0});
    ProgressReportingRunner stopped(stoppedTracker, stoppedReporter, 1s);
    assert(stopped.start());
    stoppedReporter.waitUntilEntered();
    stopped.stop();
    assert(!stopped.isRunning());
    assert(stopped.reportCount() == 2);

    bool invalidInterval = false;
    try
    {
        ProgressReportingRunner invalid(
            stoppedTracker,
            stoppedReporter,
            0ms
        );
    }
    catch (const std::invalid_argument&)
    {
        invalidInterval = true;
    }
    assert(invalidInterval);

    return 0;
}
