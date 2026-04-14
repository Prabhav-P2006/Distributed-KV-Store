#ifndef SHAUNSTORE_TEST_CLUSTER_BUILDER_H
#define SHAUNSTORE_TEST_CLUSTER_BUILDER_H

#include "ClusterNode.h"

#include <memory>
#include <random>
#include <thread>

class TestClusterBuilder {
public:
    TestClusterBuilder() = default;
    ~TestClusterBuilder() {
        shutdown();
    }

    MasterNode& startMaster(Config config) {
        config.role = NodeRole::Master;
        config.enable_logging = false;
        master_ = std::make_unique<MasterNode>(config);
        master_->start();
        return *master_;
    }

    SlaveNode& addSlave(Config config) {
        config.role = NodeRole::Slave;
        config.enable_logging = false;
        slaves_.push_back(std::make_unique<SlaveNode>(config));
        slaves_.back()->start();
        return *slaves_.back();
    }

    void wirePeers() {
        std::vector<Endpoint> all_slave_endpoints;
        all_slave_endpoints.reserve(slaves_.size());
        for (const auto& slave : slaves_) {
            all_slave_endpoints.push_back(slave->endpoint());
        }

        for (std::size_t index = 0; index < slaves_.size(); ++index) {
            std::vector<Endpoint> peers;
            peers.reserve(slaves_.size() - 1);
            for (std::size_t other = 0; other < slaves_.size(); ++other) {
                if (index != other) {
                    peers.push_back(all_slave_endpoints[other]);
                }
            }
            slaves_[index]->setPeerNodes(peers);
        }
    }

    [[nodiscard]] MasterNode& master() {
        return *master_;
    }

    [[nodiscard]] std::vector<std::unique_ptr<SlaveNode>>& slaves() {
        return slaves_;
    }

    void shutdown() {
        for (auto& slave : slaves_) {
            if (slave) {
                slave->stop();
            }
        }
        slaves_.clear();
        if (master_) {
            master_->stop();
            master_.reset();
        }
    }

private:
    std::unique_ptr<MasterNode> master_;
    std::vector<std::unique_ptr<SlaveNode>> slaves_;
};

#endif
