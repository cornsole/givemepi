#pragma once

#include "progress/ProgressMetrics.hpp"
#include "progress/ProgressReporter.hpp"
#include "progress/ProgressTracker.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>


namespace pi::progress
{

/// Dedicated lifecycle runner that samples and reports progress off workers.
///
/// The runner has no snapshot queue. After each reporter call it samples the
/// tracker again, so a slow reporter causes intermediate intervals to be
/// coalesced instead of building backlog or blocking computation workers.
class ProgressReportingRunner
{
public:
    /// Creates an inactive runner referencing externally owned collaborators.
    ///
    /// Tracker and reporter must outlive the runner. Interval must be positive.
    /// Time and memory complexity: O(1).
    ProgressReportingRunner(
        ProgressTracker& tracker,
        IProgressReporter& reporter,
        std::chrono::milliseconds interval,
        ProgressMetricPolicy metricPolicy = {}
    );

    ~ProgressReportingRunner();

    ProgressReportingRunner(const ProgressReportingRunner&) = delete;
    ProgressReportingRunner& operator=(const ProgressReportingRunner&) = delete;
    ProgressReportingRunner(ProgressReportingRunner&&) = delete;
    ProgressReportingRunner& operator=(ProgressReportingRunner&&) = delete;

    /// Starts the dedicated reporting thread.
    ///
    /// Returns false while an earlier run remains joinable. O(1) excluding OS
    /// thread creation.
    [[nodiscard]] bool start();

    /// Requests asynchronous shutdown without waiting. Time and memory: O(1).
    void requestStop() noexcept;

    /// Joins the current run, waiting for natural terminal state or a stop.
    void join();

    /// Requests shutdown and joins the current run.
    void stop();

    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] std::uint64_t reportCount() const noexcept;
    [[nodiscard]] std::uint64_t failureCount() const noexcept;

    /// Returns the latest isolated reporter failure text, if any.
    ///
    /// Time and memory complexity: O(n) for a copied message of length n.
    [[nodiscard]] std::optional<std::string> lastFailure() const;

private:
    [[nodiscard]] bool reportLatest() noexcept;
    void run() noexcept;

    ProgressTracker& tracker_;
    IProgressReporter& reporter_;
    std::chrono::milliseconds interval_;
    ProgressMetricPolicy metricPolicy_;

    mutable std::mutex lifecycleMutex_;
    std::thread thread_;

    mutable std::mutex controlMutex_;
    std::condition_variable controlChanged_;
    bool stopRequested_ = false;

    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> reportCount_{0};
    std::atomic<std::uint64_t> failureCount_{0};

    mutable std::mutex failureMutex_;
    std::optional<std::string> lastFailure_;
};

} // namespace pi::progress
