#ifndef GARA_CACHE_MANAGER_H
#define GARA_CACHE_MANAGER_H

#include "s3_service.h"
#include "../models/image_metadata.h"
#include <string>
#include <memory>

namespace gara {

class CacheManager {
public:
    explicit CacheManager(std::shared_ptr<S3Service> s3_service);
    ~CacheManager() = default;

    // Check if transformed image exists in cache (S3)
    bool existsInCache(const TransformRequest& request);

    // Get transformed image from cache
    // Returns S3 key if found, empty string if not found
    std::string getCachedImage(const TransformRequest& request);

    // Store transformed image in cache
    bool storeInCache(const TransformRequest& request, const std::string& local_path);

    // Generate presigned URL for cached image
    std::string getPresignedUrl(const TransformRequest& request, int expiration_seconds = 3600);

    // Clear cache for specific image (all transformations)
    bool clearImageCache(const std::string& image_id);

    // Clear specific transformation from cache
    bool clearTransformation(const TransformRequest& request);

private:
    std::shared_ptr<S3Service> s3_service_;

    // Generate S3 key for transformed image
    std::string getS3Key(const TransformRequest& request);
};

} // namespace gara

#endif // GARA_CACHE_MANAGER_H
