# ============================================================
# Messenger C++ Backend — Multi-stage Dockerfile
# Stage 1: builder  (drogon base image with all deps)
# Stage 2: test     (runs unit tests — targeted by `make test`)
# Stage 3: runtime  (minimal image with only the binary)
# ============================================================

# ── Stage 1: builder ──────────────────────────────────────────────────────────
FROM drogonframework/drogon:latest AS builder

# Install extra deps not in the Drogon base image
RUN apt-get update && apt-get install -y --no-install-recommends \
        uuid-dev \
        libgtest-dev \
        ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -B build \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTS=ON \
    && cmake --build build --parallel "$(nproc)"

# ── Stage 2: test ─────────────────────────────────────────────────────────────
FROM builder AS test
WORKDIR /src/build
ENV JWT_SECRET="test-secret-at-least-16-chars-long"
ENV JWT_ACCESS_TTL=3600
ENV JWT_REFRESH_TTL=604800
RUN ctest --output-on-failure

# ── Stage 3: runtime ──────────────────────────────────────────────────────────
FROM debian:bookworm-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
        libssl3 \
        libpq5 \
        libhiredis0.14 \
        libuuid1 \
        libjsoncpp25 \
        libz3-4 \
        curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /src/build/messenger_api .

# Non-root user
RUN useradd -r -u 1001 -g 0 messenger
USER 1001

EXPOSE 8080

HEALTHCHECK --interval=15s --timeout=5s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

CMD ["./messenger_api"]
