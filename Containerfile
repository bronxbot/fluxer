# ============================================================
# Fluxer Bronx Bot — Containerfile (Podman / Docker)
# Multi-stage build: compile in builder, copy binary to slim runtime
# ============================================================

# ── Stage 1: Build ──────────────────────────────────────────
FROM fedora:41 AS builder

RUN dnf install -y \
    cmake \
    make \
    gcc-c++ \
    git \
    mariadb-connector-c-devel \
    tesseract-devel \
    leptonica-devel \
    cairo-devel \
    openssl-devel \
    nlohmann-json-devel \
    pkg-config \
    && dnf clean all

WORKDIR /src

# Copy the entire fluxer library + bronx bot source
COPY . /src

# Build fluxerpp + bronx bot
RUN mkdir -p /src/bronx/build && cd /src/bronx/build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j$(nproc)

# ── Stage 2: Runtime ────────────────────────────────────────
FROM fedora:41

RUN dnf install -y \
    mariadb-connector-c \
    tesseract \
    leptonica \
    cairo \
    openssl \
    ca-certificates \
    && dnf clean all

WORKDIR /app

# Copy the compiled binary from the build stage
COPY --from=builder /src/bronx/build/bronx /app/bronx

# .env is mounted at runtime (never baked into the image)
# docker/podman run --env-file ./bronx/.env ...

# Non-root user for security
# RUN useradd -r -s /sbin/nologin bronx-bot
# USER bronx-bot

ENTRYPOINT ["/app/bronx"]
