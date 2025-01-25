#include "../include/ClusterNode.h"
#include "../include/Protocol.h"

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <optional>
#include <poll.h>
#include <random>
#include <stdexcept>
#include <sys/socket.h>
namespace {
std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}
}  // namespace

ClusterNode::ClusterNode(const Config& config)
    : config_(config),
      replication_engine_(config_),
      role_(config.role),
      current_master_(config.master),
      peer_nodes_(config.peer_nodes) {
    validateConfig(config_);
}

ClusterNode::~ClusterNode() {
    stop();
}

MasterNode::MasterNode(const Config& config)
    : ClusterNode(config) {}

SlaveNode::SlaveNode(const Config& config)
    : ClusterNode(config) {}

void ClusterNode::log(const std::string& message) const {
    if (config_.enable_logging) {
        std::clog << '[' << toString(role()) << ' ' << endpoint().toString() << "] " << message << '\n';
    }
}

bool ClusterNode::setNonBlocking(const int fd) {
    const auto flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

int ClusterNode::createClientSocket(const Endpoint& endpoint) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint.port);
    if (inet_pton(AF_INET, endpoint.host.c_str(), &address.sin_addr) <= 0) {
        close(fd);
        return -1;
    }

    if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close(fd);
        return -1;
    }

    setNonBlocking(fd);
    return fd;
}

bool ClusterNode::sendAll(const int fd, const std::string& payload, std::mutex* write_mutex) {
    std::unique_lock<std::mutex> guard;
    if (write_mutex != nullptr) {
        guard = std::unique_lock<std::mutex>(*write_mutex);
    }

    std::size_t written = 0;
    while (written < payload.size()) {
        const auto result = send(fd, payload.data() + written, payload.size() - written, 0);
        if (result > 0) {
            written += static_cast<std::size_t>(result);
            continue;
        }
        if (result < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
            pollfd poll_fd {fd, POLLOUT, 0};
            if (poll(&poll_fd, 1, 100) <= 0) {
                return false;
            }
            continue;
        }
        return false;
    }
    return true;
}

bool ClusterNode::createListener() {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }

    int enable = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(config_.self.port);
    address.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close(fd);
        return false;
    }

    if (listen(fd, SOMAXCONN) < 0) {
        close(fd);
        return false;
    }

    if (!setNonBlocking(fd)) {
        close(fd);
        return false;
    }

    sockaddr_in bound_address {};
    socklen_t bound_length = sizeof(bound_address);
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&bound_address), &bound_length) == 0) {
        config_.self.port = ntohs(bound_address.sin_port);
    }

    listener_fd_ = fd;
    return true;
}

void ClusterNode::start() {
    if (running_.exchange(true)) {
        return;
    }

    std::signal(SIGPIPE, SIG_IGN);
    if (!createListener()) {
        running_ = false;
        throw std::runtime_error("failed to start listener on " + config_.self.host);
    }

    replication_engine_.start();
    last_heartbeat_at_ = std::chrono::steady_clock::now();

    listener_thread_ = std::thread(&ClusterNode::listenerLoop, this);
    master_connection_thread_ = std::thread(&ClusterNode::masterConnectionLoop, this);
    dispatcher_thread_ = std::thread(&ClusterNode::dispatcherLoop, this);
    heartbeat_thread_ = std::thread(&ClusterNode::heartbeatLoop, this);
    election_thread_ = std::thread(&ClusterNode::electionLoop, this);
}

void ClusterNode::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    replication_engine_.stop();
    closeMasterSocket();

    const int listener = listener_fd_.exchange(-1);
    if (listener >= 0) {
        close(listener);
    }

    {
        std::scoped_lock lock(listener_mutex_);
        for (auto& [fd, connection] : connections_) {
            close(fd);
        }
        connections_.clear();
    }

    {
        std::scoped_lock lock(replica_mutex_);
        for (auto& [fd, session] : replica_sessions_) {
            close(fd);
        }
        replica_sessions_.clear();
    }

    if (listener_thread_.joinable()) {
        listener_thread_.join();
    }
    if (master_connection_thread_.joinable()) {
        master_connection_thread_.join();
    }
    if (dispatcher_thread_.joinable()) {
        dispatcher_thread_.join();
    }
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
    if (election_thread_.joinable()) {
        election_thread_.join();
    }
}

