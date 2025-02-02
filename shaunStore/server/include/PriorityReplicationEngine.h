#ifndef SHAUNSTORE_PRIORITY_REPLICATION_ENGINE_H
#define SHAUNSTORE_PRIORITY_REPLICATION_ENGINE_H

#include "Config.h"
#include "Protocol.h"

#include <condition_variable>
#include <deque>
#include <list>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <vector>

class PriorityReplicationEngine {
public:
    explicit PriorityReplicationEngine(const Config& config);
    ~PriorityReplicationEngine();

    void start();
    void stop();
    void enqueue(ReplicationEntry entry);
    std::optional<ReplicationEntry> popNext();
    [[nodiscard]] std::size_t size() const;

private:
    enum class Tier {
        Critical,
        Standard,
        Low
    };

    void ageLoop();
    void buildSchedule();
    std::optional<ReplicationEntry> popFromTierLocked(Tier tier);
    std::optional<ReplicationEntry> popAnyLocked();

    const Config& config_;
    mutable std::shared_mutex queue_mutex_;
    std::deque<ReplicationEntry> critical_queue_;
    std::list<ReplicationEntry> standard_queue_;
    std::list<ReplicationEntry> low_queue_;

    std::mutex signal_mutex_;
    std::condition_variable signal_cv_;
    std::thread aging_thread_;
    std::atomic<bool> running_ {false};

    std::vector<Tier> dispatch_schedule_;
    std::size_t schedule_index_ {0};
};

#endif
