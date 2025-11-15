#include <gtest/gtest.h>
#include "utils/id_generator.h"
#include <set>
#include <regex>
#include <thread>
#include <vector>
#include <chrono>

using namespace gara::utils;

class IdGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test fixtures if needed
    }
};

// Test basic ID generation
TEST_F(IdGeneratorTest, GenerateAlbumIdReturnsNonEmpty) {
    std::string id = IdGenerator::generateAlbumId();
    EXPECT_FALSE(id.empty());
}

// Test ID format: {timestamp}_{uuid}
TEST_F(IdGeneratorTest, GenerateAlbumIdHasCorrectFormat) {
    std::string id = IdGenerator::generateAlbumId();

    // Pattern: {timestamp}_{8hex}-{4hex}-{4hex}-{4hex}-{12hex}
    std::regex pattern(R"(^\d+_[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$)");
    EXPECT_TRUE(std::regex_match(id, pattern))
        << "ID does not match expected format: " << id;
}

// Test timestamp prefix is extractable
TEST_F(IdGeneratorTest, GenerateAlbumIdContainsTimestamp) {
    std::string id = IdGenerator::generateAlbumId();

    // Extract timestamp part (before underscore)
    size_t underscore_pos = id.find('_');
    ASSERT_NE(std::string::npos, underscore_pos);

    std::string timestamp_str = id.substr(0, underscore_pos);

    // Verify it's a valid number
    EXPECT_NO_THROW({
        std::stoull(timestamp_str);
    });

    // Verify timestamp is reasonable (within last 10 years and not in future)
    unsigned long long timestamp = std::stoull(timestamp_str);
    unsigned long long now = static_cast<unsigned long long>(std::time(nullptr));
    unsigned long long ten_years_ago = now - (10 * 365 * 24 * 60 * 60);

    EXPECT_GE(timestamp, ten_years_ago);
    EXPECT_LE(timestamp, now + 60); // Allow 60 seconds tolerance for clock skew
}

// Test UUID version 4 format (4xxx in third group)
TEST_F(IdGeneratorTest, GenerateAlbumIdHasUUIDv4Format) {
    std::string id = IdGenerator::generateAlbumId();

    // Extract UUID part (after underscore)
    size_t underscore_pos = id.find('_');
    ASSERT_NE(std::string::npos, underscore_pos);

    std::string uuid = id.substr(underscore_pos + 1);

    // Check version field (third group should start with 4)
    size_t third_dash = uuid.find('-', uuid.find('-') + 1);
    ASSERT_NE(std::string::npos, third_dash);

    char version_char = uuid[third_dash + 1];
    EXPECT_EQ('4', version_char) << "UUID version should be 4";
}

// Test UUID variant field (8xxx, 9xxx, axxx, or bxxx in fourth group)
TEST_F(IdGeneratorTest, GenerateAlbumIdHasCorrectUUIDVariant) {
    std::string id = IdGenerator::generateAlbumId();

    size_t underscore_pos = id.find('_');
    std::string uuid = id.substr(underscore_pos + 1);

    // Find fourth group (after third dash)
    size_t third_dash = uuid.find('-', uuid.find('-') + 1);
    size_t fourth_dash = uuid.find('-', third_dash + 1);
    ASSERT_NE(std::string::npos, fourth_dash);

    char variant_char = uuid[fourth_dash + 1];
    // Variant bits should be 10xx in binary, which means 8, 9, a, or b in hex
    EXPECT_TRUE(variant_char == '8' || variant_char == '9' ||
                variant_char == 'a' || variant_char == 'b')
        << "UUID variant should be 8, 9, a, or b, got: " << variant_char;
}

// Test uniqueness - generate many IDs and check for duplicates
TEST_F(IdGeneratorTest, GenerateAlbumIdProducesUniqueIds) {
    std::set<std::string> ids;
    const int num_ids = 10000;

    for (int i = 0; i < num_ids; ++i) {
        std::string id = IdGenerator::generateAlbumId();
        ids.insert(id);
    }

    // All IDs should be unique
    EXPECT_EQ(num_ids, ids.size())
        << "Generated " << num_ids << " IDs but only " << ids.size() << " were unique";
}

// Test chronological ordering
TEST_F(IdGeneratorTest, GenerateAlbumIdChronologicalOrdering) {
    std::string id1 = IdGenerator::generateAlbumId();

    // Sleep to ensure different timestamp
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    std::string id2 = IdGenerator::generateAlbumId();

    // Lexicographic comparison should work due to timestamp prefix
    EXPECT_LT(id1, id2)
        << "Newer ID should sort after older ID. ID1: " << id1 << ", ID2: " << id2;
}

// Test thread safety - concurrent ID generation
TEST_F(IdGeneratorTest, GenerateAlbumIdThreadSafety) {
    const int num_threads = 10;
    const int ids_per_thread = 1000;
    std::vector<std::thread> threads;
    std::vector<std::set<std::string>> thread_ids(num_threads);

    // Launch multiple threads generating IDs
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < ids_per_thread; ++i) {
                std::string id = IdGenerator::generateAlbumId();
                thread_ids[t].insert(id);
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Collect all IDs into one set to check global uniqueness
    std::set<std::string> all_ids;
    for (const auto& thread_set : thread_ids) {
        EXPECT_EQ(ids_per_thread, thread_set.size())
            << "Each thread should generate unique IDs";
        all_ids.insert(thread_set.begin(), thread_set.end());
    }

    // All IDs across all threads should be unique
    EXPECT_EQ(num_threads * ids_per_thread, all_ids.size())
        << "All IDs across threads should be unique";
}

// Test multiple rapid generations
TEST_F(IdGeneratorTest, GenerateAlbumIdRapidGeneration) {
    std::vector<std::string> ids;

    // Generate 100 IDs as fast as possible
    for (int i = 0; i < 100; ++i) {
        ids.push_back(IdGenerator::generateAlbumId());
    }

    // All should be valid and unique
    std::set<std::string> unique_ids(ids.begin(), ids.end());
    EXPECT_EQ(100, unique_ids.size())
        << "Rapid generation should still produce unique IDs";

    // All should have valid format
    std::regex pattern(R"(^\d+_[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$)");
    for (const auto& id : ids) {
        EXPECT_TRUE(std::regex_match(id, pattern))
            << "Rapidly generated ID has invalid format: " << id;
    }
}

// Test that IDs generated at same second are still unique
TEST_F(IdGeneratorTest, GenerateAlbumIdUniquenessWithinSameSecond) {
    std::set<std::string> ids;

    // Generate multiple IDs that will likely have same timestamp
    for (int i = 0; i < 1000; ++i) {
        ids.insert(IdGenerator::generateAlbumId());
    }

    EXPECT_EQ(1000, ids.size())
        << "IDs generated within same second should still be unique";
}

// Test ID length is reasonable
TEST_F(IdGeneratorTest, GenerateAlbumIdReasonableLength) {
    std::string id = IdGenerator::generateAlbumId();

    // Timestamp (10 digits) + underscore + UUID (36 chars) = ~47 chars
    // Allow some variance for future timestamps
    EXPECT_GE(id.length(), 40);
    EXPECT_LE(id.length(), 60);
}
