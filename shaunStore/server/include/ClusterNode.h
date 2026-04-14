#ifndef SHAUNSTORE_CLUSTER_NODE_H
#define SHAUNSTORE_CLUSTER_NODE_H

#include "Config.h"
#include "Database.h"
#include "PriorityReplicationEngine.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ClusterNode {
public:
    explicit ClusterNode(const Config& config);
    virtual ~ClusterNode();

    ClusterNode(const ClusterNode&) = delete;
    ClusterNode& operator=(const ClusterNode&) = delete;

    void start();
    void stop();
    void simulateCrash();

    [[nodiscard]] Endpoint endpoint() const;
    [[nodiscard]] std::uint16_t port() const;
    [[nodiscard]] NodeRole role() const;
    [[nodiscard]] std::uint64_t replicationOffset() const;
    [[nodiscard]] std::optional<std::string> localGet(const std::string& key) const;
    [[nodiscard]] Endpoint currentMaster() const;
    [[nodiscard]] std::vector<Endpoint> peers() const;
    [[nodiscard]] std::vector<std::string> appliedKeys() const;
    [[nodiscard]] std::uint64_t appliedIndexForKey(const std::string& key) const;
    [[nodiscard]] std::uint64_t lastSyncRequestOffset() const;

    void setPeerNodes(const std::vector<Endpoint>& peers);
    void disconnectFromMaster();
    void reconnectToMaster();

protected:
    void log(const std::string& message) const;

private:
    struct Connection {
        int fd {-1};
        std::string read_buffer;
        std::mutex write_mutex;
        bool close_after_reply {false};
        bool replica_session {false};
        Endpoint remote_endpoint {};
    };

    struct SlaveSession {
        int fd {-1};
        Endpoint endpoint {};
        std::mutex write_mutex;
    };

    struct AckTracker {
        explicit AckTracker(std::size_t required) : required_acks(required) {}

        std::size_t required_acks {0};
        std::size_t ack_count {0};
        std::unordered_set<std::uint16_t> acked_ports;
        std::mutex mutex;
        std::condition_variable cv;
    };

    struct VoteState {
        std::uint64_t term {0};
        Endpoint voted_for {};
    };

    bool createListener();
    void listenerLoop();
    void masterConnectionLoop();
    void dispatcherLoop();
    void heartbeatLoop();
    void electionLoop();

    bool handleIncomingMessage(Connection& connection, const std::vector<std::string>& tokens);
    void handleClientCommandAsync(int fd, std::vector<std::string> tokens);
    std::string processClientCommand(const std::vector<std::string>& tokens);
    std::string processReadCommand(const std::vector<std::string>& tokens);
    std::string processWriteCommand(const std::vector<std::string>& tokens);
    void applyReplicatedWrite(const ReplicationEntry& entry);
    void appendToBacklog(const ReplicationEntry& entry);
    void recordApplyOrder(const ReplicationEntry& entry);

    void connectToMasterIfNeeded();
    void closeMasterSocket();
    void becomeMaster();
    void becomeFollower(const Endpoint& master_endpoint, std::uint64_t term);
    void startElectionRound();
    bool sendVoteRequest(const Endpoint& peer, std::uint64_t term, std::uint64_t offset);
    void broadcastNewMaster();
    void sendHeartbeatToSlaves();
    void sendBacklogToReplica(const std::shared_ptr<SlaveSession>& session, std::uint64_t from_offset);
    void registerReplica(Connection& connection, const std::vector<std::string>& tokens);
    void handleAck(const std::vector<std::string>& tokens);
    bool handlePeerControl(Connection& connection, const std::vector<std::string>& tokens);
    bool isStaleRead() const;
    std::size_t liveReplicaCount() const;

    static int createClientSocket(const Endpoint& endpoint);
    static bool setNonBlocking(int fd);
    static bool sendAll(int fd, const std::string& payload, std::mutex* write_mutex = nullptr);

    Config config_;
    Database database_;
    PriorityReplicationEngine replication_engine_;

    std::atomic<bool> running_ {false};
    std::atomic<bool> master_link_enabled_ {true};
    std::atomic<int> listener_fd_ {-1};
    std::atomic<int> master_fd_ {-1};
    std::atomic<std::uint64_t> replication_offset_ {0};
    std::atomic<std::uint64_t> known_master_offset_ {0};
    std::atomic<std::uint64_t> last_sync_request_offset_ {0};
    std::atomic<std::uint64_t> current_term_ {0};
    std::atomic<NodeRole> role_ {NodeRole::Slave};

    mutable std::mutex config_mutex_;
    Endpoint current_master_;
    std::vector<Endpoint> peer_nodes_;
    VoteState vote_state_;

    mutable std::mutex listener_mutex_;
    std::unordered_map<int, std::shared_ptr<Connection>> connections_;

    mutable std::mutex replica_mutex_;
    std::unordered_map<int, std::shared_ptr<SlaveSession>> replica_sessions_;

    mutable std::mutex backlog_mutex_;
    std::deque<ReplicationEntry> backlog_;

    mutable std::mutex ack_mutex_;
    std::unordered_map<std::uint64_t, std::shared_ptr<AckTracker>> ack_trackers_;

    mutable std::mutex apply_mutex_;
    std::vector<std::string> applied_keys_;
    std::unordered_map<std::string, std::uint64_t> apply_order_;

    mutable std::mutex heartbeat_mutex_;
    std::chrono::steady_clock::time_point last_heartbeat_at_ {std::chrono::steady_clock::now()};

    std::thread listener_thread_;
    std::thread master_connection_thread_;
    std::thread dispatcher_thread_;
    std::thread heartbeat_thread_;
    std::thread election_thread_;
};

class MasterNode final : public ClusterNode {
public:
    explicit MasterNode(const Config& config);
};

class SlaveNode final : public ClusterNode {
public:
    explicit SlaveNode(const Config& config);
};

#endif
