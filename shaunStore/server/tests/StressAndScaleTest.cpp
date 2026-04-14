#include "../include/TestClusterBuilder.h"
#include "../include/Protocol.h"

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <optional>
#include <random>
#include <thread>

namespace {
Config baseConfig() {
    Config config;
    config.self.host = "127.0.0.1";
    config.self.port = 0;
    config.master = {"127.0.0.1", 0};
    config.enable_logging = false;
    config.heartbeat_interval = std::chrono::milliseconds(200);
    config.election_timeout = std::chrono::milliseconds(900);
    config.aging_threshold = std::chrono::milliseconds(100);
    config.strong_write_timeout = std::chrono::milliseconds(1500);
    config.max_staleness_offset = 25;
    return config;
}

int connectTo(const Endpoint& endpoint) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint.port);
    inet_pton(AF_INET, endpoint.host.c_str(), &address.sin_addr);
    if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

std::string readFrame(int fd) {
    char buffer[4096];
    const auto bytes = recv(fd, buffer, sizeof(buffer), 0);
    if (bytes <= 0) {
        return {};
    }
    return std::string(buffer, static_cast<std::size_t>(bytes));
}

std::string roundTrip(const Endpoint& endpoint, const std::vector<std::string>& tokens) {
    const int fd = connectTo(endpoint);
    if (fd < 0) {
        return {};
    }
    const auto payload = encodeArray(tokens);
    send(fd, payload.data(), payload.size(), 0);
    const auto response = readFrame(fd);
    close(fd);
    return response;
}

bool waitUntil(const std::function<bool()>& predicate, const std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return predicate();
}
}  // namespace

class StressAndScaleTest : public ::testing::Test {
protected:
    void TearDown() override {
        cluster_.shutdown();
    }

    TestClusterBuilder cluster_;
};

TEST_F(StressAndScaleTest, LargeScaleReplicationTest) {
    auto master_config = baseConfig();
    auto& master = cluster_.startMaster(master_config);

    for (int index = 0; index < 50; ++index) {
        auto slave_config = baseConfig();
        slave_config.master = master.endpoint();
        cluster_.addSlave(slave_config);
    }
    cluster_.wirePeers();

    constexpr int thread_count = 10;
    constexpr int writes_per_thread = 100;
    std::atomic<int> successful_writes {0};
    std::vector<std::thread> clients;

    for (int thread = 0; thread < thread_count; ++thread) {
        clients.emplace_back([&, thread] {
            for (int index = 0; index < writes_per_thread; ++index) {
                const auto key = "fanout:" + std::to_string(thread) + ":" + std::to_string(index);
                const auto value = "value:" + std::to_string(index);
                const auto response = roundTrip(master.endpoint(), {"SET", key, value, "--priority=standard"});
                if (response.rfind("+OK", 0) == 0) {
                    successful_writes.fetch_add(1);
                }
            }
        });
    }

    for (auto& client : clients) {
        client.join();
    }

    ASSERT_EQ(successful_writes.load(), thread_count * writes_per_thread);

    const auto expected_offset = static_cast<std::uint64_t>(thread_count * writes_per_thread);
    ASSERT_TRUE(waitUntil([&] {
        if (master.replicationOffset() != expected_offset) {
            return false;
        }
        for (const auto& slave : cluster_.slaves()) {
            if (slave->replicationOffset() != expected_offset) {
                return false;
            }
        }
        return true;
    }, std::chrono::seconds(15)));

    for (const auto& slave : cluster_.slaves()) {
        EXPECT_EQ(slave->replicationOffset(), master.replicationOffset());
        EXPECT_EQ(slave->localGet("fanout:0:0"), std::optional<std::string>("value:0"));
        EXPECT_EQ(slave->localGet("fanout:9:99"), std::optional<std::string>("value:99"));
    }
}

TEST_F(StressAndScaleTest, ElectionStormTest) {
    auto master_config = baseConfig();
    master_config.heartbeat_interval = std::chrono::milliseconds(100);
    auto& master = cluster_.startMaster(master_config);

    for (int index = 0; index < 30; ++index) {
        auto slave_config = baseConfig();
        slave_config.master = master.endpoint();
        slave_config.heartbeat_interval = std::chrono::milliseconds(100);
        slave_config.election_timeout = std::chrono::milliseconds(700);
        cluster_.addSlave(slave_config);
    }
    cluster_.wirePeers();

    ASSERT_TRUE(waitUntil([&] {
        for (const auto& slave : cluster_.slaves()) {
            if (slave->currentMaster().port != master.port()) {
                return false;
            }
        }
        return true;
    }, std::chrono::seconds(5)));

    master.simulateCrash();

    ASSERT_TRUE(waitUntil([&] {
        int masters = 0;
        Endpoint elected {};
        for (const auto& slave : cluster_.slaves()) {
            if (slave->role() == NodeRole::Master) {
                ++masters;
                elected = slave->endpoint();
            }
        }
        if (masters != 1) {
            return false;
        }
        for (const auto& slave : cluster_.slaves()) {
            if (slave->role() != NodeRole::Master && slave->currentMaster().port != elected.port) {
                return false;
            }
        }
        return true;
    }, std::chrono::seconds(20)));
}

