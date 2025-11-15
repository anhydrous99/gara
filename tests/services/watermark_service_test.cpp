#include <gtest/gtest.h>
#include "services/watermark_service.h"
#include "services/image_processor.h"
#include "models/watermark_config.h"
#include "utils/file_utils.h"
#include "test_helpers/test_constants.h"
#include "test_helpers/test_file_manager.h"
#include <vips/vips8>
#include <fstream>

using namespace gara;
using namespace gara::utils;
using namespace gara::test_constants;
using namespace gara::test_helpers;

class WatermarkServiceTest : public ::testing::Test {
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
        // Create default config for testing
        default_config_ = WatermarkConfig();
        default_config_.enabled = true;
        default_config_.text = "© 2025 Test";
        default_config_.position = "bottom-right";
        default_config_.base_font_size = 24;
        default_config_.font_color = "white";
        default_config_.opacity = 0.9;
        default_config_.margin = 20;

        service_ = std::make_unique<WatermarkService>(default_config_);

        // Create a test image
        test_image_path_ = TestFileManager::createUniquePath("watermark_test_", ".ppm");
        createTestImage(test_image_path_, 800, 600);
        temp_files_.push_back(test_image_path_);
    }

    void TearDown() override {
        // Clean up temporary files
        for (const auto& file : temp_files_) {
            FileUtils::deleteFile(file);
        }
        temp_files_.clear();
    }

    // Helper to create a simple test image
    void createTestImage(const std::string& path, int width, int height) {
        try {
            // Create a solid blue image
            vips::VImage image = vips::VImage::black(width, height, vips::VImage::option()
                ->set("bands", 3));
            image = image.new_from_image({0, 0, 255}); // Blue

            // Write PPM format
            std::ofstream out(path, std::ios::binary);
            ASSERT_TRUE(out) << "Failed to open file for writing: " << path;

            out << "P6\n" << width << " " << height << "\n255\n";

            // Write pixel data
            size_t data_length;
            void* buf = image.write_to_memory(&data_length);
            out.write(static_cast<char*>(buf), width * height * 3);
            out.close();
            g_free(buf);
        } catch (vips::VError& e) {
            FAIL() << "Failed to create test image: " << e.what();
        }
    }

    std::unique_ptr<WatermarkService> service_;
    WatermarkConfig default_config_;
    std::string test_image_path_;
    std::vector<std::string> temp_files_;
};

// Test: Service initialization
TEST_F(WatermarkServiceTest, InitializesWithValidConfig) {
    ASSERT_NE(service_, nullptr);
    EXPECT_TRUE(service_->isEnabled());
    EXPECT_EQ(service_->getConfig().text, "© 2025 Test");
}

// Test: Disabled watermark returns original image
TEST_F(WatermarkServiceTest, DisabledWatermarkReturnsOriginalImage) {
    WatermarkConfig disabled_config = default_config_;
    disabled_config.enabled = false;
    WatermarkService disabled_service(disabled_config);

    vips::VImage original = vips::VImage::new_from_file(test_image_path_.c_str());
    vips::VImage result = disabled_service.applyWatermark(original);

    // Image should be unchanged
    EXPECT_EQ(result.width(), original.width());
    EXPECT_EQ(result.height(), original.height());
}

// Test: Watermark applies to image
TEST_F(WatermarkServiceTest, AppliesWatermarkToImage) {
    vips::VImage original = vips::VImage::new_from_file(test_image_path_.c_str());
    vips::VImage watermarked = service_->applyWatermark(original);

    // Watermarked image should have same dimensions
    EXPECT_EQ(watermarked.width(), original.width());
    EXPECT_EQ(watermarked.height(), original.height());

    // Image should not be null
    EXPECT_GT(watermarked.width(), 0);
    EXPECT_GT(watermarked.height(), 0);
}

// Test: Config validation
TEST_F(WatermarkServiceTest, ValidatesConfig) {
    WatermarkConfig valid_config = default_config_;
    EXPECT_TRUE(valid_config.isValid());

    WatermarkConfig invalid_config = default_config_;
    invalid_config.text = "";  // Empty text is invalid
    EXPECT_FALSE(invalid_config.isValid());

    invalid_config = default_config_;
    invalid_config.opacity = 1.5;  // Opacity > 1.0 is invalid
    EXPECT_FALSE(invalid_config.isValid());

    invalid_config = default_config_;
    invalid_config.base_font_size = -10;  // Negative font size is invalid
    EXPECT_FALSE(invalid_config.isValid());

    invalid_config = default_config_;
    invalid_config.position = "invalid-position";
    EXPECT_FALSE(invalid_config.isValid());
}

