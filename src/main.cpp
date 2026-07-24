#include "chudnovsky/ChudnovskyCalculator.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"
#include "config/CommandLine.hpp"
#include "config/ConfigLoader.hpp"
#include "progress/JsonProgressReporter.hpp"
#include "progress/ProgressReportingRunner.hpp"
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
#include <thread>
#include <unistd.h>


int main(int argc, char* argv[])
{
    try
    {
        pi::config::Config config =
            pi::config::ConfigLoader::load("config.toml");
        pi::config::CommandLine::applyOverrides(config, argc, argv);

        const pi::chudnovsky::PrecisionPlan precision =
            pi::chudnovsky::PrecisionPolicy::create(config.digits);
        pi::progress::ProgressTracker tracker({
            precision.requestedDigits,
            precision.termCount,
            0
        });

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
                    pi::checkpoint::ComputationIdentity::fromPrecisionPlan(
                        precision),
                    &tracker,
                    asyncWriter.get(),
                    asyncReader.get());
        }

        std::unique_ptr<pi::progress::IProgressReporter> reporter;
        std::unique_ptr<pi::progress::ProgressReportingRunner> runner;
        if (config.progress_enabled)
        {
            if (config.progress_format == pi::config::ProgressFormat::json)
            {
                reporter = std::make_unique<
                    pi::progress::JsonProgressReporter
                >(std::cerr);
            }
            else
            {
                const auto mode = ::isatty(STDERR_FILENO) != 0
                    ? pi::progress::TextOutputMode::terminal
                    : pi::progress::TextOutputMode::log;
                reporter = std::make_unique<
                    pi::progress::TextProgressReporter
                >(std::cerr, mode);
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

        const std::size_t hardwareThreads = std::max<std::size_t>(
            1,
            std::thread::hardware_concurrency()
        );
        const std::size_t workerCount = config.threads == 0
            ? hardwareThreads
            : config.threads;
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
                pi::binary::ParallelSplitOptions{32, 4},
                &tracker,
                mergeCoordinator.get()
                )
            );
            scheduler.stop();

            if (!tracker.transitionTo(
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
            output << result->decimal << '\n';
            output.close();
            if (!output)
            {
                throw std::runtime_error("Cannot write pi output file");
            }

            std::optional<std::filesystem::path> manifestPath;
            std::optional<std::string> verificationHash;
            if (config.verification_enabled)
            {
                if (!tracker.transitionTo(
                        pi::progress::ProgressPhase::verifyingOutput))
                {
                    throw std::logic_error(
                        "Cannot enter output verification phase"
                    );
                }
                const pi::verification::VerificationIdentity identity{
                    result->precision.requestedDigits,
                    result->precision.guardDigits,
                    result->precision.workingDigits,
                    result->precision.termCount
                };
                const pi::verification::BBPSamplePolicy samplePolicy(
                    config.bbp_sample_count
                );
                const auto verification =
                    pi::verification::FinalVerifier::verifyFile(
                        config.output_file,
                        identity,
                        samplePolicy
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
            }

            if (!tracker.transitionTo(pi::progress::ProgressPhase::completed))
            {
                throw std::logic_error("Cannot complete progress lifecycle");
            }
            if (runner)
            {
                runner->join();
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
        return 1;
    }
}
