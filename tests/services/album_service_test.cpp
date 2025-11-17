#include <gtest/gtest.h>
#include "services/album_service.h"
#include "models/album.h"
#include "mocks/fake_file_service.h"
#include "mocks/fake_database_client.h"
#include "exceptions/album_exceptions.h"
#include "test_helpers/test_constants.h"
#include "test_helpers/test_builders.h"
#include "test_helpers/custom_matchers.h"
#include "utils/logger.h"
#include "utils/metrics.h"
#include <memory>

using namespace gara;
using namespace gara::testing;
using namespace gara::test_constants;
using namespace gara::test_builders;
using namespace gara::test_matchers;

class AlbumServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize logger and metrics for tests
        gara::Logger::initialize("gara-test", "error", gara::Logger::Format::TEXT, "test");
        gara::Metrics::initialize("GaraTest", "gara-test", "test", false);

        // Use fake services for testing
        fake_file_service_ = std::make_shared<FakeFileService>(TEST_BUCKET_NAME);
        fake_db_client_ = std::make_shared<FakeDatabaseClient>();

        // Create AlbumService with fake clients
        album_service_ = std::make_shared<AlbumService>(
            fake_db_client_,
            fake_file_service_
        );
    }

    void TearDown() override {
        fake_file_service_->clear();
        fake_db_client_->clear();
    }

    // Helper to upload fake images to S3
    void uploadFakeImage(const std::string& image_id, const std::string& format = FORMAT_JPG) {
        auto key = TestDataBuilder::createRawImageKey(image_id, format);
        auto fake_data = TestDataBuilder::createData(SMALL_DATA_SIZE);
        fake_file_service_->uploadData(fake_data, key);
    }

    std::shared_ptr<FakeFileService> fake_file_service_;
    std::shared_ptr<FakeDatabaseClient> fake_db_client_;
    std::shared_ptr<AlbumService> album_service_;
};

// ============================================================================
// CreateAlbumRequest JSON Parsing Tests
// ============================================================================

TEST_F(AlbumServiceTest, CreateAlbumRequest_FromValidJson_ParsesCorrectly) {
    // Arrange
    nlohmann::json j = {
        {"name", ALBUM_NAME_TEST},
        {"description", ALBUM_DESCRIPTION_TEST},
        {"tags", {ALBUM_TAG_1, ALBUM_TAG_2}},
        {"published", true}
    };

    // Act
    CreateAlbumRequest request = CreateAlbumRequest::fromJson(j);

    // Assert
    EXPECT_EQ(ALBUM_NAME_TEST, request.name)
        << "Name should match JSON value";

    EXPECT_EQ(ALBUM_DESCRIPTION_TEST, request.description)
        << "Description should match JSON value";

    EXPECT_EQ(ALBUM_TAGS_COUNT_TWO, request.tags.size())
        << "Should parse all tags from JSON";

    EXPECT_EQ(ALBUM_TAG_1, request.tags[ARRAY_INDEX_FIRST])
        << "First tag should match JSON value";

    EXPECT_TRUE(request.published)
        << "Published flag should be true from JSON";
}

TEST_F(AlbumServiceTest, CreateAlbumRequest_MissingName_ThrowsError) {
    // Arrange
    nlohmann::json j = {
        {"description", ALBUM_DESCRIPTION_TEST}
    };

    // Act & Assert
    EXPECT_THROW(
        {
            CreateAlbumRequest::fromJson(j);
        },
        std::runtime_error
    ) << "Creating album request without name should throw runtime_error";
}

// ============================================================================
// UpdateAlbumRequest JSON Parsing Tests
// ============================================================================

TEST_F(AlbumServiceTest, UpdateAlbumRequest_FromValidJson_ParsesCorrectly) {
    // Arrange
    nlohmann::json j = {
        {"name", ALBUM_NAME_UPDATED},
        {"description", ALBUM_DESCRIPTION_UPDATED},
        {"cover_image_id", TEST_COVER_IMAGE_ID},
        {"tags", {ALBUM_TAG_NEW}},
        {"published", false}
    };

    // Act
    UpdateAlbumRequest request = UpdateAlbumRequest::fromJson(j);

    // Assert
    EXPECT_EQ(ALBUM_NAME_UPDATED, request.name)
        << "Updated name should match JSON value";

    EXPECT_EQ(ALBUM_DESCRIPTION_UPDATED, request.description)
        << "Updated description should match JSON value";

    EXPECT_EQ(TEST_COVER_IMAGE_ID, request.cover_image_id)
        << "Cover image ID should match JSON value";

    EXPECT_EQ(ALBUM_TAGS_COUNT_ONE, request.tags.size())
        << "Should parse single tag from JSON";

    EXPECT_FALSE(request.published)
        << "Published flag should be false from JSON";
}

// ============================================================================
// AddImagesRequest JSON Parsing Tests
// ============================================================================

