#include <gtest/gtest.h>
#include "services/album_service.h"
#include "services/image_processor.h"
#include "services/cache_manager.h"
#include "utils/file_utils.h"
#include "models/album.h"
#include "mocks/mock_s3_service.h"
#include "mocks/fake_dynamodb_client.h"
#include "exceptions/album_exceptions.h"
#include <nlohmann/json.hpp>
#include <fstream>

using namespace gara;
using namespace gara::utils;
using namespace gara::exceptions;

class ErrorHandlingTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ImageProcessor::initialize();
    }

    static void TearDownTestSuite() {
        ImageProcessor::shutdown();
    }

    void SetUp() override {
        fake_s3_ = std::make_shared<FakeS3Service>("test-bucket", "us-east-1");
        fake_dynamodb_ = std::make_shared<FakeDynamoDBClient>();
        album_service_ = std::make_shared<AlbumService>("test-table", fake_dynamodb_, fake_s3_);
        image_processor_ = std::make_shared<ImageProcessor>();
        cache_manager_ = std::make_shared<CacheManager>(fake_s3_);
    }

    void TearDown() override {
        fake_s3_->clear();
        fake_dynamodb_->clear();
    }

    std::shared_ptr<FakeS3Service> fake_s3_;
    std::shared_ptr<FakeDynamoDBClient> fake_dynamodb_;
    std::shared_ptr<AlbumService> album_service_;
    std::shared_ptr<ImageProcessor> image_processor_;
    std::shared_ptr<CacheManager> cache_manager_;
};

// Album Service Error Handling Tests

TEST_F(ErrorHandlingTest, CreateAlbumWithEmptyName) {
    CreateAlbumRequest req;
    req.name = "";

    EXPECT_THROW({
        album_service_->createAlbum(req);
    }, std::runtime_error);
}

TEST_F(ErrorHandlingTest, CreateAlbumWithDuplicateName) {
    CreateAlbumRequest req;
    req.name = "Duplicate Album";

    album_service_->createAlbum(req);

    EXPECT_THROW({
        album_service_->createAlbum(req);
    }, ConflictException);
}

TEST_F(ErrorHandlingTest, GetNonExistentAlbum) {
    EXPECT_THROW({
        album_service_->getAlbum("nonexistent-id");
    }, NotFoundException);
}

TEST_F(ErrorHandlingTest, UpdateNonExistentAlbum) {
    UpdateAlbumRequest req;
    req.name = "Updated Name";

    EXPECT_THROW({
        album_service_->updateAlbum("nonexistent-id", req);
    }, NotFoundException);
}

TEST_F(ErrorHandlingTest, DeleteNonExistentAlbum) {
    bool result = album_service_->deleteAlbum("nonexistent-id");
    EXPECT_FALSE(result);
}

TEST_F(ErrorHandlingTest, AddNonExistentImageToAlbum) {
    CreateAlbumRequest create_req;
    create_req.name = "Test Album";
    Album album = album_service_->createAlbum(create_req);

    AddImagesRequest add_req;
    add_req.image_ids = {"nonexistent-image-id"};

    EXPECT_THROW({
        album_service_->addImages(album.album_id, add_req);
    }, ValidationException);
}

TEST_F(ErrorHandlingTest, AddEmptyImageListToAlbum) {
    CreateAlbumRequest create_req;
    create_req.name = "Test Album";
    Album album = album_service_->createAlbum(create_req);

    AddImagesRequest add_req;
    add_req.image_ids = {};

    EXPECT_THROW({
        album_service_->addImages(album.album_id, add_req);
    }, std::runtime_error);
}

TEST_F(ErrorHandlingTest, RemoveNonExistentImageFromAlbum) {
    CreateAlbumRequest create_req;
    create_req.name = "Test Album";
    Album album = album_service_->createAlbum(create_req);

    EXPECT_THROW({
        album_service_->removeImage(album.album_id, "nonexistent-image");
    }, ValidationException);
}

TEST_F(ErrorHandlingTest, ReorderWithWrongImageCount) {
    CreateAlbumRequest create_req;
    create_req.name = "Test Album";
    Album album = album_service_->createAlbum(create_req);

    // Add 2 images
    fake_s3_->uploadData({'d'}, "raw/img1.jpg");
    fake_s3_->uploadData({'d'}, "raw/img2.jpg");

    AddImagesRequest add_req;
    add_req.image_ids = {"img1", "img2"};
    album = album_service_->addImages(album.album_id, add_req);

    // Try to reorder with only 1 image
    ReorderImagesRequest reorder_req;
    reorder_req.image_ids = {"img1"};

    EXPECT_THROW({
        album_service_->reorderImages(album.album_id, reorder_req);
    }, ValidationException);
}

