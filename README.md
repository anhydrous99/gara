# Gara Image Service

[![Tests](https://github.com/anhydrous99/gara/actions/workflows/test.yml/badge.svg?branch=main)](https://github.com/anhydrous99/gara/actions/workflows/test.yml)

High-performance image transformation service built with C++, Crow, AWS S3, and libvips.

## Features

- Upload images up to 100MB (JPEG, PNG, GIF, TIFF, WebP)
- On-demand format conversion (JPEG, PNG, WebP, TIFF) and resizing
- S3-backed caching (no re-computation)
- Hash-based deduplication
- Presigned URLs for secure access
- **Static builds by default** - single binary, no dependencies

## Quick Start

```bash
# Install dependencies
brew install openssl zlib cmake meson ninja pkg-config glib jpeg libpng webp  # macOS
# OR
sudo apt-get install libssl-dev zlib1g-dev cmake build-essential \
    meson ninja-build pkg-config libglib2.0-dev \
    libjpeg-dev libpng-dev libwebp-dev  # Ubuntu

# Configure AWS
aws configure
export S3_BUCKET_NAME=gara-images
export AWS_REGION=us-east-1
aws s3 mb s3://gara-images

# Build (static by default, libvips auto-built)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j

# Run
export PORT=8080
./gara
```

See [QUICK_START.md](QUICK_START.md) for a 5-minute setup guide.

## API

### Upload Image
```bash
curl -X POST http://localhost:8080/api/images/upload -F "file=@image.jpg"
# Returns: {"image_id": "abc123...", "message": "Image uploaded successfully"}
```

### Get/Transform Image
```bash
# Get as JPEG
curl "http://localhost:8080/api/images/abc123?format=jpeg&width=800"
# Returns: {"url": "https://s3.amazonaws.com/...", "expires_in": 3600}
```

**Query params**:
- `format` (jpeg|png|webp|tiff) - default: jpeg
- `width` - target width in pixels (0 = maintain aspect ratio)
- `height` - target height in pixels (0 = maintain aspect ratio)

### Health Check
```bash
curl http://localhost:8080/api/images/health
```

## How It Works

1. **Upload**: Image → SHA256 hash → S3 `raw/{hash}.{ext}`
2. **Transform**: Request → Check S3 cache → Transform if needed → Cache → Return presigned URL
3. **Cache hit**: Instant URL (no processing)

## Build Options

```bash
# Static build (default) - single binary, no runtime deps
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j

# Dynamic build
cmake .. -DBUILD_STATIC=OFF

# Minimal size build
cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel
make -j && strip gara  # ~50MB
```

**Verify static build** (Linux):
```bash
ldd ./gara  # Should show "not a dynamic executable"
```

## Dependencies

**System** (required for libvips build):
- OpenSSL, zlib
- CMake, meson, ninja, pkg-config, glib
- Image format libraries: libjpeg, libpng, libwebp (for image processing)

**Auto-fetched by CMake**:
- ASIO, Crow, nlohmann/json, AWS SDK, Google Test, **libvips** (built from source)

## Environment Variables

```bash
S3_BUCKET_NAME=gara-images  # Required
AWS_REGION=us-east-1        # Required
PORT=80                     # Optional (default: 80)
TEMP_UPLOAD_DIR=/tmp        # Optional
```

## Testing

```bash
# Run all tests (~47 tests)
cd build && ctest --output-on-failure

# Run specific tests
./gara_tests --gtest_filter="FileUtilsTest.*"
```

See [TESTING.md](TESTING.md) for details.

## Deployment

**Static binary** - copy and run anywhere:
```bash
# Build
cmake .. -DCMAKE_BUILD_TYPE=Release && make -j && strip gara

# Deploy to server
scp gara user@server:/opt/gara/
ssh user@server '/opt/gara/gara'  # No deps needed!
```

**Docker** - minimal scratch image:
```dockerfile
FROM ubuntu:22.04 AS builder
RUN apt-get update && apt-get install -y cmake build-essential \
    libssl-dev zlib1g-dev meson ninja-build pkg-config libglib2.0-dev \
    libjpeg-dev libpng-dev libwebp-dev
COPY . /app
RUN cd /app/build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j && strip gara

FROM scratch
COPY --from=builder /app/build/gara /gara
EXPOSE 80
ENTRYPOINT ["/gara"]
```

See [DEPLOYMENT.md](DEPLOYMENT.md) for EC2, ECS, and production setups.

## Project Structure

```
src/
├── controllers/image_controller.cpp  # HTTP endpoints
├── services/
│   ├── s3_service.cpp                # S3 operations
│   ├── image_processor.cpp           # libvips wrapper
│   └── cache_manager.cpp             # Cache logic
├── utils/file_utils.cpp              # SHA256, I/O
└── models/image_metadata.cpp         # Data structures
```

## S3 Structure

```
s3://bucket/
├── raw/{hash}.{ext}                           # Original uploads
└── transformed/{hash}_{format}_{w}x{h}.{ext}  # Cached transforms
```

## License

MIT
