#include "logging/Logger.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <ios>
#include <set>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <thread>
#include <vector>


namespace
{

using pi::logging::LogLevel;
using pi::logging::Logger;

constexpr std::array<LogLevel, 4> allLevels{
    LogLevel::debug,
    LogLevel::info,
    LogLevel::warning,
    LogLevel::error
};


/// Stream buffer whose every output operation throws.
class ThrowingBuffer final : public std::streambuf
{
protected:
    int_type overflow(int_type) override
    {
        throw std::runtime_error("sink failure");
    }

    std::streamsize xsputn(const char*, std::streamsize) override
    {
        throw std::runtime_error("sink failure");
    }
};


/// Stream buffer that reports failure for every output operation.
class RejectingBuffer final : public std::streambuf
{
protected:
    int_type overflow(int_type) override
    {
        return traits_type::eof();
    }

    std::streamsize xsputn(const char*, std::streamsize) override
    {
        return 0;
    }
};


/// Unsynchronized buffer that emits one character at a time and yields
/// between characters, so unserialized writers would interleave records.
class SlowBuffer final : public std::streambuf
{
public:
    [[nodiscard]] const std::string& text() const
    {
        return text_;
    }

protected:
    int_type overflow(int_type character) override
    {
        if (traits_type::eq_int_type(character, traits_type::eof()))
        {
            return traits_type::not_eof(character);
        }
        text_.push_back(traits_type::to_char_type(character));
        std::this_thread::yield();
        return character;
    }

    std::streamsize xsputn(const char* data, std::streamsize count) override
    {
        for (std::streamsize index = 0; index < count; ++index)
        {
            overflow(traits_type::to_int_type(data[index]));
        }
        return count;
    }

private:
    std::string text_;
};


std::vector<std::string> splitLines(const std::string& text)
{
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < text.size())
    {
        const std::size_t end = text.find('\n', start);
        assert(end != std::string::npos);
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}


void testLevelNames()
{
    assert(pi::logging::toString(LogLevel::debug) == "debug");
    assert(pi::logging::toString(LogLevel::info) == "info");
    assert(pi::logging::toString(LogLevel::warning) == "warning");
    assert(pi::logging::toString(LogLevel::error) == "error");
}


void testEveryLevelAndThreshold()
{
    for (const LogLevel threshold : allLevels)
    {
        for (const LogLevel level : allLevels)
        {
            std::ostringstream sink;
            Logger logger(sink, threshold);
            const bool expected = static_cast<int>(level)
                >= static_cast<int>(threshold);

            assert(logger.enabled(level) == expected);
            logger.write(level, "core", "message");

            const std::string expectedOutput = expected
                ? "pi-engine: "
                    + std::string(pi::logging::toString(level))
                    + ": core: message\n"
                : std::string();
            assert(sink.str() == expectedOutput);
        }
    }
}


void testExactRecords()
{
    std::ostringstream sink;
    Logger logger(sink, LogLevel::debug);

    logger.write(LogLevel::debug, "scheduler", "queued 4 tasks");
    logger.write(LogLevel::info, "storage", "opened spill file");
    logger.write(LogLevel::warning, "checkpoint", "slow fsync");
    logger.write(LogLevel::error, "verification", "digit mismatch");
    logger.write(LogLevel::info, "", "");

    assert(sink.str()
        == "pi-engine: debug: scheduler: queued 4 tasks\n"
           "pi-engine: info: storage: opened spill file\n"
           "pi-engine: warning: checkpoint: slow fsync\n"
           "pi-engine: error: verification: digit mismatch\n"
           "pi-engine: info: : \n");
}


void testLineBreakNormalization()
{
    std::ostringstream sink;
    Logger logger(sink, LogLevel::info);

    logger.write(
        LogLevel::warning,
        "comp\r\nonent",
        "line one\nline two\rline three\r\n"
    );
    logger.write(LogLevel::error, "\n", "\r\n\r\n");

    assert(sink.str()
        == "pi-engine: warning: comp  onent: "
           "line one line two line three  \n"
           "pi-engine: error:  :     \n");
    assert(splitLines(sink.str()).size() == 2);
}


void testFailedSinkIsContained()
{
    std::ostringstream sink;
    sink.setstate(std::ios_base::badbit);
    Logger logger(sink, LogLevel::debug);

    logger.write(LogLevel::error, "core", "dropped");
    assert(sink.str().empty());
}


void testThrowingSinkIsContained()
{
    ThrowingBuffer throwingBuffer;
    std::ostream throwingSink(&throwingBuffer);
    throwingSink.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    Logger throwingLogger(throwingSink, LogLevel::debug);
    throwingLogger.write(LogLevel::error, "core", "dropped");
    throwingSink.clear();
    throwingLogger.write(LogLevel::warning, "core", "dropped again");

    RejectingBuffer rejectingBuffer;
    std::ostream rejectingSink(&rejectingBuffer);
    rejectingSink.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    Logger rejectingLogger(rejectingSink, LogLevel::debug);
    rejectingLogger.write(LogLevel::error, "core", "dropped");
    rejectingSink.clear();
    rejectingLogger.write(LogLevel::info, "core", "dropped again");
}


void testConcurrentWritersProduceIntactUniqueRecords()
{
    constexpr int writerCount = 8;
    constexpr int recordsPerWriter = 100;

    SlowBuffer buffer;
    std::ostream sink(&buffer);
    Logger logger(sink, LogLevel::info);

    std::vector<std::thread> writers;
    writers.reserve(writerCount);
    for (int writer = 0; writer < writerCount; ++writer)
    {
        writers.emplace_back([&logger, writer]() {
            const std::string component = "writer-" + std::to_string(writer);
            for (int record = 0; record < recordsPerWriter; ++record)
            {
                logger.write(
                    LogLevel::info,
                    component,
                    "record " + std::to_string(record)
                );
            }
        });
    }
    for (std::thread& writer : writers)
    {
        writer.join();
    }

    std::set<std::string> expected;
    for (int writer = 0; writer < writerCount; ++writer)
    {
        for (int record = 0; record < recordsPerWriter; ++record)
        {
            expected.insert(
                "pi-engine: info: writer-" + std::to_string(writer)
                + ": record " + std::to_string(record)
            );
        }
    }

    const std::vector<std::string> lines = splitLines(buffer.text());
    const std::set<std::string> actual(lines.begin(), lines.end());
    assert(lines.size()
        == static_cast<std::size_t>(writerCount * recordsPerWriter));
    assert(actual == expected);
}

} // namespace


int main()
{
    testLevelNames();
    testEveryLevelAndThreshold();
    testExactRecords();
    testLineBreakNormalization();
    testFailedSinkIsContained();
    testThrowingSinkIsContained();
    testConcurrentWritersProduceIntactUniqueRecords();
    return 0;
}
