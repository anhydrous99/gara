#include <gtest/gtest.h>
#include "middleware/auth_middleware.h"
#include "test_helpers/test_constants.h"
#include "test_helpers/custom_matchers.h"
#include <crow.h>

using namespace gara::middleware;
using namespace gara::test_constants;
using namespace gara::test_matchers;

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

// ============================================================================
// API Key Extraction Tests
// ============================================================================

TEST_F(AuthMiddlewareTest, ExtractApiKey_WithValidHeader_ReturnsKey) {
    // Arrange
    auto req = createMockRequest({{HTTP_HEADER_API_KEY, TEST_API_KEY_VALID}});

    // Act
    std::string extracted = AuthMiddleware::extractApiKey(req);

    // Assert
    EXPECT_EQ(TEST_API_KEY_VALID, extracted)
        << "Should extract API key from X-API-Key header";
}

TEST_F(AuthMiddlewareTest, ExtractApiKey_WithLowercaseHeader_ReturnsKey) {
    // Arrange
    auto req = createMockRequest({{HTTP_HEADER_API_KEY_LOWERCASE, TEST_API_KEY_LOWERCASE}});

    // Act
    std::string extracted = AuthMiddleware::extractApiKey(req);

    // Assert
    EXPECT_EQ(TEST_API_KEY_LOWERCASE, extracted)
        << "Should extract API key from lowercase header (case-insensitive)";
}

TEST_F(AuthMiddlewareTest, ExtractApiKey_WithMissingHeader_ReturnsEmpty) {
    // Arrange
    auto req = createMockRequest({});

    // Act
    std::string extracted = AuthMiddleware::extractApiKey(req);

    // Assert
    EXPECT_TRUE(extracted.empty())
        << "Should return empty string when X-API-Key header is missing";
}

TEST_F(AuthMiddlewareTest, ExtractApiKey_WithWrongHeader_ReturnsEmpty) {
    // Arrange
    auto req = createMockRequest({{HTTP_HEADER_AUTHORIZATION, TEST_BEARER_TOKEN}});

    // Act
    std::string extracted = AuthMiddleware::extractApiKey(req);

    // Assert
    EXPECT_TRUE(extracted.empty())
        << "Should return empty string when using Authorization header instead of X-API-Key";
}

// ============================================================================
// API Key Validation Tests
// ============================================================================

TEST_F(AuthMiddlewareTest, ValidateApiKey_WithCorrectKey_ReturnsTrue) {
    // Arrange
    auto req = createMockRequest({{HTTP_HEADER_API_KEY, TEST_API_KEY_CORRECT}});

    // Act
    bool result = AuthMiddleware::validateApiKey(req, TEST_API_KEY_CORRECT);

    // Assert
    EXPECT_TRUE(result)
        << "Should validate successfully when API key matches expected key";
}

TEST_F(AuthMiddlewareTest, ValidateApiKey_WithWrongKey_ReturnsFalse) {
    // Arrange
    auto req = createMockRequest({{HTTP_HEADER_API_KEY, TEST_API_KEY_WRONG}});

    // Act
    bool result = AuthMiddleware::validateApiKey(req, TEST_API_KEY_CORRECT);

    // Assert
    EXPECT_FALSE(result)
        << "Should fail validation when API key does not match expected key";
}

TEST_F(AuthMiddlewareTest, ValidateApiKey_WithMissingKey_ReturnsFalse) {
    // Arrange
    auto req = createMockRequest({});

    // Act
    bool result = AuthMiddleware::validateApiKey(req, TEST_API_KEY_CORRECT);

    // Assert
    EXPECT_FALSE(result)
        << "Should fail validation when API key header is missing";
}

TEST_F(AuthMiddlewareTest, ValidateApiKey_WithEmptyExpectedKey_ReturnsFalse) {
    // Arrange
    auto req = createMockRequest({{HTTP_HEADER_API_KEY, TEST_API_KEY_SOME}});

    // Act
    bool result = AuthMiddleware::validateApiKey(req, EMPTY_STRING);

    // Assert
    EXPECT_FALSE(result)
        << "Should fail validation when expected key is empty";
}

