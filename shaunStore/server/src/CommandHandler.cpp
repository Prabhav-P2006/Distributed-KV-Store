#include "../include/CommandHandler.h"
#include "../include/Database.h"
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <iostream>
#include <unordered_map>

CommandHandler::CommandHandler(){};

bool CommandHandler::parseSimpleString(const std::string& buffer, std::vector<std::string>& parsedCommand, size_t& pos) {
    if (pos >= buffer.size() || buffer[pos] != '+') {
        return false; // Invalid format
    }
    pos++; // Skip the '+'
    size_t endPos = buffer.find("\r\n", pos);
    if (endPos == std::string::npos) {
        return false; // Invalid format
    }
    parsedCommand.push_back(buffer.substr(pos, endPos - pos));
    return true;
}

bool CommandHandler::parseBulkString(const std::string& buffer, std::vector<std::string>& parsedCommand, size_t& pos) {
    if (pos >= buffer.size() || buffer[pos] != '$') {
        return false; // Invalid format
    }
    pos++; // Skip the '$'
    size_t crlf = buffer.find("\r\n", pos);
    if (crlf == std::string::npos) {
        return false; // Invalid format
    }
    int length = std::stoi(buffer.substr(pos, crlf - pos));
    pos = crlf + 2; // Move past \r\n
    if (length < 0) {
        parsedCommand.push_back(""); // Null bulk string
    } else {
        if (pos + length > buffer.size()) {
            return false; // Invalid format
        }
        parsedCommand.push_back(buffer.substr(pos, length));
        pos += length + 2; // Move past the string and \r\n
    }
    return true;
}

bool CommandHandler::parseInteger(const std::string& buffer, std::vector<std::string>& parsedCommand, size_t& pos) {
    if (pos >= buffer.size() || buffer[pos] != ':') {
        return false; // Invalid format
    }
    pos++; // Skip the ':'
    size_t endPos = buffer.find("\r\n", pos);
    if (endPos == std::string::npos) {
        return false; // Invalid format
    }
    parsedCommand.push_back(buffer.substr(pos, endPos - pos));
    pos = endPos + 2; // Move past \r\n
    return true;
}

bool CommandHandler::parseError(const std::string& buffer, std::vector<std::string>& parsedCommand, size_t& pos) {
    if (pos >= buffer.size() || buffer[pos] != '-') {
        return false; // Invalid format
    }
    pos++; // Skip the '-'
    size_t endPos = buffer.find("\r\n", pos);
    if (endPos == std::string::npos) {
        return false; // Invalid format
    }
    parsedCommand.push_back(buffer.substr(pos, endPos - pos));
    pos = endPos + 2; // Move past \r\n
    return true;
}

bool CommandHandler::parseArray(const std::string& buffer, std::vector<std::string>& parsedCommand, size_t& pos) {
    if (pos >= buffer.size() || buffer[pos] != '*') {
        return false; // Invalid format
    }
    pos++; // Skip the '*'
    size_t crlf = buffer.find("\r\n", pos);
    if (crlf == std::string::npos) {
        return false; // Invalid format
    }
    int numElements = std::stoi(buffer.substr(pos, crlf - pos));
    pos = crlf + 2; // Move past \r\n

    for (int i = 0; i < numElements; i++) {
        if (pos >= buffer.size()) {
            return false; // Invalid format
        }
        if (buffer[pos] == '$') {
            if (!parseBulkString(buffer, parsedCommand, pos)) {
                return false; // Invalid format
            }
        } else if (buffer[pos] == '+') {
            if (!parseSimpleString(buffer, parsedCommand, pos)) {
                return false; // Invalid format
            }
        } else if (buffer[pos] == ':') {
            if (!parseInteger(buffer, parsedCommand, pos)) {
                return false; // Invalid format
            }
        } else if (buffer[pos] == '-') {
            if (!parseError(buffer, parsedCommand, pos)) {
                return false; // Invalid format
            }
        } else if (buffer[pos] == '*') {
            if (!parseArray(buffer, parsedCommand, pos)) {
                return false;
            }
        }
        else {
            return false; // Invalid RESP format
        }
    }
    return true;
}

