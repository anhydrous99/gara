# Builder stage
FROM ubuntu:24.04 AS builder

WORKDIR /app

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
    zlib1g-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    libvips-dev \
    libglib2.0-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY . .

RUN mkdir -p build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_STATIC=OFF && \
    make -j$(nproc) && \
    make install && \
    strip --strip-unneeded /usr/local/bin/gara-image

# Runtime stage
FROM ubuntu:24.04

WORKDIR /app

# Install only runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libglib2.0-0 \
    libcurl4 \
    libssl3 \
    zlib1g \
    libstdc++6 \
    libvips42t64 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Copy the built binary from builder stage
COPY --from=builder /usr/local/bin/gara-image /usr/local/bin/gara-image

# Create non-root user
RUN groupadd -g 10000 crowuser && useradd -u 10000 -g crowuser -s /usr/sbin/nologin crowuser

USER crowuser

EXPOSE 80

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:80/health || exit 1

CMD ["gara-image"]