TEST_F(ErrorHandlingTest, ReorderWithInvalidImageIds) {
    CreateAlbumRequest create_req;
    create_req.name = "Test Album";
    Album album = album_service_->createAlbum(create_req);

    // Add images
    fake_s3_->uploadData({'d'}, "raw/img1.jpg");
    AddImagesRequest add_req;
    add_req.image_ids = {"img1"};
    album = album_service_->addImages(album.album_id, add_req);

    // Try to reorder with different image ID
    ReorderImagesRequest reorder_req;
    reorder_req.image_ids = {"wrong_img"};

    EXPECT_THROW({
        album_service_->reorderImages(album.album_id, reorder_req);
    }, ValidationException);
}

// JSON Parsing Error Handling Tests

TEST_F(ErrorHandlingTest, CreateAlbumRequestMissingName) {
    nlohmann::json j = {
        {"description", "Test"}
    };

    EXPECT_THROW({
        CreateAlbumRequest::fromJson(j);
    }, std::runtime_error);
}

TEST_F(ErrorHandlingTest, AddImagesRequestMissingImageIds) {
    nlohmann::json j = {
        {"position", 0}
    };

    EXPECT_THROW({
        AddImagesRequest::fromJson(j);
    }, std::runtime_error);
}

TEST_F(ErrorHandlingTest, AlbumFromJsonMissingName) {
    nlohmann::json j = {
        {"album_id", "123"},
        {"description", "Test"}
    };

    EXPECT_THROW({
        Album::fromJson(j);
    }, std::runtime_error);
}

// Image Processor Error Handling Tests

TEST_F(ErrorHandlingTest, TransformNonExistentFile) {
    bool result = image_processor_->transform(
        "/tmp/nonexistent_image.jpg",
        "/tmp/output.jpg",
        "jpeg"
    );

    EXPECT_FALSE(result);
}

TEST_F(ErrorHandlingTest, TransformInvalidImageFile) {
    // Create a non-image file
    std::string invalid_path = "/tmp/invalid_image.txt";
    std::ofstream file(invalid_path);
    file << "This is not an image";
    file.close();

    bool result = image_processor_->transform(
        invalid_path,
        "/tmp/output.jpg",
        "jpeg"
    );

    EXPECT_FALSE(result);
    FileUtils::deleteFile(invalid_path);
}

TEST_F(ErrorHandlingTest, GetImageInfoNonExistent) {
    ImageInfo info = image_processor_->getImageInfo("/tmp/nonexistent.jpg");

    EXPECT_FALSE(info.is_valid);
}

TEST_F(ErrorHandlingTest, GetImageInfoInvalidFile) {
    std::string invalid_path = "/tmp/invalid.jpg";
    std::ofstream file(invalid_path);
    file << "Not an image";
    file.close();

    ImageInfo info = image_processor_->getImageInfo(invalid_path);

    EXPECT_FALSE(info.is_valid);
    FileUtils::deleteFile(invalid_path);
}

// File Utils Error Handling Tests

TEST_F(ErrorHandlingTest, CalculateSHA256NonExistentFile) {
    std::string hash = FileUtils::calculateSHA256("/tmp/nonexistent_file.bin");

    EXPECT_TRUE(hash.empty());
}

TEST_F(ErrorHandlingTest, ReadFileNonExistent) {
    std::vector<char> data = FileUtils::readFile("/tmp/nonexistent.dat");

    EXPECT_TRUE(data.empty());
}

TEST_F(ErrorHandlingTest, FileExistsNonExistent) {
    bool exists = FileUtils::fileExists("/tmp/definitely_does_not_exist_12345.txt");

    EXPECT_FALSE(exists);
}

TEST_F(ErrorHandlingTest, DeleteNonExistentFile) {
    bool result = FileUtils::deleteFile("/tmp/nonexistent_to_delete.txt");

    // deleteFile should handle non-existent gracefully
    // Check the actual implementation behavior
    EXPECT_FALSE(result);
}

// Cache Manager Error Handling Tests

TEST_F(ErrorHandlingTest, CacheNonExistentTransformedImage) {
    TransformRequest req("image123", "jpeg", 800, 600);

    bool result = cache_manager_->storeInCache(req, "/tmp/nonexistent_transformed.jpg");

    EXPECT_FALSE(result);
}

TEST_F(ErrorHandlingTest, GetCachedImageNonExistent) {
    TransformRequest req("nonexistent", "jpeg", 800, 600);

    std::string cached = cache_manager_->getCachedImage(req);

    EXPECT_TRUE(cached.empty());
}

TEST_F(ErrorHandlingTest, GetPresignedUrlNonCached) {
    TransformRequest req("nonexistent", "jpeg", 800, 600);

    std::string url = cache_manager_->getPresignedUrl(req);

    EXPECT_TRUE(url.empty());
}

// Edge Cases Tests

TEST_F(ErrorHandlingTest, AlbumNameWithSpecialCharacters) {
    CreateAlbumRequest req;
    req.name = "Album with \"quotes\" and <html> tags & symbols!@#$%";

    Album album = album_service_->createAlbum(req);

    EXPECT_EQ(req.name, album.name);

    Album retrieved = album_service_->getAlbum(album.album_id);
    EXPECT_EQ(req.name, retrieved.name);
}

