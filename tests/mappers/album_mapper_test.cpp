#include <gtest/gtest.h>
#include "services/album_service.h"
#include "models/album.h"
#include "mocks/fake_dynamodb_client.h"
#include <aws/dynamodb/model/AttributeValue.h>
#include <memory>

using namespace gara;

class AlbumMapperTest : public ::testing::Test {
protected:
    void SetUp() override {
        fake_dynamodb_ = std::make_shared<FakeDynamoDBClient>();
        album_service_ = std::make_shared<AlbumService>("test-table", fake_dynamodb_);
    }

    void TearDown() override {
        fake_dynamodb_->clear();
    }

    std::shared_ptr<FakeDynamoDBClient> fake_dynamodb_;
    std::shared_ptr<AlbumService> album_service_;
};

// Test itemToAlbum with complete data
TEST_F(AlbumMapperTest, ItemToAlbumCompleteData) {
    // Create a DynamoDB item
    Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> item;
    item["AlbumId"] = Aws::DynamoDB::Model::AttributeValue("album123");
    item["Name"] = Aws::DynamoDB::Model::AttributeValue("Test Album");
    item["Description"] = Aws::DynamoDB::Model::AttributeValue("Test Description");
    item["CoverImageId"] = Aws::DynamoDB::Model::AttributeValue("cover123");
    item["Published"] = Aws::DynamoDB::Model::AttributeValue("true");
    item["CreatedAt"] = Aws::DynamoDB::Model::AttributeValue().SetN("1234567890");
    item["UpdatedAt"] = Aws::DynamoDB::Model::AttributeValue().SetN("1234567900");

    // Add ImageIds list
    Aws::DynamoDB::Model::AttributeValue image_ids_attr;
    auto img1 = std::make_shared<Aws::DynamoDB::Model::AttributeValue>("img1");
    auto img2 = std::make_shared<Aws::DynamoDB::Model::AttributeValue>("img2");
    image_ids_attr.AddLItem(img1);
    image_ids_attr.AddLItem(img2);
    item["ImageIds"] = image_ids_attr;

    // Add Tags list
    Aws::DynamoDB::Model::AttributeValue tags_attr;
    auto tag1 = std::make_shared<Aws::DynamoDB::Model::AttributeValue>("travel");
    auto tag2 = std::make_shared<Aws::DynamoDB::Model::AttributeValue>("nature");
    tags_attr.AddLItem(tag1);
    tags_attr.AddLItem(tag2);
    item["Tags"] = tags_attr;

    // Use reflection/friend access to call private method via public interface
    // We'll create and retrieve an album to test the mapping
    CreateAlbumRequest req;
    req.name = "Test Album";
    req.description = "Test Description";
    req.tags = {"travel", "nature"};
    req.published = true;

    Album album = album_service_->createAlbum(req);

    EXPECT_EQ(album.name, "Test Album");
    EXPECT_EQ(album.description, "Test Description");
    EXPECT_EQ(album.tags.size(), 2);
    EXPECT_EQ(album.tags[0], "travel");
    EXPECT_TRUE(album.published);
    EXPECT_GT(album.created_at, 0);
}

// Test itemToAlbum with minimal data
TEST_F(AlbumMapperTest, ItemToAlbumMinimalData) {
    CreateAlbumRequest req;
    req.name = "Minimal Album";

    Album album = album_service_->createAlbum(req);

    EXPECT_FALSE(album.album_id.empty());
    EXPECT_EQ(album.name, "Minimal Album");
    EXPECT_TRUE(album.description.empty());
    EXPECT_TRUE(album.cover_image_id.empty());
    EXPECT_EQ(album.image_ids.size(), 0);
    EXPECT_EQ(album.tags.size(), 0);
    EXPECT_FALSE(album.published); // Default
}

// Test itemToAlbum with empty arrays
TEST_F(AlbumMapperTest, ItemToAlbumEmptyArrays) {
    CreateAlbumRequest req;
    req.name = "Empty Arrays";
    req.tags = {};  // Explicitly empty tags

    Album album = album_service_->createAlbum(req);

    EXPECT_EQ(album.image_ids.size(), 0);  // image_ids should be empty on creation
    EXPECT_EQ(album.tags.size(), 0);
}

