#include "cache_manager.h"
#include "../utils/file_utils.h"
#include <iostream>

namespace gara {

CacheManager::CacheManager(std::shared_ptr<S3Service> s3_service)
    : s3_service_(s3_service) {
}

bool CacheManager::existsInCache(const TransformRequest& request) {
    std::string s3_key = getS3Key(request);
    return s3_service_->objectExists(s3_key);
}

std::string CacheManager::getCachedImage(const TransformRequest& request) {
    std::string s3_key = getS3Key(request);

    if (s3_service_->objectExists(s3_key)) {
        return s3_key;
    }

    return "";
}

bool CacheManager::storeInCache(const TransformRequest& request, const std::string& local_path) {
    std::string s3_key = getS3Key(request);
    std::string content_type = utils::FileUtils::getMimeType(request.target_format);

    bool success = s3_service_->uploadFile(local_path, s3_key, content_type);

    if (success) {
        std::cout << "Cached transformed image: " << s3_key << std::endl;
    } else {
        std::cerr << "Failed to cache transformed image: " << s3_key << std::endl;
    }

    return success;
}

std::string CacheManager::getPresignedUrl(const TransformRequest& request, int expiration_seconds) {
    std::string s3_key = getS3Key(request);

    if (!s3_service_->objectExists(s3_key)) {
        return "";
    }

    return s3_service_->generatePresignedUrl(s3_key, expiration_seconds);
}

bool CacheManager::clearImageCache(const std::string& image_id) {
    // In a production system, you'd want to list all objects with prefix
    // and delete them. For now, this is a placeholder.
    // You could use S3 ListObjectsV2 to find all transformed versions
    std::cerr << "clearImageCache not fully implemented - would delete all transformations for: "
              << image_id << std::endl;
    return false;
}

bool CacheManager::clearTransformation(const TransformRequest& request) {
    std::string s3_key = getS3Key(request);
    return s3_service_->deleteObject(s3_key);
}

std::string CacheManager::getS3Key(const TransformRequest& request) {
    return ImageMetadata::generateTransformedKey(
        request.image_id,
        request.target_format,
        request.width,
        request.height,
        request.watermarked
    );
}

} // namespace gara
