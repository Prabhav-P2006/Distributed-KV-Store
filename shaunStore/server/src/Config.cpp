#include "../include/Config.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <string_view>

namespace {
std::string readFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to open config file: " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::size_t skipWhitespace(const std::string& text, std::size_t position) {
    while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position]))) {
        ++position;
    }
    return position;
}

std::string extractString(const std::string& text, const std::string& key, const std::string& fallback = "") {
    const auto key_position = text.find("\"" + key + "\"");
    if (key_position == std::string::npos) {
        return fallback;
    }

    auto position = text.find(':', key_position);
    if (position == std::string::npos) {
        throw std::runtime_error("Malformed JSON near key: " + key);
    }
    position = skipWhitespace(text, position + 1);
    if (position >= text.size() || text[position] != '"') {
        throw std::runtime_error("Expected string value for key: " + key);
    }
    const auto end = text.find('"', position + 1);
    if (end == std::string::npos) {
        throw std::runtime_error("Unterminated string for key: " + key);
    }
    return text.substr(position + 1, end - position - 1);
}

long long extractInteger(const std::string& text, const std::string& key, const long long fallback) {
    const auto key_position = text.find("\"" + key + "\"");
    if (key_position == std::string::npos) {
        return fallback;
    }

    auto position = text.find(':', key_position);
    if (position == std::string::npos) {
        throw std::runtime_error("Malformed JSON near key: " + key);
    }
    position = skipWhitespace(text, position + 1);
    std::size_t end = position;
    while (end < text.size() && (text[end] == '-' || std::isdigit(static_cast<unsigned char>(text[end])))) {
        ++end;
    }
    if (end == position) {
        throw std::runtime_error("Expected integer value for key: " + key);
    }
    return std::stoll(text.substr(position, end - position));
}

bool extractBool(const std::string& text, const std::string& key, const bool fallback) {
    const auto key_position = text.find("\"" + key + "\"");
    if (key_position == std::string::npos) {
        return fallback;
    }

    auto position = text.find(':', key_position);
    if (position == std::string::npos) {
        throw std::runtime_error("Malformed JSON near key: " + key);
    }
    position = skipWhitespace(text, position + 1);
    if (text.compare(position, 4, "true") == 0) {
        return true;
    }
    if (text.compare(position, 5, "false") == 0) {
        return false;
    }
    throw std::runtime_error("Expected boolean value for key: " + key);
}

std::string extractObject(const std::string& text, const std::string& key) {
    const auto key_position = text.find("\"" + key + "\"");
    if (key_position == std::string::npos) {
        return {};
    }

    auto position = text.find(':', key_position);
    if (position == std::string::npos) {
        throw std::runtime_error("Malformed JSON near key: " + key);
    }
    position = skipWhitespace(text, position + 1);
    if (position >= text.size() || text[position] != '{') {
        throw std::runtime_error("Expected object for key: " + key);
    }

    int depth = 0;
    for (std::size_t index = position; index < text.size(); ++index) {
        if (text[index] == '{') {
            ++depth;
        } else if (text[index] == '}') {
            --depth;
            if (depth == 0) {
                return text.substr(position, index - position + 1);
            }
        }
    }

    throw std::runtime_error("Unterminated object for key: " + key);
}

std::string extractArray(const std::string& text, const std::string& key) {
    const auto key_position = text.find("\"" + key + "\"");
    if (key_position == std::string::npos) {
        return {};
    }

    auto position = text.find(':', key_position);
    if (position == std::string::npos) {
        throw std::runtime_error("Malformed JSON near key: " + key);
    }
    position = skipWhitespace(text, position + 1);
    if (position >= text.size() || text[position] != '[') {
        throw std::runtime_error("Expected array for key: " + key);
    }

    int depth = 0;
    for (std::size_t index = position; index < text.size(); ++index) {
        if (text[index] == '[') {
            ++depth;
        } else if (text[index] == ']') {
            --depth;
            if (depth == 0) {
                return text.substr(position, index - position + 1);
            }
        }
    }

    throw std::runtime_error("Unterminated array for key: " + key);
}

Endpoint parseEndpointObject(const std::string& object_text, const Endpoint& fallback = {}) {
    if (object_text.empty()) {
        return fallback;
    }
    Endpoint endpoint = fallback;
    endpoint.host = extractString(object_text, "host", endpoint.host);
    endpoint.port = static_cast<std::uint16_t>(extractInteger(object_text, "port", endpoint.port));
    return endpoint;
}

