#include <gtest/gtest.h>
#include "services/album_service.h"
#include "models/album.h"
#include "mocks/mock_s3_service.h"
#include "mocks/fake_dynamodb_client.h"
#include "exceptions/album_exceptions.h"
#include <memory>

using namespace gara;
using namespace gara::testing;

class AlbumServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use fake services for testing
        fake_s3_ = std::make_shared<FakeS3Service>("test-bucket", "us-east-1");
        fake_dynamodb_ = std::make_shared<FakeDynamoDBClient>();

        // Create AlbumService with fake clients
        album_service_ = std::make_shared<AlbumService>("test-albums-table", fake_dynamodb_, fake_s3_);
    }

    void TearDown() override {
        fake_s3_->clear();
        fake_dynamodb_->clear();
    }

    std::shared_ptr<FakeS3Service> fake_s3_;
    std::shared_ptr<FakeDynamoDBClient> fake_dynamodb_;
    std::shared_ptr<AlbumService> album_service_;
};

// Test CreateAlbumRequest JSON parsing
TEST_F(AlbumServiceTest, CreateAlbumRequestFromJson) {
    nlohmann::json j = {
        {"name", "Test Album"},
        {"description", "A test album"},
        {"tags", {"tag1", "tag2"}},
        {"published", true}
    };

    CreateAlbumRequest request = CreateAlbumRequest::fromJson(j);

    EXPECT_EQ(request.name, "Test Album");
    EXPECT_EQ(request.description, "A test album");
    EXPECT_EQ(request.tags.size(), 2);
    EXPECT_EQ(request.tags[0], "tag1");
    EXPECT_TRUE(request.published);
}

// Test CreateAlbumRequest requires name
TEST_F(AlbumServiceTest, CreateAlbumRequestRequiresName) {
    nlohmann::json j = {
        {"description", "A test album"}
    };

    EXPECT_THROW({
        CreateAlbumRequest::fromJson(j);
    }, std::runtime_error);
}

// Test UpdateAlbumRequest JSON parsing
TEST_F(AlbumServiceTest, UpdateAlbumRequestFromJson) {
    nlohmann::json j = {
        {"name", "Updated Album"},
        {"description", "Updated description"},
        {"cover_image_id", "abc123"},
        {"tags", {"new_tag"}},
        {"published", false}
    };

    UpdateAlbumRequest request = UpdateAlbumRequest::fromJson(j);

    EXPECT_EQ(request.name, "Updated Album");
    EXPECT_EQ(request.description, "Updated description");
    EXPECT_EQ(request.cover_image_id, "abc123");
    EXPECT_EQ(request.tags.size(), 1);
    EXPECT_FALSE(request.published);
}

// Test AddImagesRequest JSON parsing
TEST_F(AlbumServiceTest, AddImagesRequestFromJson) {
    nlohmann::json j = {
        {"image_ids", {"img1", "img2", "img3"}},
        {"position", 2}
    };

    AddImagesRequest request = AddImagesRequest::fromJson(j);

    EXPECT_EQ(request.image_ids.size(), 3);
    EXPECT_EQ(request.image_ids[0], "img1");
    EXPECT_EQ(request.position, 2);
}

// Test AddImagesRequest requires image_ids
TEST_F(AlbumServiceTest, AddImagesRequestRequiresImageIds) {
    nlohmann::json j = {
        {"position", 0}
    };

    EXPECT_THROW({
        AddImagesRequest::fromJson(j);
    }, std::runtime_error);
}

// Test ReorderImagesRequest JSON parsing
TEST_F(AlbumServiceTest, ReorderImagesRequestFromJson) {
    nlohmann::json j = {
        {"image_ids", {"img3", "img1", "img2"}}
    };

    ReorderImagesRequest request = ReorderImagesRequest::fromJson(j);

    EXPECT_EQ(request.image_ids.size(), 3);
    EXPECT_EQ(request.image_ids[0], "img3");
}

