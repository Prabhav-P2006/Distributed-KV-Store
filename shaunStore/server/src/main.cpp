#include "../include/ClusterNode.h"
#include "../include/Config.h"

#include <iostream>
#include <memory>
#include <thread>

int main(int argc, char* argv[]) {
    const std::string config_path = argc > 1 ? argv[1] : "config.json";

    try {
        const Config config = loadConfig(config_path);

        if (config.role == NodeRole::Master) {
            MasterNode master(config);
            master.start();
            std::cout << "Master started on " << master.endpoint().toString() << std::endl;
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

        SlaveNode slave(config);
        slave.start();
        std::cout << "Slave started on " << slave.endpoint().toString() << std::endl;
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception& exception) {
        std::cerr << "Failed to initialize node from " << config_path << ": " << exception.what() << std::endl;
        return 1;
    }
}
