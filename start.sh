#!/usr/bin/env bash
# ============================================================
# start.sh — Build and start the Fluxer Bronx bot via Podman
# Usage: ./start.sh [--rebuild] [--verbose]
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"   # fluxer/ root (has CMakeLists.txt)

CONTAINER_NAME="bronx-fluxer"
IMAGE_NAME="bronx-fluxer:latest"
ENV_FILE="$SCRIPT_DIR/.env"

REBUILD=false
EXTRA_ARGS=""

for arg in "$@"; do
    case "$arg" in
        --rebuild) REBUILD=true ;;
        --verbose) EXTRA_ARGS="--verbose" ;;
    esac
done

# ── Sanity checks ───────────────────────────────────────────
if ! command -v podman &>/dev/null; then
    echo "[bronx] error: podman is not installed." >&2
    exit 1
fi

if [[ ! -f "$ENV_FILE" ]]; then
    echo "[bronx] error: .env file not found at $ENV_FILE" >&2
    echo "        copy .env.example to .env and fill in your credentials." >&2
    exit 1
fi

# ── Stop existing container if running ──────────────────────
if podman container exists "$CONTAINER_NAME" 2>/dev/null; then
    echo "[bronx] stopping existing container '$CONTAINER_NAME'..."
    podman stop "$CONTAINER_NAME" || true
    podman rm   "$CONTAINER_NAME" || true
fi

# ── Build image ─────────────────────────────────────────────
if $REBUILD || ! podman image exists "$IMAGE_NAME" 2>/dev/null; then
    echo "[bronx] building image '$IMAGE_NAME' (context: $REPO_ROOT)..."
    podman build \
        --file "$SCRIPT_DIR/Containerfile" \
        --tag  "$IMAGE_NAME" \
        "$REPO_ROOT"
else
    echo "[bronx] using existing image '$IMAGE_NAME' (pass --rebuild to force rebuild)."
fi

# ── Launch container ─────────────────────────────────────────
echo "[bronx] starting container '$CONTAINER_NAME'..."
podman run \
    --detach \
    --name        "$CONTAINER_NAME" \
    --env-file    "$ENV_FILE" \
    --network     host \
    --restart     on-failure:5 \
    "$IMAGE_NAME" $EXTRA_ARGS

echo "[bronx] container started. logs:"
echo "  podman logs -f $CONTAINER_NAME"
echo ""
echo "[bronx] to restart:"
echo "  ./restart.sh"
echo ""
echo "[bronx] to stop:"
echo "  podman stop $CONTAINER_NAME"
