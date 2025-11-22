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
    libsqlite3-dev \
    libmysqlclient-dev \
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
    libsqlite3-0 \
    libmysqlclient21 \
    libvips42t64 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Copy the built binary from builder stage
COPY --from=builder /usr/local/bin/gara-image /usr/local/bin/gara-image

# Copy database schemas
COPY --from=builder /app/src/db/schema.sql /app/src/db/schema.sql
COPY --from=builder /app/src/db/schema_mysql.sql /app/src/db/schema_mysql.sql

# Create non-root user and data directories
RUN groupadd -g 10000 crowuser && useradd -u 10000 -g crowuser -s /usr/sbin/nologin crowuser && \
    mkdir -p /app/data/images /app/data && \
    chown -R crowuser:crowuser /app/data

USER crowuser

EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

CMD ["gara-image"]
