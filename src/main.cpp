#include "checkpoint/CheckpointFrontierWriter.hpp"
#include "checkpoint/CheckpointResumePlanner.hpp"
#include "chudnovsky/ChudnovskyCalculator.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"
#include "config/CommandLine.hpp"
#include "config/ConfigLoader.hpp"
#include "logging/Logger.hpp"
#include "progress/JsonProgressReporter.hpp"
#include "progress/ProgressCostModel.hpp"
#include "progress/ProgressReportingRunner.hpp"
#include "progress/ResumeProgress.hpp"
#include "progress/TextProgressReporter.hpp"
#include "scheduler/Scheduler.hpp"
#include "storage/StorageMergeCoordinator.hpp"
#include "storage/StorageManager.hpp"
#include "verification/BBPSamplePolicy.hpp"
#include "verification/FinalVerifier.hpp"
#include "verification/VerificationManifest.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <unistd.h>

namespace
{

void logValidationDiagnostic(
    pi::logging::Logger& logger,
    const pi::checkpoint::ValidationDiagnostic& diagnostic
)
{
    const std::string_view stage =
        pi::checkpoint::toString(diagnostic.stage);
    const std::string_view reason =
        pi::checkpoint::toString(diagnostic.reason);
    std::string message;
    message.reserve(
        stage.size() + reason.size() + diagnostic.detail.size() + 4
    );
    message.append(stage);
    message.push_back('/');
    message.append(reason);
    message.append(": ");
    message.append(diagnostic.detail);
    logger.write(
        pi::logging::LogLevel::warning,
        "checkpoint",
        message
    );
}


bool belongsToRejectedCheckpoint(
    const pi::checkpoint::CheckpointResumePlan& plan,
    const pi::checkpoint::ValidationDiagnostic& diagnostic
) noexcept
{
    for (const auto& rejected : plan.rejectedBlocks)
    {
        for (const auto& rejectedDiagnostic
             : rejected.validation.diagnostics())
        {
            if (rejectedDiagnostic == diagnostic)
            {
                return true;
            }
        }
    }
    return false;
}


/// Progress units of publishing the verification manifest.
constexpr std::uint64_t manifestPublicationUnits = 1;


pi::verification::VerificationIdentity verificationIdentityOf(
    const pi::chudnovsky::PrecisionPlan& precision
) noexcept
{
    return pi::verification::VerificationIdentity{
        precision.requestedDigits,
        precision.guardDigits,
        precision.workingDigits,
        precision.termCount
    };
}


/// Forwards final-verification units to the tracker's verifying phase.
class TrackerVerificationObserver final
    : public pi::verification::VerificationWorkObserver
{
public:
    explicit TrackerVerificationObserver(
        pi::progress::ProgressTracker& tracker
    ) noexcept
        : tracker_(tracker)
    {
    }

    void workCompleted(std::uint64_t units) noexcept override
    {
        static_cast<void>(tracker_.addPhaseUnits(
            pi::progress::ProgressPhase::verifyingOutput,
            units
        ));
    }

private:
    pi::progress::ProgressTracker& tracker_;
};

} // namespace



