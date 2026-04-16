#!/usr/bin/env bash
# scripts/launch-terminal-cluster.sh
# Opens 16 terminal windows on macOS and tails the simulated log files.

LOG_DIR="/tmp/shaunstore-logs"
mkdir -p "$LOG_DIR"

# Clear old logs
echo "Clearing old logs..."
rm -f "$LOG_DIR"/*.log

# Function to open a terminal window
open_term() {
    local title="$1"
    local logfile="$2"
    local x=$3
    local y=$4
    
    touch "$logfile"
    
    # Use a simpler osascript call with quoted variables
    osascript -e "tell application \"Terminal\"" \
              -e "  activate" \
              -e "  set newWin to do script \"printf '\\\\033]2;${title}\\\\007'; clear; echo 'Waiting for logs for ${title}...'; tail -f '${logfile}'\"" \
              -e "  set bounds of window 1 to {${x}, ${y}, ${x} + 400, ${y} + 280}" \
              -e "end tell"
}

echo "Launching 16 terminal windows..."

# Master
open_term "MASTER_8000" "$LOG_DIR/master.log" 0 0

# 10 Slaves
for i in {1..10}; do
    port=$((8000 + i))
    row=$(( i / 4 ))
    col=$(( i % 4 ))
    open_term "SLAVE_$port" "$LOG_DIR/slave-$port.log" $((col * 410)) $((row * 290))
done

# 5 Clients
for i in {1..5}; do
    col=$(( (i - 1) % 4 ))
    row=$(( 3 + (i - 1) / 4 ))
    open_term "CLIENT_$i" "$LOG_DIR/client-$i.log" $((col * 410)) $((row * 290))
done

echo "Cluster terminals launched."
echo "1. Run log server: node ../dashboard/log-server.cjs"
echo "2. Open dashboard: http://localhost:5174/"
