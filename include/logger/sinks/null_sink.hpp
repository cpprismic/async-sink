#pragma once

#include "../sink.hpp"

namespace logger {

/// @brief A no-op sink that silently discards every log message.
///
/// Useful in two scenarios:
/// - **Benchmarking**: attach a @c NullSink to measure logger throughput
///   without I/O being the bottleneck.
/// - **Testing**: temporarily suppress all output without changing caller code.
class NullSink : public Sink {
public:
    /// @brief Discards @p msg without any processing.
    void Log(const LogMessage&) override {}

    /// @brief No-op; there is nothing to flush.
    void Flush() override {}
};

} // namespace logger
