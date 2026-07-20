#include "chudnovsky/ChudnovskyCalculator.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"
#include "config/CommandLine.hpp"
#include "config/ConfigLoader.hpp"
#include "progress/JsonProgressReporter.hpp"
#include "progress/ProgressReportingRunner.hpp"
#include "progress/TextProgressReporter.hpp"
#include "scheduler/Scheduler.hpp"

#include <algorithm>
#include <cstdint>
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
                {config.digits},
                scheduler,
                pi::binary::ParallelSplitOptions{32, 4},
                &tracker
                )
            );
        }
        catch (...)
        {
            scheduler.stop();
            if (runner)
            {
                runner->stop();
            }
            throw;
        }
        scheduler.stop();
        if (runner)
        {
            runner->join();
        }

        std::ofstream output(config.output_file, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            throw std::runtime_error("Cannot open pi output file");
        }
        output << result->decimal << '\n';
        if (!output)
        {
            throw std::runtime_error("Cannot write pi output file");
        }

        std::cout << "Calculated " << result->precision.requestedDigits
                  << " digits to " << config.output_file << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "pi-engine: " << error.what() << '\n';
        return 1;
    }
}
