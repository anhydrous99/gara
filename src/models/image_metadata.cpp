#include "image_metadata.h"
#include <sstream>
#include <iomanip>

namespace gara {

ImageMetadata::ImageMetadata()
    : image_id(""), original_format(""), s3_raw_key(""),
      original_size(0), upload_timestamp(0) {}

ImageMetadata::ImageMetadata(const std::string& id, const std::string& format,
                            const std::string& key, size_t size)
    : image_id(id), original_format(format), s3_raw_key(key),
      original_size(size), upload_timestamp(std::time(nullptr)) {}

std::string ImageMetadata::generateRawKey(const std::string& hash, const std::string& format) {
    return "raw/" + hash + "." + format;
}

std::string ImageMetadata::generateTransformedKey(const std::string& hash,
                                                  const std::string& format,
                                                  int width, int height) {
    std::ostringstream oss;
    oss << "transformed/" << hash << "_" << format << "_"
        << width << "x" << height << "." << format;
    return oss.str();
}

TransformRequest::TransformRequest()
    : image_id(""), target_format("jpeg"), width(0), height(0) {}

TransformRequest::TransformRequest(const std::string& id, const std::string& format,
                                  int w, int h)
    : image_id(id), target_format(format), width(w), height(h) {}

std::string TransformRequest::getCacheKey() const {
    return ImageMetadata::generateTransformedKey(image_id, target_format, width, height);
}

} // namespace gara
