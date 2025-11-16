#include <gtest/gtest.h>
#include "controllers/album_controller.h"
#include "services/album_service.h"
#include "services/secrets_service.h"
#include "mocks/mock_s3_service.h"
#include "utils/logger.h"
#include "utils/metrics.h"
#include <memory>
#include <nlohmann/json.hpp>

using namespace gara;
using namespace gara::testing;
using json = nlohmann::json;

class AlbumControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize logger and metrics for tests
        gara::Logger::initialize("gara-test", "error", gara::Logger::Format::TEXT, "test");
        gara::Metrics::initialize("GaraTest", "gara-test", "test", false);

        // Use fake S3 service
        fake_s3_ = std::make_shared<FakeS3Service>("test-bucket", "us-east-1");

        // Note: Not creating AlbumService or AlbumController here as they would
        // initialize real AWS services. These tests focus on data serialization.
    }

    void TearDown() override {
        fake_s3_->clear();
    }

    std::shared_ptr<FakeS3Service> fake_s3_;
};

// Test JSON serialization/deserialization for requests
TEST_F(AlbumControllerTest, CreateAlbumRequestSerialization) {
    json j = {
        {"name", "Test Album"},
        {"description", "Test description"},
        {"tags", {"tag1", "tag2"}},
        {"published", false}
    };

    CreateAlbumRequest request = CreateAlbumRequest::fromJson(j);

    EXPECT_EQ(request.name, "Test Album");
    EXPECT_EQ(request.description, "Test description");
    EXPECT_EQ(request.tags.size(), 2);
    EXPECT_FALSE(request.published);
}

TEST_F(AlbumControllerTest, UpdateAlbumRequestSerialization) {
    json j = {
        {"name", "Updated Name"},
        {"description", "Updated description"},
        {"cover_image_id", "image123"},
        {"published", true}
    };

    UpdateAlbumRequest request = UpdateAlbumRequest::fromJson(j);

    EXPECT_EQ(request.name, "Updated Name");
    EXPECT_EQ(request.description, "Updated description");
    EXPECT_EQ(request.cover_image_id, "image123");
    EXPECT_TRUE(request.published);
}

TEST_F(AlbumControllerTest, AddImagesRequestSerialization) {
    json j = {
        {"image_ids", {"img1", "img2"}},
        {"position", 0}
    };

    AddImagesRequest request = AddImagesRequest::fromJson(j);

    EXPECT_EQ(request.image_ids.size(), 2);
    EXPECT_EQ(request.position, 0);
}

TEST_F(AlbumControllerTest, ReorderImagesRequestSerialization) {
    json j = {
        {"image_ids", {"img2", "img1", "img3"}}
    };

    ReorderImagesRequest request = ReorderImagesRequest::fromJson(j);

    EXPECT_EQ(request.image_ids.size(), 3);
    EXPECT_EQ(request.image_ids[0], "img2");
}

// Test Album to JSON conversion
TEST_F(AlbumControllerTest, AlbumToJsonConversion) {
    Album album;
    album.album_id = "album123";
    album.name = "My Album";
    album.description = "Description";
    album.cover_image_id = "cover123";
    album.image_ids = {"img1", "img2"};
    album.tags = {"travel", "nature"};
    album.published = true;
    album.created_at = 1234567890;
    album.updated_at = 1234567900;

    json j = album.toJson();

    EXPECT_EQ(j["album_id"], "album123");
    EXPECT_EQ(j["name"], "My Album");
    EXPECT_EQ(j["description"], "Description");
    EXPECT_EQ(j["cover_image_id"], "cover123");
    EXPECT_EQ(j["image_ids"].size(), 2);
    EXPECT_EQ(j["tags"].size(), 2);
    EXPECT_TRUE(j["published"]);
}

// Test empty album
TEST_F(AlbumControllerTest, EmptyAlbum) {
    Album album;
    album.album_id = "empty";
    album.name = "Empty Album";

    json j = album.toJson();

    EXPECT_EQ(j["album_id"], "empty");
    EXPECT_EQ(j["name"], "Empty Album");
    EXPECT_EQ(j["image_ids"].size(), 0);
    EXPECT_EQ(j["tags"].size(), 0);
}

// Note: The following tests would require Crow app integration
// For a complete test suite, you would:
// 1. Create a test Crow app instance
// 2. Register controller routes
// 3. Make HTTP requests and verify responses
// 4. Test authentication for protected endpoints
// 5. Test error handling (404, 400, 401)

/*
// Example of integration tests that would be added:

TEST_F(AlbumControllerTest, CreateAlbumEndpoint) {
    crow::SimpleApp app;
    album_controller_->registerRoutes(app);

    // Create request
    json request_body = {
        {"name", "Test Album"},
        {"description", "Test"},
        {"published", false}
    };

    // Make POST request to /api/albums
    // Verify 201 Created response
    // Verify response contains album_id
}

TEST_F(AlbumControllerTest, GetAlbumEndpoint) {
    // Setup: Create an album
    // Make GET request to /api/albums/{id}
    // Verify 200 OK response
    // Verify response contains album data with presigned URLs
}

TEST_F(AlbumControllerTest, ListAlbumsPublishedOnly) {
    // Setup: Create published and unpublished albums
    // Make GET request to /api/albums?published=true
    // Verify only published albums returned
}

TEST_F(AlbumControllerTest, UpdateAlbumRequiresAuth) {
    // Make PUT request without X-API-Key header
    // Verify 401 Unauthorized response
}

TEST_F(AlbumControllerTest, DeleteAlbumSuccess) {
    // Setup: Create an album
    // Make DELETE request with valid API key
    // Verify 200 OK response
    // Verify album is deleted
}

TEST_F(AlbumControllerTest, AddImagesToAlbum) {
    // Setup: Create album and upload images to S3
    // Make POST request to /api/albums/{id}/images
    // Verify images added to album
}

TEST_F(AlbumControllerTest, RemoveImageFromAlbum) {
    // Setup: Create album with images
    // Make DELETE request to /api/albums/{id}/images/{image_id}
    // Verify image removed
}

TEST_F(AlbumControllerTest, ReorderImagesInAlbum) {
    // Setup: Create album with images
    // Make PUT request to /api/albums/{id}/reorder
    // Verify new order applied
}

TEST_F(AlbumControllerTest, GetAlbumNotFound) {
    // Make GET request to non-existent album
    // Verify 404 Not Found response
}

TEST_F(AlbumControllerTest, UnpublishedAlbumHiddenFromUnauthenticated) {
    // Setup: Create unpublished album
    // Make GET request without auth
    // Verify 404 response
    // Make GET request with auth
    // Verify 200 response
}

TEST_F(AlbumControllerTest, CORSHeadersPresent) {
    // Make OPTIONS request
    // Verify CORS headers in response
}
*/
