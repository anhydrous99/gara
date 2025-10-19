# Builder stage
FROM alpine:latest as builder

WORKDIR /app

# Install build dependencies
RUN apk add --no-cache \
    build-base \
    cmake \
    git \
    meson \
    ninja \
    pkgconfig \
    zlib-dev \
    openssl-dev \
    curl-dev \
    libjpeg-turbo-dev \
    libpng-dev \
    libwebp-dev \
    glib-dev \
    gobject-introspection-dev

COPY . .

RUN mkdir -p build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && make install

# Runtime stage
FROM alpine:latest

WORKDIR /app

# Install only runtime dependencies
RUN apk add --no-cache \
    glib \
    libcurl \
    openssl \
    zlib

# Copy the built binary from builder stage
COPY --from=builder /usr/local/bin/gara /usr/local/bin/gara

# Create non-root user
RUN addgroup -g 1000 crowuser && adduser -u 1000 -G crowuser -s /sbin/nologin -D crowuser

USER crowuser

EXPOSE 80

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

CMD ["gara"]
