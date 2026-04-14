#!/usr/bin/env bash
echo 'Client 4: confused client writing to slave 8005'
'/Users/prabhavp/dbms project/shaunStore/server/scripts/resp_client.sh' 127.0.0.1 8005 SET rogue:key rogue-value --consistency=eventual --priority=standard