std::vector<Endpoint> parsePeerArray(const std::string& array_text) {
    std::vector<Endpoint> peers;
    if (array_text.empty()) {
        return peers;
    }

    std::size_t position = 0;
    while (true) {
        const auto start = array_text.find('{', position);
        if (start == std::string::npos) {
            break;
        }
        int depth = 0;
        for (std::size_t index = start; index < array_text.size(); ++index) {
            if (array_text[index] == '{') {
                ++depth;
            } else if (array_text[index] == '}') {
                --depth;
                if (depth == 0) {
                    peers.push_back(parseEndpointObject(array_text.substr(start, index - start + 1)));
                    position = index + 1;
                    break;
                }
            }
        }
        if (position <= start) {
            throw std::runtime_error("Malformed peer_nodes array");
        }
    }
    return peers;
}

std::chrono::milliseconds readDuration(const std::string& text, const std::string& key, const std::chrono::milliseconds fallback) {
    const auto value = extractInteger(text, key, fallback.count());
    if (value < 0) {
        throw std::runtime_error(key + " must be non-negative");
    }
    return std::chrono::milliseconds(value);
}
}  // namespace

std::string toString(NodeRole role) {
    switch (role) {
        case NodeRole::Master:
            return "master";
        case NodeRole::Slave:
            return "slave";
        case NodeRole::Candidate:
            return "candidate";
    }
    return "unknown";
}

NodeRole parseNodeRole(const std::string& raw) {
    if (raw == "master") {
        return NodeRole::Master;
    }
    if (raw == "slave") {
        return NodeRole::Slave;
    }
    throw std::runtime_error("Unsupported role: " + raw);
}

void validateConfig(const Config& config) {
    if (config.self.host.empty()) {
        throw std::runtime_error("self.host must not be empty");
    }
    if (config.dispatch_weights.critical < 0 || config.dispatch_weights.standard < 0 ||
        config.dispatch_weights.low < 0 || config.dispatch_weights.total() <= 0) {
        throw std::runtime_error("dispatch_weights must be non-negative and sum to a positive value");
    }
    if (config.heartbeat_interval.count() <= 0) {
        throw std::runtime_error("heartbeat_interval_ms must be greater than zero");
    }
    if (config.election_timeout.count() <= 0) {
        throw std::runtime_error("election_timeout_ms must be greater than zero");
    }
    if (config.aging_threshold.count() < 0) {
        throw std::runtime_error("aging_threshold_ms must be non-negative");
    }
    if (config.strong_write_timeout.count() <= 0) {
        throw std::runtime_error("strong_write_timeout_ms must be greater than zero");
    }
}

Config loadConfig(const std::string& path) {
    const auto text = readFile(path);

    Config config;
    config.role = parseNodeRole(extractString(text, "role", "master"));
    config.self = parseEndpointObject(extractObject(text, "self"), config.self);
    config.master = parseEndpointObject(extractObject(text, "master"), config.master);
    config.peer_nodes = parsePeerArray(extractArray(text, "peer_nodes"));
    config.heartbeat_interval = readDuration(text, "heartbeat_interval_ms", config.heartbeat_interval);
    config.election_timeout = readDuration(text, "election_timeout_ms", config.election_timeout);
    config.aging_threshold = readDuration(text, "aging_threshold_ms", config.aging_threshold);
    config.strong_write_timeout = readDuration(text, "strong_write_timeout_ms", config.strong_write_timeout);
    config.max_staleness_offset = static_cast<std::uint64_t>(extractInteger(text, "max_staleness_offset", config.max_staleness_offset));
    config.backlog_limit = static_cast<std::size_t>(extractInteger(text, "backlog_limit", config.backlog_limit));
    config.enable_logging = extractBool(text, "enable_logging", config.enable_logging);

    const auto dispatch_weights_object = extractObject(text, "dispatch_weights");
    if (!dispatch_weights_object.empty()) {
        config.dispatch_weights.critical = static_cast<int>(extractInteger(dispatch_weights_object, "critical", config.dispatch_weights.critical));
        config.dispatch_weights.standard = static_cast<int>(extractInteger(dispatch_weights_object, "standard", config.dispatch_weights.standard));
        config.dispatch_weights.low = static_cast<int>(extractInteger(dispatch_weights_object, "low", config.dispatch_weights.low));
    }

    validateConfig(config);
    return config;
}
