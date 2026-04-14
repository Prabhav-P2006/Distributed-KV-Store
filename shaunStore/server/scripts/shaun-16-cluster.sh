#!/usr/bin/env bash
set -euo pipefail

SESSION="shaun-16-cluster"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIG_DIR="${ROOT_DIR}/tmp-cluster-config"
SERVER_BIN="${ROOT_DIR}/server"
CLIENT_BIN="${ROOT_DIR}/scripts/resp_client.sh"

cleanup() {
  tmux has-session -t "${SESSION}" 2>/dev/null && tmux kill-session -t "${SESSION}" >/dev/null 2>&1 || true
  pkill -f "${SERVER_BIN}" >/dev/null 2>&1 || true
}

trap cleanup INT TERM

mkdir -p "${CONFIG_DIR}"
chmod +x "${CLIENT_BIN}"

cat > "${CONFIG_DIR}/master.json" <<EOF
{
  "role": "master",
  "self": { "host": "127.0.0.1", "port": 8000 },
  "master": { "host": "127.0.0.1", "port": 8000 },
  "heartbeat_interval_ms": 500,
  "election_timeout_ms": 12000,
  "aging_threshold_ms": 3000,
  "strong_write_timeout_ms": 4000,
  "max_staleness_offset": 5,
  "backlog_limit": 50000,
  "dispatch_weights": { "critical": 70, "standard": 20, "low": 10 },
  "enable_logging": true,
  "peer_nodes": []
}
EOF

generate_peer_nodes() {
  local self_port="$1"
  local peer_json=""
  local first=true
  for port in $(seq 8001 8010); do
    if [[ "${port}" -eq "${self_port}" ]]; then
      continue
    fi
    if [[ "${first}" == true ]]; then
      first=false
    else
      peer_json+=", "
    fi
    peer_json+="{ \"host\": \"127.0.0.1\", \"port\": ${port} }"
  done
  printf '[ %s ]' "${peer_json}"
}

for port in $(seq 8001 8010); do
  peers="$(generate_peer_nodes "${port}")"
  cat > "${CONFIG_DIR}/slave-${port}.json" <<EOF
{
  "role": "slave",
  "self": { "host": "127.0.0.1", "port": ${port} },
  "master": { "host": "127.0.0.1", "port": 8000 },
  "heartbeat_interval_ms": 500,
  "election_timeout_ms": 12000,
  "aging_threshold_ms": 3000,
  "strong_write_timeout_ms": 4000,
  "max_staleness_offset": 5,
  "backlog_limit": 50000,
  "dispatch_weights": { "critical": 70, "standard": 20, "low": 10 },
  "enable_logging": true,
  "peer_nodes": ${peers}
}
EOF
done

pkill -f "${SERVER_BIN}" >/dev/null 2>&1 || true
tmux has-session -t "${SESSION}" 2>/dev/null && tmux kill-session -t "${SESSION}" >/dev/null 2>&1 || true

cat > "${CONFIG_DIR}/client-11.sh" <<EOF
#!/usr/bin/env bash
echo 'Client 1: bulk low-priority eventual writer'
for i in \$(seq 1 10); do
  '${CLIENT_BIN}' 127.0.0.1 8000 SET bulk:\${i} value-\${i} --consistency=eventual --priority=low
  sleep 0.4
done
EOF

cat > "${CONFIG_DIR}/client-12.sh" <<EOF
#!/usr/bin/env bash
echo 'Client 2: VIP critical strong writer'
for i in \$(seq 1 3); do
  '${CLIENT_BIN}' 127.0.0.1 8000 SET vip:\${i} critical-\${i} --consistency=strong --priority=critical
  sleep 1
done
EOF

cat > "${CONFIG_DIR}/client-13.sh" <<EOF
#!/usr/bin/env bash
echo 'Client 3: read-heavy app cycling slaves'
for i in \$(seq 1 18); do
  port=\$((8001 + ((i - 1) % 3)))
  '${CLIENT_BIN}' 127.0.0.1 \${port} GET bulk:1
  sleep 0.25
done
EOF

cat > "${CONFIG_DIR}/client-14.sh" <<EOF
#!/usr/bin/env bash
echo 'Client 4: confused client writing to slave 8005'
'${CLIENT_BIN}' 127.0.0.1 8005 SET rogue:key rogue-value --consistency=eventual --priority=standard
EOF

cat > "${CONFIG_DIR}/client-15.sh" <<EOF
#!/usr/bin/env bash
echo 'Client 5: validator waiting for replication'
sleep 5
'${CLIENT_BIN}' 127.0.0.1 8010 GET bulk:1
'${CLIENT_BIN}' 127.0.0.1 8010 GET vip:1
EOF

chmod +x "${CONFIG_DIR}"/client-1{1,2,3,4,5}.sh

tmux new-session -d -s "${SESSION}" -c "${ROOT_DIR}"
for _ in $(seq 1 15); do
  tmux split-window -t "${SESSION}":0 -c "${ROOT_DIR}"
  tmux select-layout -t "${SESSION}":0 tiled >/dev/null
done
tmux select-layout -t "${SESSION}":0 tiled >/dev/null
tmux set-option -t "${SESSION}" remain-on-exit on >/dev/null
tmux set-hook -t "${SESSION}" session-closed "run-shell \"pkill -f '${SERVER_BIN}' >/dev/null 2>&1 || true\"" >/dev/null

tmux send-keys -t "${SESSION}":0.0 "cd '${ROOT_DIR}' && '${SERVER_BIN}' '${CONFIG_DIR}/master.json'" C-m

pane_index=1
for port in $(seq 8001 8010); do
  tmux send-keys -t "${SESSION}":0."${pane_index}" "cd '${ROOT_DIR}' && '${SERVER_BIN}' '${CONFIG_DIR}/slave-${port}.json'" C-m
  pane_index=$((pane_index + 1))
done

sleep 2

tmux send-keys -t "${SESSION}":0.11 "bash '${CONFIG_DIR}/client-11.sh'" C-m
sleep 1
tmux send-keys -t "${SESSION}":0.12 "bash '${CONFIG_DIR}/client-12.sh'" C-m
sleep 1
tmux send-keys -t "${SESSION}":0.13 "bash '${CONFIG_DIR}/client-13.sh'" C-m
sleep 1
tmux send-keys -t "${SESSION}":0.14 "bash '${CONFIG_DIR}/client-14.sh'" C-m
sleep 1
tmux send-keys -t "${SESSION}":0.15 "bash '${CONFIG_DIR}/client-15.sh'" C-m

tmux display-message -t "${SESSION}" "shaunStore 16-pane cluster ready"

if [[ "${SHAUN_CLUSTER_NO_ATTACH:-0}" != "1" ]]; then
  tmux attach-session -t "${SESSION}"
fi