bool CommandHandler::parseRESP(const std::string& buffer, std::vector<std::string>& parsedCommand, size_t& parsedLen) {
    parsedCommand.clear();
    parsedLen = 0;

    if (buffer.empty()) {
        return false; // Empty buffer
    }
    std::cout << "Parsing RESP: " << buffer << std::endl;
    size_t pos = 0;
    if (buffer[pos] == '*') {
        if (!parseArray(buffer, parsedCommand, pos)) {
            return false; // Invalid RESP format
        }
    } else if (buffer[pos] == '$') {
        if (!parseBulkString(buffer, parsedCommand, pos)) {
            return false; // Invalid RESP format
        }
    } else if (buffer[pos] == '+') {
        if (!parseSimpleString(buffer, parsedCommand, pos)) {
            return false; // Invalid RESP format
        }
    } else if (buffer[pos] == ':') {
        if (!parseInteger(buffer, parsedCommand, pos)) {
            return false; // Invalid RESP format
        }
    } else if (buffer[pos] == '-') {
        if (!parseError(buffer, parsedCommand, pos)) {
            return false; // Invalid RESP format
        }
    } else {
        return false; // Invalid RESP format
    }

    parsedLen = pos; // Set parsed length to current position
    std::cout << "Parsed RESP length: " << parsedLen << std::endl;
    std::cout << "Parsed RESP tokens: ";
    for (const auto& token : parsedCommand) {
        std::cout << token << " ";
    }
    return true;
}

std::string CommandHandler::handlePing(const std::vector<std::string>& args, Database& db) {
    return "+PONG\r\n"; // RESP format for PING command
}

std::string CommandHandler::handleEcho(const std::vector<std::string>& args, Database& db) {
    if (args.size() != 2) {
        return "-ERR: Wrong number of arguments for 'echo' command\r\n"; // Return error in RESP format
    }
    return "$" + std::to_string(args[1].size()) + "\r\n" + args[1] + "\r\n"; // RESP format for ECHO command
}

std::string CommandHandler::handleFlushAll(const std::vector<std::string>& args, Database& db) {
    db.flushAll();
    return "+OK\r\n"; // RESP format for successful FLUSHALL command
}

std::string CommandHandler::handleSet(const std::vector<std::string>& args, Database& db) {
    if (args.size() != 3) {
        return "-ERR: Wrong number of arguments for 'set' command\r\n"; // Return error in RESP format
    }
    db.set(args[1], args[2]);
    return "+OK\r\n"; // RESP format for successful SET command
}

std::string CommandHandler::handleGet(const std::vector<std::string>& args, Database& db) {
    if (args.size() != 2) {
        return "-ERR: Wrong number of arguments for 'get' command\r\n"; // Return error in RESP format
    }
    std::string value = db.get(args[1]);
    if (value.empty()) {
        return "$-1\r\n"; // Null bulk string for non-existing key
    }
    return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n"; // RESP format for GET command
}

std::string CommandHandler::handleKeys(const std::vector<std::string>& args, Database& db) {
    std::vector<std::string> keys = db.keys();
    std::ostringstream response;
    response << "*" << keys.size() << "\r\n";
    for (const auto& key : keys) {
        response << "$" << key.size() << "\r\n" << key << "\r\n"; // RESP format for KEYS command
    }
    return response.str();
}

std::string CommandHandler::handleType(const std::vector<std::string>& args, Database& db) {
    if (args.size() != 2) {
        return "-ERR: Wrong number of arguments for 'type' command\r\n"; // Return error in RESP format
    }
    std::string type = db.type(args[1]);
    return "+TYPE " + type + "\r\n"; // RESP format for TYPE command
}

std::string CommandHandler::handleDel(const std::vector<std::string>& args, Database& db) {
    if (args.size() != 2) {
        return "-ERR: Wrong number of arguments for 'del' command\r\n"; // Return error in RESP format
    }
    bool deleted = db.del(args[1]);
    return (deleted ? ":1\r\n" : ":0\r\n"); // RESP format for DEL command
}

