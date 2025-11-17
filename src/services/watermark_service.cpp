#include "watermark_service.h"
#include "../utils/logger.h"
#include "../utils/metrics.h"
#include <cmath>

namespace gara {

WatermarkService::WatermarkService(const WatermarkConfig& config)
    : config_(config) {
    if (!config_.isValid()) {
        gara::Logger::log_structured(spdlog::level::warn, "Invalid watermark configuration, using defaults", {
            {"config_enabled", config.enabled},
            {"config_text", config.text},
            {"config_position", config.position},
            {"config_opacity", config.opacity}
        });
        config_ = WatermarkConfig();
    }
}

WatermarkService::~WatermarkService() {
    // Cleanup if needed
}

vips::VImage WatermarkService::applyWatermark(const vips::VImage& image) const {
    auto timer = gara::Metrics::get()->start_timer("WatermarkDuration", {{"operation", "apply"}});

    try {
        // If watermarking is disabled, return original image
        if (!config_.enabled) {
            return image;
        }

        // Get image dimensions
        int imageWidth = image.width();
        int imageHeight = image.height();

        // Calculate appropriate font size for this image
        int fontSize = calculateFontSize(imageWidth);

        // Create text image with white text
        vips::VImage textImage = createTextImage(config_.text, fontSize);

        // Create shadow for better visibility
        vips::VImage shadowImage = createShadow(textImage);

        // Get watermark dimensions (from text image)
        int watermarkWidth = textImage.width();
        int watermarkHeight = textImage.height();

        // Calculate position based on config
        auto [x, y] = calculatePosition(imageWidth, imageHeight,
                                       watermarkWidth, watermarkHeight);

        // First, composite shadow onto the image
        vips::VImage withShadow = compositeWatermark(image, shadowImage,
                                                     x + 2, y + 2);

        // Then, composite white text on top
        vips::VImage result = compositeWatermark(withShadow, textImage, x, y);

        METRICS_COUNT("WatermarkOperations", 1.0, "Count", {{"status", "success"}});
        return result;

    } catch (const vips::VError& e) {
        // Log error and return original image (graceful degradation)
        gara::Logger::log_structured(spdlog::level::err, "Watermark application failed (libvips error)", {
            {"image_width", image.width()},
            {"image_height", image.height()},
            {"watermark_text", config_.text},
            {"watermark_position", config_.position},
            {"error_type", "vips_error"},
            {"error", e.what()}
        });
        METRICS_COUNT("WatermarkOperations", 1.0, "Count", {{"status", "error"}});
        return image;
    } catch (const std::exception& e) {
        gara::Logger::log_structured(spdlog::level::err, "Watermark application failed (general error)", {
            {"image_width", image.width()},
            {"image_height", image.height()},
            {"watermark_text", config_.text},
            {"watermark_position", config_.position},
            {"error_type", "general_exception"},
            {"error", e.what()}
        });
        METRICS_COUNT("WatermarkOperations", 1.0, "Count", {{"status", "error"}});
        return image;
    }
}

const WatermarkConfig& WatermarkService::getConfig() const {
    return config_;
}

bool WatermarkService::isEnabled() const {
    return config_.enabled;
}

int WatermarkService::calculateFontSize(int imageWidth) const {
    // Scale font size to 2.5% of image width
    // Minimum 12px, maximum 100px for reasonable sizes
    int scaledSize = static_cast<int>(imageWidth * 0.025);

    // Clamp between reasonable bounds
    if (scaledSize < 12) return 12;
    if (scaledSize > 100) return 100;

    return scaledSize;
}

std::pair<int, int> WatermarkService::calculatePosition(int imageWidth, int imageHeight,
                                                        int watermarkWidth, int watermarkHeight) const {
    int x = 0, y = 0;

    if (config_.position == "bottom-right") {
        x = imageWidth - watermarkWidth - config_.margin;
        y = imageHeight - watermarkHeight - config_.margin;
    } else if (config_.position == "bottom-left") {
        x = config_.margin;
        y = imageHeight - watermarkHeight - config_.margin;
    } else if (config_.position == "top-right") {
        x = imageWidth - watermarkWidth - config_.margin;
        y = config_.margin;
    } else if (config_.position == "top-left") {
        x = config_.margin;
        y = config_.margin;
    }

    // Ensure position is within bounds
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    return {x, y};
}

vips::VImage WatermarkService::createTextImage(const std::string& text, int fontSize) const {
    try {
        // Create text image with libvips
        // Options: text, width, height, align, dpi, font
        vips::VOption* options = vips::VImage::option()
            ->set("font", "sans")
            ->set("dpi", 150)
            ->set("rgba", true);  // Create RGBA image for transparency

        vips::VImage textImg = vips::VImage::text(text.c_str(), options);

        // Scale to desired font size
        double scale = static_cast<double>(fontSize) / 24.0;  // 24 is base size
        if (scale != 1.0) {
            textImg = textImg.resize(scale);
        }

        // Convert to RGBA if not already
        if (!textImg.has_alpha()) {
            textImg = textImg.bandjoin(255);  // Add opaque alpha channel
        }

        // Set text color to white (255, 255, 255)
        // The text() function creates black text by default, so we need to invert
        std::vector<double> white = {255, 255, 255};

        // Extract alpha channel
        vips::VImage alpha = textImg.extract_band(textImg.bands() - 1);

        // Create white RGB image
        vips::VImage whiteImg = vips::VImage::black(textImg.width(), textImg.height())
            .new_from_image(white);

        // Combine white RGB with original alpha
        textImg = whiteImg.bandjoin(alpha);

        // Apply opacity
        if (config_.opacity < 1.0) {
            // Multiply alpha channel by opacity
            vips::VImage rgb = textImg.extract_band(0, vips::VImage::option()
                ->set("n", 3));  // Extract RGB channels
            vips::VImage alpha_channel = textImg.extract_band(3);
            alpha_channel = alpha_channel.linear(config_.opacity, 0);
            textImg = rgb.bandjoin(alpha_channel);
        }

        return textImg;

    } catch (const vips::VError& e) {
        gara::Logger::log_structured(spdlog::level::err, "Failed to create text image for watermark", {
            {"text", text},
            {"font_size", fontSize},
            {"opacity", config_.opacity},
            {"error", e.what()}
        });
        throw;
    }
}

vips::VImage WatermarkService::createShadow(const vips::VImage& textImage, int offset) const {
    try {
        // Create a black version of the text for shadow
        int width = textImage.width();
        int height = textImage.height();

        // Extract alpha channel from text image
        vips::VImage alpha = textImage.extract_band(textImage.bands() - 1);

        // Create black RGB image
        std::vector<double> black = {0, 0, 0};
        vips::VImage blackImg = vips::VImage::black(width, height)
            .new_from_image(black);

        // Combine black RGB with alpha channel (slightly reduced for shadow effect)
        vips::VImage shadowAlpha = alpha.linear(0.7, 0);  // 70% opacity for shadow
        vips::VImage shadow = blackImg.bandjoin(shadowAlpha);

        return shadow;

    } catch (const vips::VError& e) {
        gara::Logger::log_structured(spdlog::level::err, "Failed to create shadow for watermark", {
            {"text_width", textImage.width()},
            {"text_height", textImage.height()},
            {"offset", offset},
            {"error", e.what()}
        });
        throw;
    }
}

vips::VImage WatermarkService::compositeWatermark(const vips::VImage& image,
                                                  const vips::VImage& watermark,
                                                  int x, int y) const {
    try {
        // Ensure image has alpha channel for compositing
        vips::VImage img = image;
        if (!img.has_alpha()) {
            img = img.bandjoin(255);  // Add opaque alpha channel
        }

        // Embed watermark at position (x, y) on a transparent canvas the size of the image
        vips::VImage canvas = vips::VImage::black(img.width(), img.height())
            .new_from_image(std::vector<double>{0, 0, 0, 0})  // Transparent black
            .copy(vips::VImage::option()->set("interpretation", VIPS_INTERPRETATION_sRGB));

        // Add alpha channel to canvas
        if (canvas.bands() == 3) {
            canvas = canvas.bandjoin(0);  // Add transparent alpha
        }

        // Embed watermark at position
        vips::VImage positioned = watermark.embed(x, y, img.width(), img.height(),
            vips::VImage::option()->set("extend", VIPS_EXTEND_BACKGROUND)
                ->set("background", std::vector<double>{0, 0, 0, 0}));

        // Composite watermark over image using "over" blend mode
        vips::VImage result = img.composite2(positioned, VIPS_BLEND_MODE_OVER);

        // If original image didn't have alpha, remove it now
        if (!image.has_alpha() && result.has_alpha()) {
            result = result.extract_band(0, vips::VImage::option()->set("n", image.bands()));
        }

        return result;

    } catch (const vips::VError& e) {
        gara::Logger::log_structured(spdlog::level::err, "Failed to composite watermark onto image", {
            {"image_width", image.width()},
            {"image_height", image.height()},
            {"watermark_width", watermark.width()},
            {"watermark_height", watermark.height()},
            {"position_x", x},
            {"position_y", y},
            {"error", e.what()}
        });
        throw;
    }
}

} // namespace gara
