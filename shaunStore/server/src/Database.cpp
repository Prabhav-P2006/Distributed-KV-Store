#include "../include/Database.h"

#include <utility>

bool Database::set(const std::string& key, const std::string& value) {
    std::unique_lock lock(mutex_);
    key_value_store_[key] = value;
    return true;
}

bool Database::del(const std::string& key) {
    std::unique_lock lock(mutex_);
    return key_value_store_.erase(key) > 0;
}

std::optional<std::string> Database::get(const std::string& key) const {
    std::shared_lock lock(mutex_);
    const auto iterator = key_value_store_.find(key);
    if (iterator == key_value_store_.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

bool Database::exists(const std::string& key) const {
    std::shared_lock lock(mutex_);
    return key_value_store_.contains(key);
}

void Database::clear() {
    std::unique_lock lock(mutex_);
    key_value_store_.clear();
}

std::size_t Database::size() const {
    std::shared_lock lock(mutex_);
    return key_value_store_.size();
}

std::unordered_map<std::string, std::string> Database::snapshot() const {
    std::shared_lock lock(mutex_);
    return key_value_store_;
}