void ClusterNode::simulateCrash() {
    stop();
}

Endpoint ClusterNode::endpoint() const {
    return config_.self;
}

std::uint16_t ClusterNode::port() const {
    return config_.self.port;
}

NodeRole ClusterNode::role() const {
    return role_.load();
}

std::uint64_t ClusterNode::replicationOffset() const {
    return replication_offset_.load();
}

std::optional<std::string> ClusterNode::localGet(const std::string& key) const {
    return database_.get(key);
}

Endpoint ClusterNode::currentMaster() const {
    std::scoped_lock lock(config_mutex_);
    return current_master_;
}

std::vector<Endpoint> ClusterNode::peers() const {
    std::scoped_lock lock(config_mutex_);
    return peer_nodes_;
}

std::vector<std::string> ClusterNode::appliedKeys() const {
    std::scoped_lock lock(apply_mutex_);
    return applied_keys_;
}

std::uint64_t ClusterNode::appliedIndexForKey(const std::string& key) const {
    std::scoped_lock lock(apply_mutex_);
    const auto iterator = apply_order_.find(key);
    return iterator == apply_order_.end() ? 0 : iterator->second;
}

std::uint64_t ClusterNode::lastSyncRequestOffset() const {
    return last_sync_request_offset_.load();
}

void ClusterNode::setPeerNodes(const std::vector<Endpoint>& peers) {
    std::scoped_lock lock(config_mutex_);
    peer_nodes_ = peers;
}

void ClusterNode::disconnectFromMaster() {
    master_link_enabled_ = false;
    closeMasterSocket();
}

void ClusterNode::reconnectToMaster() {
    master_link_enabled_ = true;
}

void ClusterNode::closeMasterSocket() {
    const int fd = master_fd_.exchange(-1);
    if (fd >= 0) {
        close(fd);
    }
}

void ClusterNode::recordApplyOrder(const ReplicationEntry& entry) {
    if (entry.tokens.size() < 2) {
        return;
    }
    std::scoped_lock lock(apply_mutex_);
    applied_keys_.push_back(entry.tokens[1]);
    apply_order_[entry.tokens[1]] = applied_keys_.size();
}

void ClusterNode::appendToBacklog(const ReplicationEntry& entry) {
    std::scoped_lock lock(backlog_mutex_);
    backlog_.push_back(entry);
    while (backlog_.size() > config_.backlog_limit) {
        backlog_.pop_front();
    }
}

bool ClusterNode::isStaleRead() const {
    if (role() == NodeRole::Master) {
        return false;
    }
    const auto master_offset = known_master_offset_.load();
    const auto local_offset = replication_offset_.load();
    return master_offset > local_offset &&
           (master_offset - local_offset) > config_.max_staleness_offset;
}

std::size_t ClusterNode::liveReplicaCount() const {
    std::scoped_lock lock(replica_mutex_);
    return replica_sessions_.size();
}

void ClusterNode::applyReplicatedWrite(const ReplicationEntry& entry) {
    if (entry.tokens.empty()) {
        return;
    }

    const auto command = toLower(entry.tokens[0]);
    if (command == "set" && entry.tokens.size() >= 3) {
        database_.set(entry.tokens[1], entry.tokens[2]);
    } else if (command == "del" && entry.tokens.size() >= 2) {
        database_.del(entry.tokens[1]);
    }

    replication_offset_ = std::max(replication_offset_.load(), entry.offset);
    recordApplyOrder(entry);
}

void ClusterNode::registerReplica(Connection& connection, const std::vector<std::string>& tokens) {
    if (tokens.size() < 4) {
        sendAll(connection.fd, errorString("ERR invalid replica register"), &connection.write_mutex);
        connection.close_after_reply = true;
        return;
    }

    connection.replica_session = true;
    connection.remote_endpoint = Endpoint{tokens[1], static_cast<std::uint16_t>(std::stoi(tokens[2]))};
    const auto requested_offset = static_cast<std::uint64_t>(std::stoull(tokens[3]));

    auto session = std::make_shared<SlaveSession>();
    session->fd = connection.fd;
    session->endpoint = connection.remote_endpoint;
    {
        std::scoped_lock lock(replica_mutex_);
        replica_sessions_[connection.fd] = session;
    }

    log("Registered replica " + session->endpoint.toString() + " from offset " + std::to_string(requested_offset));
    sendBacklogToReplica(session, requested_offset);
}

