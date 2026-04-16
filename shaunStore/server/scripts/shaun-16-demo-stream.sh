#!/usr/bin/env bash
# shaun-demo-streaming.sh - A high-fidelity terminal demo for the Distributed KV Store
# This script creates a 16-pane tmux session with colorful, simulated log streaming
# to visualize master-slave replication and client traffic.

SESSION="shaun-demo-visualizer"

# Colors for terminal output
C_MASTER="\e[1;31m"    # Red
C_SLAVE="\e[1;34m"     # Blue
C_CLIENT="\e[1;32m"    # Green
C_INFO="\e[1;37m"      # White
C_WARN="\e[1;33m"      # Yellow
C_RESET="\e[0m"

# Cleanup function to kill the session
cleanup() {
    tmux has-session -t "${SESSION}" 2>/dev/null && tmux kill-session -t "${SESSION}"
    echo "Demo session stopped."
    exit
}

# Ensure tmux is installed
if ! command -v tmux &> /dev/null; then
    echo "Error: tmux is not installed."
    exit 1
fi

case "${1:-}" in
    "master")
        clear
        echo -e "${C_MASTER}>>> SHAUNSTORE MASTER [PORT 8000] <<${C_RESET}"
        OFFSET=1000
        while true; do
            sleep $((RANDOM % 2 + 1))
            ((OFFSET++))
            KEYS=("user:882" "active_sessions" "system_config" "shard_alpha" "metrics_v2")
            K=${KEYS[$RANDOM % ${#KEYS[@]}]}
            
            echo -e "[$(date +%H:%M:%S)] ${C_INFO}INCOMING${C_RESET} | SET ${K} ..."
            echo -e "[$(date +%H:%M:%S)] ${C_WARN}QUORUM${C_RESET}   | Waiting for 6 ACKs (Offset: ${OFFSET})"
            sleep 0.2
            echo -e "[$(date +%H:%M:%S)] ${C_SLAVE}REPLICA${C_RESET}  | Broadcasting to 10 slaves"
            sleep 0.3
            echo -e "[$(date +%H:%M:%S)] ${C_MASTER}COMMIT${C_RESET}   | Quorum reached. Offset ${OFFSET} applied."
            echo ""
        done
        ;;
    "slave")
        PORT=$2
        clear
        echo -e "${C_SLAVE}>>> SHAUNSTORE SLAVE [PORT ${PORT}] <<${C_RESET}"
        while true; do
            sleep $((RANDOM % 3 + 1))
            echo -e "[$(date +%H:%M:%S)] ${C_INFO}HEARTBEAT${C_RESET} | Master 8000 online (Term: 12)"
            if [ $((RANDOM % 10)) -gt 6 ]; then
                echo -e "[$(date +%H:%M:%S)] ${C_MASTER}APPLY${C_RESET}     | Received delta sync (Port: 8000)"
                echo -e "[$(date +%H:%M:%S)] ${C_INFO}STATUS${C_RESET}    | Replication Offset: $((RANDOM % 500 + 1000))"
            fi
        done
        ;;
    "client")
        ID=$2
        clear
        echo -e "${C_CLIENT}>>> CLIENT INSTANCE #${ID} <<${C_RESET}"
        while true; do
            sleep $((RANDOM % 4 + 2))
            CMDS=("GET" "SET" "PING" "EXISTS" "DEL")
            CMD=${CMDS[$RANDOM % ${#CMDS[@]}]}
            echo -e "[$(date +%H:%M:%S)] ${C_CLIENT}REQUEST${C_RESET}  | SEND -> ${CMD} user:$((RANDOM%1000))"
            sleep 0.1
            echo -e "[$(date +%H:%M:%S)] ${C_INFO}RESPONSE${C_RESET} | OK (Latency: $((RANDOM%20 + 2))ms)"
            echo ""
        done
        ;;
    *)
        # Setup tmux layout
        tmux has-session -t "${SESSION}" 2>/dev/null && tmux kill-session -t "${SESSION}"
        
        # New session, detached
        tmux new-session -d -s "${SESSION}" "bash $0 master"
        
        # Create 10 Slave panes
        for port in {8001..8010}; do
            tmux split-window -t "${SESSION}" "bash $0 slave ${port}"
            tmux select-layout -t "${SESSION}" tiled
        done
        
        # Create 5 Client panes
        for id in {1..5}; do
            tmux split-window -t "${SESSION}" "bash $0 client ${id}"
            tmux select-layout -t "${SESSION}" tiled
        done
        
        tmux display-message -t "${SESSION}" "ShaunStore Visualizer Started"
        tmux attach-session -t "${SESSION}"
        ;;
esac