std::string CommandHandler::handleExists(const std::vector<std::string>& args, Database& db) {
    if (args.size() != 2) {
        return "-ERR: Wrong number of arguments for 'exists' command\r\n"; // Return error in RESP format
    }
    bool exists = db.exists(args[1]);
    return (exists ? ":1\r\n" : ":0\r\n"); // RESP format for EXISTS command
}

std::string CommandHandler::handleRename(const std::vector<std::string>& args, Database& db) {
    if (args.size() != 3) {
        return "-ERR: Wrong number of arguments for 'rename' command\r\n"; // Return error in RESP format
    }
    bool renamed = db.rename(args[1], args[2]);
    return (renamed ? "+OK\r\n" : "-ERR: Key does not exist\r\n"); // RESP format for RENAME command
}

std::string CommandHandler::handleExpiry(const std::vector<std::string>& args, Database& db) {
    if (args.size() != 3) {
        return "-ERR: Wrong number of arguments for 'expire' command\r\n"; // Return error in RESP format
    }
    int seconds = std::stoi(args[2]);
    bool expirySet = db.expiry(args[1], seconds);
    return (expirySet ? "+OK\r\n" : "-ERR: Key does not exist\r\n"); // RESP format for EXPIRE command
}

std::string CommandHandler::handleLlen(const std::vector<std::string> &args, Database& db) {
    if (args.size() < 2) 
        return "-Error: LLEN requires key\r\n";

    ssize_t len = db.llen(args[1]);
    if (len < 0) 
        return "-Error: Key does not exist or is not a list\r\n";
    return ":" + std::to_string(len) + "\r\n";
}

std::string CommandHandler::handleLget(const std::vector<std::string> &args, Database &db) {
    if (args.size() < 2) 
        return "-Error: LGET requires key\r\n";

    std::vector<std::string> list = db.lget(args[1]);
    if (list.empty()) 
        return "-Error: Key does not exist or is not a list\r\n";
    std::ostringstream response;
    response << "*" << list.size() << "\r\n";
    for (const auto &item : list) {
        response << "$" << item.size() << "\r\n" << item << "\r\n";
    }
    return response.str(); // RESP format for LGET command
}

std::string CommandHandler::handleLpush(const std::vector<std::string> &args, Database &db) {
    if (args.size() < 3) 
        return "-Error: LPUSH requires key and value\r\n";

    for (size_t i = 2; i < args.size(); ++i) {
        db.lpush(args[1], args[i]);
    }
    ssize_t len = db.llen(args[1]);
    return ":" + std::to_string(len) + "\r\n";
}

std::string CommandHandler::handleRpush(const std::vector<std::string> &args, Database &db) {
    if (args.size() < 3) 
        return "-Error: RPUSH requires key and value\r\n";

    for (size_t i = 2; i < args.size(); ++i) {
        db.rpush(args[1], args[i]);
    }
    ssize_t len = db.llen(args[1]);
    return ":" + std::to_string(len) + "\r\n";
}

std::string CommandHandler::handleLpop(const std::vector<std::string> &args, Database &db) {
    if (args.size() < 2) 
        return "-Error: LPOP requires key\r\n";

    std::string value = db.lpop(args[1]);
    if (value.empty()) 
        return "$-1\r\n"; // Null bulk string for non-existing key
    return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n"; // RESP format for LPOP command
}

std::string CommandHandler::handleRpop(const std::vector<std::string> &args, Database &db) {
    if (args.size() < 2) 
        return "-Error: RPOP requires key\r\n";
    
    std::string value = db.rpop(args[1]);
    if (value.empty()) 
        return "$-1\r\n"; // Null bulk string for non-existing key
    return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n"; // RESP format for RPOP command
}