void ClusterNode::handleAck(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) {
        return;
    }
    const auto offset = static_cast<std::uint64_t>(std::stoull(tokens[1]));
    const auto replica_port = static_cast<std::uint16_t>(std::stoi(tokens[2]));

    std::shared_ptr<AckTracker> tracker;
    {
        std::scoped_lock lock(ack_mutex_);
        const auto iterator = ack_trackers_.find(offset);
        if (iterator == ack_trackers_.end()) {
            return;
        }
        tracker = iterator->second;
    }

    std::scoped_lock lock(tracker->mutex);
    if (tracker->acked_ports.insert(replica_port).second) {
        ++tracker->ack_count;
        tracker->cv.notify_all();
    }
}

bool ClusterNode::handlePeerControl(Connection& connection, const std::vector<std::string>& tokens) {
    if (tokens.empty()) {
        return false;
    }

    const auto type = toLower(tokens[0]);
    if (type == "vote_request") {
        if (tokens.size() < 5) {
            sendAll(connection.fd, encodeArray({"VOTE_RESPONSE", "0", "0"}), &connection.write_mutex);
            connection.close_after_reply = true;
            return true;
        }

        const auto term = static_cast<std::uint64_t>(std::stoull(tokens[1]));
        const Endpoint candidate {tokens[2], static_cast<std::uint16_t>(std::stoi(tokens[3]))};
        const auto candidate_offset = static_cast<std::uint64_t>(std::stoull(tokens[4]));

        bool granted = false;
        {
            std::scoped_lock lock(config_mutex_);
            if (term > vote_state_.term) {
                vote_state_.term = term;
                vote_state_.voted_for = {};
            }

            const auto local_offset = replication_offset_.load();
            const bool up_to_date = candidate_offset >= local_offset;
            const bool preferred_candidate = !vote_state_.voted_for.valid() ||
                candidate_offset > local_offset ||
                (candidate_offset == local_offset && candidate < vote_state_.voted_for);

            if (term >= vote_state_.term && up_to_date && preferred_candidate) {
                vote_state_.term = term;
                vote_state_.voted_for = candidate;
                granted = true;
            }
        }

        sendAll(connection.fd,
                encodeArray({"VOTE_RESPONSE", std::to_string(term), granted ? "1" : "0"}),
                &connection.write_mutex);
        connection.close_after_reply = true;
        return true;
    }

    if (type == "new_master") {
        if (tokens.size() >= 4) {
            const auto term = static_cast<std::uint64_t>(std::stoull(tokens[1]));
            const Endpoint leader {tokens[2], static_cast<std::uint16_t>(std::stoi(tokens[3]))};
            becomeFollower(leader, term);
            sendAll(connection.fd, simpleString("OK"), &connection.write_mutex);
        }
        connection.close_after_reply = true;
        return true;
    }

    return false;
}

bool ClusterNode::handleIncomingMessage(Connection& connection, const std::vector<std::string>& tokens) {
    if (tokens.empty()) {
        return false;
    }

    const auto command = toLower(tokens[0]);
    if (command == "replica_register") {
        registerReplica(connection, tokens);
        return true;
    }
    if (command == "ack") {
        handleAck(tokens);
        return true;
    }
    if (command == "vote_request" || command == "new_master") {
        return handlePeerControl(connection, tokens);
    }

    std::thread(&ClusterNode::handleClientCommandAsync, this, connection.fd, tokens).detach();
    return true;
}

void ClusterNode::handleClientCommandAsync(const int fd, std::vector<std::string> tokens) {
    const auto response = processClientCommand(tokens);
    std::shared_ptr<Connection> connection;
    {
        std::scoped_lock lock(listener_mutex_);
        const auto iterator = connections_.find(fd);
        if (iterator == connections_.end()) {
            return;
        }
        connection = iterator->second;
    }
    sendAll(fd, response, &connection->write_mutex);
}

