#include "../include/Protocol.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace {
bool parseBulkString(const std::string& buffer, std::vector<std::string>& tokens, std::size_t& position) {
    if (position >= buffer.size() || buffer[position] != '$') {
        return false;
    }
    ++position;
    const auto crlf = buffer.find("\r\n", position);
    if (crlf == std::string::npos) {
        return false;
    }
    const auto length = std::stoi(buffer.substr(position, crlf - position));
    position = crlf + 2;
    if (length < 0) {
        tokens.emplace_back();
        return true;
    }
    if (position + static_cast<std::size_t>(length) + 2 > buffer.size()) {
        return false;
    }
    tokens.push_back(buffer.substr(position, static_cast<std::size_t>(length)));
    position += static_cast<std::size_t>(length) + 2;
    return true;
}
}  // namespace

bool ClientCommand::isWrite() const {
    std::string lowered = name;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return lowered == "set" || lowered == "del";
}

bool parseRESP(const std::string& buffer, std::vector<std::string>& tokens, std::size_t& parsed_length) {
    tokens.clear();
    parsed_length = 0;

    if (buffer.empty() || buffer.front() != '*') {
        return false;
    }

    std::size_t position = 1;
    const auto crlf = buffer.find("\r\n", position);
    if (crlf == std::string::npos) {
        return false;
    }

    const auto elements = std::stoi(buffer.substr(position, crlf - position));
    position = crlf + 2;

    for (int index = 0; index < elements; ++index) {
        if (!parseBulkString(buffer, tokens, position)) {
            return false;
        }
    }

    parsed_length = position;
    return true;
}

std::string encodeArray(const std::vector<std::string>& tokens) {
    std::ostringstream response;
    response << '*' << tokens.size() << "\r\n";
    for (const auto& token : tokens) {
        response << '$' << token.size() << "\r\n" << token << "\r\n";
    }
    return response.str();
}

std::string simpleString(const std::string& value) {
    return "+" + value + "\r\n";
}

std::string errorString(const std::string& value) {
    return "-" + value + "\r\n";
}

std::string integerString(std::int64_t value) {
    return ":" + std::to_string(value) + "\r\n";
}

std::string bulkString(const std::optional<std::string>& value) {
    if (!value.has_value()) {
        return "$-1\r\n";
    }
    return "$" + std::to_string(value->size()) + "\r\n" + *value + "\r\n";
}

std::string toString(ReplicationPriority priority) {
    switch (priority) {
        case ReplicationPriority::Critical:
            return "critical";
        case ReplicationPriority::Standard:
            return "standard";
        case ReplicationPriority::Low:
            return "low";
    }
    return "standard";
}

std::string toString(ConsistencyMode consistency) {
    switch (consistency) {
        case ConsistencyMode::Strong:
            return "strong";
        case ConsistencyMode::Eventual:
            return "eventual";
        case ConsistencyMode::BoundedStaleness:
            return "staleness";
    }
    return "eventual";
}

std::optional<ReplicationPriority> parsePriority(const std::string& raw) {
    if (raw == "critical") {
        return ReplicationPriority::Critical;
    }
    if (raw == "standard") {
        return ReplicationPriority::Standard;
    }
    if (raw == "low") {
        return ReplicationPriority::Low;
    }
    return std::nullopt;
}

std::optional<ConsistencyMode> parseConsistency(const std::string& raw) {
    if (raw == "strong") {
        return ConsistencyMode::Strong;
    }
    if (raw == "eventual") {
        return ConsistencyMode::Eventual;
    }
    if (raw == "staleness") {
        return ConsistencyMode::BoundedStaleness;
    }
    return std::nullopt;
}

ClientCommand parseClientCommand(const std::vector<std::string>& tokens) {
    if (tokens.empty()) {
        throw std::runtime_error("empty command");
    }

    ClientCommand command;
    command.name = tokens.front();

    for (std::size_t index = 1; index < tokens.size(); ++index) {
        if (tokens[index].rfind("--consistency=", 0) == 0) {
            const auto parsed = parseConsistency(tokens[index].substr(14));
            if (!parsed.has_value()) {
                throw std::runtime_error("invalid consistency flag");
            }
            command.consistency = *parsed;
            continue;
        }
        if (tokens[index].rfind("--priority=", 0) == 0) {
            const auto parsed = parsePriority(tokens[index].substr(11));
            if (!parsed.has_value()) {
                throw std::runtime_error("invalid priority flag");
            }
            command.priority = *parsed;
            continue;
        }
        command.args.push_back(tokens[index]);
    }

    return command;
}