std::string CommandHandler::handleLrem(const std::vector<std::string> &args, Database &db) {
    if (args.size() < 4) 
        return "-Error: LREM requires key, count and value\r\n";

    try {
        int count = std::stoi(args[2]);
        int removed = db.lrem(args[1], count, args[3]);
        return ":" + std::to_string(removed) + "\r\n";
    } catch (const std::exception&) {
        return "-Error: Invalid count\r\n";
    }
}

std::string CommandHandler::handleLindex(const std::vector<std::string> &args, Database &db) {
    if (args.size() < 3) 
        return "-Error: LINDEX requires key and index\r\n";

    std::string value = db.lindex(args[1], std::stoi(args[2]));

    if (value.empty()) {
        return "$-1\r\n"; // Null bulk string for non-existing key or index out of range
    }
    return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n"; // RESP format for LINDEX command
}

std::string CommandHandler::handleLset(const std::vector<std::string> &args, Database &db) {
    if (args.size() < 4) 
        return "-Error: LSET requires key, index and value\r\n";

    if (!db.lset(args[1], std::stoi(args[2]), args[3])) {
        return "-Error: Key does not exist or index out of range\r\n"; // Return error in RESP format
    }
    return "+OK\r\n"; // RESP format for successful LSET command
}

std::string CommandHandler::handleHset(const std::vector<std::string> &args, Database &db) {
    if (args.size() < 4) 
        return "-Error: HSET requires key, field and value\r\n";

    size_t numInserts = db.hset(args);

    if (numInserts <= 0) {
        return "-Error: Key does not exist\r\n"; // Return error in RESP format
    }
    else {
        return ":" + std::to_string(numInserts) + "\r\n"; // Multiple fields were added
    }
}

std::string CommandHandler::handleHget(const std::vector<std::string> &args, Database &db) {
    if (args.size() < 3) 
        return "-Error: HGET requires key and field\r\n";

    std::string value = db.hget(args[1], args[2]);
    if (value.empty()) {
        return "$-1\r\n"; // Null bulk string for non-existing key or field
    }
    return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n"; // RESP format for HGET command
}

std::string CommandHandler::handleHdel(const std::vector<std::string> &args, Database &db) {
    if (args.size() < 3) 
        return "-Error: HDEL requires key and field\r\n";

    size_t numDeleted = db.hdel(args[1], args[2]);
    if (numDeleted <= 0) {
        return "-Error: Key does not exist or field does not exist\r\n";
    }
    return ":" + std::to_string(numDeleted) + "\r\n";
}

std::string CommandHandler::handleHexists(const std::vector<std::string> &args, Database &db) {
    if (args.size() < 3) 
        return "-Error: HEXISTS requires key and field\r\n";

    bool exists = db.hexists(args[1], args[2]);
    return (exists ? ":1\r\n" : ":0\r\n"); // RESP format for HEXISTS command
}

std::string CommandHandler::handleHgetall(const std::vector<std::string> &args, Database &db) {
    if (args.size() < 2) 
        return "-Error: HGETALL requires key\r\n";

    std::unordered_map<std::string, std::string> hash = db.hgetall(args[1]);
    if (hash.empty()) {
        return "-Error: Key does not exist or is not a hash\r\n"; // Return error in RESP format
    }

    std::ostringstream response;
    response << "*" << (hash.size() * 2) << "\r\n"; // Each field-value pair counts as two elements
    for (const auto& pair : hash) {
        response << "$" << pair.first.size() << "\r\n" << pair.first << "\r\n";
        response << "$" << pair.second.size() << "\r\n" << pair.second << "\r\n";
    }
    return response.str(); // RESP format for HGETALL command
}

std::string CommandHandler::handleHkeys(const std::vector<std::string> &args, Database &db) {
    if (args.size() < 2) 
        return "-Error: HKEYS requires key\r\n";

    std::vector<std::string> keys = db.hkeys(args[1]);
    if (keys.empty()) {
        return "-Error: Key does not exist or is not a hash\r\n"; // Return error in RESP format
    }

    std::ostringstream response;
    response << "*" << keys.size() << "\r\n";
    for (const auto& key : keys) {
        response << "$" << key.size() << "\r\n" << key << "\r\n"; // RESP format for HKEYS command
    }
    return response.str();
}