// Test Album JSON serialization
TEST_F(AlbumServiceTest, AlbumToJson) {
    Album album;
    album.album_id = "123";
    album.name = "Test Album";
    album.description = "Description";
    album.cover_image_id = "cover123";
    album.image_ids = {"img1", "img2"};
    album.tags = {"tag1"};
    album.published = true;
    album.created_at = 1234567890;
    album.updated_at = 1234567900;

    nlohmann::json j = album.toJson();

    EXPECT_EQ(j["album_id"], "123");
    EXPECT_EQ(j["name"], "Test Album");
    EXPECT_EQ(j["description"], "Description");
    EXPECT_EQ(j["cover_image_id"], "cover123");
    EXPECT_EQ(j["image_ids"].size(), 2);
    EXPECT_EQ(j["tags"].size(), 1);
    EXPECT_TRUE(j["published"]);
    EXPECT_EQ(j["created_at"], 1234567890);
}

// Test Album JSON deserialization
TEST_F(AlbumServiceTest, AlbumFromJson) {
    nlohmann::json j = {
        {"album_id", "123"},
        {"name", "Test Album"},
        {"description", "Description"},
        {"cover_image_id", "cover123"},
        {"image_ids", {"img1", "img2"}},
        {"tags", {"tag1"}},
        {"published", true},
        {"created_at", 1234567890},
        {"updated_at", 1234567900}
    };

    Album album = Album::fromJson(j);

    EXPECT_EQ(album.album_id, "123");
    EXPECT_EQ(album.name, "Test Album");
    EXPECT_EQ(album.description, "Description");
    EXPECT_EQ(album.cover_image_id, "cover123");
    EXPECT_EQ(album.image_ids.size(), 2);
    EXPECT_EQ(album.tags.size(), 1);
    EXPECT_TRUE(album.published);
    EXPECT_EQ(album.created_at, 1234567890);
}

// Test Album requires name
TEST_F(AlbumServiceTest, AlbumRequiresName) {
    nlohmann::json j = {
        {"album_id", "123"},
        {"description", "Description"}
    };

    EXPECT_THROW({
        Album::fromJson(j);
    }, std::runtime_error);
}

// Integration tests with FakeDynamoDBClient

TEST_F(AlbumServiceTest, CreateAlbumSuccess) {
    CreateAlbumRequest request;
    request.name = "Test Album";
    request.description = "Test description";
    request.published = false;

    Album album = album_service_->createAlbum(request);

    EXPECT_FALSE(album.album_id.empty());
    EXPECT_EQ(album.name, "Test Album");
    EXPECT_EQ(album.description, "Test description");
    EXPECT_FALSE(album.published);
    EXPECT_GT(album.created_at, 0);
}

TEST_F(AlbumServiceTest, GetAlbumNotFound) {
    EXPECT_THROW({
        album_service_->getAlbum("nonexistent");
    }, exceptions::NotFoundException);
}

TEST_F(AlbumServiceTest, CreateAlbumDuplicateName) {
    CreateAlbumRequest request;
    request.name = "Duplicate Album";

    album_service_->createAlbum(request);

    EXPECT_THROW({
        album_service_->createAlbum(request);
    }, exceptions::ConflictException);
}

TEST_F(AlbumServiceTest, AddImagesValidatesExistence) {
    // Setup: Create an album
    CreateAlbumRequest create_req;
    create_req.name = "Test Album";
    Album album = album_service_->createAlbum(create_req);

    // Test: Try to add non-existent images
    AddImagesRequest add_req;
    add_req.image_ids = {"nonexistent_image"};
    add_req.position = -1;

    EXPECT_THROW({
        album_service_->addImages(album.album_id, add_req);
    }, exceptions::ValidationException);
}

TEST_F(AlbumServiceTest, ReorderImagesValidatesCount) {
    // Setup: Create album with images
    CreateAlbumRequest create_req;
    create_req.name = "Test Album";
    Album album = album_service_->createAlbum(create_req);

    // Add images
    fake_s3_->uploadData({'d'}, "raw/img1.jpg");
    fake_s3_->uploadData({'d'}, "raw/img2.jpg");

    AddImagesRequest add_req;
    add_req.image_ids = {"img1", "img2"};
    album = album_service_->addImages(album.album_id, add_req);

    // Test: Try to reorder with wrong count
    ReorderImagesRequest reorder_req;
    reorder_req.image_ids = {"img1"};  // Missing img2

    EXPECT_THROW({
        album_service_->reorderImages(album.album_id, reorder_req);
    }, exceptions::ValidationException);
}
