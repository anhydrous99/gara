# Builder stage
FROM alpine:latest as builder

WORKDIR /app

# Install build dependencies
RUN apk add --no-cache \
    build-base \
    cmake \
    git \
    pkgconfig \
    zlib-dev \
    openssl-dev \
    curl-dev \
    vips-dev \
    glib-dev

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
    libvips-cpp42 \
    libvips42 \
    libglib2.0-0 \
    libcurl4 \
    libssl3 \
    zlib1g \
    libstdc++6 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Copy the built binary from builder stage
COPY --from=builder /usr/local/bin/gara-image /usr/local/bin/gara-image

# Create non-root user
RUN addgroup -g 1000 crowuser && adduser -u 1000 -G crowuser -s /sbin/nologin -D crowuser

USER crowuser

EXPOSE 80

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:80/health || exit 1

CMD ["gara-image"]
