#include <gtest/gtest.h>
#include "utils/file_utils.h"
#include "test_helpers/test_constants.h"
#include "test_helpers/test_file_manager.h"
#include "test_helpers/test_builders.h"
#include "test_helpers/custom_matchers.h"
#include <fstream>
#include <vector>

using namespace gara::utils;
using namespace gara::test_constants;
using namespace gara::test_helpers;
using namespace gara::test_builders;
using namespace gara::test_matchers;

class FileUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary test file with unique path
        test_file_path_ = TestFileManager::createUniquePath("file_utils_test_", ".txt");

        std::ofstream file(test_file_path_);
        file << TEST_CONTENT;  // "Hello, Gara!"
        file.close();
    }

    void TearDown() override {
        // Clean up test file
        FileUtils::deleteFile(test_file_path_);

        // Clean up any additional files created during tests
        for (const auto& file : temp_files_) {
            FileUtils::deleteFile(file);
        }
        temp_files_.clear();
    }

    // Helper to track temporary files for cleanup
    void trackTempFile(const std::string& path) {
        temp_files_.push_back(path);
    }

    std::string test_file_path_;
    std::vector<std::string> temp_files_;
};

// ============================================================================
// SHA256 Hash Calculation Tests
// ============================================================================

TEST_F(FileUtilsTest, CalculateSHA256_FromValidFile_ReturnsValidHash) {
    // Act
    std::string hash = FileUtils::calculateSHA256(test_file_path_);

    // Assert
    EXPECT_THAT(hash, IsNonEmptyString())
        << "Hash should not be empty for valid file";

    EXPECT_EQ(SHA256_HEX_LENGTH, hash.length())
        << "SHA256 hash should be " << SHA256_HEX_LENGTH << " hex characters";

    EXPECT_THAT(hash, IsValidSHA256Hash())
        << "Hash should be valid hex string";
}

TEST_F(FileUtilsTest, CalculateSHA256_SameFile_ProducesConsistentHash) {
    // Act
    std::string hash1 = FileUtils::calculateSHA256(test_file_path_);
    std::string hash2 = FileUtils::calculateSHA256(test_file_path_);

    // Assert
    EXPECT_EQ(hash1, hash2)
        << "Same file should produce identical hash on multiple calculations";
}

TEST_F(FileUtilsTest, CalculateSHA256_FromData_ReturnsValidHash) {
    // Arrange
    auto data = TestDataBuilder::createTextData("Hello");

    // Act
    std::string hash = FileUtils::calculateSHA256(data);

    // Assert
    EXPECT_EQ(SHA256_HEX_LENGTH, hash.length())
        << "Hash of data vector should be " << SHA256_HEX_LENGTH << " characters";

    EXPECT_THAT(hash, IsValidSHA256Hash());
}

TEST_F(FileUtilsTest, CalculateSHA256_SameData_ProducesConsistentHash) {
    // Arrange
    auto data = TestDataBuilder::createTextData("Test Data");

    // Act
    std::string hash1 = FileUtils::calculateSHA256(data);
    std::string hash2 = FileUtils::calculateSHA256(data);

    // Assert
    EXPECT_EQ(hash1, hash2)
        << "Same data should produce identical hash";
}

TEST_F(FileUtilsTest, CalculateSHA256_DifferentData_ProducesDifferentHashes) {
    // Arrange
    auto data1 = TestDataBuilder::createTextData("Hello");
    auto data2 = TestDataBuilder::createTextData("World");

    // Act
    std::string hash1 = FileUtils::calculateSHA256(data1);
    std::string hash2 = FileUtils::calculateSHA256(data2);

    // Assert
    EXPECT_NE(hash1, hash2)
        << "Different data should produce different hashes";
}

TEST_F(FileUtilsTest, CalculateSHA256_NonExistentFile_ReturnsEmptyString) {
    // Arrange
    std::string nonexistent = TestFileManager::createUniquePath("nonexistent_", ".txt");

    // Act
    std::string hash = FileUtils::calculateSHA256(nonexistent);

    // Assert
    EXPECT_TRUE(hash.empty())
        << "Calculating hash of non-existent file should return empty string";
}

// ============================================================================
// File Extension Tests
// ============================================================================

TEST_F(FileUtilsTest, GetFileExtension_WithJpgFile_ReturnsLowercaseJpg) {
    // Act & Assert
    EXPECT_EQ(FORMAT_JPG, FileUtils::getFileExtension("image.jpg"))
        << "Should extract 'jpg' extension";
}

TEST_F(FileUtilsTest, GetFileExtension_WithUppercaseExtension_ReturnsLowercase) {
    // Act & Assert
    EXPECT_EQ(FORMAT_PNG, FileUtils::getFileExtension("photo.PNG"))
        << "Extension should be converted to lowercase";
}

TEST_F(FileUtilsTest, GetFileExtension_WithPathPrefix_ExtractsExtension) {
    // Act & Assert
    EXPECT_EQ(FORMAT_JPEG, FileUtils::getFileExtension("/path/to/file.jpeg"))
        << "Should extract extension regardless of path";
}

