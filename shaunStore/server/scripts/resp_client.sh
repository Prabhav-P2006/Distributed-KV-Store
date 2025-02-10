#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "usage: $0 <host> <port> <command> [args...]" >&2
  exit 1
fi

HOST="$1"
PORT="$2"
shift 2

python3 - "$HOST" "$PORT" "$@" <<'PY'
import socket
import sys

host = sys.argv[1]
port = int(sys.argv[2])
tokens = sys.argv[3:]

payload = f"*{len(tokens)}\r\n"
for token in tokens:
    encoded = token.encode("utf-8")
    payload += f"${len(encoded)}\r\n{token}\r\n"

with socket.create_connection((host, port), timeout=3) as sock:
    sock.settimeout(1.0)
    sock.sendall(payload.encode("utf-8"))
    response = bytearray()
    while True:
        try:
            chunk = sock.recv(4096)
        except TimeoutError:
            break
        if not chunk:
            break
        response.extend(chunk)

sys.stdout.write(response.decode("utf-8", errors="replace"))
PY
