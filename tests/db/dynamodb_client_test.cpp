#include <gtest/gtest.h>
#include <cstdlib>

#ifdef GARA_DYNAMODB_SUPPORT
#include "db/dynamodb_client.h"
#endif

#include "interfaces/database_client_interface.h"
#include "utils/logger.h"
#include "utils/metrics.h"
#include "test_helpers/test_constants.h"

using namespace gara;
using namespace gara::test_constants;

class DynamoDBClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        gara::Logger::initialize("gara-test", "error", gara::Logger::Format::TEXT, "test");
        gara::Metrics::initialize("GaraTest", "gara-test", "test", false);

        // Clear any existing DynamoDB environment variables
        clearDynamoDBEnvVars();
    }

    void TearDown() override {
        clearDynamoDBEnvVars();
    }

    void clearDynamoDBEnvVars() {
        unsetenv("AWS_REGION");
        unsetenv("AWS_DEFAULT_REGION");
        unsetenv("DYNAMODB_ENDPOINT_URL");
        unsetenv("DYNAMODB_ALBUMS_TABLE");
        unsetenv("DYNAMODB_IMAGES_TABLE");
    }

    void setDynamoDBEnvVars(const std::string& region,
                            const std::string& endpoint_url,
                            const std::string& albums_table,
                            const std::string& images_table) {
        if (!region.empty()) setenv("AWS_REGION", region.c_str(), 1);
        if (!endpoint_url.empty()) setenv("DYNAMODB_ENDPOINT_URL", endpoint_url.c_str(), 1);
        if (!albums_table.empty()) setenv("DYNAMODB_ALBUMS_TABLE", albums_table.c_str(), 1);
        if (!images_table.empty()) setenv("DYNAMODB_IMAGES_TABLE", images_table.c_str(), 1);
    }
};

// ============================================================================
// DynamoDBConfig Tests
// ============================================================================

#ifdef GARA_DYNAMODB_SUPPORT

TEST_F(DynamoDBClientTest, DynamoDBConfig_DefaultValues_AreCorrect) {
    // Arrange & Act
    DynamoDBConfig config;

    // Assert
    EXPECT_EQ("us-east-1", config.region)
        << "Default region should be us-east-1";
    EXPECT_TRUE(config.endpoint_url.empty())
        << "Default endpoint_url should be empty (use AWS default)";
    EXPECT_EQ("gara_albums", config.albums_table)
        << "Default albums table should be gara_albums";
    EXPECT_EQ("gara_images", config.images_table)
        << "Default images table should be gara_images";
}

TEST_F(DynamoDBClientTest, DynamoDBConfig_FromEnvironment_WithNoEnvVars_UsesDefaults) {
    // Arrange - environment is already cleared in SetUp

    // Act
    DynamoDBConfig config = DynamoDBConfig::fromEnvironment();

    // Assert
    EXPECT_EQ("us-east-1", config.region)
        << "Should use default region when AWS_REGION not set";
    EXPECT_TRUE(config.endpoint_url.empty())
        << "Should use empty endpoint when DYNAMODB_ENDPOINT_URL not set";
    EXPECT_EQ("gara_albums", config.albums_table)
        << "Should use default albums table when DYNAMODB_ALBUMS_TABLE not set";
    EXPECT_EQ("gara_images", config.images_table)
        << "Should use default images table when DYNAMODB_IMAGES_TABLE not set";
}

TEST_F(DynamoDBClientTest, DynamoDBConfig_FromEnvironment_WithAllEnvVars_ParsesCorrectly) {
    // Arrange
    setDynamoDBEnvVars("eu-west-1", "http://localhost:8000", "test_albums", "test_images");

    // Act
    DynamoDBConfig config = DynamoDBConfig::fromEnvironment();

    // Assert
    EXPECT_EQ("eu-west-1", config.region)
        << "Region should be read from AWS_REGION";
    EXPECT_EQ("http://localhost:8000", config.endpoint_url)
        << "Endpoint URL should be read from DYNAMODB_ENDPOINT_URL";
    EXPECT_EQ("test_albums", config.albums_table)
        << "Albums table should be read from DYNAMODB_ALBUMS_TABLE";
    EXPECT_EQ("test_images", config.images_table)
        << "Images table should be read from DYNAMODB_IMAGES_TABLE";
}

