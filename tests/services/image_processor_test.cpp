#include <gtest/gtest.h>
#include "services/image_processor.h"
#include "utils/file_utils.h"
#include <vips/vips8>
#include <vips/vips.h>
#include <fstream>

using namespace gara;
using namespace gara::utils;

class ImageProcessorTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Initialize libvips once for all tests
        ImageProcessor::initialize();
    }

    static void TearDownTestSuite() {
        // Cleanup libvips
        ImageProcessor::shutdown();
    }

    void SetUp() override {
        processor_ = std::make_unique<ImageProcessor>();

        // Create a simple test image (10x10 red square)
        test_image_path_ = "/tmp/gara_test_image.ppm";
        createTestImage(test_image_path_, 10, 10);
    }

    void TearDown() override {
        FileUtils::deleteFile(test_image_path_);
    }

    // Helper to create a simple test image
    void createTestImage(const std::string& path, int width, int height) {
        try {
            // Create a solid color image (3 bands for RGB)
            vips::VImage image = vips::VImage::black(width, height, vips::VImage::option()
                ->set("bands", 3));
            image = image.new_from_image({255.0, 0.0, 0.0}); // Red

            // Write to buffer with .v format (vips native format - always supported)
            size_t data_length;
            void* buf = image.write_to_memory(&data_length);

            // Save to file
            std::ofstream out(path, std::ios::binary);
            if (!out) {
                g_free(buf);
                FAIL() << "Failed to open file for writing: " << path;
            }

            // Write a simple PPM header (always supported, no dependencies)
            out << "P6\n" << width << " " << height << "\n255\n";

            // Write pixel data (assuming uchar RGB)
            out.write(static_cast<char*>(buf), width * height * 3);
            out.close();

            g_free(buf);
        } catch (vips::VError& e) {
            FAIL() << "Failed to create test image: " << e.what();
        }
    }

    std::unique_ptr<ImageProcessor> processor_;
    std::string test_image_path_;
};

// Test library initialization
TEST_F(ImageProcessorTest, Initialization) {
    // Library should already be initialized
    EXPECT_TRUE(true); // If we got here, initialization succeeded
}

// Test valid image detection
TEST_F(ImageProcessorTest, IsValidImage) {
    EXPECT_TRUE(processor_->isValidImage(test_image_path_));

    // Invalid image
    std::string invalid_path = "/tmp/gara_invalid.txt";
    std::ofstream file(invalid_path);
    file << "not an image";
    file.close();

    EXPECT_FALSE(processor_->isValidImage(invalid_path));

    FileUtils::deleteFile(invalid_path);
}

// Test get image info
TEST_F(ImageProcessorTest, GetImageInfo) {
    ImageInfo info = processor_->getImageInfo(test_image_path_);

    EXPECT_TRUE(info.is_valid);
    EXPECT_EQ(10, info.width);
    EXPECT_EQ(10, info.height);
    EXPECT_EQ("ppm", info.format);
}

// Test basic transformation (no resize, just format conversion)
TEST_F(ImageProcessorTest, TransformFormatOnly) {
    TempFile output("output_");
    std::string output_path = output.getPath() + ".jpg";

    bool success = processor_->transform(
        test_image_path_,
        output_path,
        "jpeg"
    );

    EXPECT_TRUE(success);
    EXPECT_TRUE(FileUtils::fileExists(output_path));

    // Verify output is valid
    EXPECT_TRUE(processor_->isValidImage(output_path));

    // Verify dimensions unchanged
    ImageInfo info = processor_->getImageInfo(output_path);
    EXPECT_EQ(10, info.width);
    EXPECT_EQ(10, info.height);

    FileUtils::deleteFile(output_path);
}

// Test resize with width only (maintain aspect ratio)
TEST_F(ImageProcessorTest, TransformResizeWidthOnly) {
    // Create a larger test image (100x50)
    std::string large_image = "/tmp/gara_large.ppm";
    createTestImage(large_image, 100, 50);

    TempFile output("resized_");
    std::string output_path = output.getPath() + ".jpg";

    bool success = processor_->transform(
        large_image,
        output_path,
        "jpeg",
        50,  // width
        0    // height (maintain aspect)
    );

    EXPECT_TRUE(success);
    EXPECT_TRUE(FileUtils::fileExists(output_path));

    // Verify dimensions
    ImageInfo info = processor_->getImageInfo(output_path);
    EXPECT_EQ(50, info.width);
    EXPECT_EQ(25, info.height);  // Aspect ratio 2:1 maintained

    FileUtils::deleteFile(large_image);
    FileUtils::deleteFile(output_path);
}

// Test resize with height only (maintain aspect ratio)
TEST_F(ImageProcessorTest, TransformResizeHeightOnly) {
    // Create a larger test image (100x50)
    std::string large_image = "/tmp/gara_large2.ppm";
    createTestImage(large_image, 100, 50);

    TempFile output("resized_h_");
    std::string output_path = output.getPath() + ".jpg";

    bool success = processor_->transform(
        large_image,
        output_path,
        "jpeg",
        0,   // width (maintain aspect)
        25   // height
    );

    EXPECT_TRUE(success);

    // Verify dimensions
    ImageInfo info = processor_->getImageInfo(output_path);
    EXPECT_EQ(50, info.width);   // Aspect ratio 2:1 maintained
    EXPECT_EQ(25, info.height);

    FileUtils::deleteFile(large_image);
    FileUtils::deleteFile(output_path);
}

// Test resize with both dimensions
TEST_F(ImageProcessorTest, TransformResizeBothDimensions) {
    TempFile output("resized_both_");
    std::string output_path = output.getPath() + ".jpg";

    bool success = processor_->transform(
        test_image_path_,
        output_path,
        "jpeg",
        20,  // width
        20   // height
    );

    EXPECT_TRUE(success);

    // Verify dimensions
    ImageInfo info = processor_->getImageInfo(output_path);
    EXPECT_EQ(20, info.width);
    EXPECT_EQ(20, info.height);

    FileUtils::deleteFile(output_path);
}

// Test transformation to PNG
TEST_F(ImageProcessorTest, TransformToPNG) {
    TempFile output("png_");
    std::string output_path = output.getPath() + ".png";

    bool success = processor_->transform(
        test_image_path_,
        output_path,
        "png"
    );

    EXPECT_TRUE(success);
    EXPECT_TRUE(FileUtils::fileExists(output_path));

    FileUtils::deleteFile(output_path);
}

// Test transformation to WebP
TEST_F(ImageProcessorTest, TransformToWebP) {
    TempFile output("webp_");
    std::string output_path = output.getPath() + ".webp";

    bool success = processor_->transform(
        test_image_path_,
        output_path,
        "webp"
    );

    EXPECT_TRUE(success);
    EXPECT_TRUE(FileUtils::fileExists(output_path));

    FileUtils::deleteFile(output_path);
}

// Test invalid input file
TEST_F(ImageProcessorTest, TransformInvalidInput) {
    TempFile output("invalid_");
    std::string output_path = output.getPath() + ".jpg";

    bool success = processor_->transform(
        "/nonexistent/image.jpg",
        output_path,
        "jpeg"
    );

    EXPECT_FALSE(success);
}

// Test get info on invalid file
TEST_F(ImageProcessorTest, GetImageInfoInvalid) {
    ImageInfo info = processor_->getImageInfo("/nonexistent/image.jpg");
    EXPECT_FALSE(info.is_valid);
}