TEST_F(FileUtilsTest, GetFileExtension_WithoutExtension_ReturnsEmptyString) {
    // Act & Assert
    EXPECT_EQ("", FileUtils::getFileExtension("no_extension"))
        << "File without extension should return empty string";

    EXPECT_EQ("", FileUtils::getFileExtension(""))
        << "Empty filename should return empty string";
}

// ============================================================================
// Image Format Validation Tests
// ============================================================================

class ValidImageFormatTest : public FileUtilsTest,
                             public ::testing::WithParamInterface<std::string> {};

TEST_P(ValidImageFormatTest, IsValidImageFormat_WithValidFormat_ReturnsTrue) {
    // Arrange
    std::string format = GetParam();

    // Act
    bool is_valid = FileUtils::isValidImageFormat(format);

    // Assert
    EXPECT_TRUE(is_valid)
        << "Format '" << format << "' should be recognized as valid image format";
}

INSTANTIATE_TEST_SUITE_P(
    SupportedFormats,
    ValidImageFormatTest,
    ::testing::Values("jpg", "JPEG", "png", "gif", "webp", "tiff")
);

class InvalidImageFormatTest : public FileUtilsTest,
                               public ::testing::WithParamInterface<std::string> {};

TEST_P(InvalidImageFormatTest, IsValidImageFormat_WithInvalidFormat_ReturnsFalse) {
    // Arrange
    std::string format = GetParam();

    // Act
    bool is_valid = FileUtils::isValidImageFormat(format);

    // Assert
    EXPECT_FALSE(is_valid)
        << "Format '" << format << "' should not be recognized as valid image format";
}

INSTANTIATE_TEST_SUITE_P(
    UnsupportedFormats,
    InvalidImageFormatTest,
    ::testing::Values("txt", "pdf", "exe", "doc", "")
);

// ============================================================================
// MIME Type Tests
// ============================================================================

struct MimeTypeTestCase {
    std::string extension;
    std::string expected_mime;
};

class MimeTypeTest : public FileUtilsTest,
                     public ::testing::WithParamInterface<MimeTypeTestCase> {};

TEST_P(MimeTypeTest, GetMimeType_ForExtension_ReturnsCorrectMimeType) {
    // Arrange
    auto test_case = GetParam();

    // Act
    std::string mime_type = FileUtils::getMimeType(test_case.extension);

    // Assert
    EXPECT_EQ(test_case.expected_mime, mime_type)
        << "Extension '" << test_case.extension << "' should map to MIME type '"
        << test_case.expected_mime << "'";
}

INSTANTIATE_TEST_SUITE_P(
    CommonFormats,
    MimeTypeTest,
    ::testing::Values(
        MimeTypeTestCase{FORMAT_JPG, MIME_JPEG},
        MimeTypeTestCase{"JPEG", MIME_JPEG},
        MimeTypeTestCase{FORMAT_PNG, MIME_PNG},
        MimeTypeTestCase{FORMAT_GIF, MIME_GIF},
        MimeTypeTestCase{FORMAT_WEBP, MIME_WEBP},
        MimeTypeTestCase{"unknown", MIME_DEFAULT}
    )
);

// ============================================================================
// Temporary File Creation Tests
// ============================================================================

TEST_F(FileUtilsTest, CreateTempFile_WithPrefix_CreatesUniqueFile) {
    // Act
    std::string temp_path = FileUtils::createTempFile("test_prefix_");
    trackTempFile(temp_path);

    // Assert
    EXPECT_THAT(temp_path, IsNonEmptyString())
        << "Temp file path should not be empty";

    EXPECT_TRUE(FileUtils::fileExists(temp_path))
        << "Temp file should exist after creation";

    EXPECT_THAT(temp_path, ContainsSubstring("test_prefix_"))
        << "Temp file path should contain the specified prefix";
}

TEST_F(FileUtilsTest, CreateTempFile_MultipleCalls_CreatesDifferentFiles) {
    // Act
    std::string path1 = FileUtils::createTempFile("multi_");
    std::string path2 = FileUtils::createTempFile("multi_");
    trackTempFile(path1);
    trackTempFile(path2);

    // Assert
    EXPECT_NE(path1, path2)
        << "Multiple calls should create different temp files";

    EXPECT_TRUE(FileUtils::fileExists(path1));
    EXPECT_TRUE(FileUtils::fileExists(path2));
}

// ============================================================================
// File Write and Read Tests
// ============================================================================

TEST_F(FileUtilsTest, WriteAndReadFile_WithBinaryData_PreservesData) {
    // Arrange
    auto test_path = TestFileManager::createUniquePath("write_read_", ".bin");
    trackTempFile(test_path);
    auto data = TestDataBuilder::createTextData("Test Data");

    // Act - Write
    bool write_success = FileUtils::writeToFile(test_path, data);

    // Assert - Write succeeded
    EXPECT_TRUE(write_success)
        << "Writing to file should succeed";
    EXPECT_TRUE(FileUtils::fileExists(test_path))
        << "File should exist after write";

    // Act - Read
    auto read_data = FileUtils::readFile(test_path);

    // Assert - Data preserved
    EXPECT_EQ(data, read_data)
        << "Read data should match written data exactly";
}

