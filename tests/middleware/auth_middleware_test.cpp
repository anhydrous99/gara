#include <gtest/gtest.h>
#include "middleware/auth_middleware.h"
#include <crow.h>

using namespace gara::middleware;

class AuthMiddlewareTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test fixtures if needed
    }

    // Helper to create a mock request with headers
    crow::request createMockRequest(const std::map<std::string, std::string>& headers) {
        crow::request req;
        for (const auto& [key, value] : headers) {
            req.add_header(key, value);
        }
        return req;
    }
};

// Test extractApiKey with valid X-API-Key header
TEST_F(AuthMiddlewareTest, ExtractApiKeyValid) {
    auto req = createMockRequest({{"X-API-Key", "test-api-key-123"}});

    std::string extracted = AuthMiddleware::extractApiKey(req);
    EXPECT_EQ("test-api-key-123", extracted);
}

// Test extractApiKey with lowercase header
TEST_F(AuthMiddlewareTest, ExtractApiKeyLowercase) {
    auto req = createMockRequest({{"x-api-key", "test-key-lowercase"}});

    std::string extracted = AuthMiddleware::extractApiKey(req);
    EXPECT_EQ("test-key-lowercase", extracted);
}

// Test extractApiKey with missing header
TEST_F(AuthMiddlewareTest, ExtractApiKeyMissing) {
    auto req = createMockRequest({});

    std::string extracted = AuthMiddleware::extractApiKey(req);
    EXPECT_TRUE(extracted.empty());
}

// Test extractApiKey with different header
TEST_F(AuthMiddlewareTest, ExtractApiKeyWrongHeader) {
    auto req = createMockRequest({{"Authorization", "Bearer token123"}});

    std::string extracted = AuthMiddleware::extractApiKey(req);
    EXPECT_TRUE(extracted.empty());
}

// Test validateApiKey with valid key
TEST_F(AuthMiddlewareTest, ValidateApiKeyValid) {
    auto req = createMockRequest({{"X-API-Key", "correct-key"}});

    bool result = AuthMiddleware::validateApiKey(req, "correct-key");
    EXPECT_TRUE(result);
}

// Test validateApiKey with invalid key
TEST_F(AuthMiddlewareTest, ValidateApiKeyInvalid) {
    auto req = createMockRequest({{"X-API-Key", "wrong-key"}});

    bool result = AuthMiddleware::validateApiKey(req, "correct-key");
    EXPECT_FALSE(result);
}

// Test validateApiKey with missing key
TEST_F(AuthMiddlewareTest, ValidateApiKeyMissing) {
    auto req = createMockRequest({});

    bool result = AuthMiddleware::validateApiKey(req, "correct-key");
    EXPECT_FALSE(result);
}

// Test validateApiKey with empty expected key
TEST_F(AuthMiddlewareTest, ValidateApiKeyEmptyExpected) {
    auto req = createMockRequest({{"X-API-Key", "some-key"}});

    bool result = AuthMiddleware::validateApiKey(req, "");
    EXPECT_FALSE(result);
}

// Test constantTimeCompare with equal strings
TEST_F(AuthMiddlewareTest, ConstantTimeCompareEqual) {
    bool result = AuthMiddleware::constantTimeCompare("test123", "test123");
    EXPECT_TRUE(result);
}

// Test constantTimeCompare with different strings
TEST_F(AuthMiddlewareTest, ConstantTimeCompareDifferent) {
    bool result = AuthMiddleware::constantTimeCompare("test123", "test456");
    EXPECT_FALSE(result);
}

// Test constantTimeCompare with different lengths
TEST_F(AuthMiddlewareTest, ConstantTimeCompareDifferentLengths) {
    bool result1 = AuthMiddleware::constantTimeCompare("short", "much-longer-string");
    bool result2 = AuthMiddleware::constantTimeCompare("long-string", "tiny");

    EXPECT_FALSE(result1);
    EXPECT_FALSE(result2);
}

