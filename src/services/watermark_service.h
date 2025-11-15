#ifndef GARA_WATERMARK_SERVICE_H
#define GARA_WATERMARK_SERVICE_H

#include <vips/vips8>
#include <string>
#include <utility>
#include "../models/watermark_config.h"

namespace gara {

class WatermarkService {
public:
    // Constructor with configuration
    explicit WatermarkService(const WatermarkConfig& config);

    // Destructor
    ~WatermarkService();

    // Main public interface: Apply watermark to an image
    // Returns watermarked image or original if watermarking fails/disabled
    vips::VImage applyWatermark(const vips::VImage& image) const;

    // Get current configuration (for testing/debugging)
    const WatermarkConfig& getConfig() const;

    // Check if watermarking is enabled
    bool isEnabled() const;

private:
    WatermarkConfig config_;

    // Calculate appropriate font size based on image dimensions
    // Uses 2.5% of image width as base scale
    int calculateFontSize(int imageWidth) const;

    // Calculate position coordinates for watermark placement
    // Returns (x, y) coordinates for top-left corner of watermark
    std::pair<int, int> calculatePosition(int imageWidth, int imageHeight,
                                          int watermarkWidth, int watermarkHeight) const;

    // Create text image using libvips text rendering
    vips::VImage createTextImage(const std::string& text, int fontSize) const;

    // Create shadow/outline effect for text visibility
    // Returns a dark version of the text image for background
    vips::VImage createShadow(const vips::VImage& textImage, int offset = 2) const;

    // Composite watermark onto image at specified position
    vips::VImage compositeWatermark(const vips::VImage& image,
                                   const vips::VImage& watermark,
                                   int x, int y) const;
};

} // namespace gara

#endif // GARA_WATERMARK_SERVICE_H
