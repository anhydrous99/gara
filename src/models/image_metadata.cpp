#include "image_metadata.h"
#include <sstream>
#include <iomanip>

namespace gara {

ImageMetadata::ImageMetadata()
    : image_id(""), original_format(""), s3_raw_key(""),
      original_size(0), upload_timestamp(0), name(""), width(0), height(0) {}

ImageMetadata::ImageMetadata(const std::string& id, const std::string& format,
                            const std::string& key, size_t size)
    : image_id(id), original_format(format), s3_raw_key(key),
      original_size(size), upload_timestamp(std::time(nullptr)), name(""), width(0), height(0) {}

std::string ImageMetadata::generateRawKey(const std::string& hash, const std::string& format) {
    return "raw/" + hash + "." + format;
}

std::string ImageMetadata::generateTransformedKey(const std::string& hash,
                                                  const std::string& format,
                                                  int width, int height,
                                                  bool watermarked) {
    std::ostringstream oss;
    oss << "transformed/" << hash << "_" << format << "_"
        << width << "x" << height;

    // Add watermark identifier if watermarked
    if (watermarked) {
        oss << "_wm";
    }

    oss << "." << format;
    return oss.str();
}

TransformRequest::TransformRequest()
    : image_id(""), target_format("jpeg"), width(0), height(0), watermarked(true) {}

TransformRequest::TransformRequest(const std::string& id, const std::string& format,
                                  int w, int h, bool wm)
    : image_id(id), target_format(format), width(w), height(h), watermarked(wm) {}

std::string TransformRequest::getCacheKey() const {
    return ImageMetadata::generateTransformedKey(image_id, target_format, width, height, watermarked);
}

nlohmann::json ImageMetadata::toJson() const {
    nlohmann::json j;
    j["id"] = image_id;
    j["name"] = name;
    j["size"] = original_size;
    j["format"] = original_format;

    // Convert Unix timestamp to ISO 8601 format
    char buffer[80];
    std::tm* timeinfo = std::gmtime(&upload_timestamp);
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S.000Z", timeinfo);
    j["uploadedAt"] = buffer;

    // Only include dimensions if they're known (not 0)
    if (width > 0) {
        j["width"] = width;
    }
    if (height > 0) {
        j["height"] = height;
    }

    return j;
}

} // namespace gara
