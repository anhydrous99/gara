#include <gtest/gtest.h>
#include "services/cache_manager.h"
#include "models/image_metadata.h"
#include "utils/file_utils.h"
#include "utils/logger.h"
#include "utils/metrics.h"
#include "mocks/fake_file_service.h"
#include "test_helpers/test_constants.h"
#include "test_helpers/test_builders.h"
#include "test_helpers/test_file_manager.h"
#include "test_helpers/custom_matchers.h"
#include <memory>

using namespace gara;
using namespace gara::utils;
using namespace gara::testing;
using namespace gara::test_constants;
using namespace gara::test_builders;
using namespace gara::test_helpers;
using namespace gara::test_matchers;

class CacheManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize logger and metrics for tests
        gara::Logger::initialize("gara-test", "error", gara::Logger::Format::TEXT, "test");
        gara::Metrics::initialize("GaraTest", "gara-test", "test", false);

        fake_file_service_ = std::make_shared<FakeFileService>(TEST_BUCKET_NAME);
        cache_manager_ = std::make_shared<CacheManager>(fake_file_service_);
    }

    void TearDown() override {
        fake_file_service_->clear();
    }

    // Helper to upload fake transformed image
    void uploadFakeTransformedImage(const TransformRequest& request) {
        std::string s3_key = request.getCacheKey();
        auto fake_data = TestDataBuilder::createData(SMALL_DATA_SIZE);
        fake_file_service_->uploadData(fake_data, s3_key);
    }

    std::shared_ptr<FakeFileService> fake_file_service_;
    std::shared_ptr<CacheManager> cache_manager_;
};

// ============================================================================
// Cache Existence Tests
// ============================================================================

TEST_F(CacheManagerTest, ExistsInCache_WhenImageNotCached_ReturnsFalse) {
    // Arrange
    auto request = TransformRequestBuilder::defaultJpeg();

    // Act
    bool exists = cache_manager_->existsInCache(request);

    // Assert
    EXPECT_FALSE(exists)
        << "Transform should not exist in cache initially";
}

TEST_F(CacheManagerTest, ExistsInCache_WhenImageCached_ReturnsTrue) {
    // Arrange
    auto request = TransformRequestBuilder::defaultJpeg();
    uploadFakeTransformedImage(request);

    // Act
    bool exists = cache_manager_->existsInCache(request);

    // Assert
    EXPECT_TRUE(exists)
        << "Transform should exist in cache after upload";
}

// ============================================================================
// Get Cached Image Tests
// ============================================================================

TEST_F(CacheManagerTest, GetCachedImage_WhenImageCached_ReturnsS3Key) {
    // Arrange
    auto request = TransformRequestBuilder::defaultJpeg();
    uploadFakeTransformedImage(request);

    std::string expected_key = request.getCacheKey();

    // Act
    std::string cached_key = cache_manager_->getCachedImage(request);

    // Assert
    EXPECT_THAT(cached_key, IsNonEmptyString())
        << "Cached image key should not be empty";

    EXPECT_EQ(expected_key, cached_key)
        << "Returned key should match the cache key";
}

TEST_F(CacheManagerTest, GetCachedImage_WhenImageNotCached_ReturnsEmptyString) {
    // Arrange
    auto request = TransformRequestBuilder::defaultJpeg();

    // Act
    std::string cached_key = cache_manager_->getCachedImage(request);

    // Assert
    EXPECT_TRUE(cached_key.empty())
        << "Should return empty string when image not in cache";
}

// ============================================================================
// Store in Cache Tests
// ============================================================================

TEST_F(CacheManagerTest, StoreInCache_WithValidFile_Succeeds) {
    // Arrange
    auto request = TransformRequestBuilder::defaultJpeg();

    TempFile temp("cache_test_");
    auto data = TestDataBuilder::createData(SMALL_DATA_SIZE);
    temp.write(data);

    // Act
    bool success = cache_manager_->storeInCache(request, temp.getPath());

    // Assert
    EXPECT_TRUE(success)
        << "Storing valid file in cache should succeed";

    EXPECT_TRUE(cache_manager_->existsInCache(request))
        << "Image should exist in cache after successful store";

    std::string cached_key = cache_manager_->getCachedImage(request);
    EXPECT_THAT(cached_key, IsNonEmptyString())
        << "Should be able to retrieve cached image";
}

TEST_F(CacheManagerTest, StoreInCache_NonExistentFile_ReturnsFalse) {
    // Arrange
    auto request = TransformRequestBuilder::defaultJpeg();
    auto nonexistent_path = TestFileManager::createUniquePath("nonexistent_", ".jpg");

    // Act
    bool success = cache_manager_->storeInCache(request, nonexistent_path);

    // Assert
    EXPECT_FALSE(success)
        << "Storing non-existent file should fail";

    EXPECT_FALSE(cache_manager_->existsInCache(request))
        << "Failed store should not add to cache";
}