TEST_F(FileUtilsTest, WriteToFile_WithEmptyData_CreatesEmptyFile) {
    // Arrange
    auto test_path = TestFileManager::createUniquePath("empty_", ".bin");
    trackTempFile(test_path);
    std::vector<char> empty_data;

    // Act
    bool success = FileUtils::writeToFile(test_path, empty_data);

    // Assert
    EXPECT_TRUE(success);
    EXPECT_EQ(0, FileUtils::getFileSize(test_path))
        << "File should be empty";
}

TEST_F(FileUtilsTest, ReadFile_NonExistentFile_ReturnsEmptyVector) {
    // Arrange
    auto nonexistent = TestFileManager::createUniquePath("nonexistent_", ".dat");

    // Act
    auto data = FileUtils::readFile(nonexistent);

    // Assert
    EXPECT_TRUE(data.empty())
        << "Reading non-existent file should return empty vector";
}

// ============================================================================
// File Size Tests
// ============================================================================

TEST_F(FileUtilsTest, GetFileSize_ExistingFile_ReturnsCorrectSize) {
    // Act
    size_t size = FileUtils::getFileSize(test_file_path_);

    // Assert
    EXPECT_EQ(TEST_CONTENT_SIZE, size)
        << "File containing '" << TEST_CONTENT << "' should be "
        << TEST_CONTENT_SIZE << " bytes";
}

TEST_F(FileUtilsTest, GetFileSize_NonExistentFile_ReturnsZero) {
    // Arrange
    auto nonexistent = TestFileManager::createUniquePath("nonexistent_", ".txt");

    // Act
    size_t size = FileUtils::getFileSize(nonexistent);

    // Assert
    EXPECT_EQ(0, size)
        << "Non-existent file should return size of 0";
}

// ============================================================================
// File Existence Tests
// ============================================================================

TEST_F(FileUtilsTest, FileExists_ExistingFile_ReturnsTrue) {
    // Act & Assert
    EXPECT_TRUE(FileUtils::fileExists(test_file_path_))
        << "Created test file should exist";
}

TEST_F(FileUtilsTest, FileExists_NonExistentFile_ReturnsFalse) {
    // Arrange
    auto nonexistent = TestFileManager::createUniquePath("nonexistent_", ".txt");

    // Act & Assert
    EXPECT_FALSE(FileUtils::fileExists(nonexistent))
        << "Non-existent file should return false";
}

// ============================================================================
// File Deletion Tests
// ============================================================================

TEST_F(FileUtilsTest, DeleteFile_ExistingFile_DeletesAndReturnsTrue) {
    // Arrange
    auto temp_path = TestFileManager::createUniquePath("delete_test_", ".txt");

    std::ofstream file(temp_path);
    file << "delete me";
    file.close();

    ASSERT_TRUE(FileUtils::fileExists(temp_path))
        << "Setup: File should exist before deletion";

    // Act
    bool deleted = FileUtils::deleteFile(temp_path);

    // Assert
    EXPECT_TRUE(deleted)
        << "Deleting existing file should return true";

    EXPECT_FALSE(FileUtils::fileExists(temp_path))
        << "File should not exist after deletion";
}

TEST_F(FileUtilsTest, DeleteFile_NonExistentFile_ReturnsFalse) {
    // Arrange
    auto nonexistent = TestFileManager::createUniquePath("nonexistent_del_", ".txt");

    // Act
    bool deleted = FileUtils::deleteFile(nonexistent);

    // Assert
    EXPECT_FALSE(deleted)
        << "Attempting to delete non-existent file should return false";
}

// ============================================================================
// TempFile RAII Wrapper Tests
// ============================================================================

TEST_F(FileUtilsTest, TempFileRAII_WhenOutOfScope_DeletesFile) {
    // Arrange
    std::string temp_path;

    {
        // Act - Create TempFile in scope
        TempFile temp("raii_test_");
        temp_path = temp.getPath();

        // Assert - File exists in scope
        EXPECT_THAT(temp_path, IsNonEmptyString());
        EXPECT_TRUE(FileUtils::fileExists(temp_path))
            << "TempFile should exist while in scope";

        // Write some data
        auto data = TestDataBuilder::createTextData("RAII");
        EXPECT_TRUE(temp.write(data));

    } // TempFile goes out of scope here

    // Assert - File deleted after scope
    EXPECT_FALSE(FileUtils::fileExists(temp_path))
        << "TempFile should be automatically deleted when out of scope";
}

TEST_F(FileUtilsTest, TempFileRAII_MoveConstruction_TransfersOwnership) {
    // Arrange
    std::string temp_path1, temp_path2;

    TempFile temp1("move_test_");
    temp_path1 = temp1.getPath();

    ASSERT_TRUE(FileUtils::fileExists(temp_path1));

    // Act - Move construction
    TempFile temp2(std::move(temp1));
    temp_path2 = temp2.getPath();

    // Assert - Path transferred
    EXPECT_EQ(temp_path1, temp_path2)
        << "Move construction should transfer the same path";

    EXPECT_TRUE(FileUtils::fileExists(temp_path2))
        << "File should still exist after move";
}
