#include "image_processor.h"
#include "../utils/logger.h"
#include "../utils/metrics.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace gara {

ImageProcessor::ImageProcessor() {
}

ImageProcessor::~ImageProcessor() {
}

bool ImageProcessor::initialize() {
    if (VIPS_INIT("gara")) {
        LOG_CRITICAL("Failed to initialize libvips");
        METRICS_COUNT("LibVipsErrors", 1.0, "Count", {{"error_type", "init_failed"}});
        return false;
    }
    LOG_INFO("libvips initialized successfully");
    return true;
}

void ImageProcessor::shutdown() {
    vips_shutdown();
}

bool ImageProcessor::transform(const std::string& input_path,
                              const std::string& output_path,
                              const std::string& target_format,
                              int target_width,
                              int target_height,
                              int quality) {
    auto timer = gara::Metrics::get()->start_timer("ImageProcessingDuration", {
        {"operation", "transform"},
        {"format", target_format}
    });

    try {
        // Load image
        vips::VImage image = vips::VImage::new_from_file(input_path.c_str());

        int original_width = image.width();
        int original_height = image.height();

        // Calculate target dimensions if needed
        if (target_width > 0 || target_height > 0) {
            calculateDimensions(original_width, original_height, target_width, target_height);

            // Resize image
            double h_scale = static_cast<double>(target_width) / original_width;
            double v_scale = static_cast<double>(target_height) / original_height;

            image = image.resize(h_scale, vips::VImage::option()
                ->set("vscale", v_scale)
                ->set("kernel", VIPS_KERNEL_LANCZOS3));
        }

        // Prepare save options
        vips::VOption* save_options = vips::VImage::option();

        // Set quality for JPEG
        if (target_format == "jpeg" || target_format == "jpg") {
            save_options->set("Q", quality);
            // Strip metadata to reduce file size
            save_options->set("strip", true);
            // Optimize for smaller file size
            save_options->set("optimize_coding", true);
        } else if (target_format == "png") {
            // PNG compression level (1-9)
            save_options->set("compression", 6);
            save_options->set("strip", true);
        } else if (target_format == "webp") {
            save_options->set("Q", quality);
            save_options->set("strip", true);
        }

        // Convert suffix for saving
        std::string suffix = formatToSuffix(target_format);

        // Save image with options
        image.write_to_file(output_path.c_str(), save_options);

        METRICS_COUNT("ImageTransformations", 1.0, "Count", {
            {"format", target_format},
            {"status", "success"}
        });

        return true;

    } catch (vips::VError& e) {
        gara::Logger::log_structured(spdlog::level::err, "Image transformation failed", {
            {"input_path", input_path},
            {"output_path", output_path},
            {"target_format", target_format},
            {"target_width", target_width},
            {"target_height", target_height},
            {"error", e.what()}
        });
        METRICS_COUNT("ImageTransformations", 1.0, "Count", {
            {"format", target_format},
            {"status", "error"}
        });
        return false;
    }
}

ImageInfo ImageProcessor::getImageInfo(const std::string& filepath) {
    ImageInfo info;
    info.is_valid = false;

    try {
        vips::VImage image = vips::VImage::new_from_file(filepath.c_str());

        info.width = image.width();
        info.height = image.height();
        info.is_valid = true;

        // Try to determine format from filename
        size_t dot_pos = filepath.find_last_of('.');
        if (dot_pos != std::string::npos) {
            info.format = filepath.substr(dot_pos + 1);
            std::transform(info.format.begin(), info.format.end(),
                         info.format.begin(), ::tolower);
        }

        // Calculate approximate size (width * height * channels)
        int bands = image.bands();
        info.size_bytes = info.width * info.height * bands;

    } catch (vips::VError& e) {
        gara::Logger::log_structured(spdlog::level::err, "Failed to get image info", {
            {"filepath", filepath},
            {"error", e.what()}
        });
    }

    return info;
}

bool ImageProcessor::isValidImage(const std::string& filepath) {
    try {
        vips::VImage image = vips::VImage::new_from_file(filepath.c_str());
        return true;
    } catch (vips::VError& e) {
        return false;
    }
}

void ImageProcessor::calculateDimensions(int original_width, int original_height,
                                        int& target_width, int& target_height) {
    // If both dimensions specified, use them as-is
    if (target_width > 0 && target_height > 0) {
        return;
    }

    double aspect_ratio = static_cast<double>(original_width) / original_height;

    // If only width specified
    if (target_width > 0 && target_height == 0) {
        target_height = static_cast<int>(std::round(target_width / aspect_ratio));
    }
    // If only height specified
    else if (target_height > 0 && target_width == 0) {
        target_width = static_cast<int>(std::round(target_height * aspect_ratio));
    }
    // If neither specified, use original dimensions
    else {
        target_width = original_width;
        target_height = original_height;
    }
}

std::string ImageProcessor::formatToSuffix(const std::string& format) {
    std::string lower_format = format;
    std::transform(lower_format.begin(), lower_format.end(),
                  lower_format.begin(), ::tolower);

    // Map common formats to libvips suffixes
    if (lower_format == "jpg") return ".jpg";
    if (lower_format == "jpeg") return ".jpg";
    if (lower_format == "png") return ".png";
    if (lower_format == "webp") return ".webp";
    if (lower_format == "tiff" || lower_format == "tif") return ".tif";
    if (lower_format == "gif") return ".gif";

    // Default to jpg if unknown
    return ".jpg";
}

} // namespace gara
