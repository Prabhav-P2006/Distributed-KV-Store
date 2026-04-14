#include "../include/PriorityReplicationEngine.h"

#include <algorithm>
#include <chrono>
#include <numeric>

PriorityReplicationEngine::PriorityReplicationEngine(const Config& config)
    : config_(config) {
    buildSchedule();
}

PriorityReplicationEngine::~PriorityReplicationEngine() {
    stop();
}

void PriorityReplicationEngine::start() {
    if (running_.exchange(true)) {
        return;
    }
    aging_thread_ = std::thread(&PriorityReplicationEngine::ageLoop, this);
}

void PriorityReplicationEngine::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    signal_cv_.notify_all();
    if (aging_thread_.joinable()) {
        aging_thread_.join();
    }
}

void PriorityReplicationEngine::buildSchedule() {
    const auto a = std::max(1, config_.dispatch_weights.critical);
    const auto b = std::max(1, config_.dispatch_weights.standard);
    const auto c = std::max(1, config_.dispatch_weights.low);
    const auto gcd = std::gcd(std::gcd(a, b), c);
    dispatch_schedule_.clear();
    dispatch_schedule_.insert(dispatch_schedule_.end(), a / gcd, Tier::Critical);
    dispatch_schedule_.insert(dispatch_schedule_.end(), b / gcd, Tier::Standard);
    dispatch_schedule_.insert(dispatch_schedule_.end(), c / gcd, Tier::Low);
}

void PriorityReplicationEngine::enqueue(ReplicationEntry entry) {
    {
        std::unique_lock lock(queue_mutex_);
        entry.enqueued_at = std::chrono::steady_clock::now();
        switch (entry.priority) {
            case ReplicationPriority::Critical:
                critical_queue_.push_back(std::move(entry));
                break;
            case ReplicationPriority::Standard:
                standard_queue_.push_back(std::move(entry));
                break;
            case ReplicationPriority::Low:
                low_queue_.push_back(std::move(entry));
                break;
        }
    }
    signal_cv_.notify_one();
}

std::optional<ReplicationEntry> PriorityReplicationEngine::popFromTierLocked(Tier tier) {
    switch (tier) {
        case Tier::Critical:
            if (!critical_queue_.empty()) {
                auto entry = critical_queue_.front();
                critical_queue_.pop_front();
                return entry;
            }
            break;
        case Tier::Standard:
            if (!standard_queue_.empty()) {
                auto entry = standard_queue_.front();
                standard_queue_.pop_front();
                return entry;
            }
            break;
        case Tier::Low:
            if (!low_queue_.empty()) {
                auto entry = low_queue_.front();
                low_queue_.pop_front();
                return entry;
            }
            break;
    }
    return std::nullopt;
}

std::optional<ReplicationEntry> PriorityReplicationEngine::popAnyLocked() {
    if (auto entry = popFromTierLocked(Tier::Critical); entry.has_value()) {
        return entry;
    }
    if (auto entry = popFromTierLocked(Tier::Standard); entry.has_value()) {
        return entry;
    }
    if (auto entry = popFromTierLocked(Tier::Low); entry.has_value()) {
        return entry;
    }
    return std::nullopt;
}

std::optional<ReplicationEntry> PriorityReplicationEngine::popNext() {
    std::unique_lock wait_lock(signal_mutex_);
    signal_cv_.wait_for(wait_lock, std::chrono::milliseconds(50), [this] {
        return !running_ || size() > 0;
    });
    wait_lock.unlock();

    std::unique_lock lock(queue_mutex_);
    if (!running_ && critical_queue_.empty() && standard_queue_.empty() && low_queue_.empty()) {
        return std::nullopt;
    }
    if (critical_queue_.empty() && standard_queue_.empty() && low_queue_.empty()) {
        return std::nullopt;
    }

    for (std::size_t attempts = 0; attempts < dispatch_schedule_.size(); ++attempts) {
        const auto tier = dispatch_schedule_[schedule_index_ % dispatch_schedule_.size()];
        ++schedule_index_;
        if (auto entry = popFromTierLocked(tier); entry.has_value()) {
            return entry;
        }
    }

    return popAnyLocked();
}

std::size_t PriorityReplicationEngine::size() const {
    std::shared_lock lock(queue_mutex_);
    return critical_queue_.size() + standard_queue_.size() + low_queue_.size();
}

void PriorityReplicationEngine::ageLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        const auto now = std::chrono::steady_clock::now();

        std::unique_lock lock(queue_mutex_);

        for (auto iterator = low_queue_.begin(); iterator != low_queue_.end();) {
            if (now - iterator->enqueued_at >= config_.aging_threshold) {
                iterator->priority = ReplicationPriority::Standard;
                iterator->enqueued_at = now;
                standard_queue_.push_back(*iterator);
                iterator = low_queue_.erase(iterator);
            } else {
                ++iterator;
            }
        }

        for (auto iterator = standard_queue_.begin(); iterator != standard_queue_.end();) {
            if (now - iterator->enqueued_at >= config_.aging_threshold) {
                iterator->priority = ReplicationPriority::Critical;
                iterator->enqueued_at = now;
                critical_queue_.push_back(*iterator);
                iterator = standard_queue_.erase(iterator);
            } else {
                ++iterator;
            }
        }
    }
}
