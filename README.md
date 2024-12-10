# shaunStore: Distributed Key-Value Store

shaunStore is a Redis-inspired distributed key-value store supporting concurrent client requests, in-memory caching, and low-latency distributed data access across master-slave node clusters.

## 📌 Project Statement
To develop a high-performance, distributed key-value store that supports fault-tolerant replication, Raft-style leader election, and high concurrent throughput using custom TCP-socket communication.

## 📚 Background
Scalable and highly available data storage is critical for modern distributed systems. While tools like Redis offer robust solutions, understanding the underlying mechanisms of distributed consensus, asynchronous replication, and non-blocking I/O is crucial. This project implements a master-slave cluster architecture from scratch, introducing adaptive consistency models and aging-based priority queues to improve data consistency and scalability in high-concurrency environments.

## 🔧 Methodology
### Techniques & Approach
- **Master-Slave Architecture** for distributed data access and redundancy.
- **Non-Blocking I/O** using `epoll` (or equivalent) for handling multiple connections on a single thread.
- **Raft-Style Leader Election** for fault-tolerant node management and high availability.
- **RESP (Redis Serialization Protocol)** for efficient client-server communication.
- **Asynchronous Replication** with a priority queue-based engine to keep slave nodes in sync.

## 🖼️ System Architecture
- **Core Node Phase:** Established a single-node in-memory KV, List, and Hash store.
- **Multi-Node Cluster:** Implemented custom TCP socket-based communication to form master-slave configurations.
- **Replication & Election:** Integrated fault-tolerant replication workflows and election mechanisms to handle simulated node failures.

## 🛠 Tools & Technologies
- **Languages:** C++
- **Networking:** TCP Sockets
- **Build Tools:** Makefile

## 🧰 Prerequisites
Make sure you are using Linux or macOS.
- G++ Compiler (C++17 or higher)

## 📦 Getting Started
First, clone the main repository:

```bash
git clone https://github.com/Prabhav-P2006/Distributed-KV-Store.git
cd Distributed-KV-Store
```

## ⚙️ Backend Server Setup
### 🔧 Build the Server
Inside the project directory, navigate to the server directory and build:

```bash
cd shaunStore/server
make
```

### ▶️ Run the Cluster Nodes
Run the following components to start your cluster:

**🧩 Master Node**
```bash
./build/shaunStore --config tmp-manual/master.json
```

**🖥️ Slave Nodes**
```bash
./build/shaunStore --config tmp-manual/slave-8001.json
./build/shaunStore --config tmp-manual/slave-8002.json
```

*(Alternatively, use the provided launch script to start the cluster automatically: `./scripts/launch-terminal-cluster.sh`)*

## ✅ Verify Cluster Status
Connect to the master node using the RESP client or standard `nc`:
```bash
./scripts/resp_client.sh 127.0.0.1 8000
```
Try running some commands:
```
PING
SET mykey "Hello Distributed World"
GET mykey
```

## 📈 Results & Analysis
### ✅ Key Outcomes
- Seamless distributed replication and automated leader election.
- Efficient processing of 1000+ concurrent operations across distributed nodes.

### 📊 Performance Metrics
- **Throughput:** Maximized through non-blocking event loops and optimized memory access.
- **Scalability:** Stable performance utilizing three-level synchronization mechanisms across multi-node setups.

## ✅ Conclusion
shaunStore showcases a robust architecture for distributed data storage. It successfully demonstrates core distributed systems concepts such as consensus, replication, and high-concurrency connection handling in a scalable environment.
