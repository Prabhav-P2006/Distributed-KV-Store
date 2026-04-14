#!/usr/bin/env bash
echo 'Client 1: bulk low-priority eventual writer'
for i in $(seq 1 10); do
  '/Users/prabhavp/dbms project/shaunStore/server/scripts/resp_client.sh' 127.0.0.1 8000 SET bulk:${i} value-${i} --consistency=eventual --priority=low
  sleep 0.4
done
