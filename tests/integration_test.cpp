#include <gtest/gtest.h>
#include "services/image_processor.h"
#include "services/cache_manager.h"
#include "models/image_metadata.h"
#include "utils/file_utils.h"
#include "mocks/fake_file_service.h"
#include <vips/vips8>
#include <vips/vips.h>
#include <memory>
#include <fstream>

using namespace gara;
using namespace gara::utils;
using namespace gara::testing;

class IntegrationTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ImageProcessor::initialize();
    }

    static void TearDownTestSuite() {
        ImageProcessor::shutdown();
    }

    void SetUp() override {
        fake_file_service_ = std::make_shared<FakeFileService>("test-bucket");
        image_processor_ = std::make_shared<ImageProcessor>();
        cache_manager_ = std::make_shared<CacheManager>(fake_file_service_);

        // Create test image
        test_image_path_ = "/tmp/gara_integration_test.ppm";
        createTestImage(test_image_path_, 200, 100);
    }

    void TearDown() override {
        fake_file_service_->clear();
        FileUtils::deleteFile(test_image_path_);
    }

    void createTestImage(const std::string& path, int width, int height) {
        try {
            // Create a solid color image (3 bands for RGB)
            vips::VImage image = vips::VImage::black(width, height, vips::VImage::option()
                ->set("bands", 3));
            image = image.new_from_image({0.0, 128.0, 255.0}); // Blue

            // Write to buffer
            size_t data_length;
            void* buf = image.write_to_memory(&data_length);

            // Save to file
            std::ofstream out(path, std::ios::binary);
            if (!out) {
                g_free(buf);
                FAIL() << "Failed to open file for writing: " << path;
            }

            // Write a simple PPM header (always supported, no dependencies)
            out << "P6\n" << width << " " << height << "\n255\n";

            // Write pixel data (assuming uchar RGB)
            out.write(static_cast<char*>(buf), width * height * 3);
            out.close();

            g_free(buf);
        } catch (vips::VError& e) {
            FAIL() << "Failed to create test image: " << e.what();
        }
    }

    std::shared_ptr<FakeFileService> fake_file_service_;
    std::shared_ptr<ImageProcessor> image_processor_;
    std::shared_ptr<CacheManager> cache_manager_;
    std::string test_image_path_;
};

// Test end-to-end flow: Upload -> Transform -> Cache -> Retrieve
TEST_F(IntegrationTest, UploadTransformCacheRetrieve) {
    // Step 1: Simulate upload - calculate hash and upload to S3
    std::string image_hash = FileUtils::calculateSHA256(test_image_path_);
    EXPECT_FALSE(image_hash.empty());

    std::string raw_key = ImageMetadata::generateRawKey(image_hash, "ppm");
    bool upload_success = fake_file_service_->uploadFile(test_image_path_, raw_key, "image/x-portable-pixmap");
    EXPECT_TRUE(upload_success);
    EXPECT_TRUE(fake_file_service_->objectExists(raw_key));

    // Step 2: Transform image
    TransformRequest transform_req(image_hash, "jpeg", 100, 50);

    // Check cache miss
    EXPECT_FALSE(cache_manager_->existsInCache(transform_req));

    // Download raw image
    TempFile raw_download("raw_dl_");
    bool download_success = fake_file_service_->downloadFile(raw_key, raw_download.getPath());
    EXPECT_TRUE(download_success);

    // Transform
    TempFile transformed("transformed_");
    std::string transformed_path = transformed.getPath() + ".jpg";
    bool transform_success = image_processor_->transform(
        raw_download.getPath(),
        transformed_path,
        "jpeg",
        100,
        50
    );
    EXPECT_TRUE(transform_success);

    // Verify transformation
    ImageInfo info = image_processor_->getImageInfo(transformed_path);
    EXPECT_TRUE(info.is_valid);
    EXPECT_EQ(100, info.width);
    EXPECT_EQ(50, info.height);

    // Step 3: Cache transformed image
    bool cache_success = cache_manager_->storeInCache(transform_req, transformed_path);
    EXPECT_TRUE(cache_success);
    EXPECT_TRUE(cache_manager_->existsInCache(transform_req));

    // Step 4: Retrieve from cache (simulates second request for same transformation)
    std::string cached_key = cache_manager_->getCachedImage(transform_req);
    EXPECT_FALSE(cached_key.empty());

    // Generate presigned URL
    std::string url = cache_manager_->getPresignedUrl(transform_req, 3600);
    EXPECT_FALSE(url.empty());

    FileUtils::deleteFile(transformed_path);
}

