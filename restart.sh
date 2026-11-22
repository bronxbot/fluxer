#!/usr/bin/env bash
# ============================================================
# restart.sh — Restart the Fluxer Bronx bot Podman container
# Usage: ./restart.sh [--rebuild] [--verbose]
#
#   --rebuild   Force a full image rebuild before restarting
#   --verbose   Pass --verbose to the bot binary
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONTAINER_NAME="bronx-fluxer"

echo "[bronx] restarting '$CONTAINER_NAME'..."

# If the container is running, stop it gracefully (10s timeout)
if podman container exists "$CONTAINER_NAME" 2>/dev/null; then
    echo "[bronx] stopping container..."
    podman stop --time 10 "$CONTAINER_NAME" || true
    podman rm "$CONTAINER_NAME"              || true
fi

# Delegate to start.sh, forwarding any flags (--rebuild, --verbose)
exec "$SCRIPT_DIR/start.sh" "$@"