TEST_F(ErrorHandlingTest, AlbumNameMaxLength) {
    CreateAlbumRequest req;
    req.name = std::string(10000, 'A'); // Very long name

    Album album = album_service_->createAlbum(req);
    Album retrieved = album_service_->getAlbum(album.album_id);

    EXPECT_EQ(req.name, retrieved.name);
}

TEST_F(ErrorHandlingTest, AlbumDescriptionUnicode) {
    CreateAlbumRequest req;
    req.name = "Unicode Test";
    req.description = "你好世界 مرحبا بالعالم Привет мир 🌍🌎🌏";

    Album album = album_service_->createAlbum(req);
    Album retrieved = album_service_->getAlbum(album.album_id);

    EXPECT_EQ(req.description, retrieved.description);
}

TEST_F(ErrorHandlingTest, ManyTagsEdgeCase) {
    CreateAlbumRequest req;
    req.name = "Many Tags";

    // Add maximum reasonable number of tags
    for (int i = 0; i < 1000; ++i) {
        req.tags.push_back("tag" + std::to_string(i));
    }

    Album album = album_service_->createAlbum(req);
    Album retrieved = album_service_->getAlbum(album.album_id);

    EXPECT_EQ(1000, retrieved.tags.size());
}

TEST_F(ErrorHandlingTest, EmptyAlbumDescription) {
    CreateAlbumRequest req;
    req.name = "Empty Description";
    req.description = "";

    Album album = album_service_->createAlbum(req);

    EXPECT_EQ("", album.description);
}

TEST_F(ErrorHandlingTest, ZeroDimensionTransform) {
    // This should maintain aspect ratio
    TransformRequest req("image123", "jpeg", 0, 0);

    std::string cache_key = req.getCacheKey();
    EXPECT_FALSE(cache_key.empty());
    EXPECT_NE(std::string::npos, cache_key.find("0"));
}

TEST_F(ErrorHandlingTest, NegativeImageDimensions) {
    // TransformRequest should handle negative dimensions
    TransformRequest req("image123", "jpeg", -100, -100);

    // Verify it doesn't crash
    std::string cache_key = req.getCacheKey();
    EXPECT_FALSE(cache_key.empty());
}

TEST_F(ErrorHandlingTest, VeryLargeImageDimensions) {
    TransformRequest req("image123", "jpeg", 100000, 100000);

    std::string cache_key = req.getCacheKey();
    EXPECT_FALSE(cache_key.empty());
}

TEST_F(ErrorHandlingTest, InvalidFormatString) {
    TransformRequest req("image123", "", 800, 600);

    std::string cache_key = req.getCacheKey();
    EXPECT_FALSE(cache_key.empty());
}

TEST_F(ErrorHandlingTest, UpdateAlbumNameToDuplicate) {
    CreateAlbumRequest req1;
    req1.name = "Album 1";
    Album album1 = album_service_->createAlbum(req1);

    CreateAlbumRequest req2;
    req2.name = "Album 2";
    Album album2 = album_service_->createAlbum(req2);

    UpdateAlbumRequest update_req;
    update_req.name = "Album 1"; // Try to rename to duplicate

    EXPECT_THROW({
        album_service_->updateAlbum(album2.album_id, update_req);
    }, ConflictException);
}

TEST_F(ErrorHandlingTest, ConcurrentAlbumCreation) {
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i, &success_count]() {
            try {
                CreateAlbumRequest req;
                req.name = "Concurrent Album " + std::to_string(i);
                album_service_->createAlbum(req);
                success_count++;
            } catch (...) {
                // Ignore errors in concurrent test
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Most should succeed
    EXPECT_GE(success_count.load(), 8);
}

TEST_F(ErrorHandlingTest, UpdateWithNoChanges) {
    CreateAlbumRequest create_req;
    create_req.name = "Original Album";
    Album album = album_service_->createAlbum(create_req);

    UpdateAlbumRequest update_req;
    update_req.name = "Original Album"; // Same name

    Album updated = album_service_->updateAlbum(album.album_id, update_req);

    EXPECT_EQ(album.name, updated.name);
}

TEST_F(ErrorHandlingTest, AddSameImageTwice) {
    CreateAlbumRequest create_req;
    create_req.name = "Test Album";
    Album album = album_service_->createAlbum(create_req);

    fake_s3_->uploadData({'d'}, "raw/img1.jpg");

    AddImagesRequest add_req;
    add_req.image_ids = {"img1"};
    album = album_service_->addImages(album.album_id, add_req);

    // Try to add same image again
    EXPECT_THROW({
        album_service_->addImages(album.album_id, add_req);
    }, ValidationException);
}

TEST_F(ErrorHandlingTest, BoundaryTimestamps) {
    CreateAlbumRequest req;
    req.name = "Timestamp Boundary Test";

    Album album = album_service_->createAlbum(req);

    // Verify timestamp is valid
    EXPECT_GT(album.created_at, 0);
    EXPECT_GT(album.updated_at, 0);
    EXPECT_GE(album.updated_at, album.created_at);
}
