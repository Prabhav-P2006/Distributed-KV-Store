#ifndef SHAUNSTORE_PROTOCOL_H
#define SHAUNSTORE_PROTOCOL_H

#include "Config.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class ReplicationPriority {
    Critical,
    Standard,
    Low
};

enum class ConsistencyMode {
    Strong,
    Eventual,
    BoundedStaleness
};

struct ClientCommand {
    std::string name;
    std::vector<std::string> args;
    ReplicationPriority priority {ReplicationPriority::Standard};
    ConsistencyMode consistency {ConsistencyMode::Eventual};

    [[nodiscard]] bool isWrite() const;
};

struct ReplicationEntry {
    std::uint64_t offset {0};
    std::vector<std::string> tokens;
    ReplicationPriority priority {ReplicationPriority::Standard};
    ConsistencyMode consistency {ConsistencyMode::Eventual};
    std::chrono::steady_clock::time_point enqueued_at {std::chrono::steady_clock::now()};
};

[[nodiscard]] bool parseRESP(const std::string& buffer, std::vector<std::string>& tokens, std::size_t& parsed_length);
[[nodiscard]] std::string encodeArray(const std::vector<std::string>& tokens);
[[nodiscard]] std::string simpleString(const std::string& value);
[[nodiscard]] std::string errorString(const std::string& value);
[[nodiscard]] std::string integerString(std::int64_t value);
[[nodiscard]] std::string bulkString(const std::optional<std::string>& value);
[[nodiscard]] std::string toString(ReplicationPriority priority);
[[nodiscard]] std::string toString(ConsistencyMode consistency);
[[nodiscard]] std::optional<ReplicationPriority> parsePriority(const std::string& raw);
[[nodiscard]] std::optional<ConsistencyMode> parseConsistency(const std::string& raw);
[[nodiscard]] ClientCommand parseClientCommand(const std::vector<std::string>& tokens);

#endif