// Test multiple transformations of same image
TEST_F(IntegrationTest, MultipleTransformations) {
    std::string image_hash = FileUtils::calculateSHA256(test_image_path_);
    std::string raw_key = ImageMetadata::generateRawKey(image_hash, "ppm");

    // Upload raw image
    fake_file_service_->uploadFile(test_image_path_, raw_key, "image/x-portable-pixmap");

    // Define multiple transformations
    std::vector<TransformRequest> transformations = {
        TransformRequest(image_hash, "jpeg", 100, 50),
        TransformRequest(image_hash, "jpeg", 50, 25),
        TransformRequest(image_hash, "png", 100, 50),
        TransformRequest(image_hash, "webp", 80, 40)
    };

    // Process each transformation
    for (const auto& req : transformations) {
        // Download raw
        TempFile raw_dl("raw_");
        fake_file_service_->downloadFile(raw_key, raw_dl.getPath());

        // Transform
        TempFile transformed("trans_");
        std::string ext = (req.target_format == "jpeg") ? ".jpg" :
                         (req.target_format == "png") ? ".png" : ".webp";
        std::string transformed_path = transformed.getPath() + ext;

        bool success = image_processor_->transform(
            raw_dl.getPath(),
            transformed_path,
            req.target_format,
            req.width,
            req.height
        );
        EXPECT_TRUE(success);

        // Cache
        cache_manager_->storeInCache(req, transformed_path);
        FileUtils::deleteFile(transformed_path);
    }

    // Verify all transformations are cached
    for (const auto& req : transformations) {
        EXPECT_TRUE(cache_manager_->existsInCache(req));
    }

    // Should have 1 raw + 4 transformed = 5 objects
    EXPECT_EQ(5, fake_file_service_->getObjectCount());
}

// Test deduplication (same image uploaded twice)
TEST_F(IntegrationTest, Deduplication) {
    // Upload image first time
    std::string hash1 = FileUtils::calculateSHA256(test_image_path_);
    std::string raw_key1 = ImageMetadata::generateRawKey(hash1, "ppm");
    fake_file_service_->uploadFile(test_image_path_, raw_key1, "image/x-portable-pixmap");

    // Simulate uploading same image again
    std::string hash2 = FileUtils::calculateSHA256(test_image_path_);
    EXPECT_EQ(hash1, hash2);  // Same hash

    std::string raw_key2 = ImageMetadata::generateRawKey(hash2, "ppm");
    EXPECT_EQ(raw_key1, raw_key2);  // Same S3 key

    // Check if already exists (deduplication check)
    bool already_exists = fake_file_service_->objectExists(raw_key2);
    EXPECT_TRUE(already_exists);

    // Should still have only 1 object
    EXPECT_EQ(1, fake_file_service_->getObjectCount());
}

// Test cache hit performance (no re-transformation)
TEST_F(IntegrationTest, CacheHitPerformance) {
    std::string image_hash = FileUtils::calculateSHA256(test_image_path_);
    std::string raw_key = ImageMetadata::generateRawKey(image_hash, "ppm");

    fake_file_service_->uploadFile(test_image_path_, raw_key, "image/x-portable-pixmap");

    TransformRequest req(image_hash, "jpeg", 100, 50);

    // First request - cache miss, needs transformation
    EXPECT_FALSE(cache_manager_->existsInCache(req));

    TempFile raw_dl("raw_");
    fake_file_service_->downloadFile(raw_key, raw_dl.getPath());

    TempFile transformed("trans_");
    std::string transformed_path = transformed.getPath() + ".jpg";
    image_processor_->transform(raw_dl.getPath(), transformed_path, "jpeg", 100, 50);
    cache_manager_->storeInCache(req, transformed_path);

    // Second request - cache hit, no transformation needed
    EXPECT_TRUE(cache_manager_->existsInCache(req));
    std::string cached_key = cache_manager_->getCachedImage(req);
    EXPECT_FALSE(cached_key.empty());

    // Can directly generate URL without transformation
    std::string url = cache_manager_->getPresignedUrl(req, 3600);
    EXPECT_FALSE(url.empty());

    FileUtils::deleteFile(transformed_path);
}

// Test error handling - invalid image transformation
TEST_F(IntegrationTest, InvalidImageTransformation) {
    // Create a non-image file
    std::string invalid_file = "/tmp/gara_invalid.txt";
    std::ofstream file(invalid_file);
    file << "not an image";
    file.close();

    // Try to transform
    TempFile output("output_");
    std::string output_path = output.getPath() + ".jpg";

    bool success = image_processor_->transform(
        invalid_file,
        output_path,
        "jpeg"
    );

    EXPECT_FALSE(success);
    FileUtils::deleteFile(invalid_file);
}

// Test complete workflow with aspect ratio preservation
TEST_F(IntegrationTest, AspectRatioPreservation) {
    std::string image_hash = FileUtils::calculateSHA256(test_image_path_);
    std::string raw_key = ImageMetadata::generateRawKey(image_hash, "ppm");

    fake_file_service_->uploadFile(test_image_path_, raw_key, "image/x-portable-pixmap");

    // Request resize with only width (should preserve 2:1 aspect ratio)
    TransformRequest req(image_hash, "jpeg", 100, 0);

    TempFile raw_dl("raw_");
    fake_file_service_->downloadFile(raw_key, raw_dl.getPath());

    TempFile transformed("trans_");
    std::string transformed_path = transformed.getPath() + ".jpg";

    bool success = image_processor_->transform(
        raw_dl.getPath(),
        transformed_path,
        "jpeg",
        100,
        0  // Height 0 = maintain aspect ratio
    );

    EXPECT_TRUE(success);

    // Verify dimensions
    ImageInfo info = image_processor_->getImageInfo(transformed_path);
    EXPECT_EQ(100, info.width);
    EXPECT_EQ(50, info.height);  // 2:1 ratio preserved

    FileUtils::deleteFile(transformed_path);
}