TEST_F(AlbumServiceTest, AddImagesRequest_FromValidJson_ParsesCorrectly) {
    // Arrange
    nlohmann::json j = {
        {"image_ids", {TEST_IMAGE_ID_1, TEST_IMAGE_ID_2, TEST_IMAGE_ID_3}},
        {"position", ALBUM_IMAGE_POSITION_2}
    };

    // Act
    AddImagesRequest request = AddImagesRequest::fromJson(j);

    // Assert
    EXPECT_EQ(ALBUM_IMAGES_COUNT_THREE, request.image_ids.size())
        << "Should parse all image IDs from JSON";

    EXPECT_EQ(TEST_IMAGE_ID_1, request.image_ids[ARRAY_INDEX_FIRST])
        << "First image ID should match JSON value";

    EXPECT_EQ(ALBUM_IMAGE_POSITION_2, request.position)
        << "Position should match JSON value";
}

TEST_F(AlbumServiceTest, AddImagesRequest_MissingImageIds_ThrowsError) {
    // Arrange
    nlohmann::json j = {
        {"position", ALBUM_IMAGE_POSITION_0}
    };

    // Act & Assert
    EXPECT_THROW(
        {
            AddImagesRequest::fromJson(j);
        },
        std::runtime_error
    ) << "Creating add images request without image_ids should throw runtime_error";
}

// ============================================================================
// ReorderImagesRequest JSON Parsing Tests
// ============================================================================

TEST_F(AlbumServiceTest, ReorderImagesRequest_FromValidJson_ParsesCorrectly) {
    // Arrange
    nlohmann::json j = {
        {"image_ids", {TEST_IMAGE_ID_3, TEST_IMAGE_ID_1, TEST_IMAGE_ID_2}}
    };

    // Act
    ReorderImagesRequest request = ReorderImagesRequest::fromJson(j);

    // Assert
    EXPECT_EQ(ALBUM_IMAGES_COUNT_THREE, request.image_ids.size())
        << "Should parse all reordered image IDs from JSON";

    EXPECT_EQ(TEST_IMAGE_ID_3, request.image_ids[ARRAY_INDEX_FIRST])
        << "First image ID should be img3 after reordering";
}

// ============================================================================
// Album JSON Serialization Tests
// ============================================================================

TEST_F(AlbumServiceTest, Album_ToJson_SerializesCorrectly) {
    // Arrange
    Album album;
    album.album_id = TEST_ALBUM_ID_123;
    album.name = ALBUM_NAME_TEST;
    album.description = ALBUM_DESCRIPTION_TEST;
    album.cover_image_id = TEST_COVER_IMAGE_ID;
    album.image_ids = {TEST_IMAGE_ID_1, TEST_IMAGE_ID_2};
    album.tags = {ALBUM_TAG_1};
    album.published = true;
    album.created_at = TEST_TIMESTAMP_CREATED;
    album.updated_at = TEST_TIMESTAMP_UPDATED;

    // Act
    nlohmann::json j = album.toJson();

    // Assert
    EXPECT_EQ(TEST_ALBUM_ID_123, j["album_id"])
        << "Album ID should be serialized correctly";

    EXPECT_EQ(ALBUM_NAME_TEST, j["name"])
        << "Album name should be serialized correctly";

    EXPECT_EQ(ALBUM_DESCRIPTION_TEST, j["description"])
        << "Album description should be serialized correctly";

    EXPECT_EQ(TEST_COVER_IMAGE_ID, j["cover_image_id"])
        << "Cover image ID should be serialized correctly";

    EXPECT_EQ(ALBUM_IMAGES_COUNT_TWO, j["image_ids"].size())
        << "All image IDs should be serialized";

    EXPECT_EQ(ALBUM_TAGS_COUNT_ONE, j["tags"].size())
        << "All tags should be serialized";

    EXPECT_TRUE(j["published"])
        << "Published flag should be serialized correctly";

    EXPECT_EQ(TEST_TIMESTAMP_CREATED, j["created_at"])
        << "Created timestamp should be serialized correctly";
}

// ============================================================================
// Album JSON Deserialization Tests
// ============================================================================

TEST_F(AlbumServiceTest, Album_FromValidJson_DeserializesCorrectly) {
    // Arrange
    nlohmann::json j = {
        {"album_id", TEST_ALBUM_ID_123},
        {"name", ALBUM_NAME_TEST},
        {"description", ALBUM_DESCRIPTION_TEST},
        {"cover_image_id", TEST_COVER_IMAGE_ID},
        {"image_ids", {TEST_IMAGE_ID_1, TEST_IMAGE_ID_2}},
        {"tags", {ALBUM_TAG_1}},
        {"published", true},
        {"created_at", TEST_TIMESTAMP_CREATED},
        {"updated_at", TEST_TIMESTAMP_UPDATED}
    };

    // Act
    Album album = Album::fromJson(j);

    // Assert
    EXPECT_EQ(TEST_ALBUM_ID_123, album.album_id)
        << "Album ID should be deserialized correctly";

    EXPECT_EQ(ALBUM_NAME_TEST, album.name)
        << "Album name should be deserialized correctly";

    EXPECT_EQ(ALBUM_DESCRIPTION_TEST, album.description)
        << "Album description should be deserialized correctly";

    EXPECT_EQ(TEST_COVER_IMAGE_ID, album.cover_image_id)
        << "Cover image ID should be deserialized correctly";

    EXPECT_EQ(ALBUM_IMAGES_COUNT_TWO, album.image_ids.size())
        << "All image IDs should be deserialized";

    EXPECT_EQ(ALBUM_TAGS_COUNT_ONE, album.tags.size())
        << "All tags should be deserialized";

    EXPECT_TRUE(album.published)
        << "Published flag should be deserialized correctly";

    EXPECT_EQ(TEST_TIMESTAMP_CREATED, album.created_at)
        << "Created timestamp should be deserialized correctly";
}

