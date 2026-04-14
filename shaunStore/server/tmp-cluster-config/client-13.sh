#!/usr/bin/env bash
echo 'Client 3: read-heavy app cycling slaves'
for i in $(seq 1 18); do
  port=$((8001 + ((i - 1) % 3)))
  '/Users/prabhavp/dbms project/shaunStore/server/scripts/resp_client.sh' 127.0.0.1 ${port} GET bulk:1
  sleep 0.25
done
