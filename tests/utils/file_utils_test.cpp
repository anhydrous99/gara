#include <gtest/gtest.h>
#include "utils/file_utils.h"
#include <fstream>
#include <vector>

using namespace gara::utils;

class FileUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary test file
        test_file_path_ = "/tmp/gara_test_file.txt";
        std::ofstream file(test_file_path_);
        file << "Hello, Gara!";
        file.close();
    }

    void TearDown() override {
        // Clean up test file
        FileUtils::deleteFile(test_file_path_);
    }

    std::string test_file_path_;
};

// Test SHA256 hash calculation
TEST_F(FileUtilsTest, CalculateSHA256FromFile) {
    std::string hash = FileUtils::calculateSHA256(test_file_path_);

    // Verify hash is not empty
    EXPECT_FALSE(hash.empty());

    // Verify hash length (SHA256 = 64 hex characters)
    EXPECT_EQ(64, hash.length());

    // Verify hash is consistent
    std::string hash2 = FileUtils::calculateSHA256(test_file_path_);
    EXPECT_EQ(hash, hash2);
}

TEST_F(FileUtilsTest, CalculateSHA256FromData) {
    std::vector<char> data = {'H', 'e', 'l', 'l', 'o'};
    std::string hash = FileUtils::calculateSHA256(data);

    // Verify hash is not empty
    EXPECT_FALSE(hash.empty());

    // Verify hash length
    EXPECT_EQ(64, hash.length());

    // Verify same data produces same hash
    std::string hash2 = FileUtils::calculateSHA256(data);
    EXPECT_EQ(hash, hash2);

    // Verify different data produces different hash
    std::vector<char> data2 = {'W', 'o', 'r', 'l', 'd'};
    std::string hash3 = FileUtils::calculateSHA256(data2);
    EXPECT_NE(hash, hash3);
}

// Test file extension extraction
TEST_F(FileUtilsTest, GetFileExtension) {
    EXPECT_EQ("jpg", FileUtils::getFileExtension("image.jpg"));
    EXPECT_EQ("png", FileUtils::getFileExtension("photo.PNG"));  // Lowercase conversion
    EXPECT_EQ("jpeg", FileUtils::getFileExtension("/path/to/file.jpeg"));
    EXPECT_EQ("", FileUtils::getFileExtension("no_extension"));
    EXPECT_EQ("", FileUtils::getFileExtension(""));
}

// Test valid image format detection
TEST_F(FileUtilsTest, IsValidImageFormat) {
    EXPECT_TRUE(FileUtils::isValidImageFormat("jpg"));
    EXPECT_TRUE(FileUtils::isValidImageFormat("JPEG"));
    EXPECT_TRUE(FileUtils::isValidImageFormat("png"));
    EXPECT_TRUE(FileUtils::isValidImageFormat("gif"));
    EXPECT_TRUE(FileUtils::isValidImageFormat("webp"));
    EXPECT_TRUE(FileUtils::isValidImageFormat("tiff"));

    EXPECT_FALSE(FileUtils::isValidImageFormat("txt"));
    EXPECT_FALSE(FileUtils::isValidImageFormat("pdf"));
    EXPECT_FALSE(FileUtils::isValidImageFormat("exe"));
    EXPECT_FALSE(FileUtils::isValidImageFormat(""));
}

// Test MIME type detection
TEST_F(FileUtilsTest, GetMimeType) {
    EXPECT_EQ("image/jpeg", FileUtils::getMimeType("jpg"));
    EXPECT_EQ("image/jpeg", FileUtils::getMimeType("JPEG"));
    EXPECT_EQ("image/png", FileUtils::getMimeType("png"));
    EXPECT_EQ("image/gif", FileUtils::getMimeType("gif"));
    EXPECT_EQ("image/webp", FileUtils::getMimeType("webp"));
    EXPECT_EQ("application/octet-stream", FileUtils::getMimeType("unknown"));
}

// Test temporary file creation
TEST_F(FileUtilsTest, CreateTempFile) {
    std::string temp_path = FileUtils::createTempFile("test_");

    // Verify temp file was created
    EXPECT_FALSE(temp_path.empty());
    EXPECT_TRUE(FileUtils::fileExists(temp_path));

    // Verify prefix is included
    EXPECT_NE(std::string::npos, temp_path.find("test_"));

    // Clean up
    FileUtils::deleteFile(temp_path);
}

// Test file write and read
TEST_F(FileUtilsTest, WriteAndReadFile) {
    std::string test_path = "/tmp/gara_test_write.bin";
    std::vector<char> data = {'T', 'e', 's', 't', ' ', 'D', 'a', 't', 'a'};

    // Write data
    bool write_success = FileUtils::writeToFile(test_path, data);
    EXPECT_TRUE(write_success);
    EXPECT_TRUE(FileUtils::fileExists(test_path));

    // Read data back
    std::vector<char> read_data = FileUtils::readFile(test_path);
    EXPECT_EQ(data, read_data);

    // Clean up
    FileUtils::deleteFile(test_path);
}

// Test file size
TEST_F(FileUtilsTest, GetFileSize) {
    size_t size = FileUtils::getFileSize(test_file_path_);

    // "Hello, Gara!" = 12 bytes
    EXPECT_EQ(12, size);

    // Non-existent file
    EXPECT_EQ(0, FileUtils::getFileSize("/nonexistent/file.txt"));
}

// Test file exists
TEST_F(FileUtilsTest, FileExists) {
    EXPECT_TRUE(FileUtils::fileExists(test_file_path_));
    EXPECT_FALSE(FileUtils::fileExists("/nonexistent/file.txt"));
}

// Test file deletion
TEST_F(FileUtilsTest, DeleteFile) {
    std::string temp_path = "/tmp/gara_delete_test.txt";

    // Create file
    std::ofstream file(temp_path);
    file << "delete me";
    file.close();

    EXPECT_TRUE(FileUtils::fileExists(temp_path));

    // Delete file
    bool deleted = FileUtils::deleteFile(temp_path);
    EXPECT_TRUE(deleted);
    EXPECT_FALSE(FileUtils::fileExists(temp_path));

    // Delete non-existent file
    bool deleted2 = FileUtils::deleteFile(temp_path);
    EXPECT_FALSE(deleted2);
}

// Test TempFile RAII wrapper
TEST_F(FileUtilsTest, TempFileRAII) {
    std::string temp_path;

    {
        TempFile temp("raii_test_");
        temp_path = temp.getPath();

        // Verify temp file exists
        EXPECT_FALSE(temp_path.empty());
        EXPECT_TRUE(FileUtils::fileExists(temp_path));

        // Write data
        std::vector<char> data = {'R', 'A', 'I', 'I'};
        EXPECT_TRUE(temp.write(data));
    } // TempFile goes out of scope

    // Verify temp file was deleted
    EXPECT_FALSE(FileUtils::fileExists(temp_path));
}

TEST_F(FileUtilsTest, TempFileMove) {
    std::string temp_path1, temp_path2;

    TempFile temp1("move_test_");
    temp_path1 = temp1.getPath();

    // Move construction
    TempFile temp2(std::move(temp1));
    temp_path2 = temp2.getPath();

    EXPECT_EQ(temp_path1, temp_path2);
    EXPECT_TRUE(FileUtils::fileExists(temp_path2));
}