TEST_F(AlbumServiceTest, Album_FromJsonMissingName_ThrowsError) {
    // Arrange
    nlohmann::json j = {
        {"album_id", TEST_ALBUM_ID_123},
        {"description", ALBUM_DESCRIPTION_TEST}
    };

    // Act & Assert
    EXPECT_THROW(
        {
            Album::fromJson(j);
        },
        std::runtime_error
    ) << "Creating album from JSON without name should throw runtime_error";
}

// ============================================================================
// Album Creation Tests
// ============================================================================

TEST_F(AlbumServiceTest, CreateAlbum_WithValidRequest_ReturnsCreatedAlbum) {
    // Arrange
    auto request = CreateAlbumRequestBuilder()
        .withName(ALBUM_NAME_TEST)
        .withDescription(ALBUM_DESCRIPTION_TEST)
        .published(false)
        .build();

    // Act
    Album album = album_service_->createAlbum(request);

    // Assert
    EXPECT_THAT(album.album_id, IsNonEmptyString())
        << "Created album should have non-empty album_id";

    EXPECT_EQ(ALBUM_NAME_TEST, album.name)
        << "Album name should match request";

    EXPECT_EQ(ALBUM_DESCRIPTION_TEST, album.description)
        << "Album description should match request";

    EXPECT_FALSE(album.published)
        << "Album published status should match request";

    EXPECT_GT(album.created_at, TIMESTAMP_ZERO)
        << "Created timestamp should be set to a positive value";
}

TEST_F(AlbumServiceTest, CreateAlbum_WithDuplicateName_ThrowsConflictException) {
    // Arrange
    auto request = CreateAlbumRequestBuilder()
        .withName(ALBUM_NAME_DUPLICATE)
        .build();

    album_service_->createAlbum(request);

    // Act & Assert
    EXPECT_THROW(
        {
            album_service_->createAlbum(request);
        },
        exceptions::ConflictException
    ) << "Creating album with duplicate name should throw ConflictException";
}

// ============================================================================
// Album Retrieval Tests
// ============================================================================

TEST_F(AlbumServiceTest, GetAlbum_WithNonexistentId_ThrowsNotFoundException) {
    // Arrange
    std::string nonexistent_id = ALBUM_ID_NONEXISTENT;

    // Act & Assert
    EXPECT_THROW(
        {
            album_service_->getAlbum(nonexistent_id);
        },
        exceptions::NotFoundException
    ) << "Getting non-existent album should throw NotFoundException";
}

// ============================================================================
// Add Images Tests
// ============================================================================

TEST_F(AlbumServiceTest, AddImages_WithNonexistentImages_ThrowsValidationException) {
    // Arrange - Create an album
    auto create_req = CreateAlbumRequestBuilder()
        .withName(ALBUM_NAME_TEST)
        .build();
    Album album = album_service_->createAlbum(create_req);

    // Arrange - Try to add non-existent images
    auto add_req = AddImagesRequestBuilder()
        .addImageId(IMAGE_ID_NONEXISTENT)
        .appendToEnd()
        .build();

    // Act & Assert
    EXPECT_THROW(
        {
            album_service_->addImages(album.album_id, add_req);
        },
        exceptions::ValidationException
    ) << "Adding non-existent images should throw ValidationException";
}

// ============================================================================
// Reorder Images Tests
// ============================================================================

TEST_F(AlbumServiceTest, ReorderImages_WithIncorrectCount_ThrowsValidationException) {
    // Arrange - Create album
    auto create_req = CreateAlbumRequestBuilder()
        .withName(ALBUM_NAME_TEST)
        .build();
    Album album = album_service_->createAlbum(create_req);

    // Arrange - Upload and add images
    uploadFakeImage(TEST_IMAGE_ID_1, FORMAT_JPG);
    uploadFakeImage(TEST_IMAGE_ID_2, FORMAT_JPG);

    auto add_req = AddImagesRequestBuilder()
        .addImageId(TEST_IMAGE_ID_1)
        .addImageId(TEST_IMAGE_ID_2)
        .build();
    album = album_service_->addImages(album.album_id, add_req);

    // Arrange - Try to reorder with wrong count (missing img2)
    ReorderImagesRequest reorder_req;
    reorder_req.image_ids = {TEST_IMAGE_ID_1};  // Missing TEST_IMAGE_ID_2

    // Act & Assert
    EXPECT_THROW(
        {
            album_service_->reorderImages(album.album_id, reorder_req);
        },
        exceptions::ValidationException
    ) << "Reordering with incorrect image count should throw ValidationException";
}