// ============================================================================
// Constant Time Comparison Tests
// ============================================================================

TEST_F(AuthMiddlewareTest, ConstantTimeCompare_WithEqualStrings_ReturnsTrue) {
    // Arrange & Act
    bool result = AuthMiddleware::constantTimeCompare(
        TEST_STRING_EQUAL_A,
        TEST_STRING_EQUAL_B
    );

    // Assert
    EXPECT_TRUE(result)
        << "Should return true for identical strings";
}

TEST_F(AuthMiddlewareTest, ConstantTimeCompare_WithDifferentStrings_ReturnsFalse) {
    // Arrange & Act
    bool result = AuthMiddleware::constantTimeCompare(
        TEST_STRING_123,
        TEST_STRING_456
    );

    // Assert
    EXPECT_FALSE(result)
        << "Should return false for different strings";
}

TEST_F(AuthMiddlewareTest, ConstantTimeCompare_WithDifferentLengths_ReturnsFalse) {
    // Arrange & Act
    bool result1 = AuthMiddleware::constantTimeCompare(
        TEST_STRING_SHORT,
        TEST_STRING_LONG
    );
    bool result2 = AuthMiddleware::constantTimeCompare(
        TEST_STRING_LONG,
        TEST_STRING_TINY
    );

    // Assert
    EXPECT_FALSE(result1)
        << "Should return false when first string is shorter";

    EXPECT_FALSE(result2)
        << "Should return false when second string is shorter";
}

TEST_F(AuthMiddlewareTest, ConstantTimeCompare_WithEmptyStrings_HandlesCorrectly) {
    // Arrange & Act
    bool result_both_empty = AuthMiddleware::constantTimeCompare(EMPTY_STRING, EMPTY_STRING);
    bool result_first_empty = AuthMiddleware::constantTimeCompare(TEST_STRING_TEST, EMPTY_STRING);
    bool result_second_empty = AuthMiddleware::constantTimeCompare(EMPTY_STRING, TEST_STRING_TEST);

    // Assert
    EXPECT_TRUE(result_both_empty)
        << "Should return true when both strings are empty";

    EXPECT_FALSE(result_first_empty)
        << "Should return false when first string is not empty but second is";

    EXPECT_FALSE(result_second_empty)
        << "Should return false when second string is not empty but first is";
}

TEST_F(AuthMiddlewareTest, ConstantTimeCompare_IsCaseSensitive_ReturnsFalse) {
    // Arrange & Act
    bool result = AuthMiddleware::constantTimeCompare(
        TEST_STRING_MIXED_CASE,
        TEST_STRING_LOWER_CASE
    );

    // Assert
    EXPECT_FALSE(result)
        << "Comparison should be case-sensitive (TestKey != testkey)";
}

TEST_F(AuthMiddlewareTest, ConstantTimeCompare_WithSpecialChars_ComparesCorrectly) {
    // Arrange
    std::string key1 = TEST_API_KEY_SPECIAL_CHARS;
    std::string key2 = TEST_API_KEY_SPECIAL_CHARS;
    std::string key3 = TEST_API_KEY_SPECIAL_CHARS_DIFFERENT;

    // Act & Assert
    EXPECT_TRUE(AuthMiddleware::constantTimeCompare(key1, key2))
        << "Should return true for identical strings with special characters";

    EXPECT_FALSE(AuthMiddleware::constantTimeCompare(key1, key3))
        << "Should return false when special character strings differ";
}

// ============================================================================
// Unauthorized Response Tests
// ============================================================================

TEST_F(AuthMiddlewareTest, UnauthorizedResponse_WithCustomMessage_ReturnsCorrectFormat) {
    // Arrange & Act
    crow::response resp = AuthMiddleware::unauthorizedResponse(ERROR_MESSAGE_TEST);

    // Assert
    EXPECT_EQ(HTTP_STATUS_UNAUTHORIZED, resp.code)
        << "Response should have 401 Unauthorized status code";

    EXPECT_EQ(HTTP_CONTENT_TYPE_JSON, resp.get_header_value(HTTP_HEADER_CONTENT_TYPE))
        << "Response should have JSON content type";

    // Verify JSON structure
    std::string body = resp.body;
    EXPECT_THAT(body, ContainsSubstring(JSON_KEY_ERROR))
        << "Response body should contain 'error' field";

    EXPECT_THAT(body, ContainsSubstring(ERROR_UNAUTHORIZED_TEXT))
        << "Response body should contain 'Unauthorized' error type";

    EXPECT_THAT(body, ContainsSubstring(JSON_KEY_MESSAGE))
        << "Response body should contain 'message' field";

    EXPECT_THAT(body, ContainsSubstring(ERROR_MESSAGE_TEST))
        << "Response body should contain custom error message";
}