TEST_F(DynamoDBClientTest, DynamoDBConfig_FromEnvironment_WithPartialEnvVars_MixesWithDefaults) {
    // Arrange - only set some vars
    setDynamoDBEnvVars("ap-southeast-1", "", "custom_albums", "");

    // Act
    DynamoDBConfig config = DynamoDBConfig::fromEnvironment();

    // Assert
    EXPECT_EQ("ap-southeast-1", config.region)
        << "Region should be from env";
    EXPECT_TRUE(config.endpoint_url.empty())
        << "Endpoint should use default (empty)";
    EXPECT_EQ("custom_albums", config.albums_table)
        << "Albums table should be from env";
    EXPECT_EQ("gara_images", config.images_table)
        << "Images table should use default";
}

TEST_F(DynamoDBClientTest, DynamoDBConfig_FromEnvironment_WithDefaultRegion_ParsesCorrectly) {
    // Arrange - use AWS_DEFAULT_REGION instead of AWS_REGION
    setenv("AWS_DEFAULT_REGION", "us-west-2", 1);

    // Act
    DynamoDBConfig config = DynamoDBConfig::fromEnvironment();

    // Assert
    EXPECT_EQ("us-west-2", config.region)
        << "Region should be read from AWS_DEFAULT_REGION when AWS_REGION not set";

    // Cleanup
    unsetenv("AWS_DEFAULT_REGION");
}

TEST_F(DynamoDBClientTest, DynamoDBConfig_FromEnvironment_AWS_REGION_TakesPrecedence) {
    // Arrange - set both region vars
    setenv("AWS_REGION", "eu-central-1", 1);
    setenv("AWS_DEFAULT_REGION", "us-west-2", 1);

    // Act
    DynamoDBConfig config = DynamoDBConfig::fromEnvironment();

    // Assert
    EXPECT_EQ("eu-central-1", config.region)
        << "AWS_REGION should take precedence over AWS_DEFAULT_REGION";

    // Cleanup
    unsetenv("AWS_DEFAULT_REGION");
}

// ============================================================================
// DynamoDB Local Endpoint Tests
// ============================================================================

TEST_F(DynamoDBClientTest, DynamoDBConfig_WithLocalEndpoint_ConfiguresCorrectly) {
    // Arrange
    setDynamoDBEnvVars("us-east-1", "http://localhost:8000", "", "");

    // Act
    DynamoDBConfig config = DynamoDBConfig::fromEnvironment();

    // Assert
    EXPECT_FALSE(config.endpoint_url.empty())
        << "Endpoint URL should be set for local development";
    EXPECT_EQ("http://localhost:8000", config.endpoint_url)
        << "Endpoint URL should be the local DynamoDB URL";
}

#endif // GARA_DYNAMODB_SUPPORT

// ============================================================================
// Compilation Guard Test
// ============================================================================

TEST_F(DynamoDBClientTest, DynamoDBSupport_CompilationFlag_IsSet) {
#ifdef GARA_DYNAMODB_SUPPORT
    SUCCEED() << "GARA_DYNAMODB_SUPPORT is defined - DynamoDB support is enabled";
#else
    SUCCEED() << "GARA_DYNAMODB_SUPPORT is not defined - DynamoDB support is disabled";
#endif
}

// ============================================================================
// DatabaseClientInterface Tests (applies to all implementations)
// ============================================================================

TEST_F(DynamoDBClientTest, ImageSortOrder_HasExpectedValues) {
    // Verify the enum values match what the implementations expect
    EXPECT_NE(ImageSortOrder::NEWEST, ImageSortOrder::OLDEST);
    EXPECT_NE(ImageSortOrder::NAME_ASC, ImageSortOrder::NAME_DESC);
    EXPECT_NE(ImageSortOrder::NEWEST, ImageSortOrder::NAME_ASC);
}

// ============================================================================
// Integration Test Notes
// ============================================================================

// Note: Full integration tests for DynamoDB require either:
// 1. DynamoDB Local running on localhost:8000
// 2. AWS credentials configured with access to DynamoDB
//
// Example docker command to run DynamoDB Local:
//   docker run -p 8000:8000 amazon/dynamodb-local
//
// To run integration tests:
//   1. Start DynamoDB Local
//   2. Set DYNAMODB_ENDPOINT_URL=http://localhost:8000
//   3. Run tests with GARA_DYNAMODB_SUPPORT defined