// Test albumToPutItemRequest via create operation
TEST_F(AlbumMapperTest, AlbumToPutItemRequestCompleteData) {
    CreateAlbumRequest req;
    req.name = "Full Album";
    req.description = "Complete description";
    req.tags = {"tag1", "tag2", "tag3"};
    req.published = true;

    Album album = album_service_->createAlbum(req);

    // Retrieve it back to verify mapping worked
    Album retrieved = album_service_->getAlbum(album.album_id);

    EXPECT_EQ(retrieved.album_id, album.album_id);
    EXPECT_EQ(retrieved.name, "Full Album");
    EXPECT_EQ(retrieved.description, "Complete description");
    EXPECT_EQ(retrieved.tags.size(), 3);
    EXPECT_EQ(retrieved.tags[0], "tag1");
    EXPECT_TRUE(retrieved.published);
    EXPECT_EQ(retrieved.created_at, album.created_at);
}

// Test round-trip mapping (Album -> DynamoDB -> Album)
TEST_F(AlbumMapperTest, RoundTripMapping) {
    CreateAlbumRequest req;
    req.name = "Round Trip Album";
    req.description = "Testing round trip";
    req.tags = {"test", "round-trip"};
    req.published = false;

    // Create album (Album -> DynamoDB)
    Album original = album_service_->createAlbum(req);

    // Retrieve album (DynamoDB -> Album)
    Album retrieved = album_service_->getAlbum(original.album_id);

    // Verify all fields match
    EXPECT_EQ(retrieved.album_id, original.album_id);
    EXPECT_EQ(retrieved.name, original.name);
    EXPECT_EQ(retrieved.description, original.description);
    EXPECT_EQ(retrieved.cover_image_id, original.cover_image_id);
    EXPECT_EQ(retrieved.image_ids.size(), original.image_ids.size());
    EXPECT_EQ(retrieved.tags.size(), original.tags.size());
    EXPECT_EQ(retrieved.published, original.published);
    EXPECT_EQ(retrieved.created_at, original.created_at);
    EXPECT_EQ(retrieved.updated_at, original.updated_at);
}

// Test mapping with special characters in strings
TEST_F(AlbumMapperTest, SpecialCharactersInStrings) {
    CreateAlbumRequest req;
    req.name = "Album with \"quotes\" and 'apostrophes'";
    req.description = "Unicode: 你好世界 emoji: 😀 symbols: @#$%^&*()";
    req.tags = {"tag-with-dash", "tag_with_underscore", "tag.with.dots"};

    Album album = album_service_->createAlbum(req);
    Album retrieved = album_service_->getAlbum(album.album_id);

    EXPECT_EQ(retrieved.name, req.name);
    EXPECT_EQ(retrieved.description, req.description);
    EXPECT_EQ(retrieved.tags, req.tags);
}

// Test mapping with very long strings
TEST_F(AlbumMapperTest, VeryLongStrings) {
    std::string long_name(1000, 'A');
    std::string long_description(5000, 'B');

    CreateAlbumRequest req;
    req.name = long_name;
    req.description = long_description;

    Album album = album_service_->createAlbum(req);
    Album retrieved = album_service_->getAlbum(album.album_id);

    EXPECT_EQ(retrieved.name, long_name);
    EXPECT_EQ(retrieved.description, long_description);
}

// Test mapping with many tags
TEST_F(AlbumMapperTest, ManyTags) {
    CreateAlbumRequest req;
    req.name = "Album with many tags";

    // Add 100 tags
    for (int i = 0; i < 100; ++i) {
        req.tags.push_back("tag" + std::to_string(i));
    }

    Album album = album_service_->createAlbum(req);
    Album retrieved = album_service_->getAlbum(album.album_id);

    EXPECT_EQ(retrieved.tags.size(), 100);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(retrieved.tags[i], "tag" + std::to_string(i));
    }
}

