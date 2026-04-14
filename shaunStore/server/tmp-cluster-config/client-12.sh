#!/usr/bin/env bash
echo 'Client 2: VIP critical strong writer'
for i in $(seq 1 3); do
  '/Users/prabhavp/dbms project/shaunStore/server/scripts/resp_client.sh' 127.0.0.1 8000 SET vip:${i} critical-${i} --consistency=strong --priority=critical
  sleep 1
done
