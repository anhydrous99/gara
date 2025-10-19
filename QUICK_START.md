# Quick Start Guide

## 5-Minute Setup (macOS)

```bash
# 1. Install dependencies (libvips auto-built by CMake!)
brew install openssl zlib cmake meson ninja pkg-config glib jpeg libpng webp

# 2. Configure AWS
aws configure
export S3_BUCKET_NAME=gara-images
export AWS_REGION=us-east-1

# 3. Create S3 bucket
aws s3 mb s3://gara-images

# 4. Build
cd /path/to/gara
mkdir -p cmake-build-debug && cd cmake-build-debug
cmake .. && make -j

# 5. Run (on port 8080)
export PORT=8080
./gara
```

## Test It

```bash
# In another terminal

# Upload an image
curl -X POST http://localhost:8080/api/images/upload \
  -F "file=@/path/to/image.jpg"

# Response:
# {
#   "image_id": "abc123...",
#   "message": "Image uploaded successfully"
# }

# Get as JPEG 800px wide
curl "http://localhost:8080/api/images/abc123?format=jpeg&width=800" | jq .

# Response:
# {
#   "url": "https://s3.amazonaws.com/...",
#   "expires_in": 3600
# }
```

## Key Endpoints

| Method | Endpoint | Purpose |
|--------|----------|---------|
| POST | `/api/images/upload` | Upload image |
| GET | `/api/images/{id}?format=jpeg&width=800` | Get/transform image |
| GET | `/health` | Health check |

## Common Transformations

```bash
# Convert to PNG
?format=png

# Resize to 800px width (maintain aspect ratio)
?format=jpeg&width=800

# Resize to specific dimensions
?format=jpeg&width=1024&height=768

# Get thumbnail
?format=jpeg&width=200&height=200
```

## Environment Variables

```bash
export S3_BUCKET_NAME=gara-images      # Required
export AWS_REGION=us-east-1            # Required
export PORT=8080                       # Optional (default: 80)
export TEMP_UPLOAD_DIR=/tmp/uploads    # Optional (default: /tmp)
```

## Troubleshooting

**Build takes a long time**:
- First build downloads and compiles libvips (~5-10 minutes)
- Subsequent builds are much faster (cached)

**AWS credentials error**:
```bash
aws configure
# Enter your AWS Access Key ID and Secret Access Key
```

**Port 80 permission denied**:
```bash
# Use port 8080 instead
export PORT=8080
```

## What Happens

1. **Upload**: Image → SHA256 hash → S3 `raw/{hash}.{ext}`
2. **Transform**: Request → Check cache → Transform (if needed) → S3 `transformed/{hash}_{format}_{size}.{ext}` → Return URL
3. **Cache hit**: Instant presigned URL (no transformation)

## Next Steps

- Read [README.md](README.md) for full documentation
- See [DEPLOYMENT.md](DEPLOYMENT.md) for production deployment
- Run `./test_api.sh /path/to/image.jpg` for automated testing
