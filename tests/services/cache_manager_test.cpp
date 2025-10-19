#include <gtest/gtest.h>
#include "services/cache_manager.h"
#include "models/image_metadata.h"
#include "utils/file_utils.h"
#include "mocks/mock_s3_service.h"
#include <memory>

using namespace gara;
using namespace gara::utils;
using namespace gara::testing;

class CacheManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use fake S3 service for testing
        fake_s3_ = std::make_shared<FakeS3Service>("test-bucket", "us-east-1");
        cache_manager_ = std::make_shared<CacheManager>(fake_s3_);
    }

    void TearDown() override {
        fake_s3_->clear();
    }

    std::shared_ptr<FakeS3Service> fake_s3_;
    std::shared_ptr<CacheManager> cache_manager_;
};

// Test cache miss (transformed image doesn't exist)
TEST_F(CacheManagerTest, ExistsInCacheMiss) {
    TransformRequest request("test_image_id", "jpeg", 800, 600);

    bool exists = cache_manager_->existsInCache(request);
    EXPECT_FALSE(exists);
}

// Test cache hit (transformed image exists)
TEST_F(CacheManagerTest, ExistsInCacheHit) {
    TransformRequest request("test_image_id", "jpeg", 800, 600);

    // Upload a fake transformed image
    std::string s3_key = request.getCacheKey();
    std::vector<char> fake_data = {'f', 'a', 'k', 'e'};
    fake_s3_->uploadData(fake_data, s3_key);

    bool exists = cache_manager_->existsInCache(request);
    EXPECT_TRUE(exists);
}

// Test get cached image when it exists
TEST_F(CacheManagerTest, GetCachedImageHit) {
    TransformRequest request("test_image_id", "jpeg", 800, 600);

    // Upload a fake transformed image
    std::string s3_key = request.getCacheKey();
    std::vector<char> fake_data = {'f', 'a', 'k', 'e'};
    fake_s3_->uploadData(fake_data, s3_key);

    std::string cached_key = cache_manager_->getCachedImage(request);
    EXPECT_FALSE(cached_key.empty());
    EXPECT_EQ(s3_key, cached_key);
}

// Test get cached image when it doesn't exist
TEST_F(CacheManagerTest, GetCachedImageMiss) {
    TransformRequest request("test_image_id", "jpeg", 800, 600);

    std::string cached_key = cache_manager_->getCachedImage(request);
    EXPECT_TRUE(cached_key.empty());
}

// Test store in cache
TEST_F(CacheManagerTest, StoreInCache) {
    TransformRequest request("test_image_id", "jpeg", 800, 600);

    // Create a temporary file to cache
    TempFile temp("cache_test_");
    std::vector<char> data = {'t', 'e', 's', 't'};
    temp.write(data);

    bool success = cache_manager_->storeInCache(request, temp.getPath());
    EXPECT_TRUE(success);

    // Verify it's now in cache
    EXPECT_TRUE(cache_manager_->existsInCache(request));

    // Verify we can retrieve it
    std::string cached_key = cache_manager_->getCachedImage(request);
    EXPECT_FALSE(cached_key.empty());
}

// Test generate presigned URL for cached image
TEST_F(CacheManagerTest, GetPresignedUrlForCached) {
    TransformRequest request("test_image_id", "jpeg", 800, 600);

    // Upload a fake transformed image
    std::string s3_key = request.getCacheKey();
    std::vector<char> fake_data = {'f', 'a', 'k', 'e'};
    fake_s3_->uploadData(fake_data, s3_key);

    std::string url = cache_manager_->getPresignedUrl(request, 3600);
    EXPECT_FALSE(url.empty());
    EXPECT_NE(std::string::npos, url.find("fake-s3.amazonaws.com"));
    EXPECT_NE(std::string::npos, url.find("expires=3600"));
}

// Test generate presigned URL for non-existent image
TEST_F(CacheManagerTest, GetPresignedUrlForNonExistent) {
    TransformRequest request("nonexistent_id", "jpeg", 800, 600);

    std::string url = cache_manager_->getPresignedUrl(request, 3600);
    EXPECT_TRUE(url.empty());
}

// Test clear specific transformation
TEST_F(CacheManagerTest, ClearTransformation) {
    TransformRequest request("test_image_id", "jpeg", 800, 600);

    // Upload a fake transformed image
    std::string s3_key = request.getCacheKey();
    std::vector<char> fake_data = {'f', 'a', 'k', 'e'};
    fake_s3_->uploadData(fake_data, s3_key);

    EXPECT_TRUE(cache_manager_->existsInCache(request));

    // Clear the transformation
    bool deleted = cache_manager_->clearTransformation(request);
    EXPECT_TRUE(deleted);

    // Verify it's no longer in cache
    EXPECT_FALSE(cache_manager_->existsInCache(request));
}

// Test cache key generation
TEST_F(CacheManagerTest, CacheKeyGeneration) {
    TransformRequest req1("abc123", "jpeg", 800, 600);
    TransformRequest req2("abc123", "jpeg", 800, 600);
    TransformRequest req3("abc123", "png", 800, 600);
    TransformRequest req4("abc123", "jpeg", 1024, 768);

    // Same parameters should produce same key
    EXPECT_EQ(req1.getCacheKey(), req2.getCacheKey());

    // Different format should produce different key
    EXPECT_NE(req1.getCacheKey(), req3.getCacheKey());

    // Different dimensions should produce different key
    EXPECT_NE(req1.getCacheKey(), req4.getCacheKey());
}

// Test cache with different dimensions
TEST_F(CacheManagerTest, CacheDifferentDimensions) {
    std::string image_id = "test_image";

    TransformRequest req1(image_id, "jpeg", 800, 600);
    TransformRequest req2(image_id, "jpeg", 1024, 768);

    // Create temp file
    TempFile temp("dim_test_");
    std::vector<char> data = {'t', 'e', 's', 't'};
    temp.write(data);

    // Cache both transformations
    EXPECT_TRUE(cache_manager_->storeInCache(req1, temp.getPath()));
    EXPECT_TRUE(cache_manager_->storeInCache(req2, temp.getPath()));

    // Both should exist in cache
    EXPECT_TRUE(cache_manager_->existsInCache(req1));
    EXPECT_TRUE(cache_manager_->existsInCache(req2));

    // Should have different keys
    EXPECT_NE(cache_manager_->getCachedImage(req1),
             cache_manager_->getCachedImage(req2));
}

// Test cache with different formats
TEST_F(CacheManagerTest, CacheDifferentFormats) {
    std::string image_id = "test_image";

    TransformRequest req_jpeg(image_id, "jpeg", 800, 600);
    TransformRequest req_png(image_id, "png", 800, 600);
    TransformRequest req_webp(image_id, "webp", 800, 600);

    // Create temp file
    TempFile temp("format_test_");
    std::vector<char> data = {'t', 'e', 's', 't'};
    temp.write(data);

    // Cache all formats
    EXPECT_TRUE(cache_manager_->storeInCache(req_jpeg, temp.getPath()));
    EXPECT_TRUE(cache_manager_->storeInCache(req_png, temp.getPath()));
    EXPECT_TRUE(cache_manager_->storeInCache(req_webp, temp.getPath()));

    // All should exist
    EXPECT_TRUE(cache_manager_->existsInCache(req_jpeg));
    EXPECT_TRUE(cache_manager_->existsInCache(req_png));
    EXPECT_TRUE(cache_manager_->existsInCache(req_webp));

    // Verify object count
    EXPECT_EQ(3, fake_s3_->getObjectCount());
}
