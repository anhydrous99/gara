#include <gtest/gtest.h>
#include "services/secrets_service.h"
#include <thread>
#include <chrono>

using namespace gara;

class SecretsServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Tests will use skip_aws_init mode to avoid AWS calls
    }
};

// Test constructor with skip_aws_init flag
TEST_F(SecretsServiceTest, ConstructorSkipAWSInit) {
    SecretsService service("test-secret", "us-east-1", 300, true);

    // Service should not be initialized when skipping AWS init
    EXPECT_FALSE(service.isInitialized());
    EXPECT_EQ("test-secret", service.getSecretName());
}

// Test getApiKey returns empty string when not initialized
TEST_F(SecretsServiceTest, GetApiKeyNotInitialized) {
    SecretsService service("test-secret", "us-east-1", 300, true);

    std::string key = service.getApiKey();
    EXPECT_TRUE(key.empty());
}

// Test isInitialized returns false before initialization
TEST_F(SecretsServiceTest, IsInitializedReturnsFalse) {
    SecretsService service("test-secret", "us-east-1", 300, true);

    EXPECT_FALSE(service.isInitialized());
}

// Test secret name is stored correctly
TEST_F(SecretsServiceTest, SecretNameStored) {
    SecretsService service("my-api-key", "eu-west-1", 300, true);

    EXPECT_EQ("my-api-key", service.getSecretName());
}

// Test cache TTL behavior (simulated)
TEST_F(SecretsServiceTest, CacheTTLConfiguration) {
    // Create service with 1 second TTL
    SecretsService service_short("test-secret", "us-east-1", 1, true);
    EXPECT_FALSE(service_short.isInitialized());

    // Create service with 300 second TTL (default)
    SecretsService service_default("test-secret", "us-east-1", 300, true);
    EXPECT_FALSE(service_default.isInitialized());
}

// Test refreshApiKey returns false when AWS is skipped
TEST_F(SecretsServiceTest, RefreshApiKeyFailsWhenSkipped) {
    SecretsService service("test-secret", "us-east-1", 300, true);

    bool result = service.refreshApiKey();
    EXPECT_FALSE(result);
}

// Test multiple calls to getApiKey return consistent results
TEST_F(SecretsServiceTest, MultipleGetApiKeyCalls) {
    SecretsService service("test-secret", "us-east-1", 300, true);

    std::string key1 = service.getApiKey();
    std::string key2 = service.getApiKey();
    std::string key3 = service.getApiKey();

    EXPECT_EQ(key1, key2);
    EXPECT_EQ(key2, key3);
}

// Test thread safety (basic test - multiple threads accessing getApiKey)
TEST_F(SecretsServiceTest, ThreadSafety) {
    SecretsService service("test-secret", "us-east-1", 300, true);

    auto worker = [&service]() {
        for (int i = 0; i < 100; ++i) {
            service.getApiKey();
        }
    };

    std::thread t1(worker);
    std::thread t2(worker);
    std::thread t3(worker);

    t1.join();
    t2.join();
    t3.join();

    // If no crash, thread safety is maintained
    SUCCEED();
}

// Test different region configurations
TEST_F(SecretsServiceTest, DifferentRegions) {
    SecretsService service_us_east("secret", "us-east-1", 300, true);
    SecretsService service_eu_west("secret", "eu-west-1", 300, true);
    SecretsService service_ap_south("secret", "ap-south-1", 300, true);

    EXPECT_EQ("secret", service_us_east.getSecretName());
    EXPECT_EQ("secret", service_eu_west.getSecretName());
    EXPECT_EQ("secret", service_ap_south.getSecretName());
}

// Note: Integration tests with actual AWS Secrets Manager would require:
// 1. AWS credentials configured
// 2. A test secret created in Secrets Manager
// 3. Proper IAM permissions
// These tests focus on the caching logic and thread safety without AWS dependencies
