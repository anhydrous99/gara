# Gara Image Service API Documentation

## Viewing the Documentation

### Option 1: Online Swagger Editor (Easiest)

1. Go to [Swagger Editor](https://editor.swagger.io/)
2. Click `File` → `Import file`
3. Select the `openapi.yaml` file from this repository
4. The interactive documentation will appear on the right side

### Option 2: Local HTML File

1. Open `docs/api.html` in your web browser
2. Make sure the `openapi.yaml` file is in the parent directory
3. You'll see an interactive API documentation with:
   - All endpoints and their descriptions
   - Request/response schemas
   - Try-it-out functionality

### Option 3: Serve from the API (Coming Soon)

Add a route to serve the OpenAPI spec directly from the running service:

```bash
curl http://localhost:8080/api/openapi.json
```

### Option 4: VS Code Extension

Install the [OpenAPI (Swagger) Editor](https://marketplace.visualstudio.com/items?itemName=42Crunch.vscode-openapi) extension and open `openapi.yaml`.

## API Overview

### Endpoints

- `POST /api/images/upload` - Upload a new image
- `GET /api/images/{image_id}` - Get or transform an image
- `GET /api/images/health` - Health check with S3 status
- `GET /health` - Simple health check

### Authentication

Currently, no authentication is required. For production use, consider adding:
- API keys
- OAuth 2.0
- AWS SigV4 signatures

### Rate Limiting

⚠️ **Not yet implemented** - Consider adding rate limiting before production deployment.

### CORS

⚠️ **Not yet configured** - Add CORS headers if accessing from web browsers.

## Example Usage

### Upload an Image

```bash
curl -X POST http://localhost:8080/api/images/upload \
  -F "file=@/path/to/image.jpg"
```

Response:
```json
{
  "image_id": "a3b5c7d9e1f2a4b6c8d0e2f4a6b8c0d2e4f6a8b0c2d4e6f8a0b2c4d6e8f0a2b4",
  "original_filename": "image.jpg",
  "size": 1048576,
  "upload_timestamp": 1699564800,
  "message": "Image uploaded successfully"
}
```

### Get Transformed Image

```bash
curl "http://localhost:8080/api/images/a3b5c7d9e1f2a4b6c8d0e2f4a6b8c0d2e4f6a8b0c2d4e6f8a0b2c4d6e8f0a2b4?format=png&width=800&height=600"
```

Response:
```json
{
  "image_id": "a3b5c7d9e1f2a4b6c8d0e2f4a6b8c0d2e4f6a8b0c2d4e6f8a0b2c4d6e8f0a2b4",
  "format": "png",
  "width": 800,
  "height": 600,
  "url": "https://gara-images.s3.amazonaws.com/transformed/...?X-Amz-Signature=...",
  "expires_in": 3600
}
```

## Generating Client SDKs

You can use the OpenAPI spec to generate client libraries in various languages:

### Using OpenAPI Generator

```bash
# Install openapi-generator
npm install -g @openapitools/openapi-generator-cli

# Generate Python client
openapi-generator-cli generate \
  -i openapi.yaml \
  -g python \
  -o clients/python

# Generate JavaScript/TypeScript client
openapi-generator-cli generate \
  -i openapi.yaml \
  -g typescript-axios \
  -o clients/typescript

# Generate Go client
openapi-generator-cli generate \
  -i openapi.yaml \
  -g go \
  -o clients/go
```

### Supported Languages

- Python
- JavaScript/TypeScript
- Go
- Java
- Rust
- Ruby
- PHP
- C#
- And 50+ more languages

## Updating the Documentation

When you add new endpoints or modify existing ones:

1. Update `openapi.yaml` with the new specification
2. Validate the spec at [Swagger Editor](https://editor.swagger.io/)
3. Commit the changes to version control
4. (Optional) Regenerate client SDKs

## Tools

- **Swagger UI**: Interactive API documentation
- **Swagger Editor**: Online editor for OpenAPI specs
- **OpenAPI Generator**: Generate client SDKs
- **Postman**: Import OpenAPI spec for testing
- **Insomnia**: Alternative API testing tool with OpenAPI support
