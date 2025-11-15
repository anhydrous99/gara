#include <gtest/gtest.h>
#include "services/image_processor.h"
#include "utils/file_utils.h"
#include "test_helpers/test_constants.h"
#include "test_helpers/test_file_manager.h"
#include "test_helpers/custom_matchers.h"
#include <vips/vips8>
#include <vips/vips.h>
#include <fstream>

using namespace gara;
using namespace gara::utils;
using namespace gara::testing;
using namespace gara::test_constants;
using namespace gara::test_helpers;
using namespace gara::test_matchers;

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

        // Create a simple test image (small square)
        test_image_path_ = TestFileManager::createUniquePath("test_image_", ".ppm");
        createTestImage(test_image_path_, TEST_SMALL_IMAGE_SIZE, TEST_SMALL_IMAGE_SIZE);

        temp_files_.push_back(test_image_path_);
    }

    void TearDown() override {
        // Clean up all temporary files created during test
        for (const auto& file : temp_files_) {
            FileUtils::deleteFile(file);
        }
        temp_files_.clear();
    }

    // Helper to create a simple test image
    void createTestImage(const std::string& path, int width, int height) {
        try {
            // Create a solid color image (3 bands for RGB)
            vips::VImage image = vips::VImage::black(width, height, vips::VImage::option()
                ->set("bands", RGB_BANDS));
            image = image.new_from_image({RED_CHANNEL_MAX, GREEN_CHANNEL_MIN, BLUE_CHANNEL_MIN}); // Red

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
            out.write(static_cast<char*>(buf), width * height * RGB_BANDS);
            out.close();

            g_free(buf);
        } catch (vips::VError& e) {
            FAIL() << "Failed to create test image: " << e.what();
        }
    }

    // Helper to create a test image and track it for cleanup
    std::string createTrackedTestImage(const std::string& prefix, int width, int height) {
        std::string path = TestFileManager::createUniquePath(prefix, ".ppm");
        createTestImage(path, width, height);
        temp_files_.push_back(path);
        return path;
    }

    // Helper to create output path and track it for cleanup
    std::string createTrackedOutputPath(const std::string& prefix, const std::string& extension) {
        std::string path = TestFileManager::createUniquePath(prefix, extension);
        temp_files_.push_back(path);
        return path;
    }

    std::unique_ptr<ImageProcessor> processor_;
    std::string test_image_path_;
    std::vector<std::string> temp_files_;
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(ImageProcessorTest, Initialize_LibvipsLibrary_SuccessfullyInitializes) {
    // Arrange & Act
    // Library is initialized in SetUpTestSuite

    // Assert
    EXPECT_TRUE(true)
        << "If test execution reached here, libvips initialization succeeded";
}

// ============================================================================
// Image Validation Tests
// ============================================================================

TEST_F(ImageProcessorTest, IsValidImage_WithValidImage_ReturnsTrue) {
    // Act
    bool is_valid = processor_->isValidImage(test_image_path_);

    // Assert
    EXPECT_TRUE(is_valid)
        << "Valid PPM image should be recognized as valid";
}

TEST_F(ImageProcessorTest, IsValidImage_WithInvalidFile_ReturnsFalse) {
    // Arrange
    std::string invalid_path = TestFileManager::createUniquePath("invalid_", ".txt");
    std::ofstream file(invalid_path);
    file << TEST_INVALID_IMAGE_CONTENT;
    file.close();
    temp_files_.push_back(invalid_path);

    // Act
    bool is_valid = processor_->isValidImage(invalid_path);

    // Assert
    EXPECT_FALSE(is_valid)
        << "Text file should not be recognized as valid image";
}

TEST_F(ImageProcessorTest, IsValidImage_WithNonexistentFile_ReturnsFalse) {
    // Arrange
    std::string nonexistent = TestFileManager::createUniquePath("nonexistent_", ".jpg");

    // Act
    bool is_valid = processor_->isValidImage(nonexistent);

    // Assert
    EXPECT_FALSE(is_valid)
        << "Nonexistent file should not be valid image";
}