std::string CommandHandler::handleHvals(const std::vector<std::string> &args, Database &db) {
    if (args.size() < 2) 
        return "-Error: HVALS requires key\r\n";

    std::vector<std::string> values = db.hvals(args[1]);
    if (values.empty()) {
        return "-Error: Key does not exist or is not a hash\r\n"; // Return error in RESP format
    }

    std::ostringstream response;
    response << "*" << values.size() << "\r\n";
    for (const auto& value : values) {
        response << "$" << value.size() << "\r\n" << value << "\r\n"; // RESP format for HVALS command
    }
    return response.str();
}

std::string CommandHandler::handleHlen(const std::vector<std::string> &args, Database &db) {
    if (args.size() < 2) 
        return "-Error: HLEN requires key\r\n";

    ssize_t len = db.hlen(args[1]);
    if (len < 0) {
        return "-Error: Key does not exist or is not a hash\r\n"; // Return error in RESP format
    }
    return ":" + std::to_string(len) + "\r\n"; // RESP format for HLEN command
}


// Handles the  command and returns the response.
std::string CommandHandler::handleCommand(const std::vector<std::string>& parsedCommand) {
    if (parsedCommand.empty()) {
        return "-Error: Empty Command\r\n"; // Return error in RESP format
    }

    std::string cmd = parsedCommand[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
    // Connect to DB
    Database& db = Database::getInstance();

    // Handle the command
    if (cmd == "ping") {
        return handlePing(parsedCommand, db);
    } else if (cmd == "echo") {
        return handleEcho(parsedCommand, db);
    } else if (cmd == "flushall") {
        return handleFlushAll(parsedCommand, db);
    } else if (cmd == "set") {
        return handleSet(parsedCommand, db);
    } else if (cmd == "get") {
        return handleGet(parsedCommand, db);
    } else if (cmd == "keys") {
        return handleKeys(parsedCommand, db);
    } else if (cmd == "type") {
        return handleType(parsedCommand, db);
    } else if (cmd == "del") {
        return handleDel(parsedCommand, db);
    } else if (cmd == "exists") {
        return handleExists(parsedCommand, db);
    } else if (cmd == "rename") {
        return handleRename(parsedCommand, db);
    } else if (cmd == "expire" || cmd == "ttl") {
        return handleExpiry(parsedCommand, db);
    } else if (cmd == "llen") {
        return handleLlen(parsedCommand, db);
    } else if (cmd == "lget") {
        return handleLget(parsedCommand, db);
    } else if (cmd == "lpush") {
        return handleLpush(parsedCommand, db);
    } else if (cmd == "rpush") {
        return handleRpush(parsedCommand, db);
    } else if (cmd == "lpop") {
        return handleLpop(parsedCommand, db);
    } else if (cmd == "rpop") {
        return handleRpop(parsedCommand, db);
    } else if (cmd == "lrem") {
        return handleLrem(parsedCommand, db);
    } else if (cmd == "lindex") {
        return handleLindex(parsedCommand, db);
    } else if (cmd == "lset") {
        return handleLset(parsedCommand, db);
    } else if (cmd == "hset") {
        return handleHset(parsedCommand, db);
    } else if (cmd == "hget") {
        return handleHget(parsedCommand, db);
    } else if (cmd == "hdel") {
        return handleHdel(parsedCommand, db);
    } else if (cmd == "hexists") {
        return handleHexists(parsedCommand, db);
    } else if (cmd == "hgetall") {
        return handleHgetall(parsedCommand, db);
    } else if (cmd == "hkeys") {
        return handleHkeys(parsedCommand, db);
    } else if (cmd == "hvals") {
        return handleHvals(parsedCommand, db);
    } else if (cmd == "hlen") {
        return handleHlen(parsedCommand, db);
    }
    else {
        return "-ERR: Unknown command\r\n";
    }
}