// ============================================================================
// Presigned URL Tests
// ============================================================================

TEST_F(CacheManagerTest, GetPresignedUrl_ForCachedImage_ReturnsValidUrl) {
    // Arrange
    auto request = TransformRequestBuilder::defaultJpeg();
    uploadFakeTransformedImage(request);

    // Act
    std::string url = cache_manager_->getPresignedUrl(request, ONE_HOUR_SECONDS);

    // Assert
    EXPECT_THAT(url, IsNonEmptyString())
        << "Presigned URL should not be empty";

    EXPECT_THAT(url, IsValidPresignedUrl())
        << "Should return a valid presigned URL format";

    EXPECT_THAT(url, ContainsSubstring("fake-s3.amazonaws.com"))
        << "URL should contain S3 domain";

    EXPECT_THAT(url, ContainsSubstring("expires=" + std::to_string(ONE_HOUR_SECONDS)))
        << "URL should include expiration parameter";
}

TEST_F(CacheManagerTest, GetPresignedUrl_ForNonCachedImage_ReturnsEmptyString) {
    // Arrange
    auto request = TransformRequestBuilder()
        .withImageId("nonexistent_id")
        .build();

    // Act
    std::string url = cache_manager_->getPresignedUrl(request, ONE_HOUR_SECONDS);

    // Assert
    EXPECT_TRUE(url.empty())
        << "Presigned URL should be empty for non-cached image";
}

TEST_F(CacheManagerTest, GetPresignedUrl_WithDifferentExpiration_ReflectsInUrl) {
    // Arrange
    auto request = TransformRequestBuilder::defaultJpeg();
    uploadFakeTransformedImage(request);

    // Act
    std::string url_1h = cache_manager_->getPresignedUrl(request, ONE_HOUR_SECONDS);
    std::string url_24h = cache_manager_->getPresignedUrl(request, ONE_DAY_SECONDS);

    // Assert
    EXPECT_THAT(url_1h, ContainsSubstring("expires=" + std::to_string(ONE_HOUR_SECONDS)))
        << "1-hour URL should have 3600 second expiration";

    EXPECT_THAT(url_24h, ContainsSubstring("expires=" + std::to_string(ONE_DAY_SECONDS)))
        << "24-hour URL should have 86400 second expiration";
}

// ============================================================================
// Clear Transformation Tests
// ============================================================================

TEST_F(CacheManagerTest, ClearTransformation_ForCachedImage_RemovesFromCache) {
    // Arrange
    auto request = TransformRequestBuilder::defaultJpeg();
    uploadFakeTransformedImage(request);

    ASSERT_TRUE(cache_manager_->existsInCache(request))
        << "Setup: Image should be cached initially";

    // Act
    bool deleted = cache_manager_->clearTransformation(request);

    // Assert
    EXPECT_TRUE(deleted)
        << "Clear operation should return true for existing cached image";

    EXPECT_FALSE(cache_manager_->existsInCache(request))
        << "Image should no longer exist in cache after clear";
}

TEST_F(CacheManagerTest, ClearTransformation_ForNonCachedImage_ReturnsFalse) {
    // Arrange
    auto request = TransformRequestBuilder::defaultJpeg();

    // Act
    bool deleted = cache_manager_->clearTransformation(request);

    // Assert
    EXPECT_FALSE(deleted)
        << "Clearing non-existent cached image should return false";
}

// ============================================================================
// Cache Key Generation Tests
// ============================================================================

TEST_F(CacheManagerTest, CacheKey_SameParameters_GeneratesSameKey) {
    // Arrange
    auto req1 = TransformRequestBuilder()
        .withImageId("abc123")
        .withFormat(FORMAT_JPEG)
        .withDimensions(STANDARD_WIDTH_800, STANDARD_HEIGHT_600)
        .build();

    auto req2 = TransformRequestBuilder()
        .withImageId("abc123")
        .withFormat(FORMAT_JPEG)
        .withDimensions(STANDARD_WIDTH_800, STANDARD_HEIGHT_600)
        .build();

    // Act & Assert
    EXPECT_EQ(req1.getCacheKey(), req2.getCacheKey())
        << "Same parameters should produce identical cache keys";
}

TEST_F(CacheManagerTest, CacheKey_DifferentFormat_GeneratesDifferentKey) {
    // Arrange
    auto req_jpeg = TransformRequestBuilder()
        .withImageId("abc123")
        .withFormat(FORMAT_JPEG)
        .build();

    auto req_png = TransformRequestBuilder()
        .withImageId("abc123")
        .withFormat(FORMAT_PNG)
        .build();

    // Act & Assert
    EXPECT_NE(req_jpeg.getCacheKey(), req_png.getCacheKey())
        << "Different formats should produce different cache keys";
}