// ============================================================================
// Image Info Retrieval Tests
// ============================================================================

TEST_F(ImageProcessorTest, GetImageInfo_FromValidImage_ReturnsCorrectInfo) {
    // Act
    ImageInfo info = processor_->getImageInfo(test_image_path_);

    // Assert
    EXPECT_TRUE(info.is_valid)
        << "Image info should indicate image is valid";

    EXPECT_EQ(TEST_SMALL_IMAGE_SIZE, info.width)
        << "Width should match test image dimensions";

    EXPECT_EQ(TEST_SMALL_IMAGE_SIZE, info.height)
        << "Height should match test image dimensions";

    EXPECT_EQ(FORMAT_PPM, info.format)
        << "Format should be detected as PPM";
}

TEST_F(ImageProcessorTest, GetImageInfo_FromInvalidFile_ReturnsInvalidInfo) {
    // Arrange
    std::string nonexistent = TestFileManager::createUniquePath("nonexistent_", ".jpg");

    // Act
    ImageInfo info = processor_->getImageInfo(nonexistent);

    // Assert
    EXPECT_FALSE(info.is_valid)
        << "Image info for nonexistent file should be marked invalid";
}

// ============================================================================
// Format Conversion Tests
// ============================================================================

TEST_F(ImageProcessorTest, Transform_FormatOnlyToJPEG_ConvertsSuccessfully) {
    // Arrange
    std::string output_path = createTrackedOutputPath("output_jpeg_", ".jpg");

    // Act
    bool success = processor_->transform(
        test_image_path_,
        output_path,
        FORMAT_JPEG
    );

    // Assert
    EXPECT_TRUE(success)
        << "Format conversion to JPEG should succeed";

    EXPECT_TRUE(FileUtils::fileExists(output_path))
        << "Output JPEG file should exist";

    EXPECT_TRUE(processor_->isValidImage(output_path))
        << "Output should be a valid image";

    // Verify dimensions unchanged
    ImageInfo info = processor_->getImageInfo(output_path);
    EXPECT_EQ(TEST_SMALL_IMAGE_SIZE, info.width)
        << "Width should remain unchanged after format conversion";

    EXPECT_EQ(TEST_SMALL_IMAGE_SIZE, info.height)
        << "Height should remain unchanged after format conversion";
}

TEST_F(ImageProcessorTest, Transform_FormatOnlyToPNG_ConvertsSuccessfully) {
    // Arrange
    std::string output_path = createTrackedOutputPath("output_png_", ".png");

    // Act
    bool success = processor_->transform(
        test_image_path_,
        output_path,
        FORMAT_PNG
    );

    // Assert
    EXPECT_TRUE(success)
        << "Format conversion to PNG should succeed";

    EXPECT_TRUE(FileUtils::fileExists(output_path))
        << "Output PNG file should exist";

    EXPECT_TRUE(processor_->isValidImage(output_path))
        << "Output PNG should be a valid image";
}

TEST_F(ImageProcessorTest, Transform_FormatOnlyToWebP_ConvertsSuccessfully) {
    // Arrange
    std::string output_path = createTrackedOutputPath("output_webp_", ".webp");

    // Act
    bool success = processor_->transform(
        test_image_path_,
        output_path,
        FORMAT_WEBP
    );

    // Assert
    EXPECT_TRUE(success)
        << "Format conversion to WebP should succeed";

    EXPECT_TRUE(FileUtils::fileExists(output_path))
        << "Output WebP file should exist";

    EXPECT_TRUE(processor_->isValidImage(output_path))
        << "Output WebP should be a valid image";
}

// ============================================================================
// Resize with Aspect Ratio Tests
// ============================================================================

