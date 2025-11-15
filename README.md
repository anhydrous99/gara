# Gara Image Service

[![Tests](https://github.com/anhydrous99/gara/actions/workflows/test.yml/badge.svg?branch=main)](https://github.com/anhydrous99/gara/actions/workflows/test.yml)

High-performance image transformation service built with C++, Crow, AWS S3, and libvips.

## Features

- Upload images up to 100MB (JPEG, PNG, GIF, TIFF, WebP)
- On-demand format conversion (JPEG, PNG, WebP, TIFF) and resizing
- **Automatic watermarking** with customizable text, position, and styling
- S3-backed caching (no re-computation)
- Hash-based deduplication
- Presigned URLs for secure access
- API key authentication via AWS Secrets Manager

## Quick Start

```bash
# Install dependencies
brew install openssl zlib cmake pkg-config glib vips  # macOS
# OR
sudo apt-get install libssl-dev zlib1g-dev cmake build-essential \
    pkg-config libglib2.0-dev libvips-dev  # Ubuntu

# Configure AWS
aws configure
export S3_BUCKET_NAME=gara-images
export AWS_REGION=us-east-1
aws s3 mb s3://gara-images

# Create API key in Secrets Manager
aws secretsmanager create-secret \
  --name gara-api-key \
  --secret-string "your-secure-api-key-here" \
  --region us-east-1
export SECRETS_MANAGER_API_KEY_NAME=gara-api-key

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j

# Run
export PORT=8080
./gara
```

See [QUICK_START.md](QUICK_START.md) for a 5-minute setup guide.

## API

## Authentication

Upload endpoints require API key authentication. Include the `X-API-Key` header in your requests:

```bash
export API_KEY="your-api-key-from-secrets-manager"
```

### Upload Image

**⚠️ Requires authentication**

```bash
curl -X POST http://localhost:8080/api/images/upload \
  -H "X-API-Key: $API_KEY" \
  -F "file=@image.jpg"
# Returns: {"image_id": "abc123...", "message": "Image uploaded successfully"}
```

**Error responses:**
- `401 Unauthorized` - Missing or invalid API key

### Get/Transform Image

**Public endpoint - no authentication required**

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
2. **Transform**: Request → Check S3 cache → Transform if needed → **Apply watermark** → Cache → Return presigned URL
3. **Cache hit**: Instant URL (no processing)

## Watermarking

All retrieved images are automatically watermarked with customizable text. The watermark:

- **Scales automatically** with image size (font size is 2.5% of image width)
- **White text with dark shadow** for visibility on any background
- **Positioned** in bottom-right corner by default (configurable)
- **Applied once** during transformation and then cached
- **Configurable** via environment variables (text, position, opacity, etc.)

To disable watermarking, set `WATERMARK_ENABLED=false`.

## Dependencies

**System packages** (required):
- OpenSSL, zlib
- CMake, pkg-config, glib
- libvips (image processing library)

**Auto-fetched by CMake**:
- ASIO, Crow, nlohmann/json, AWS SDK, Google Test

## Environment Variables

```bash
# Required
S3_BUCKET_NAME=gara-images              # S3 bucket for image storage
AWS_REGION=us-east-1                    # AWS region
SECRETS_MANAGER_API_KEY_NAME=gara-api-key  # API key secret name

# Optional - Server
PORT=80                                  # Server port (default: 80)
TEMP_UPLOAD_DIR=/tmp                    # Temporary upload directory

# Optional - Watermark
WATERMARK_ENABLED=true                  # Enable/disable watermarking (default: true)
WATERMARK_TEXT="© 2025 Armando Herrera" # Watermark text (default: "© 2025 Armando Herrera")
WATERMARK_POSITION=bottom-right         # Position: top-left, top-right, bottom-left, bottom-right (default: bottom-right)
WATERMARK_FONT_SIZE=24                  # Base font size, scales with image (default: 24)
WATERMARK_COLOR=white                   # Text color (default: white)
WATERMARK_OPACITY=0.9                   # Opacity 0.0-1.0 (default: 0.9)
WATERMARK_MARGIN=20                     # Margin from edges in pixels (default: 20)
```

## IAM Permissions

The application requires the following IAM permissions:

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": [
        "s3:GetObject",
        "s3:PutObject",
        "s3:DeleteObject"
      ],
      "Resource": "arn:aws:s3:::gara-images/*"
    },
    {
      "Effect": "Allow",
      "Action": "secretsmanager:GetSecretValue",
      "Resource": "arn:aws:secretsmanager:us-east-1:*:secret:gara-api-key*"
    }
  ]
}
```

**Key rotation**: The API key is cached for 5 minutes. After updating the secret in Secrets Manager, allow up to 5 minutes for the change to take effect.

## Testing

```bash
# Run all tests (~47 tests)
cd build && ctest --output-on-failure

# Run specific tests
./gara_tests --gtest_filter="FileUtilsTest.*"
```

See [TESTING.md](TESTING.md) for details.

## Deployment

See [DEPLOYMENT.md](DEPLOYMENT.md) for Docker, EC2, ECS, and production setups.

## Project Structure

```
src/
├── controllers/image_controller.cpp  # HTTP endpoints
├── services/
│   ├── s3_service.cpp                # S3 operations
│   ├── image_processor.cpp           # libvips wrapper
│   ├── watermark_service.cpp         # Watermark text rendering
│   └── cache_manager.cpp             # Cache logic
├── utils/file_utils.cpp              # SHA256, I/O
└── models/
    ├── image_metadata.cpp            # Data structures
    └── watermark_config.h            # Watermark configuration
```

## S3 Structure

```
s3://bucket/
├── raw/{hash}.{ext}                                # Original uploads
└── transformed/{hash}_{format}_{w}x{h}_wm.{ext}   # Cached transforms with watermark
```

Note: The `_wm` suffix in the cache key indicates a watermarked image.

## License

MIT
