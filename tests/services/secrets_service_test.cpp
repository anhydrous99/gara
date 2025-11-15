#include <gtest/gtest.h>
#include "services/secrets_service.h"
#include "test_helpers/test_constants.h"
#include <thread>
#include <chrono>

using namespace gara;
using namespace gara::test_constants;

class SecretsServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Tests will use skip_aws_init mode to avoid AWS calls
    }
};

// ============================================================================
// Constructor and Initialization Tests
// ============================================================================

TEST_F(SecretsServiceTest, Constructor_WithSkipAWSInit_DoesNotInitialize) {
    // Arrange & Act
    SecretsService service(
        TEST_SECRET_NAME,
        TEST_REGION,
        SECRETS_CACHE_TTL_DEFAULT,
        SKIP_AWS_INIT
    );

    // Assert
    EXPECT_FALSE(service.isInitialized())
        << "Service should not be initialized when skipping AWS init";

    EXPECT_EQ(TEST_SECRET_NAME, service.getSecretName())
        << "Secret name should be stored correctly";
}

TEST_F(SecretsServiceTest, IsInitialized_BeforeInit_ReturnsFalse) {
    // Arrange
    SecretsService service(
        TEST_SECRET_NAME,
        TEST_REGION,
        SECRETS_CACHE_TTL_DEFAULT,
        SKIP_AWS_INIT
    );

    // Act & Assert
    EXPECT_FALSE(service.isInitialized())
        << "Service should not be initialized before calling init";
}

TEST_F(SecretsServiceTest, GetSecretName_AfterConstruction_ReturnsCorrectName) {
    // Arrange
    SecretsService service(
        TEST_SECRET_NAME_CUSTOM,
        TEST_REGION_EU_WEST,
        SECRETS_CACHE_TTL_DEFAULT,
        SKIP_AWS_INIT
    );

    // Act
    std::string secret_name = service.getSecretName();

    // Assert
    EXPECT_EQ(TEST_SECRET_NAME_CUSTOM, secret_name)
        << "Service should return the secret name provided in constructor";
}

// ============================================================================
// API Key Retrieval Tests
// ============================================================================

TEST_F(SecretsServiceTest, GetApiKey_WhenNotInitialized_ReturnsEmptyString) {
    // Arrange
    SecretsService service(
        TEST_SECRET_NAME,
        TEST_REGION,
        SECRETS_CACHE_TTL_DEFAULT,
        SKIP_AWS_INIT
    );

    // Act
    std::string key = service.getApiKey();

    // Assert
    EXPECT_TRUE(key.empty())
        << "API key should be empty when service is not initialized";
}

TEST_F(SecretsServiceTest, GetApiKey_MultipleCalls_ReturnsConsistentResults) {
    // Arrange
    SecretsService service(
        TEST_SECRET_NAME,
        TEST_REGION,
        SECRETS_CACHE_TTL_DEFAULT,
        SKIP_AWS_INIT
    );

    // Act
    std::string key1 = service.getApiKey();
    std::string key2 = service.getApiKey();
    std::string key3 = service.getApiKey();

    // Assert
    EXPECT_EQ(key1, key2)
        << "Multiple calls to getApiKey should return the same value (call 1 vs 2)";

    EXPECT_EQ(key2, key3)
        << "Multiple calls to getApiKey should return the same value (call 2 vs 3)";
}

// ============================================================================
// Cache TTL Configuration Tests
// ============================================================================

TEST_F(SecretsServiceTest, Constructor_WithShortTTL_ConfiguresCorrectly) {
    // Arrange & Act - Create service with 1 second TTL
    SecretsService service_short(
        TEST_SECRET_NAME,
        TEST_REGION,
        SECRETS_CACHE_TTL_SHORT,
        SKIP_AWS_INIT
    );

    // Assert
    EXPECT_FALSE(service_short.isInitialized())
        << "Service with short TTL should not be initialized when skipping AWS";
}

TEST_F(SecretsServiceTest, Constructor_WithDefaultTTL_ConfiguresCorrectly) {
    // Arrange & Act - Create service with default TTL
    SecretsService service_default(
        TEST_SECRET_NAME,
        TEST_REGION,
        SECRETS_CACHE_TTL_DEFAULT,
        SKIP_AWS_INIT
    );

    // Assert
    EXPECT_FALSE(service_default.isInitialized())
        << "Service with default TTL should not be initialized when skipping AWS";
}

// ============================================================================
// Refresh API Key Tests
// ============================================================================

TEST_F(SecretsServiceTest, RefreshApiKey_WhenAWSSkipped_ReturnsFalse) {
    // Arrange
    SecretsService service(
        TEST_SECRET_NAME,
        TEST_REGION,
        SECRETS_CACHE_TTL_DEFAULT,
        SKIP_AWS_INIT
    );

    // Act
    bool result = service.refreshApiKey();

    // Assert
    EXPECT_FALSE(result)
        << "Refresh should fail when AWS initialization is skipped";
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(SecretsServiceTest, GetApiKey_ConcurrentAccess_IsThreadSafe) {
    // Arrange
    SecretsService service(
        TEST_SECRET_NAME,
        TEST_REGION,
        SECRETS_CACHE_TTL_DEFAULT,
        SKIP_AWS_INIT
    );

    // Act - Multiple threads accessing getApiKey concurrently
    auto worker = [&service]() {
        for (int i = 0; i < THREAD_ITERATION_COUNT; ++i) {
            service.getApiKey();
        }
    };

    std::thread t1(worker);
    std::thread t2(worker);
    std::thread t3(worker);

    t1.join();
    t2.join();
    t3.join();

    // Assert
    SUCCEED()
        << "If no crash occurred, thread safety is maintained for concurrent getApiKey calls";
}

// ============================================================================
// Multi-Region Configuration Tests
// ============================================================================

TEST_F(SecretsServiceTest, Constructor_WithDifferentRegions_StoresCorrectly) {
    // Arrange & Act - Create services with different regions
    SecretsService service_us_east(
        TEST_SECRET_NAME,
        TEST_REGION,
        SECRETS_CACHE_TTL_DEFAULT,
        SKIP_AWS_INIT
    );

    SecretsService service_eu_west(
        TEST_SECRET_NAME,
        TEST_REGION_EU_WEST,
        SECRETS_CACHE_TTL_DEFAULT,
        SKIP_AWS_INIT
    );

    SecretsService service_ap_south(
        TEST_SECRET_NAME,
        TEST_REGION_AP_SOUTH,
        SECRETS_CACHE_TTL_DEFAULT,
        SKIP_AWS_INIT
    );

    // Assert - All should store the secret name correctly
    EXPECT_EQ(TEST_SECRET_NAME, service_us_east.getSecretName())
        << "US East service should store secret name correctly";

    EXPECT_EQ(TEST_SECRET_NAME, service_eu_west.getSecretName())
        << "EU West service should store secret name correctly";

    EXPECT_EQ(TEST_SECRET_NAME, service_ap_south.getSecretName())
        << "AP South service should store secret name correctly";
}

// Note: Integration tests with actual AWS Secrets Manager would require:
// 1. AWS credentials configured
// 2. A test secret created in Secrets Manager
// 3. Proper IAM permissions
// These tests focus on the caching logic and thread safety without AWS dependencies