std::string ClusterNode::processReadCommand(const std::vector<std::string>& tokens) {
    const auto command = toLower(tokens[0]);

    if (command == "ping") {
        return simpleString("PONG");
    }
    if (command == "get") {
        if (tokens.size() != 2) {
            return errorString("ERR wrong number of arguments for GET");
        }
        if (isStaleRead()) {
            return errorString("STALE_DATA");
        }
        return bulkString(database_.get(tokens[1]));
    }
    return errorString("ERR unsupported read command");
}

std::string ClusterNode::processWriteCommand(const std::vector<std::string>& tokens) {
    ClientCommand command;
    try {
        command = parseClientCommand(tokens);
    } catch (const std::exception& exception) {
        return errorString(std::string("ERR ") + exception.what());
    }

    const auto lowered = toLower(command.name);
    if (role() != NodeRole::Master) {
        const auto leader = currentMaster();
        return errorString("READONLY MASTER_AT " + leader.host + " " + std::to_string(leader.port));
    }

    if ((lowered == "set" && command.args.size() != 2) || (lowered == "del" && command.args.size() != 1)) {
        return errorString("ERR invalid write command");
    }

    if (lowered == "set") {
        database_.set(command.args[0], command.args[1]);
    } else {
        database_.del(command.args[0]);
    }

    const auto offset = replication_offset_.fetch_add(1) + 1;
    ReplicationEntry entry;
    entry.offset = offset;
    entry.priority = command.priority;
    entry.consistency = command.consistency;
    entry.tokens.push_back(lowered == "set" ? "SET" : "DEL");
    entry.tokens.insert(entry.tokens.end(), command.args.begin(), command.args.end());

    appendToBacklog(entry);
    replication_engine_.enqueue(entry);

    if (command.consistency != ConsistencyMode::Strong) {
        return simpleString("OK");
    }

    const auto required = liveReplicaCount() == 0 ? 0U : static_cast<unsigned int>(liveReplicaCount() / 2 + 1);
    if (required == 0) {
        return simpleString("OK");
    }

    auto tracker = std::make_shared<AckTracker>(required);
    {
        std::scoped_lock lock(ack_mutex_);
        ack_trackers_[offset] = tracker;
    }

    std::unique_lock wait_lock(tracker->mutex);
    const auto success = tracker->cv.wait_for(wait_lock, config_.strong_write_timeout, [&tracker] {
        return tracker->ack_count >= tracker->required_acks;
    });
    wait_lock.unlock();

    {
        std::scoped_lock lock(ack_mutex_);
        ack_trackers_.erase(offset);
    }

    if (!success) {
        return errorString("ERR STRONG_WRITE_TIMEOUT");
    }
    return simpleString("OK");
}

std::string ClusterNode::processClientCommand(const std::vector<std::string>& tokens) {
    if (tokens.empty()) {
        return errorString("ERR empty command");
    }

    ClientCommand command;
    try {
        command = parseClientCommand(tokens);
    } catch (const std::exception& exception) {
        return errorString(std::string("ERR ") + exception.what());
    }

    if (command.isWrite()) {
        return processWriteCommand(tokens);
    }
    return processReadCommand(tokens);
}