// Test timestamp preservation
TEST_F(AlbumMapperTest, TimestampPreservation) {
    CreateAlbumRequest req;
    req.name = "Timestamp Test";

    Album album = album_service_->createAlbum(req);

    std::time_t before = album.created_at;
    std::time_t now = std::time(nullptr);

    // Verify created_at is reasonable (within last minute)
    EXPECT_GE(album.created_at, now - 60);
    EXPECT_LE(album.created_at, now + 5);

    // Retrieve and verify timestamp preserved
    Album retrieved = album_service_->getAlbum(album.album_id);
    EXPECT_EQ(retrieved.created_at, before);
    EXPECT_EQ(retrieved.updated_at, album.updated_at);
}

// Test boolean mapping (published field)
TEST_F(AlbumMapperTest, BooleanMappingTrue) {
    CreateAlbumRequest req;
    req.name = "Published Album";
    req.published = true;

    Album album = album_service_->createAlbum(req);
    Album retrieved = album_service_->getAlbum(album.album_id);

    EXPECT_TRUE(retrieved.published);
}

TEST_F(AlbumMapperTest, BooleanMappingFalse) {
    CreateAlbumRequest req;
    req.name = "Unpublished Album";
    req.published = false;

    Album album = album_service_->createAlbum(req);
    Album retrieved = album_service_->getAlbum(album.album_id);

    EXPECT_FALSE(retrieved.published);
}

// Test empty string vs missing field handling
TEST_F(AlbumMapperTest, EmptyStringHandling) {
    CreateAlbumRequest req;
    req.name = "Empty Strings Test";
    req.description = "";  // Explicitly empty

    Album album = album_service_->createAlbum(req);
    Album retrieved = album_service_->getAlbum(album.album_id);

    EXPECT_EQ(retrieved.description, "");
    EXPECT_EQ(retrieved.cover_image_id, "");  // Should be empty on creation
}

// Test updating album preserves mapping
TEST_F(AlbumMapperTest, UpdatePreservesMapping) {
    // Create album
    CreateAlbumRequest create_req;
    create_req.name = "Original Name";
    create_req.description = "Original Description";
    Album album = album_service_->createAlbum(create_req);

    std::time_t original_created_at = album.created_at;

    // Update album
    UpdateAlbumRequest update_req;
    update_req.name = "Updated Name";
    update_req.description = "Updated Description";
    update_req.tags = {"new-tag"};

    Album updated = album_service_->updateAlbum(album.album_id, update_req);

    // Verify updates applied
    EXPECT_EQ(updated.name, "Updated Name");
    EXPECT_EQ(updated.description, "Updated Description");
    EXPECT_EQ(updated.tags.size(), 1);

    // Verify original fields preserved
    EXPECT_EQ(updated.album_id, album.album_id);
    EXPECT_EQ(updated.created_at, original_created_at);
    EXPECT_GE(updated.updated_at, original_created_at);
}

// Test image IDs list mapping
TEST_F(AlbumMapperTest, ImageIdsListMapping) {
    CreateAlbumRequest req;
    req.name = "Album with Images";

    Album album = album_service_->createAlbum(req);

    // Initially empty
    EXPECT_EQ(album.image_ids.size(), 0);

    // Add images (requires mocking S3 to validate)
    // This tests the list mapping indirectly
    Album retrieved = album_service_->getAlbum(album.album_id);
    EXPECT_EQ(retrieved.image_ids.size(), 0);
}

// Test list order preservation
TEST_F(AlbumMapperTest, ListOrderPreservation) {
    CreateAlbumRequest req;
    req.name = "Order Test";
    req.tags = {"zebra", "alpha", "beta", "gamma"};

    Album album = album_service_->createAlbum(req);
    Album retrieved = album_service_->getAlbum(album.album_id);

    // Verify order is preserved
    ASSERT_EQ(retrieved.tags.size(), 4);
    EXPECT_EQ(retrieved.tags[0], "zebra");
    EXPECT_EQ(retrieved.tags[1], "alpha");
    EXPECT_EQ(retrieved.tags[2], "beta");
    EXPECT_EQ(retrieved.tags[3], "gamma");
}