TEST_F(CacheManagerTest, CacheKey_DifferentDimensions_GeneratesDifferentKey) {
    // Arrange
    auto req_800x600 = TransformRequestBuilder()
        .withImageId("abc123")
        .withDimensions(STANDARD_WIDTH_800, STANDARD_HEIGHT_600)
        .build();

    auto req_1024x768 = TransformRequestBuilder()
        .withImageId("abc123")
        .withDimensions(STANDARD_WIDTH_1024, STANDARD_HEIGHT_768)
        .build();

    // Act & Assert
    EXPECT_NE(req_800x600.getCacheKey(), req_1024x768.getCacheKey())
        << "Different dimensions should produce different cache keys";
}

// ============================================================================
// Multiple Dimension Caching Tests
// ============================================================================

TEST_F(CacheManagerTest, Cache_DifferentDimensionsSameImage_StoresSeparately) {
    // Arrange
    std::string image_id = TEST_IMAGE_ID;

    auto req_800x600 = TransformRequestBuilder()
        .withImageId(image_id)
        .withDimensions(STANDARD_WIDTH_800, STANDARD_HEIGHT_600)
        .build();

    auto req_1024x768 = TransformRequestBuilder()
        .withImageId(image_id)
        .withDimensions(STANDARD_WIDTH_1024, STANDARD_HEIGHT_768)
        .build();

    TempFile temp("dim_test_");
    auto data = TestDataBuilder::createData(SMALL_DATA_SIZE);
    temp.write(data);

    // Act
    bool success1 = cache_manager_->storeInCache(req_800x600, temp.getPath());
    bool success2 = cache_manager_->storeInCache(req_1024x768, temp.getPath());

    // Assert
    EXPECT_TRUE(success1 && success2)
        << "Both transformations should be cached successfully";

    EXPECT_TRUE(cache_manager_->existsInCache(req_800x600))
        << "800x600 transformation should exist in cache";

    EXPECT_TRUE(cache_manager_->existsInCache(req_1024x768))
        << "1024x768 transformation should exist in cache";

    EXPECT_NE(cache_manager_->getCachedImage(req_800x600),
              cache_manager_->getCachedImage(req_1024x768))
        << "Different dimensions should have different cache keys";
}

// ============================================================================
// Multiple Format Caching Tests
// ============================================================================

TEST_F(CacheManagerTest, Cache_DifferentFormatsSameImage_StoresSeparately) {
    // Arrange
    std::string image_id = TEST_IMAGE_ID;

    auto req_jpeg = TransformRequestBuilder()
        .withImageId(image_id)
        .withFormat(FORMAT_JPEG)
        .build();

    auto req_png = TransformRequestBuilder()
        .withImageId(image_id)
        .withFormat(FORMAT_PNG)
        .build();

    auto req_webp = TransformRequestBuilder()
        .withImageId(image_id)
        .withFormat(FORMAT_WEBP)
        .build();

    TempFile temp("format_test_");
    auto data = TestDataBuilder::createData(SMALL_DATA_SIZE);
    temp.write(data);

    // Act
    ASSERT_TRUE(cache_manager_->storeInCache(req_jpeg, temp.getPath()));
    ASSERT_TRUE(cache_manager_->storeInCache(req_png, temp.getPath()));
    ASSERT_TRUE(cache_manager_->storeInCache(req_webp, temp.getPath()));

    // Assert - All formats cached
    EXPECT_TRUE(cache_manager_->existsInCache(req_jpeg))
        << "JPEG format should be cached";

    EXPECT_TRUE(cache_manager_->existsInCache(req_png))
        << "PNG format should be cached";

    EXPECT_TRUE(cache_manager_->existsInCache(req_webp))
        << "WebP format should be cached";

    // Assert - Verify object count in S3
    EXPECT_EQ(3, fake_file_service_->getObjectCount())
        << "Should have 3 separate cached objects in S3";
}

// ============================================================================
// Cache Isolation Tests
// ============================================================================

TEST_F(CacheManagerTest, ClearTransformation_SpecificFormat_DoesNotAffectOthers) {
    // Arrange
    std::string image_id = TEST_IMAGE_ID;

    auto req_jpeg = TransformRequestBuilder()
        .withImageId(image_id)
        .withFormat(FORMAT_JPEG)
        .build();

    auto req_png = TransformRequestBuilder()
        .withImageId(image_id)
        .withFormat(FORMAT_PNG)
        .build();

    uploadFakeTransformedImage(req_jpeg);
    uploadFakeTransformedImage(req_png);

    // Act - Clear only JPEG
    bool deleted = cache_manager_->clearTransformation(req_jpeg);

    // Assert
    EXPECT_TRUE(deleted);

    EXPECT_FALSE(cache_manager_->existsInCache(req_jpeg))
        << "JPEG format should be removed from cache";

    EXPECT_TRUE(cache_manager_->existsInCache(req_png))
        << "PNG format should still exist in cache";
}
