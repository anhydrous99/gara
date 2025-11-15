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
const std::string TEST_INVALID_IMAGE_CONTENT = "not an image";

// Image formats
const std::string FORMAT_JPEG = "jpeg";
const std::string FORMAT_JPG = "jpg";
const std::string FORMAT_PNG = "png";
const std::string FORMAT_WEBP = "webp";
const std::string FORMAT_GIF = "gif";
const std::string FORMAT_TIFF = "tiff";
const std::string FORMAT_PPM = "ppm";

// MIME types
const std::string MIME_JPEG = "image/jpeg";
const std::string MIME_PNG = "image/png";
const std::string MIME_WEBP = "image/webp";
const std::string MIME_GIF = "image/gif";
const std::string MIME_DEFAULT = "application/octet-stream";

// Test identifiers
const std::string TEST_IMAGE_ID = "test_image_id";
const std::string TEST_IMAGE_ID_1 = "img1";
const std::string TEST_IMAGE_ID_2 = "img2";
const std::string TEST_IMAGE_ID_3 = "img3";
const std::string TEST_ALBUM_ID = "test_album_id";
const std::string TEST_ALBUM_ID_123 = "123";
const std::string TEST_BUCKET_NAME = "test-bucket";
const std::string TEST_REGION = "us-east-1";
const std::string TEST_REGION_EU_WEST = "eu-west-1";
const std::string TEST_REGION_AP_SOUTH = "ap-south-1";
const std::string TEST_TABLE_NAME = "test-table";
const std::string TEST_ALBUMS_TABLE_NAME = "test-albums-table";
const std::string TEST_SECRET_NAME = "test-secret";
const std::string TEST_SECRET_NAME_CUSTOM = "my-api-key";
const std::string TEST_COVER_IMAGE_ID = "cover123";
const std::string IMAGE_ID_NONEXISTENT = "nonexistent_image";
const std::string ALBUM_ID_NONEXISTENT = "nonexistent";

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

// Image processing constants
constexpr int RGB_BANDS = 3;
constexpr double RED_CHANNEL_MAX = 255.0;
constexpr double GREEN_CHANNEL_MIN = 0.0;
constexpr double BLUE_CHANNEL_MIN = 0.0;

// Test image dimensions (specific sizes for testing)
constexpr int TEST_SMALL_IMAGE_SIZE = 10;
constexpr int TEST_MEDIUM_IMAGE_WIDTH = 100;
constexpr int TEST_MEDIUM_IMAGE_HEIGHT = 50;

// Resize test constants
constexpr int RESIZE_MAINTAIN_ASPECT = 0;  // Use 0 to maintain aspect ratio
constexpr int RESIZE_TARGET_WIDTH_20 = 20;
constexpr int RESIZE_TARGET_HEIGHT_20 = 20;
constexpr int RESIZE_TARGET_WIDTH_50 = 50;
constexpr int RESIZE_TARGET_HEIGHT_25 = 25;

// Secrets service constants
constexpr int SECRETS_CACHE_TTL_SHORT = 1;       // 1 second for testing
constexpr int SECRETS_CACHE_TTL_DEFAULT = 300;   // 5 minutes (default)
constexpr bool SKIP_AWS_INIT = true;             // Skip AWS initialization in tests

// Thread testing constants
constexpr int THREAD_ITERATION_COUNT = 100;  // Number of iterations per thread

// Album constants
const std::string ALBUM_NAME_TEST = "Test Album";
const std::string ALBUM_NAME_UPDATED = "Updated Album";
const std::string ALBUM_NAME_DUPLICATE = "Duplicate Album";
const std::string ALBUM_DESCRIPTION_TEST = "A test album";
const std::string ALBUM_DESCRIPTION_UPDATED = "Updated description";
const std::string ALBUM_TAG_1 = "tag1";
const std::string ALBUM_TAG_2 = "tag2";
const std::string ALBUM_TAG_NEW = "new_tag";

// Album array and collection constants
constexpr size_t ALBUM_TAGS_COUNT_ONE = 1;
constexpr size_t ALBUM_TAGS_COUNT_TWO = 2;
constexpr size_t ALBUM_IMAGES_COUNT_TWO = 2;
constexpr size_t ALBUM_IMAGES_COUNT_THREE = 3;
constexpr size_t ARRAY_INDEX_FIRST = 0;

// Album image positioning
constexpr int ALBUM_IMAGE_POSITION_0 = 0;
constexpr int ALBUM_IMAGE_POSITION_2 = 2;

// Timestamp constants
constexpr std::time_t TEST_TIMESTAMP_CREATED = 1234567890;
constexpr std::time_t TEST_TIMESTAMP_UPDATED = 1234567900;
constexpr std::time_t TIMESTAMP_ZERO = 0;

} // namespace test_constants
} // namespace gara

#endif // GARA_TEST_HELPERS_TEST_CONSTANTS_H
