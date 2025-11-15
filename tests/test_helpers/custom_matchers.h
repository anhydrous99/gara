#ifndef GARA_TEST_HELPERS_CUSTOM_MATCHERS_H
#define GARA_TEST_HELPERS_CUSTOM_MATCHERS_H

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <regex>
#include "test_constants.h"

namespace gara {
namespace test_matchers {

/**
 * @brief Custom matcher for SHA256 hashes
 *
 * Example:
 *   EXPECT_THAT(hash, IsValidSHA256Hash());
 */
MATCHER(IsValidSHA256Hash, "is a valid SHA256 hash") {
    if (arg.length() != test_constants::SHA256_HEX_LENGTH) {
        *result_listener << "length is " << arg.length()
                        << ", expected " << test_constants::SHA256_HEX_LENGTH;
        return false;
    }

    std::regex hex_pattern("^[0-9a-f]{64}$");
    if (!std::regex_match(arg, hex_pattern)) {
        *result_listener << "contains non-hex characters";
        return false;
    }

    return true;
}

/**
 * @brief Custom matcher for valid image formats
 */
MATCHER(IsValidImageFormat, "is a valid image format") {
    const std::vector<std::string> valid_formats = {
        test_constants::FORMAT_JPEG,
        test_constants::FORMAT_JPG,
        test_constants::FORMAT_PNG,
        test_constants::FORMAT_WEBP,
        test_constants::FORMAT_GIF,
        test_constants::FORMAT_TIFF
    };

    auto lower_arg = arg;
    std::transform(lower_arg.begin(), lower_arg.end(), lower_arg.begin(), ::tolower);

    bool found = std::find(valid_formats.begin(), valid_formats.end(), lower_arg)
                 != valid_formats.end();

    if (!found) {
        *result_listener << "'" << arg << "' is not in the list of valid formats";
    }

    return found;
}

/**
 * @brief Matcher for checking if string contains substring
 */
MATCHER_P(ContainsSubstring, substring, "contains substring") {
    if (arg.find(substring) != std::string::npos) {
        return true;
    }
    *result_listener << "'" << arg << "' does not contain '" << substring << "'";
    return false;
}

/**
 * @brief Matcher for presigned URLs
 */
MATCHER(IsValidPresignedUrl, "is a valid presigned URL") {
    // Check it's a URL
    if (arg.find("http://") != 0 && arg.find("https://") != 0) {
        *result_listener << "does not start with http:// or https://";
        return false;
    }

    // Check it has expiration parameter
    if (arg.find("expires=") == std::string::npos) {
        *result_listener << "missing 'expires=' parameter";
        return false;
    }

    return true;
}

/**
 * @brief Matcher for UUID format
 */
MATCHER(IsValidUUID, "is a valid UUID") {
    std::regex uuid_pattern(
        "^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"
    );

    if (std::regex_match(arg, uuid_pattern)) {
        return true;
    }

    *result_listener << "'" << arg << "' does not match UUID pattern";
    return false;
}

/**
 * @brief Matcher for album ID format (timestamp_UUID)
 */
MATCHER(IsValidAlbumId, "is a valid album ID") {
    std::regex album_id_pattern(
        R"(^\d+_[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$)"
    );

    if (std::regex_match(arg, album_id_pattern)) {
        return true;
    }

    *result_listener << "'" << arg << "' does not match album ID pattern (timestamp_UUID)";
    return false;
}

/**
 * @brief Matcher for checking timestamp is recent (within last N seconds)
 */
MATCHER_P(IsRecentTimestamp, seconds, "is a recent timestamp") {
    std::time_t now = std::time(nullptr);
    std::time_t diff = now - arg;

    if (diff < 0 || diff > seconds) {
        *result_listener << "timestamp " << arg << " is " << diff
                        << " seconds from now, expected within " << seconds;
        return false;
    }

    return true;
}

/**
 * @brief Matcher for non-empty string
 */
MATCHER(IsNonEmptyString, "is a non-empty string") {
    if (arg.empty()) {
        *result_listener << "string is empty";
        return false;
    }
    return true;
}

/**
 * @brief Matcher for vector size
 */
MATCHER_P(HasSize, expected_size, "has size") {
    if (arg.size() == static_cast<size_t>(expected_size)) {
        return true;
    }

    *result_listener << "size is " << arg.size()
                    << ", expected " << expected_size;
    return false;
}

} // namespace test_matchers
} // namespace gara

#endif // GARA_TEST_HELPERS_CUSTOM_MATCHERS_H