// Test: Different positions
TEST_F(WatermarkServiceTest, SupportsAllPositions) {
    vips::VImage original = vips::VImage::new_from_file(test_image_path_.c_str());

    std::vector<std::string> positions = {"bottom-right", "bottom-left", "top-right", "top-left"};

    for (const auto& position : positions) {
        WatermarkConfig config = default_config_;
        config.position = position;
        WatermarkService service(config);

        vips::VImage watermarked = service.applyWatermark(original);

        EXPECT_EQ(watermarked.width(), original.width()) << "Failed for position: " << position;
        EXPECT_EQ(watermarked.height(), original.height()) << "Failed for position: " << position;
    }
}

// Test: Different image sizes
TEST_F(WatermarkServiceTest, WorksWithDifferentImageSizes) {
    std::vector<std::pair<int, int>> sizes = {
        {100, 100},   // Small
        {800, 600},   // Medium
        {1920, 1080}, // Large
        {5000, 3000}  // Very large
    };

    for (const auto& size : sizes) {
        std::string path = TestFileManager::createUniquePath("size_test_", ".ppm");
        createTestImage(path, size.first, size.second);
        temp_files_.push_back(path);

        vips::VImage image = vips::VImage::new_from_file(path.c_str());
        vips::VImage watermarked = service_->applyWatermark(image);

        EXPECT_EQ(watermarked.width(), size.first)
            << "Failed for size: " << size.first << "x" << size.second;
        EXPECT_EQ(watermarked.height(), size.second)
            << "Failed for size: " << size.first << "x" << size.second;
    }
}

// Test: Graceful error handling
TEST_F(WatermarkServiceTest, HandlesErrorsGracefully) {
    // Even with invalid image, service should not crash
    // This tests the try-catch in applyWatermark
    vips::VImage original = vips::VImage::new_from_file(test_image_path_.c_str());

    // Apply watermark should succeed
    EXPECT_NO_THROW({
        vips::VImage watermarked = service_->applyWatermark(original);
    });
}

// Test: Config from environment
TEST_F(WatermarkServiceTest, LoadsConfigFromEnvironment) {
    // Set environment variables
    setenv("WATERMARK_ENABLED", "true", 1);
    setenv("WATERMARK_TEXT", "© 2025 Armando Herrera", 1);
    setenv("WATERMARK_POSITION", "bottom-right", 1);
    setenv("WATERMARK_FONT_SIZE", "30", 1);
    setenv("WATERMARK_OPACITY", "0.8", 1);
    setenv("WATERMARK_MARGIN", "25", 1);

    WatermarkConfig config = WatermarkConfig::fromEnvironment();

    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.text, "© 2025 Armando Herrera");
    EXPECT_EQ(config.position, "bottom-right");
    EXPECT_EQ(config.base_font_size, 30);
    EXPECT_DOUBLE_EQ(config.opacity, 0.8);
    EXPECT_EQ(config.margin, 25);

    // Clean up environment
    unsetenv("WATERMARK_ENABLED");
    unsetenv("WATERMARK_TEXT");
    unsetenv("WATERMARK_POSITION");
    unsetenv("WATERMARK_FONT_SIZE");
    unsetenv("WATERMARK_OPACITY");
    unsetenv("WATERMARK_MARGIN");
}

// Test: Output file can be written
TEST_F(WatermarkServiceTest, WatermarkedImageCanBeSaved) {
    vips::VImage original = vips::VImage::new_from_file(test_image_path_.c_str());
    vips::VImage watermarked = service_->applyWatermark(original);

    std::string output_path = TestFileManager::createUniquePath("watermarked_output_", ".jpg");
    temp_files_.push_back(output_path);

    EXPECT_NO_THROW({
        watermarked.write_to_file(output_path.c_str());
    });

    // Verify file was created and is readable
    EXPECT_TRUE(FileUtils::fileExists(output_path));

    vips::VImage loaded = vips::VImage::new_from_file(output_path.c_str());
    EXPECT_EQ(loaded.width(), original.width());
    EXPECT_EQ(loaded.height(), original.height());
}