// Test constantTimeCompare with empty strings
TEST_F(AuthMiddlewareTest, ConstantTimeCompareEmpty) {
    bool result1 = AuthMiddleware::constantTimeCompare("", "");
    bool result2 = AuthMiddleware::constantTimeCompare("test", "");
    bool result3 = AuthMiddleware::constantTimeCompare("", "test");

    EXPECT_TRUE(result1);   // Both empty
    EXPECT_FALSE(result2);  // One empty
    EXPECT_FALSE(result3);  // One empty
}

// Test constantTimeCompare case sensitivity
TEST_F(AuthMiddlewareTest, ConstantTimeCompareCaseSensitive) {
    bool result = AuthMiddleware::constantTimeCompare("TestKey", "testkey");
    EXPECT_FALSE(result);  // Should be case-sensitive
}

// Test constantTimeCompare with special characters
TEST_F(AuthMiddlewareTest, ConstantTimeCompareSpecialChars) {
    std::string key1 = "api-key!@#$%^&*()";
    std::string key2 = "api-key!@#$%^&*()";
    std::string key3 = "api-key!@#$%^&*(?";

    EXPECT_TRUE(AuthMiddleware::constantTimeCompare(key1, key2));
    EXPECT_FALSE(AuthMiddleware::constantTimeCompare(key1, key3));
}

// Test unauthorizedResponse format
TEST_F(AuthMiddlewareTest, UnauthorizedResponseFormat) {
    crow::response resp = AuthMiddleware::unauthorizedResponse("Test error message");

    EXPECT_EQ(401, resp.code);
    EXPECT_EQ("application/json", resp.get_header_value("Content-Type"));

    // Parse JSON response
    std::string body = resp.body;
    EXPECT_NE(std::string::npos, body.find("\"error\""));
    EXPECT_NE(std::string::npos, body.find("\"Unauthorized\""));
    EXPECT_NE(std::string::npos, body.find("\"message\""));
    EXPECT_NE(std::string::npos, body.find("Test error message"));
}

// Test unauthorizedResponse with missing key message
TEST_F(AuthMiddlewareTest, UnauthorizedResponseMissingKey) {
    crow::response resp = AuthMiddleware::unauthorizedResponse("Missing X-API-Key header");

    EXPECT_EQ(401, resp.code);
    EXPECT_NE(std::string::npos, resp.body.find("Missing X-API-Key header"));
}

// Test unauthorizedResponse with invalid key message
TEST_F(AuthMiddlewareTest, UnauthorizedResponseInvalidKey) {
    crow::response resp = AuthMiddleware::unauthorizedResponse("Invalid API key");

    EXPECT_EQ(401, resp.code);
    EXPECT_NE(std::string::npos, resp.body.find("Invalid API key"));
}

// Test full authentication flow - valid
TEST_F(AuthMiddlewareTest, FullAuthenticationFlowValid) {
    std::string expected_key = "my-secret-api-key-12345";
    auto req = createMockRequest({{"X-API-Key", expected_key}});

    bool authenticated = AuthMiddleware::validateApiKey(req, expected_key);
    EXPECT_TRUE(authenticated);
}

// Test full authentication flow - invalid
TEST_F(AuthMiddlewareTest, FullAuthenticationFlowInvalid) {
    std::string expected_key = "my-secret-api-key-12345";
    auto req = createMockRequest({{"X-API-Key", "wrong-key"}});

    bool authenticated = AuthMiddleware::validateApiKey(req, expected_key);
    EXPECT_FALSE(authenticated);

    if (!authenticated) {
        crow::response error_resp = AuthMiddleware::unauthorizedResponse("Invalid API key");
        EXPECT_EQ(401, error_resp.code);
    }
}

// Test timing attack resistance (basic test)
TEST_F(AuthMiddlewareTest, TimingAttackResistance) {
    std::string key1 = "aaaaaaaaaaaaaaaaaaaa";
    std::string key2 = "zzzzzzzzzzzzzzzzzzzz";
    std::string key3 = "aaaaaaaaaaaaaaaaaaab";  // Only last char different

    // All should return false, and ideally take similar time
    // (We can't easily test timing here, but we test correctness)
    EXPECT_FALSE(AuthMiddleware::constantTimeCompare(key1, key2));
    EXPECT_FALSE(AuthMiddleware::constantTimeCompare(key1, key3));
}
