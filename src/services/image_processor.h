#ifndef GARA_IMAGE_PROCESSOR_H
#define GARA_IMAGE_PROCESSOR_H

#include <string>
#include <memory>
#include <vips/vips8>

namespace gara {

struct ImageInfo {
    int width;
    int height;
    std::string format;
    size_t size_bytes;
    bool is_valid;
};

class ImageProcessor {
public:
    ImageProcessor();
    ~ImageProcessor();

    // Initialize libvips (call once at startup)
    static bool initialize();

    // Shutdown libvips (call once at shutdown)
    static void shutdown();

    // Transform image: convert format and/or resize
    // If width or height is 0, maintains aspect ratio
    // Returns true on success
    bool transform(const std::string& input_path,
                  const std::string& output_path,
                  const std::string& target_format = "jpeg",
                  int target_width = 0,
                  int target_height = 0,
                  int quality = 85);

    // Get image information without loading full image
    ImageInfo getImageInfo(const std::string& filepath);

    // Validate if file is a valid image
    bool isValidImage(const std::string& filepath);

private:
    // Calculate dimensions maintaining aspect ratio
    void calculateDimensions(int original_width, int original_height,
                           int& target_width, int& target_height);

    // Convert format name to libvips suffix
    std::string formatToSuffix(const std::string& format);
};

} // namespace gara

#endif // GARA_IMAGE_PROCESSOR_H
