# shaunStore - A redis like key value store

### Overview
Tried to implement a redis like key value store in C++. This server utilizes a single event loop with `epoll` to manage multiple clients at once.  It supports a subset of Redis commands and follows the Redis Serialization Protocol (RESP) for client-server communication. It supports Key Value stores, List Stores and Hash Stores.

---

### Features
- RESP Parsing
- Non-Blocking I/O : Uses `epoll` for handling multiple connections on a single thread.
- Persists data to disk
- Graceful shutdown with signal handling

---

### Commands that are implemented

#### Basic Commands
- `PING`
- `ECHO`
- `FLUSHALL`

#### KV Commands
- `SET`
- `GET`
- `KEYS`
- `TYPE`
- `DEL`
- `EXISTS`
- `RENAME`
- `EXPIRE`

#### List Commands
- `LLEN`
- `LGET`
- `LPUSH`
- `RPUSH`
- `LPOP`
- `RPOP`
- `LREM`
- `LINDEX`
- `LSET`

#### Hash Commands
- `HSET`
- `HGET`
- `HDEL`
- `HEXISTS`
- `HGETALL`
- `HKEYS`
- `HVALS`
- `HLEN`

---

### Todo:
- [] Implement more commands
- [] Add connection timeout and buffer limit