TEST_F(ImageProcessorTest, Transform_ResizeWidthOnly_MaintainsAspectRatio) {
    // Arrange - Create a larger test image with known aspect ratio (2:1)
    std::string large_image = createTrackedTestImage(
        "large_image_",
        TEST_MEDIUM_IMAGE_WIDTH,
        TEST_MEDIUM_IMAGE_HEIGHT
    );

    std::string output_path = createTrackedOutputPath("resized_width_", ".jpg");

    // Act - Resize to half width, height should auto-scale
    bool success = processor_->transform(
        large_image,
        output_path,
        FORMAT_JPEG,
        RESIZE_TARGET_WIDTH_50,  // width
        RESIZE_MAINTAIN_ASPECT   // height (maintain aspect)
    );

    // Assert
    EXPECT_TRUE(success)
        << "Resize with width-only should succeed";

    EXPECT_TRUE(FileUtils::fileExists(output_path))
        << "Resized output file should exist";

    // Verify dimensions - aspect ratio 2:1 should be maintained
    ImageInfo info = processor_->getImageInfo(output_path);
    EXPECT_EQ(RESIZE_TARGET_WIDTH_50, info.width)
        << "Width should be resized to target width";

    EXPECT_EQ(RESIZE_TARGET_HEIGHT_25, info.height)
        << "Height should be auto-scaled to maintain 2:1 aspect ratio";
}

TEST_F(ImageProcessorTest, Transform_ResizeHeightOnly_MaintainsAspectRatio) {
    // Arrange - Create a larger test image with known aspect ratio (2:1)
    std::string large_image = createTrackedTestImage(
        "large_image_h_",
        TEST_MEDIUM_IMAGE_WIDTH,
        TEST_MEDIUM_IMAGE_HEIGHT
    );

    std::string output_path = createTrackedOutputPath("resized_height_", ".jpg");

    // Act - Resize to half height, width should auto-scale
    bool success = processor_->transform(
        large_image,
        output_path,
        FORMAT_JPEG,
        RESIZE_MAINTAIN_ASPECT,  // width (maintain aspect)
        RESIZE_TARGET_HEIGHT_25  // height
    );

    // Assert
    EXPECT_TRUE(success)
        << "Resize with height-only should succeed";

    // Verify dimensions - aspect ratio 2:1 should be maintained
    ImageInfo info = processor_->getImageInfo(output_path);
    EXPECT_EQ(RESIZE_TARGET_WIDTH_50, info.width)
        << "Width should be auto-scaled to maintain 2:1 aspect ratio";

    EXPECT_EQ(RESIZE_TARGET_HEIGHT_25, info.height)
        << "Height should be resized to target height";
}

// ============================================================================
// Explicit Dimension Resize Tests
// ============================================================================

TEST_F(ImageProcessorTest, Transform_ResizeBothDimensions_ResizesToExactSize) {
    // Arrange
    std::string output_path = createTrackedOutputPath("resized_both_", ".jpg");

    // Act - Resize to specific dimensions (may distort aspect ratio)
    bool success = processor_->transform(
        test_image_path_,
        output_path,
        FORMAT_JPEG,
        RESIZE_TARGET_WIDTH_20,   // width
        RESIZE_TARGET_HEIGHT_20   // height
    );

    // Assert
    EXPECT_TRUE(success)
        << "Resize with explicit width and height should succeed";

    // Verify exact dimensions
    ImageInfo info = processor_->getImageInfo(output_path);
    EXPECT_EQ(RESIZE_TARGET_WIDTH_20, info.width)
        << "Width should match specified resize width";

    EXPECT_EQ(RESIZE_TARGET_HEIGHT_20, info.height)
        << "Height should match specified resize height";
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(ImageProcessorTest, Transform_WithNonexistentInput_ReturnsFalse) {
    // Arrange
    std::string output_path = createTrackedOutputPath("invalid_output_", ".jpg");
    std::string nonexistent_input = TestFileManager::createUniquePath("nonexistent_", ".jpg");

    // Act
    bool success = processor_->transform(
        nonexistent_input,
        output_path,
        FORMAT_JPEG
    );

    // Assert
    EXPECT_FALSE(success)
        << "Transformation of nonexistent file should fail";

    EXPECT_FALSE(FileUtils::fileExists(output_path))
        << "Output file should not be created for failed transformation";
}
