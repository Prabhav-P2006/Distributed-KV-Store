#!/usr/bin/env bash
echo 'Client 5: validator waiting for replication'
sleep 5
'/Users/prabhavp/dbms project/shaunStore/server/scripts/resp_client.sh' 127.0.0.1 8010 GET bulk:1
'/Users/prabhavp/dbms project/shaunStore/server/scripts/resp_client.sh' 127.0.0.1 8010 GET vip:1