void ClusterNode::listenerLoop() {
    while (running_) {
        std::vector<pollfd> poll_fds;
        poll_fds.push_back({listener_fd_.load(), POLLIN, 0});

        {
            std::scoped_lock lock(listener_mutex_);
            for (const auto& [fd, connection] : connections_) {
                poll_fds.push_back({fd, POLLIN, 0});
            }
        }

        if (poll(poll_fds.data(), poll_fds.size(), 100) <= 0) {
            continue;
        }

        for (auto& poll_fd : poll_fds) {
            if (!(poll_fd.revents & POLLIN)) {
                continue;
            }

            if (poll_fd.fd == listener_fd_.load()) {
                while (running_) {
                    sockaddr_in client_address {};
                    socklen_t length = sizeof(client_address);
                    const int client_fd = accept(listener_fd_.load(), reinterpret_cast<sockaddr*>(&client_address), &length);
                    if (client_fd < 0) {
                        break;
                    }
                    setNonBlocking(client_fd);
                    auto connection = std::make_shared<Connection>();
                    connection->fd = client_fd;
                    {
                        std::scoped_lock lock(listener_mutex_);
                        connections_[client_fd] = connection;
                    }
                }
                continue;
            }

            std::shared_ptr<Connection> connection;
            {
                std::scoped_lock lock(listener_mutex_);
                const auto iterator = connections_.find(poll_fd.fd);
                if (iterator == connections_.end()) {
                    continue;
                }
                connection = iterator->second;
            }

            char buffer[4096];
            while (running_) {
                const auto bytes = recv(poll_fd.fd, buffer, sizeof(buffer), 0);
                if (bytes > 0) {
                    connection->read_buffer.append(buffer, static_cast<std::size_t>(bytes));
                    continue;
                }
                if (bytes == 0) {
                    close(poll_fd.fd);
                    std::scoped_lock lock(listener_mutex_);
                    connections_.erase(poll_fd.fd);
                    std::scoped_lock replica_lock(replica_mutex_);
                    replica_sessions_.erase(poll_fd.fd);
                    break;
                }
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    break;
                }
                close(poll_fd.fd);
                std::scoped_lock lock(listener_mutex_);
                connections_.erase(poll_fd.fd);
                std::scoped_lock replica_lock(replica_mutex_);
                replica_sessions_.erase(poll_fd.fd);
                break;
            }

            while (true) {
                std::vector<std::string> tokens;
                std::size_t parsed_length = 0;
                if (!parseRESP(connection->read_buffer, tokens, parsed_length)) {
                    break;
                }
                connection->read_buffer.erase(0, parsed_length);
                handleIncomingMessage(*connection, tokens);
                if (connection->close_after_reply) {
                    close(connection->fd);
                    std::scoped_lock lock(listener_mutex_);
                    connections_.erase(connection->fd);
                    break;
                }
            }
        }
    }
}

void ClusterNode::connectToMasterIfNeeded() {
    if (role() == NodeRole::Master || !master_link_enabled_) {
        return;
    }
    if (master_fd_.load() >= 0) {
        return;
    }

    const auto leader = currentMaster();
    if (!leader.valid()) {
        return;
    }

    const int fd = createClientSocket(leader);
    if (fd < 0) {
        log("Failed to connect to master at " + leader.toString());
        return;
    }

    master_fd_ = fd;
    last_sync_request_offset_ = replication_offset_.load();
    const auto registration = encodeArray({
        "REPLICA_REGISTER",
        endpoint().host,
        std::to_string(endpoint().port),
        std::to_string(last_sync_request_offset_.load())
    });

    if (!sendAll(fd, registration)) {
        close(fd);
        master_fd_ = -1;
        log("Failed to register with master at " + leader.toString());
    } else {
        log("Connected to master at " + leader.toString() + " with offset " + std::to_string(last_sync_request_offset_.load()));
    }
}

