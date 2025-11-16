#include "cache_manager.h"
#include "../utils/file_utils.h"
#include "../utils/logger.h"
#include "../utils/metrics.h"

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
    auto timer = gara::Metrics::get()->start_timer("CacheDuration", {{"operation", "put"}});

    std::string s3_key = getS3Key(request);
    std::string content_type = utils::FileUtils::getMimeType(request.target_format);

    bool success = s3_service_->uploadFile(local_path, s3_key, content_type);

    if (success) {
        gara::Logger::log_structured(spdlog::level::info, "Cached transformed image", {
            {"s3_key", s3_key},
            {"image_id", request.image_id},
            {"format", request.target_format},
            {"width", request.width},
            {"height", request.height},
            {"watermarked", request.watermarked}
        });
        METRICS_COUNT("CacheOperations", 1.0, "Count", {{"operation", "put"}, {"status", "success"}});
    } else {
        gara::Logger::log_structured(spdlog::level::err, "Failed to cache transformed image", {
            {"s3_key", s3_key},
            {"image_id", request.image_id},
            {"format", request.target_format},
            {"local_path", local_path}
        });
        METRICS_COUNT("CacheOperations", 1.0, "Count", {{"operation", "put"}, {"status", "failure"}});
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
    gara::Logger::log_structured(spdlog::level::warn, "clearImageCache not fully implemented", {
        {"image_id", image_id},
        {"operation", "clear_all_transformations"},
        {"status", "not_implemented"}
    });
    METRICS_COUNT("CacheOperations", 1.0, "Count", {{"operation", "clear_all"}, {"status", "not_implemented"}});
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