TEST_F(StressAndScaleTest, PriorityStressTest) {
    auto master_config = baseConfig();
    master_config.aging_threshold = std::chrono::milliseconds(50);
    auto& master = cluster_.startMaster(master_config);

    for (int index = 0; index < 5; ++index) {
        auto slave_config = baseConfig();
        slave_config.master = master.endpoint();
        slave_config.aging_threshold = std::chrono::milliseconds(50);
        cluster_.addSlave(slave_config);
    }
    cluster_.wirePeers();

    const Endpoint master_endpoint = master.endpoint();
    std::atomic<int> launched {0};

    auto writer = [&](int start, int count, const std::string& prefix, const std::string& priority) {
        launched.fetch_add(1);
        while (launched.load() < 8) {
            std::this_thread::yield();
        }
        for (int index = 0; index < count; ++index) {
            const auto key = prefix + ":" + std::to_string(start + index);
            roundTrip(master_endpoint, {"SET", key, key, "--priority=" + priority});
        }
    };

    std::vector<std::thread> pool;
    pool.emplace_back(writer, 0, 1000, "low", "low");
    pool.emplace_back(writer, 1000, 1000, "low", "low");
    pool.emplace_back(writer, 2000, 1000, "low", "low");
    pool.emplace_back(writer, 3000, 1000, "low", "low");
    pool.emplace_back(writer, 4000, 1500, "std", "standard");
    pool.emplace_back(writer, 5500, 1500, "std", "standard");
    pool.emplace_back(writer, 7000, 250, "critical", "critical");
    pool.emplace_back(writer, 7250, 250, "critical", "critical");

    for (auto& thread : pool) {
        thread.join();
    }

    auto& probe = *cluster_.slaves().front();
    ASSERT_TRUE(waitUntil([&] {
        return probe.replicationOffset() == master.replicationOffset() &&
               probe.localGet("low:0").has_value() &&
               probe.localGet("critical:7499").has_value();
    }, std::chrono::seconds(20)));

    std::vector<std::uint64_t> critical_positions;
    std::vector<std::uint64_t> low_positions;
    for (int index = 0; index < 500; ++index) {
        critical_positions.push_back(probe.appliedIndexForKey("critical:" + std::to_string(7000 + index)));
    }
    for (int index = 0; index < 8000; ++index) {
        low_positions.push_back(probe.appliedIndexForKey("low:" + std::to_string(index)));
    }

    const auto critical_average = std::accumulate(critical_positions.begin(), critical_positions.end(), 0.0) /
                                  static_cast<double>(critical_positions.size());
    const auto low_average = std::accumulate(low_positions.begin(), low_positions.end(), 0.0) /
                             static_cast<double>(low_positions.size());

    EXPECT_LT(critical_average, low_average);
    EXPECT_TRUE(probe.localGet("low:3999").has_value());
    EXPECT_TRUE(probe.localGet("low:0").has_value());
}

TEST_F(StressAndScaleTest, ConnectionChurnTest) {
    auto master_config = baseConfig();
    master_config.election_timeout = std::chrono::seconds(10);
    auto& master = cluster_.startMaster(master_config);

    for (int index = 0; index < 10; ++index) {
        auto slave_config = baseConfig();
        slave_config.master = master.endpoint();
        slave_config.election_timeout = std::chrono::seconds(10);
        cluster_.addSlave(slave_config);
    }
    cluster_.wirePeers();

    std::atomic<bool> keep_writing {true};
    std::thread writer([&] {
        int index = 0;
        while (keep_writing.load()) {
            roundTrip(master.endpoint(), {"SET", "churn:" + std::to_string(index), std::to_string(index)});
            ++index;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    ASSERT_TRUE(waitUntil([&] {
        return master.replicationOffset() >= 20;
    }, std::chrono::seconds(5)));

    std::vector<SlaveNode*> disconnected;
    disconnected.push_back(cluster_.slaves()[1].get());
    disconnected.push_back(cluster_.slaves()[4].get());
    disconnected.push_back(cluster_.slaves()[7].get());

    for (auto* slave : disconnected) {
        slave->disconnectFromMaster();
    }

    const auto offset_before_disconnect = master.replicationOffset();

    ASSERT_TRUE(waitUntil([&] {
        return master.replicationOffset() >= offset_before_disconnect + 50;
    }, std::chrono::seconds(8)));

    for (auto* slave : disconnected) {
        slave->reconnectToMaster();
    }

    ASSERT_TRUE(waitUntil([&] {
        const auto target = master.replicationOffset();
        for (auto* slave : disconnected) {
            if (slave->replicationOffset() != target) {
                return false;
            }
        }
        return true;
    }, std::chrono::seconds(10)));

    keep_writing = false;
    writer.join();

    for (auto* slave : disconnected) {
        EXPECT_GT(slave->lastSyncRequestOffset(), 0U);
        EXPECT_GE(slave->lastSyncRequestOffset(), offset_before_disconnect);
        EXPECT_EQ(slave->replicationOffset(), master.replicationOffset());
        EXPECT_TRUE(slave->localGet("churn:0").has_value());
    }
}
