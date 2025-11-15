#ifndef GARA_TEST_HELPERS_TEST_CONSTANTS_H
#define GARA_TEST_HELPERS_TEST_CONSTANTS_H

#include <string>

namespace gara {
namespace test_constants {

// Hash and crypto constants
constexpr int SHA256_HEX_LENGTH = 64;
constexpr int SHA256_BINARY_LENGTH = 32;

// Time constants
constexpr int ONE_HOUR_SECONDS = 3600;
constexpr int ONE_DAY_SECONDS = 86400;
constexpr int FIVE_MINUTES_SECONDS = 300;

// Image dimension constants
constexpr int SMALL_IMAGE_SIZE = 10;
constexpr int MEDIUM_IMAGE_SIZE = 100;
constexpr int LARGE_IMAGE_SIZE = 1000;

// Standard test dimensions
constexpr int STANDARD_WIDTH_800 = 800;
constexpr int STANDARD_HEIGHT_600 = 600;
constexpr int STANDARD_WIDTH_1024 = 1024;
constexpr int STANDARD_HEIGHT_768 = 768;

// Test content
const std::string TEST_CONTENT = "Hello, Gara!";
constexpr size_t TEST_CONTENT_SIZE = 12;  // Length of "Hello, Gara!"

// Image formats
const std::string FORMAT_JPEG = "jpeg";
const std::string FORMAT_JPG = "jpg";
const std::string FORMAT_PNG = "png";
const std::string FORMAT_WEBP = "webp";
const std::string FORMAT_GIF = "gif";
const std::string FORMAT_TIFF = "tiff";

// MIME types
const std::string MIME_JPEG = "image/jpeg";
const std::string MIME_PNG = "image/png";
const std::string MIME_WEBP = "image/webp";
const std::string MIME_GIF = "image/gif";
const std::string MIME_DEFAULT = "application/octet-stream";

// Test identifiers
const std::string TEST_IMAGE_ID = "test_image_id";
const std::string TEST_ALBUM_ID = "test_album_id";
const std::string TEST_BUCKET_NAME = "test-bucket";
const std::string TEST_REGION = "us-east-1";
const std::string TEST_TABLE_NAME = "test-table";

// AWS S3 key patterns
const std::string S3_RAW_PREFIX = "raw/";
const std::string S3_TRANSFORMED_PREFIX = "transformed/";

// Test data sizes
constexpr size_t SMALL_DATA_SIZE = 256;
constexpr size_t MEDIUM_DATA_SIZE = 1024;
constexpr size_t LARGE_DATA_SIZE = 1024 * 1024;  // 1MB

// Concurrency test constants
constexpr int CONCURRENT_THREADS_SMALL = 10;
constexpr int CONCURRENT_THREADS_LARGE = 100;
constexpr int UNIQUENESS_TEST_COUNT = 10000;

// Error messages (for testing error handling)
const std::string ERROR_NOT_FOUND = "not found";
const std::string ERROR_INVALID_INPUT = "Invalid input";
const std::string ERROR_UNAUTHORIZED = "Unauthorized";

} // namespace test_constants
} // namespace gara

#endif // GARA_TEST_HELPERS_TEST_CONSTANTS_H
