# shaunStore: Distributed Key-Value Store

shaunStore is a Redis-inspired distributed key-value store built in C++. It supports distributed replication, adaptive consistency models, leader election, and concurrent client request handling using custom TCP socket communication.

---

# Features

* Distributed Master-Slave Architecture
* In-Memory Key-Value Storage
* Custom TCP Socket Communication
* RESP (Redis Serialization Protocol) Support
* Adaptive Consistency Models

  * Strong Consistency
  * Eventual Consistency
  * Bounded Staleness
* Priority-Based Replication Scheduling

  * Critical
  * Standard
  * Low
* Aging-Based Queue Promotion to Prevent Starvation
* Replication Backlog Recovery
* Raft-Style Leader Election
* Heartbeat-Based Failure Detection
* Multi-Threaded Cluster Management
* Thread-Safe Data Storage using Shared Mutexes

---

# Project Objective

Modern distributed systems require high availability, fault tolerance, and scalable data access. The objective of shaunStore is to demonstrate how these properties can be implemented from scratch using fundamental distributed systems concepts.

The project focuses on:

* Distributed replication
* Fault-tolerant leader election
* Consistency management
* Concurrent request processing
* Recovery from node failures

---

## Components

### ClusterNode

Core distributed systems component responsible for:

* Client communication
* Replication
* Heartbeats
* Leader election
* Failure recovery

### Database

Thread-safe in-memory key-value store built using:

```cpp
std::unordered_map<std::string, std::string>
```

Protected by:

```cpp
std::shared_mutex
```

to allow concurrent reads.

### PriorityReplicationEngine

Handles asynchronous replication using three priority levels:

```text
Critical
Standard
Low
```

Operations are scheduled using configurable weights and promoted through aging to prevent starvation.

### Protocol Layer

Implements RESP parsing and encoding for communication between:

* Clients and servers
* Masters and replicas

---

# Replication Workflow

```text
Client
  |
SET key value
  |
Master Database Update
  |
Create ReplicationEntry
  |
PriorityReplicationEngine
  |
Dispatcher Thread
  |
Replica Nodes
  |
Acknowledgement
```

Each write operation receives a replication offset that allows replicas to:

* Track synchronization state
* Recover missing operations
* Support bounded staleness checks

---

# Leader Election

shaunStore implements a Raft-inspired leader election mechanism.

## Election Flow

```text
Heartbeat Timeout
        |
     Candidate
        |
 Increase Term
        |
 Request Votes
        |
 Majority Votes
        |
 Become Master
```

Followers monitor periodic heartbeats from the leader.

If heartbeats stop arriving before the configured election timeout, a new election is triggered automatically.

---

# Consistency Models

## Eventual Consistency

The master acknowledges writes immediately.

Replication occurs asynchronously.

Provides the highest throughput.

---

## Strong Consistency

The master waits for acknowledgements from replicas before confirming a write.

Provides stronger correctness guarantees.

---

## Bounded Staleness

Allows replica reads while limiting acceptable replication lag.

Prevents serving excessively stale data.

---

# Technologies Used

| Category        | Technology                                   |
| --------------- | -------------------------------------------- |
| Language        | C++20                                        |
| Networking      | TCP Sockets                                  |
| Concurrency     | std::thread                                  |
| Synchronization | Mutexes, Shared Mutexes, Condition Variables |
| Protocol        | RESP                                         |
| Build System    | Make                                         |
| Data Structures | STL Containers                               |

---


# Building

## Prerequisites

* Linux or macOS
* C++20 Compatible Compiler
* Make

## Build

```bash
cd shaunStore
make
```

The executable is generated as:

```bash
./server
```

---

# Running the Cluster

## Start Master

```bash
./server tmp-manual/master.json
```

## Start Slave 1

```bash
./server tmp-manual/slave-8001.json
```

## Start Slave 2

```bash
./server tmp-manual/slave-8002.json
```

---

# Testing

## Ping

```bash
./scripts/resp_client.sh 127.0.0.1 8000 PING
```

Expected:

```text
+PONG
```

## Set a Value

```bash
./scripts/resp_client.sh 127.0.0.1 8000 SET name YourName
```

## Retrieve a Value

```bash
./scripts/resp_client.sh 127.0.0.1 8000 GET name
```

---

# Failure Recovery Demo

1. Start the cluster.
2. Terminate the master process.
3. Wait for the election timeout.
4. Observe a slave becoming the new leader.

This demonstrates:

* Heartbeat monitoring
* Failure detection
* Leader election
* Automatic recovery
