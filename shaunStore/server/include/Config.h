#ifndef SHAUNSTORE_CONFIG_H
#define SHAUNSTORE_CONFIG_H

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

enum class NodeRole {
    Master,
    Slave,
    Candidate
};

struct Endpoint {
    std::string host {"127.0.0.1"};
    std::uint16_t port {0};

    [[nodiscard]] bool valid() const noexcept {
        return !host.empty() && port > 0;
    }

    [[nodiscard]] std::string toString() const {
        return host + ":" + std::to_string(port);
    }

    auto operator<=>(const Endpoint&) const = default;
};

struct DispatchWeights {
    int critical {70};
    int standard {20};
    int low {10};

    [[nodiscard]] int total() const noexcept {
        return critical + standard + low;
    }
};

struct Config {
    NodeRole role {NodeRole::Master};
    Endpoint self {"127.0.0.1", 6379};
    Endpoint master {"127.0.0.1", 6379};
    std::vector<Endpoint> peer_nodes;
    std::chrono::milliseconds heartbeat_interval {500};
    std::chrono::milliseconds election_timeout {2000};
    std::chrono::milliseconds aging_threshold {250};
    std::chrono::milliseconds strong_write_timeout {3000};
    std::uint64_t max_staleness_offset {50};
    std::size_t backlog_limit {50000};
    DispatchWeights dispatch_weights {};
    bool enable_logging {true};
};

[[nodiscard]] Config loadConfig(const std::string& path);
void validateConfig(const Config& config);
[[nodiscard]] std::string toString(NodeRole role);
[[nodiscard]] NodeRole parseNodeRole(const std::string& raw);

#endif
