#ifndef GARA_IMAGE_METADATA_H
#define GARA_IMAGE_METADATA_H

#include <string>
#include <ctime>

namespace gara {

struct ImageMetadata {
    std::string image_id;           // SHA256 hash of original file
    std::string original_format;    // Original file extension (png, jpg, etc)
    std::string s3_raw_key;        // S3 key for raw image
    size_t original_size;          // File size in bytes
    std::time_t upload_timestamp;  // When uploaded

    // Constructor
    ImageMetadata();
    ImageMetadata(const std::string& id, const std::string& format,
                  const std::string& key, size_t size);

    // Generate S3 key for raw image
    static std::string generateRawKey(const std::string& hash, const std::string& format);

    // Generate S3 key for transformed image
    static std::string generateTransformedKey(const std::string& hash,
                                             const std::string& format,
                                             int width, int height);
};

struct TransformRequest {
    std::string image_id;
    std::string target_format;  // Default: "jpeg"
    int width;                  // Target width (0 = maintain aspect)
    int height;                 // Target height (0 = maintain aspect)

    TransformRequest();
    TransformRequest(const std::string& id, const std::string& format,
                    int w, int h);

    // Generate cache key for this transform
    std::string getCacheKey() const;
};

} // namespace gara

#endif // GARA_IMAGE_METADATA_H