TEST_F(AuthMiddlewareTest, UnauthorizedResponse_WithMissingKeyMessage_IncludesMessage) {
    // Arrange & Act
    crow::response resp = AuthMiddleware::unauthorizedResponse(ERROR_MESSAGE_MISSING_KEY);

    // Assert
    EXPECT_EQ(HTTP_STATUS_UNAUTHORIZED, resp.code)
        << "Response should have 401 status for missing key";

    EXPECT_THAT(resp.body, ContainsSubstring(ERROR_MESSAGE_MISSING_KEY))
        << "Response should include missing key error message";
}

TEST_F(AuthMiddlewareTest, UnauthorizedResponse_WithInvalidKeyMessage_IncludesMessage) {
    // Arrange & Act
    crow::response resp = AuthMiddleware::unauthorizedResponse(ERROR_MESSAGE_INVALID_KEY);

    // Assert
    EXPECT_EQ(HTTP_STATUS_UNAUTHORIZED, resp.code)
        << "Response should have 401 status for invalid key";

    EXPECT_THAT(resp.body, ContainsSubstring(ERROR_MESSAGE_INVALID_KEY))
        << "Response should include invalid key error message";
}

// ============================================================================
// Full Authentication Flow Tests
// ============================================================================

TEST_F(AuthMiddlewareTest, FullAuthFlow_WithValidKey_AuthenticatesSuccessfully) {
    // Arrange
    std::string expected_key = TEST_API_KEY_SECRET;
    auto req = createMockRequest({{HTTP_HEADER_API_KEY, expected_key}});

    // Act
    bool authenticated = AuthMiddleware::validateApiKey(req, expected_key);

    // Assert
    EXPECT_TRUE(authenticated)
        << "Full authentication flow should succeed with correct API key";
}

TEST_F(AuthMiddlewareTest, FullAuthFlow_WithInvalidKey_FailsAndReturns401) {
    // Arrange
    std::string expected_key = TEST_API_KEY_SECRET;
    auto req = createMockRequest({{HTTP_HEADER_API_KEY, TEST_API_KEY_WRONG}});

    // Act
    bool authenticated = AuthMiddleware::validateApiKey(req, expected_key);

    // Assert
    EXPECT_FALSE(authenticated)
        << "Authentication should fail with wrong API key";

    // Verify error response format
    if (!authenticated) {
        crow::response error_resp = AuthMiddleware::unauthorizedResponse(ERROR_MESSAGE_INVALID_KEY);
        EXPECT_EQ(HTTP_STATUS_UNAUTHORIZED, error_resp.code)
            << "Failed authentication should return 401 status";
    }
}

// ============================================================================
// Timing Attack Resistance Tests
// ============================================================================

TEST_F(AuthMiddlewareTest, ConstantTimeCompare_ResistsTimingAttacks_ReturnsCorrectly) {
    // Arrange - Strings designed to test timing attack resistance
    std::string key_all_a = TEST_TIMING_KEY_ALL_A;
    std::string key_all_z = TEST_TIMING_KEY_ALL_Z;
    std::string key_last_diff = TEST_TIMING_KEY_LAST_DIFF;  // Only last char different

    // Act & Assert
    // All should return false, and ideally take similar time
    // (We can't easily test timing here, but we test correctness)
    EXPECT_FALSE(AuthMiddleware::constantTimeCompare(key_all_a, key_all_z))
        << "Should correctly compare strings with all different characters";

    EXPECT_FALSE(AuthMiddleware::constantTimeCompare(key_all_a, key_last_diff))
        << "Should correctly compare strings with only last character different";
}