int main(int argc, char* argv[])
{
    constexpr int exitFailure = 1;
    constexpr int exitUsage = 2;

    pi::config::CommandLineOptions options;
    try
    {
        options = pi::config::CommandLine::parse(argc, argv);
    }
    catch (const pi::config::CommandLineUsageError& error)
    {
        std::cerr << "pi-engine: " << error.what() << '\n'
                  << "Try 'pi-engine --help' for usage.\n";
        return exitUsage;
    }
    catch (const std::exception& error)
    {
        std::cerr << "pi-engine: " << error.what() << '\n';
        return exitFailure;
    }
    if (options.help_requested)
    {
        pi::config::CommandLine::writeHelp(std::cout);
        if (!std::cout.flush())
        {
            std::cerr << "pi-engine: cannot write help\n";
            return exitFailure;
        }
        return 0;
    }

    pi::logging::Logger logger(
        std::cerr,
        pi::logging::LogLevel::warning
    );

    try
    {
        pi::config::Config config =
            pi::config::ConfigLoader::load("config.toml");
        pi::config::CommandLine::applyOverrides(config, options);

        const pi::chudnovsky::PrecisionPlan precision =
            pi::chudnovsky::PrecisionPolicy::create(config.digits);
        const pi::checkpoint::ComputationIdentity checkpointIdentity =
            pi::checkpoint::ComputationIdentity::fromPrecisionPlan(precision);
        const std::size_t hardwareThreads = std::max<std::size_t>(
            1,
            std::thread::hardware_concurrency()
        );
        const std::size_t workerCount = config.threads == 0
            ? hardwareThreads
            : config.threads;
        const pi::binary::ParallelSplitOptions splitOptions{32, 4};

        pi::checkpoint::CheckpointResumePlan resumePlan;
        if (config.resume_enabled)
        {
            const bool resumeDirectoryExisted =
                std::filesystem::exists(config.checkpoint_directory);
            resumePlan = pi::checkpoint::CheckpointResumePlanner(
                config.checkpoint_directory,
                config.checkpoint_quarantine_directory
            ).create(checkpointIdentity);
            for (const pi::checkpoint::RejectedCheckpoint& rejected
                 : resumePlan.rejectedBlocks)
            {
                std::string message = "rejected ";
                message.append(rejected.path.string());
                logger.write(
                    pi::logging::LogLevel::warning,
                    "checkpoint",
                    message
                );
                for (const auto& diagnostic
                     : rejected.validation.diagnostics())
                {
                    logValidationDiagnostic(logger, diagnostic);
                }
                if (rejected.quarantinedPath)
                {
                    message = "quarantined ";
                    message.append(rejected.path.string());
                    message.append(" to ");
                    message.append(rejected.quarantinedPath->string());
                    logger.write(
                        pi::logging::LogLevel::warning,
                        "checkpoint",
                        message
                    );
                }
                if (rejected.quarantineOperation
                    && !rejected.quarantineOperation->isAccepted())
                {
                    for (const auto& diagnostic
                         : rejected.quarantineOperation->diagnostics())
                    {
                        logValidationDiagnostic(logger, diagnostic);
                    }
                }
            }
            if (resumeDirectoryExisted)
            {
                for (const auto& diagnostic : resumePlan.diagnostics)
                {
                    if (diagnostic.stage
                            == pi::checkpoint::ValidationStage::manifest
                        && !belongsToRejectedCheckpoint(
                            resumePlan, diagnostic))
                    {
                        logValidationDiagnostic(logger, diagnostic);
                    }
                }
            }
        }
        else
        {
            resumePlan.rangesToRecalculate.push_back({
                0,
                checkpointIdentity.termCount
            });
        }

        const std::uint64_t outputBytes = precision.requestedDigits + 3;
        std::uint64_t verificationUnits = 0;
        if (config.verification_enabled)
        {
            const std::uint64_t fileWork =
                pi::verification::FinalVerifier::plannedFileWork(
                    verificationIdentityOf(precision),
                    pi::verification::BBPSamplePolicy(config.bbp_sample_count)
                );
            verificationUnits = fileWork == UINT64_MAX
                ? fileWork
                : fileWork + manifestPublicationUnits;
        }
        pi::progress::ProgressTracker tracker({
            precision.requestedDigits,
            precision.termCount,
            pi::progress::planCheckpointBlocks(
                resumePlan,
                splitOptions,
                workerCount,
                config.checkpoint_enabled
            ),
            pi::progress::planPipelineCosts({
                precision.workingDigits,
                precision.termCount,
                pi::progress::planFrontierNodes(
                    resumePlan,
                    splitOptions,
                    workerCount
                ),
                workerCount,
                outputBytes,
                verificationUnits
            })
        });
        // Checkpoint validation ran above, before the tracker and reporter
        // exist, so its warnings precede every progress record. It therefore
        // publishes no phase units and carries no pipeline cost; elapsed time
        // and rates start after it, and restored work is never new work.
        if (config.resume_enabled)
        {
            if (!tracker.transitionTo(
                    pi::progress::ProgressPhase::validatingCheckpoints)
                || !pi::progress::ResumeProgressRestorer::restore(
                    resumePlan, tracker))
            {
                throw std::runtime_error(
                    "Validated checkpoint progress could not be restored"
                );
            }
        }

        std::vector<pi::binary::BinaryFrontierNode> initialFrontier;
        std::vector<pi::checkpoint::ManifestEntry> acceptedEntries;
        initialFrontier.reserve(resumePlan.acceptedBlocks.size());
        acceptedEntries.reserve(resumePlan.acceptedBlocks.size());
        for (pi::checkpoint::AcceptedCheckpoint& accepted
             : resumePlan.acceptedBlocks)
        {
            pi::binary::BinaryNode node(
                static_cast<std::size_t>(accepted.block.location.start),
                static_cast<std::size_t>(accepted.block.location.end)
            );
            node.P() = std::move(accepted.block.p);
            node.Q() = std::move(accepted.block.q);
            node.T() = std::move(accepted.block.t);
            initialFrontier.push_back({
                std::move(node),
                accepted.block.location.treeLevel
            });
            acceptedEntries.push_back(std::move(accepted.manifestEntry));
        }

        std::unique_ptr<pi::checkpoint::CheckpointFrontierWriter>
            checkpointWriter;
        if (config.checkpoint_enabled)
        {
            checkpointWriter = std::make_unique<
                pi::checkpoint::CheckpointFrontierWriter
            >(
                config.checkpoint_directory,
                checkpointIdentity,
                std::chrono::seconds(config.checkpoint_interval_seconds),
                std::move(acceptedEntries),
                nullptr,
                &tracker
            );
        }

        std::unique_ptr<pi::storage::StorageManager> storageManager;
        std::unique_ptr<pi::storage::AsyncChunkWriter> asyncWriter;
        std::unique_ptr<pi::storage::AsyncChunkReader> asyncReader;
        std::unique_ptr<pi::storage::StorageMergeCoordinator>
            mergeCoordinator;
        if (config.out_of_core_enabled)
        {
            const auto storagePolicy =
                pi::config::makeStoragePolicy(config);
            storageManager = std::make_unique<pi::storage::StorageManager>(
                storagePolicy);
            const auto ioWorkers = std::max<std::uint32_t>(
                1,
                storagePolicy.max_concurrent_io);
            const auto ioQueueCapacity = std::max<std::size_t>(
                1,
                static_cast<std::size_t>(ioWorkers) * 8);
            asyncWriter = std::make_unique<pi::storage::AsyncChunkWriter>(
                *storageManager,
                ioQueueCapacity,
                ioWorkers);
            asyncReader = std::make_unique<pi::storage::AsyncChunkReader>(
                *storageManager,
                ioQueueCapacity,
                ioWorkers);
            mergeCoordinator =
                std::make_unique<pi::storage::StorageMergeCoordinator>(
                    *storageManager,
                    checkpointIdentity,
                    &tracker,
                    asyncWriter.get(),
                    asyncReader.get());
        }

        std::unique_ptr<pi::progress::IProgressReporter> reporter;
        std::unique_ptr<pi::progress::ProgressReportingRunner> runner;
        if (config.progress_enabled)
        {
            const pi::progress::EtaSelection etaSelection{
                config.progress_eta_total,
                config.progress_eta_phase
            };
            if (config.progress_format == pi::config::ProgressFormat::json)
            {
                reporter = std::make_unique<
                    pi::progress::JsonProgressReporter
                >(std::cerr, etaSelection);
            }
            else
            {
                const auto mode = ::isatty(STDERR_FILENO) != 0
                    ? pi::progress::TextOutputMode::terminal
                    : pi::progress::TextOutputMode::log;
                reporter = std::make_unique<
                    pi::progress::TextProgressReporter
                >(std::cerr, mode, etaSelection, config.progress_eta_format);
            }
            runner = std::make_unique<pi::progress::ProgressReportingRunner>(
                tracker,
                *reporter,
                std::chrono::milliseconds(config.progress_interval_ms)
            );
            if (!runner->start())
            {
                throw std::runtime_error("Progress reporter failed to start");
            }
        }

        const std::size_t queueCapacity = std::max<std::size_t>(
            1024,
            workerCount * 8
        );

        pi::scheduler::Scheduler scheduler(workerCount, queueCapacity);
        scheduler.start();
        std::optional<pi::chudnovsky::PiCalculationResult> result;
        try
        {
            result.emplace(
                pi::chudnovsky::ChudnovskyCalculator::calculateParallel(
                {
                    config.digits,
                    pi::chudnovsky::PrecisionPolicy::defaultGuardDigits,
                    true
                },
                scheduler,
                splitOptions,
                &tracker,
                mergeCoordinator.get(),
                std::move(initialFrontier),
                checkpointWriter.get()
                )
            );
            scheduler.stop();

            if (result->decimal.size() + 1 != outputBytes
                || !tracker.declarePhaseUnits(
                    pi::progress::ProgressPhase::writingOutput,
                    outputBytes)
                || !tracker.transitionTo(
                    pi::progress::ProgressPhase::writingOutput))
            {
                throw std::logic_error("Cannot enter output writing phase");
            }

            std::ofstream output(
                config.output_file,
                std::ios::binary | std::ios::trunc
            );
            if (!output)
            {
                throw std::runtime_error("Cannot open pi output file");
            }
            // Chunked writes on the coordinating thread publish byte progress.
            constexpr std::size_t outputChunkBytes = 4 * 1024 * 1024;
            const std::string& decimal = result->decimal;
            for (std::size_t offset = 0; offset < decimal.size();)
            {
                const std::size_t count =
                    std::min(outputChunkBytes, decimal.size() - offset);
                output.write(
                    decimal.data() + offset,
                    static_cast<std::streamsize>(count)
                );
                if (!output)
                {
                    throw std::runtime_error("Cannot write pi output file");
                }
                offset += count;
                static_cast<void>(tracker.addPhaseUnits(
                    pi::progress::ProgressPhase::writingOutput,
                    count
                ));
            }
            output.put('\n');
            output.close();
            if (!output)
            {
                throw std::runtime_error("Cannot write pi output file");
            }
            static_cast<void>(tracker.addPhaseUnits(
                pi::progress::ProgressPhase::writingOutput,
                1
            ));

            std::optional<std::filesystem::path> manifestPath;
            std::optional<std::string> verificationHash;
            if (config.verification_enabled)
            {
                const pi::verification::VerificationIdentity identity =
                    verificationIdentityOf(result->precision);
                const pi::verification::BBPSamplePolicy samplePolicy(
                    config.bbp_sample_count
                );
                const std::uint64_t fileWork =
                    pi::verification::FinalVerifier::plannedFileWork(
                        identity,
                        samplePolicy
                    );
                if (!tracker.declarePhaseUnits(
                        pi::progress::ProgressPhase::verifyingOutput,
                        fileWork == UINT64_MAX
                            ? fileWork
                            : fileWork + manifestPublicationUnits)
                    || !tracker.transitionTo(
                        pi::progress::ProgressPhase::verifyingOutput))
                {
                    throw std::logic_error(
                        "Cannot enter output verification phase"
                    );
                }
                TrackerVerificationObserver verificationProgress(tracker);
                const auto verification =
                    pi::verification::FinalVerifier::verifyFile(
                        config.output_file,
                        identity,
                        samplePolicy,
                        &verificationProgress
                    );
                if (verification.report.status()
                    != pi::verification::VerificationStatus::passed)
                {
                    for (const auto& check : verification.report.checks())
                    {
                        if (check.status()
                            == pi::verification::VerificationStatus::passed)
                        {
                            continue;
                        }
                        std::cerr << "verification "
                                  << pi::verification::toString(check.stage())
                                  << ": "
                                  << pi::verification::toString(check.status())
                                  << '\n';
                        for (const auto& diagnostic : check.diagnostics())
                        {
                            std::cerr << "  "
                                      << pi::verification::toString(
                                          diagnostic.reason
                                      )
                                      << ": " << diagnostic.detail << '\n';
                        }
                    }
                    throw std::runtime_error(
                        "Final output verification did not pass"
                    );
                }
                verificationHash = pi::verification::toHex(
                    *verification.sha256
                );
                const std::filesystem::path outputPath(config.output_file);
                manifestPath = config.verification_manifest_file.empty()
                    ? std::filesystem::path(config.output_file + ".verify")
                    : std::filesystem::path(config.verification_manifest_file);
                const auto timestamp = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count()
                );
                const auto manifest =
                    pi::verification::VerificationManifest::fromRun(
                        verification,
                        outputPath.filename().string(),
                        result->decimal.size(),
                        timestamp
                    );
                pi::verification::VerificationManifestStore::save(
                    *manifestPath,
                    manifest
                );
                static_cast<void>(tracker.addPhaseUnits(
                    pi::progress::ProgressPhase::verifyingOutput,
                    manifestPublicationUnits
                ));
            }

            if (!tracker.transitionTo(pi::progress::ProgressPhase::completed))
            {
                throw std::logic_error("Cannot complete progress lifecycle");
            }
            if (runner)
            {
                runner->joinChecked();
            }

            std::cout << "Calculated " << result->precision.requestedDigits
                      << " digits to " << config.output_file;
            if (manifestPath)
            {
                std::cout << " (verified; manifest "
                          << manifestPath->string()
                          << "; sha256 " << *verificationHash << ')';
            }
            std::cout << '\n';
        }
        catch (const std::exception& error)
        {
            scheduler.stop();
            static_cast<void>(tracker.markFailed(error.what()));
            if (runner)
            {
                runner->join();
            }
            throw;
        }
        catch (...)
        {
            scheduler.stop();
            static_cast<void>(tracker.markFailed("Unknown CLI pipeline failure"));
            if (runner)
            {
                runner->join();
            }
            throw;
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "pi-engine: " << error.what() << '\n';
        return exitFailure;
    }
}
