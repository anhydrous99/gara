#ifndef GARA_CONSTANTS_ALBUM_CONSTANTS_H
#define GARA_CONSTANTS_ALBUM_CONSTANTS_H

#include <string>
#include <vector>

namespace gara {
namespace constants {

// Presigned URL expiration time in seconds (1 hour)
constexpr int PRESIGNED_URL_EXPIRATION_SECONDS = 3600;

// CORS max age in seconds (1 hour)
constexpr int CORS_MAX_AGE_SECONDS = 3600;

// Supported image file formats
const std::vector<std::string> SUPPORTED_IMAGE_FORMATS = {
    "jpg", "jpeg", "png", "webp", "gif"
};

// Error message identifiers
constexpr const char* ERROR_NOT_FOUND = "not found";
constexpr const char* ERROR_INVALID_JSON = "Invalid JSON";
constexpr const char* ERROR_VALIDATION = "Validation error";

// Query parameter values
constexpr const char* PARAM_TRUE = "true";

} // namespace constants
} // namespace gara

#endif // GARA_CONSTANTS_ALBUM_CONSTANTS_H