void ClusterNode::masterConnectionLoop() {
    std::string buffer;
    while (running_) {
        if (role() == NodeRole::Master || !master_link_enabled_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        connectToMasterIfNeeded();
        const int fd = master_fd_.load();
        if (fd < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        pollfd poll_fd {fd, POLLIN, 0};
        if (poll(&poll_fd, 1, 100) <= 0) {
            continue;
        }

        char chunk[4096];
        const auto bytes = recv(fd, chunk, sizeof(chunk), 0);
        if (bytes <= 0) {
            closeMasterSocket();
            continue;
        }
        buffer.append(chunk, static_cast<std::size_t>(bytes));

        while (true) {
            std::vector<std::string> tokens;
            std::size_t parsed_length = 0;
            if (!parseRESP(buffer, tokens, parsed_length)) {
                break;
            }
            buffer.erase(0, parsed_length);

            if (tokens.empty()) {
                continue;
            }

            const auto type = toLower(tokens[0]);
            if (type == "repl_write" && tokens.size() >= 5) {
                ReplicationEntry entry;
                entry.offset = static_cast<std::uint64_t>(std::stoull(tokens[1]));
                entry.priority = parsePriority(tokens[2]).value_or(ReplicationPriority::Standard);
                entry.consistency = parseConsistency(tokens[3]).value_or(ConsistencyMode::Eventual);
                entry.tokens.assign(tokens.begin() + 4, tokens.end());
                applyReplicatedWrite(entry);
                known_master_offset_ = std::max(known_master_offset_.load(), entry.offset);
                const auto ack = encodeArray({"ACK", std::to_string(entry.offset), std::to_string(endpoint().port)});
                sendAll(fd, ack);
                log("Applied replicated write offset " + std::to_string(entry.offset));
                continue;
            }

            if (type == "heartbeat" && tokens.size() >= 5) {
                {
                    std::scoped_lock lock(heartbeat_mutex_);
                    last_heartbeat_at_ = std::chrono::steady_clock::now();
                }
                current_term_ = std::max(current_term_.load(), static_cast<std::uint64_t>(std::stoull(tokens[1])));
                known_master_offset_ = static_cast<std::uint64_t>(std::stoull(tokens[4]));
                log("Received heartbeat from " + tokens[2] + ":" + tokens[3] + " offset " + tokens[4]);
                continue;
            }

            if (type == "ok") {
                continue;
            }
        }
    }
}

void ClusterNode::sendBacklogToReplica(const std::shared_ptr<SlaveSession>& session, const std::uint64_t from_offset) {
    std::vector<ReplicationEntry> backlog_snapshot;
    {
        std::scoped_lock lock(backlog_mutex_);
        for (const auto& entry : backlog_) {
            if (entry.offset > from_offset) {
                backlog_snapshot.push_back(entry);
            }
        }
    }

    for (const auto& entry : backlog_snapshot) {
        auto payload = std::vector<std::string>{
            "REPL_WRITE",
            std::to_string(entry.offset),
            toString(entry.priority),
            toString(entry.consistency)
        };
        payload.insert(payload.end(), entry.tokens.begin(), entry.tokens.end());
        if (!sendAll(session->fd, encodeArray(payload), &session->write_mutex)) {
            break;
        }
    }
}

void ClusterNode::dispatcherLoop() {
    while (running_) {
        if (role() != NodeRole::Master) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        auto entry = replication_engine_.popNext();
        if (!entry.has_value()) {
            continue;
        }

        std::vector<std::shared_ptr<SlaveSession>> sessions;
        {
            std::scoped_lock lock(replica_mutex_);
            for (const auto& [fd, session] : replica_sessions_) {
                sessions.push_back(session);
            }
        }

        auto payload = std::vector<std::string>{
            "REPL_WRITE",
            std::to_string(entry->offset),
            toString(entry->priority),
            toString(entry->consistency)
        };
        payload.insert(payload.end(), entry->tokens.begin(), entry->tokens.end());
        const auto frame = encodeArray(payload);

        for (const auto& session : sessions) {
            if (!sendAll(session->fd, frame, &session->write_mutex)) {
                std::scoped_lock lock(replica_mutex_);
                replica_sessions_.erase(session->fd);
                close(session->fd);
            }
        }
    }
}

void ClusterNode::sendHeartbeatToSlaves() {
    const auto frame = encodeArray({
        "HEARTBEAT",
        std::to_string(current_term_.load()),
        endpoint().host,
        std::to_string(endpoint().port),
        std::to_string(replication_offset_.load())
    });

    std::vector<std::shared_ptr<SlaveSession>> sessions;
    {
        std::scoped_lock lock(replica_mutex_);
        for (const auto& [fd, session] : replica_sessions_) {
            sessions.push_back(session);
        }
    }

    for (const auto& session : sessions) {
        if (!sendAll(session->fd, frame, &session->write_mutex)) {
            std::scoped_lock lock(replica_mutex_);
            replica_sessions_.erase(session->fd);
            close(session->fd);
            log("Dropped replica session " + session->endpoint.toString() + " during heartbeat");
        } else {
            log("Heartbeat sent to " + session->endpoint.toString());
        }
    }
}

void ClusterNode::heartbeatLoop() {
    while (running_) {
        std::this_thread::sleep_for(config_.heartbeat_interval);
        if (role() == NodeRole::Master) {
            sendHeartbeatToSlaves();
        }
    }
}

void ClusterNode::becomeMaster() {
    role_ = NodeRole::Master;
    {
        std::scoped_lock lock(config_mutex_);
        current_master_ = endpoint();
    }
    closeMasterSocket();
    log("Promoted to master");
}

void ClusterNode::becomeFollower(const Endpoint& master_endpoint, const std::uint64_t term) {
    role_ = NodeRole::Slave;
    current_term_ = std::max(current_term_.load(), term);
    {
        std::scoped_lock lock(config_mutex_);
        current_master_ = master_endpoint;
        vote_state_.term = current_term_.load();
        vote_state_.voted_for = {};
    }
    {
        std::scoped_lock lock(heartbeat_mutex_);
        last_heartbeat_at_ = std::chrono::steady_clock::now();
    }
    closeMasterSocket();
    log("Following new master " + master_endpoint.toString() + " for term " + std::to_string(term));
}

bool ClusterNode::sendVoteRequest(const Endpoint& peer, const std::uint64_t term, const std::uint64_t offset) {
    const int fd = createClientSocket(peer);
    if (fd < 0) {
        return false;
    }

    const auto request = encodeArray({
        "VOTE_REQUEST",
        std::to_string(term),
        endpoint().host,
        std::to_string(endpoint().port),
        std::to_string(offset)
    });

    if (!sendAll(fd, request)) {
        close(fd);
        return false;
    }

    std::string buffer;
    char chunk[512];
    pollfd poll_fd {fd, POLLIN, 0};
    if (poll(&poll_fd, 1, 500) <= 0) {
        close(fd);
        return false;
    }
    const auto bytes = recv(fd, chunk, sizeof(chunk), 0);
    close(fd);
    if (bytes <= 0) {
        return false;
    }
    buffer.append(chunk, static_cast<std::size_t>(bytes));
    std::vector<std::string> tokens;
    std::size_t parsed_length = 0;
    if (!parseRESP(buffer, tokens, parsed_length) || tokens.size() < 3) {
        return false;
    }
    return tokens[0] == "VOTE_RESPONSE" && tokens[2] == "1";
}

void ClusterNode::broadcastNewMaster() {
    const auto peers_snapshot = peers();
    const auto frame = encodeArray({
        "NEW_MASTER",
        std::to_string(current_term_.load()),
        endpoint().host,
        std::to_string(endpoint().port)
    });
    for (const auto& peer : peers_snapshot) {
        const int fd = createClientSocket(peer);
        if (fd < 0) {
            continue;
        }
        sendAll(fd, frame);
        close(fd);
    }
}

void ClusterNode::startElectionRound() {
    role_ = NodeRole::Candidate;
    const auto term = current_term_.fetch_add(1) + 1;
    const auto local_offset = replication_offset_.load();
    log("Starting election term " + std::to_string(term) + " with offset " + std::to_string(local_offset));

    {
        std::scoped_lock lock(config_mutex_);
        vote_state_.term = term;
        vote_state_.voted_for = endpoint();
    }

    std::size_t votes = 1;
    const auto peers_snapshot = peers();
    for (const auto& peer : peers_snapshot) {
        if (sendVoteRequest(peer, term, local_offset)) {
            ++votes;
        }
    }

    const auto quorum = peers_snapshot.size() / 2 + 1;
    if (votes >= quorum + 1) {
        becomeMaster();
        broadcastNewMaster();
    } else {
        role_ = NodeRole::Slave;
        log("Election term " + std::to_string(term) + " failed with " + std::to_string(votes) + " vote(s)");
    }
}

void ClusterNode::electionLoop() {
    std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<int> jitter(0, 250);

    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (role() == NodeRole::Master || !master_link_enabled_) {
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point last_seen;
        {
            std::scoped_lock lock(heartbeat_mutex_);
            last_seen = last_heartbeat_at_;
        }

        if (now - last_seen < config_.election_timeout + std::chrono::milliseconds(jitter(generator))) {
            continue;
        }

        startElectionRound();
        {
            std::scoped_lock lock(heartbeat_mutex_);
            last_heartbeat_at_ = std::chrono::steady_clock::now();
        }
    }
}
