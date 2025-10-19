#!/bin/bash

# Test script for Gara Image Service API

BASE_URL="http://localhost:8080"

echo "=== Testing Gara Image Service ==="
echo ""

# Test 1: Health Check
echo "1. Testing health endpoint..."
curl -s "${BASE_URL}/health" | jq .
echo ""

# Test 2: Image Service Health
echo "2. Testing image service health..."
curl -s "${BASE_URL}/api/images/health" | jq .
echo ""

# Test 3: Upload Image (requires a test image)
if [ -f "$1" ]; then
    echo "3. Uploading test image: $1"
    RESPONSE=$(curl -s -X POST "${BASE_URL}/api/images/upload" -F "file=@$1")
    echo "$RESPONSE" | jq .

    # Extract image_id
    IMAGE_ID=$(echo "$RESPONSE" | jq -r '.image_id')
    echo ""

    if [ "$IMAGE_ID" != "null" ] && [ -n "$IMAGE_ID" ]; then
        echo "4. Getting original image..."
        curl -s "${BASE_URL}/api/images/${IMAGE_ID}" | jq .
        echo ""

        echo "5. Getting image as PNG..."
        curl -s "${BASE_URL}/api/images/${IMAGE_ID}?format=png" | jq .
        echo ""

        echo "6. Getting resized image (800px width)..."
        curl -s "${BASE_URL}/api/images/${IMAGE_ID}?format=jpeg&width=800" | jq .
        echo ""

        echo "7. Getting resized image (800x600)..."
        curl -s "${BASE_URL}/api/images/${IMAGE_ID}?format=jpeg&width=800&height=600" | jq .
        echo ""

        echo "=== Test Complete ==="
    else
        echo "Upload failed - cannot continue with transformation tests"
    fi
else
    echo "3. Skipping upload test - no image file provided"
    echo "   Usage: ./test_api.sh /path/to/test/image.jpg"
fi
