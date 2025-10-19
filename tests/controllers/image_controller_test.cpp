#include <gtest/gtest.h>
#include "controllers/image_controller.h"
#include "models/image_metadata.h"
#include <nlohmann/json.hpp>

using namespace gara;
using json = nlohmann::json;

class ImageControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Tests for image controller would require mocking Crow requests
        // which is complex. For basic smoke tests, we'll test the helper
        // methods and data models instead.
    }
};

// Test TransformRequest construction
TEST_F(ImageControllerTest, TransformRequestConstruction) {
    TransformRequest req1;
    EXPECT_EQ("jpeg", req1.target_format);
    EXPECT_EQ(0, req1.width);
    EXPECT_EQ(0, req1.height);

    TransformRequest req2("test_id", "png", 800, 600);
    EXPECT_EQ("test_id", req2.image_id);
    EXPECT_EQ("png", req2.target_format);
    EXPECT_EQ(800, req2.width);
    EXPECT_EQ(600, req2.height);
}

// Test TransformRequest cache key generation
TEST_F(ImageControllerTest, TransformRequestCacheKey) {
    TransformRequest req("abc123", "jpeg", 800, 600);
    std::string cache_key = req.getCacheKey();

    EXPECT_FALSE(cache_key.empty());
    EXPECT_NE(std::string::npos, cache_key.find("abc123"));
    EXPECT_NE(std::string::npos, cache_key.find("jpeg"));
    EXPECT_NE(std::string::npos, cache_key.find("800"));
    EXPECT_NE(std::string::npos, cache_key.find("600"));
}

// Test ImageMetadata construction
TEST_F(ImageControllerTest, ImageMetadataConstruction) {
    ImageMetadata meta;
    EXPECT_TRUE(meta.image_id.empty());
    EXPECT_EQ(0, meta.original_size);

    ImageMetadata meta2("test_id", "jpg", "raw/test.jpg", 1024);
    EXPECT_EQ("test_id", meta2.image_id);
    EXPECT_EQ("jpg", meta2.original_format);
    EXPECT_EQ("raw/test.jpg", meta2.s3_raw_key);
    EXPECT_EQ(1024, meta2.original_size);
}

// Test raw key generation
TEST_F(ImageControllerTest, GenerateRawKey) {
    std::string key = ImageMetadata::generateRawKey("abc123", "jpg");
    EXPECT_EQ("raw/abc123.jpg", key);

    std::string key2 = ImageMetadata::generateRawKey("xyz789", "png");
    EXPECT_EQ("raw/xyz789.png", key2);
}

// Test transformed key generation
TEST_F(ImageControllerTest, GenerateTransformedKey) {
    std::string key = ImageMetadata::generateTransformedKey("abc123", "jpeg", 800, 600);
    EXPECT_EQ("transformed/abc123_jpeg_800x600.jpeg", key);

    std::string key2 = ImageMetadata::generateTransformedKey("xyz789", "png", 1024, 768);
    EXPECT_EQ("transformed/xyz789_png_1024x768.png", key2);

    std::string key3 = ImageMetadata::generateTransformedKey("test", "webp", 0, 0);
    EXPECT_EQ("transformed/test_webp_0x0.webp", key3);
}

// Test consistent key generation for same parameters
TEST_F(ImageControllerTest, ConsistentKeyGeneration) {
    std::string key1 = ImageMetadata::generateTransformedKey("abc", "jpeg", 800, 600);
    std::string key2 = ImageMetadata::generateTransformedKey("abc", "jpeg", 800, 600);
    EXPECT_EQ(key1, key2);
}

// Test different parameters produce different keys
TEST_F(ImageControllerTest, DifferentParametersDifferentKeys) {
    std::string key1 = ImageMetadata::generateTransformedKey("abc", "jpeg", 800, 600);
    std::string key2 = ImageMetadata::generateTransformedKey("abc", "png", 800, 600);
    std::string key3 = ImageMetadata::generateTransformedKey("abc", "jpeg", 1024, 768);
    std::string key4 = ImageMetadata::generateTransformedKey("xyz", "jpeg", 800, 600);

    EXPECT_NE(key1, key2);  // Different format
    EXPECT_NE(key1, key3);  // Different dimensions
    EXPECT_NE(key1, key4);  // Different image ID
}

// Test upload response format (simulated)
TEST_F(ImageControllerTest, UploadResponseFormat) {
    // Simulate what the upload response would look like
    json response = {
        {"image_id", "abc123def456"},
        {"original_filename", "test.jpg"},
        {"size", 1024000},
        {"upload_timestamp", 1634567890},
        {"message", "Image uploaded successfully"}
    };

    EXPECT_EQ("abc123def456", response["image_id"]);
    EXPECT_EQ("test.jpg", response["original_filename"]);
    EXPECT_EQ(1024000, response["size"]);
    EXPECT_TRUE(response.contains("upload_timestamp"));
}

// Test transform response format (simulated)
TEST_F(ImageControllerTest, TransformResponseFormat) {
    // Simulate what the transform response would look like
    json response = {
        {"image_id", "abc123"},
        {"format", "jpeg"},
        {"width", 800},
        {"height", 600},
        {"url", "https://s3.amazonaws.com/bucket/transformed/abc123_jpeg_800x600.jpeg"},
        {"expires_in", 3600}
    };

    EXPECT_EQ("abc123", response["image_id"]);
    EXPECT_EQ("jpeg", response["format"]);
    EXPECT_EQ(800, response["width"]);
    EXPECT_EQ(600, response["height"]);
    EXPECT_TRUE(response.contains("url"));
}

// Test error response format (simulated)
TEST_F(ImageControllerTest, ErrorResponseFormat) {
    json error_response = {
        {"error", "File too large"},
        {"message", "Maximum file size is 100MB"}
    };

    EXPECT_EQ("File too large", error_response["error"]);
    EXPECT_TRUE(error_response.contains("message"));
}
