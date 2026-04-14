#ifndef SHAUNSTORE_DATABASE_H
#define SHAUNSTORE_DATABASE_H

#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

class Database {
public:
    Database() = default;

    bool set(const std::string& key, const std::string& value);
    bool del(const std::string& key);
    std::optional<std::string> get(const std::string& key) const;
    bool exists(const std::string& key) const;
    void clear();
    std::size_t size() const;
    std::unordered_map<std::string, std::string> snapshot() const;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> key_value_store_;
};

#endif
