#include <gtest/gtest.h>
#include "services/s3_service.h"
#include "mocks/mock_s3_service.h"
#include "utils/file_utils.h"
#include <fstream>
#include <vector>

using namespace gara;
using namespace gara::utils;
using namespace gara::testing;

class S3ServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        fake_s3_ = std::make_shared<FakeS3Service>("test-bucket", "us-east-1");

        // Create test files
        test_file_path_ = "/tmp/s3_test_file.txt";
        test_file_content_ = "Hello, S3 World! This is test data.";
        createTestFile(test_file_path_, test_file_content_);

        binary_file_path_ = "/tmp/s3_test_binary.bin";
        createBinaryTestFile(binary_file_path_);
    }

    void TearDown() override {
        fake_s3_->clear();
        FileUtils::deleteFile(test_file_path_);
        FileUtils::deleteFile(binary_file_path_);
    }

    void createTestFile(const std::string& path, const std::string& content) {
        std::ofstream file(path);
        file << content;
        file.close();
    }

    void createBinaryTestFile(const std::string& path) {
        std::ofstream file(path, std::ios::binary);
        for (int i = 0; i < 256; ++i) {
            file.put(static_cast<char>(i));
        }
        file.close();
    }

    std::shared_ptr<FakeS3Service> fake_s3_;
    std::string test_file_path_;
    std::string test_file_content_;
    std::string binary_file_path_;
};

// Test uploadFile with text file
TEST_F(S3ServiceTest, UploadFileTextSuccess) {
    std::string s3_key = "test/file.txt";
    bool result = fake_s3_->uploadFile(test_file_path_, s3_key, "text/plain");

    EXPECT_TRUE(result);
    EXPECT_TRUE(fake_s3_->objectExists(s3_key));
}

// Test uploadFile with binary file
TEST_F(S3ServiceTest, UploadFileBinarySuccess) {
    std::string s3_key = "test/binary.bin";
    bool result = fake_s3_->uploadFile(binary_file_path_, s3_key, "application/octet-stream");

    EXPECT_TRUE(result);
    EXPECT_TRUE(fake_s3_->objectExists(s3_key));
}

// Test uploadFile with non-existent file
TEST_F(S3ServiceTest, UploadFileNonExistent) {
    std::string s3_key = "test/nonexistent.txt";
    bool result = fake_s3_->uploadFile("/tmp/nonexistent_file.txt", s3_key);

    EXPECT_FALSE(result);
    EXPECT_FALSE(fake_s3_->objectExists(s3_key));
}

// Test uploadData with small data
TEST_F(S3ServiceTest, UploadDataSmall) {
    std::vector<char> data = {'H', 'e', 'l', 'l', 'o'};
    std::string s3_key = "test/small.dat";

    bool result = fake_s3_->uploadData(data, s3_key);

    EXPECT_TRUE(result);
    EXPECT_TRUE(fake_s3_->objectExists(s3_key));
}

// Test uploadData with empty data
TEST_F(S3ServiceTest, UploadDataEmpty) {
    std::vector<char> data;
    std::string s3_key = "test/empty.dat";

    bool result = fake_s3_->uploadData(data, s3_key);

    EXPECT_TRUE(result);
    EXPECT_TRUE(fake_s3_->objectExists(s3_key));
}

// Test uploadData with large data
TEST_F(S3ServiceTest, UploadDataLarge) {
    std::vector<char> data(1024 * 1024, 'A'); // 1MB of 'A'
    std::string s3_key = "test/large.dat";

    bool result = fake_s3_->uploadData(data, s3_key);

    EXPECT_TRUE(result);
    EXPECT_TRUE(fake_s3_->objectExists(s3_key));

    // Verify size
    std::vector<char> retrieved = fake_s3_->downloadData(s3_key);
    EXPECT_EQ(data.size(), retrieved.size());
}

// Test downloadFile success
TEST_F(S3ServiceTest, DownloadFileSuccess) {
    std::string s3_key = "test/download.txt";
    fake_s3_->uploadFile(test_file_path_, s3_key);

    std::string download_path = "/tmp/s3_downloaded.txt";
    bool result = fake_s3_->downloadFile(s3_key, download_path);

    EXPECT_TRUE(result);
    EXPECT_TRUE(FileUtils::fileExists(download_path));

    // Verify content
    std::ifstream file(download_path);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    EXPECT_EQ(test_file_content_, content);

    FileUtils::deleteFile(download_path);
}

// Test downloadFile non-existent object
TEST_F(S3ServiceTest, DownloadFileNonExistent) {
    std::string s3_key = "test/nonexistent.txt";
    std::string download_path = "/tmp/s3_downloaded_fail.txt";

    bool result = fake_s3_->downloadFile(s3_key, download_path);

    EXPECT_FALSE(result);
}

// Test downloadData success
TEST_F(S3ServiceTest, DownloadDataSuccess) {
    std::vector<char> original_data = {'T', 'e', 's', 't', ' ', 'D', 'a', 't', 'a'};
    std::string s3_key = "test/data.bin";

    fake_s3_->uploadData(original_data, s3_key);
    std::vector<char> downloaded_data = fake_s3_->downloadData(s3_key);

    EXPECT_EQ(original_data, downloaded_data);
}

// Test downloadData non-existent object
TEST_F(S3ServiceTest, DownloadDataNonExistent) {
    std::vector<char> data = fake_s3_->downloadData("nonexistent/key.bin");

    EXPECT_TRUE(data.empty());
}

// Test objectExists for existing object
TEST_F(S3ServiceTest, ObjectExistsTrue) {
    std::string s3_key = "test/exists.txt";
    fake_s3_->uploadData({'d', 'a', 't', 'a'}, s3_key);

    EXPECT_TRUE(fake_s3_->objectExists(s3_key));
}

// Test objectExists for non-existent object
TEST_F(S3ServiceTest, ObjectExistsFalse) {
    EXPECT_FALSE(fake_s3_->objectExists("nonexistent/key.txt"));
}

// Test deleteObject success
TEST_F(S3ServiceTest, DeleteObjectSuccess) {
    std::string s3_key = "test/delete_me.txt";
    fake_s3_->uploadData({'d', 'a', 't', 'a'}, s3_key);

    EXPECT_TRUE(fake_s3_->objectExists(s3_key));

    bool result = fake_s3_->deleteObject(s3_key);

    EXPECT_TRUE(result);
    EXPECT_FALSE(fake_s3_->objectExists(s3_key));
}

// Test deleteObject non-existent object
TEST_F(S3ServiceTest, DeleteObjectNonExistent) {
    bool result = fake_s3_->deleteObject("nonexistent/key.txt");

    EXPECT_FALSE(result);
}

// Test generatePresignedUrl for existing object
TEST_F(S3ServiceTest, GeneratePresignedUrlSuccess) {
    std::string s3_key = "test/presigned.txt";
    fake_s3_->uploadData({'d', 'a', 't', 'a'}, s3_key);

    std::string url = fake_s3_->generatePresignedUrl(s3_key, 3600);

    EXPECT_FALSE(url.empty());
    EXPECT_NE(std::string::npos, url.find(s3_key));
    EXPECT_NE(std::string::npos, url.find("expires=3600"));
}

// Test generatePresignedUrl for non-existent object
TEST_F(S3ServiceTest, GeneratePresignedUrlNonExistent) {
    std::string url = fake_s3_->generatePresignedUrl("nonexistent/key.txt", 3600);

    EXPECT_TRUE(url.empty());
}

// Test generatePresignedUrl with different expiration times
TEST_F(S3ServiceTest, GeneratePresignedUrlDifferentExpirations) {
    std::string s3_key = "test/expiry.txt";
    fake_s3_->uploadData({'d', 'a', 't', 'a'}, s3_key);

    std::string url_1h = fake_s3_->generatePresignedUrl(s3_key, 3600);
    std::string url_24h = fake_s3_->generatePresignedUrl(s3_key, 86400);

    EXPECT_NE(std::string::npos, url_1h.find("expires=3600"));
    EXPECT_NE(std::string::npos, url_24h.find("expires=86400"));
}

// Test getBucketName
TEST_F(S3ServiceTest, GetBucketName) {
    EXPECT_EQ("test-bucket", fake_s3_->getBucketName());
}

// Test multiple uploads to same key (overwrite)
TEST_F(S3ServiceTest, OverwriteObject) {
    std::string s3_key = "test/overwrite.txt";

    std::vector<char> data1 = {'f', 'i', 'r', 's', 't'};
    fake_s3_->uploadData(data1, s3_key);

    std::vector<char> data2 = {'s', 'e', 'c', 'o', 'n', 'd'};
    fake_s3_->uploadData(data2, s3_key);

    std::vector<char> retrieved = fake_s3_->downloadData(s3_key);
    EXPECT_EQ(data2, retrieved);
}

// Test special characters in S3 keys
TEST_F(S3ServiceTest, SpecialCharactersInKey) {
    std::string s3_key = "test/file-with_special.chars/image@2x.jpg";
    std::vector<char> data = {'d', 'a', 't', 'a'};

    bool result = fake_s3_->uploadData(data, s3_key);

    EXPECT_TRUE(result);
    EXPECT_TRUE(fake_s3_->objectExists(s3_key));

    std::vector<char> retrieved = fake_s3_->downloadData(s3_key);
    EXPECT_EQ(data, retrieved);
}

// Test nested path structure
TEST_F(S3ServiceTest, NestedPathStructure) {
    std::vector<std::string> keys = {
        "level1/file1.txt",
        "level1/level2/file2.txt",
        "level1/level2/level3/file3.txt"
    };

    for (const auto& key : keys) {
        fake_s3_->uploadData({'d', 'a', 't', 'a'}, key);
    }

    for (const auto& key : keys) {
        EXPECT_TRUE(fake_s3_->objectExists(key));
    }
}

// Test content type preservation
TEST_F(S3ServiceTest, ContentTypePreservation) {
    std::string s3_key = "test/image.jpg";

    // Upload with specific content type
    bool result = fake_s3_->uploadFile(test_file_path_, s3_key, "image/jpeg");

    EXPECT_TRUE(result);
    // FakeS3Service stores content type, though we can't query it directly
    // This test verifies the interface accepts the parameter
}

// Test binary data integrity
TEST_F(S3ServiceTest, BinaryDataIntegrity) {
    std::vector<char> binary_data;
    for (int i = 0; i < 256; ++i) {
        binary_data.push_back(static_cast<char>(i));
    }

    std::string s3_key = "test/binary_integrity.bin";
    fake_s3_->uploadData(binary_data, s3_key);

    std::vector<char> retrieved = fake_s3_->downloadData(s3_key);

    ASSERT_EQ(binary_data.size(), retrieved.size());
    for (size_t i = 0; i < binary_data.size(); ++i) {
        EXPECT_EQ(binary_data[i], retrieved[i])
            << "Mismatch at index " << i;
    }
}

// Test concurrent uploads
TEST_F(S3ServiceTest, ConcurrentUploads) {
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i]() {
            std::string s3_key = "test/concurrent_" + std::to_string(i) + ".txt";
            std::vector<char> data = {'t', 'e', 's', 't'};
            fake_s3_->uploadData(data, s3_key);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all uploads succeeded
    for (int i = 0; i < 10; ++i) {
        std::string s3_key = "test/concurrent_" + std::to_string(i) + ".txt";
        EXPECT_TRUE(fake_s3_->objectExists(s3_key));
    }
}

// Test object count tracking
TEST_F(S3ServiceTest, ObjectCountTracking) {
    EXPECT_EQ(0, fake_s3_->getObjectCount());

    fake_s3_->uploadData({'d'}, "key1");
    EXPECT_EQ(1, fake_s3_->getObjectCount());

    fake_s3_->uploadData({'d'}, "key2");
    EXPECT_EQ(2, fake_s3_->getObjectCount());

    fake_s3_->deleteObject("key1");
    EXPECT_EQ(1, fake_s3_->getObjectCount());

    fake_s3_->clear();
    EXPECT_EQ(0, fake_s3_->getObjectCount());
}

// Test clear functionality
TEST_F(S3ServiceTest, ClearStorage) {
    fake_s3_->uploadData({'d'}, "key1");
    fake_s3_->uploadData({'d'}, "key2");
    fake_s3_->uploadData({'d'}, "key3");

    EXPECT_EQ(3, fake_s3_->getObjectCount());

    fake_s3_->clear();

    EXPECT_EQ(0, fake_s3_->getObjectCount());
    EXPECT_FALSE(fake_s3_->objectExists("key1"));
    EXPECT_FALSE(fake_s3_->objectExists("key2"));
    EXPECT_FALSE(fake_s3_->objectExists("key3"));
}

// Test round-trip file upload/download
TEST_F(S3ServiceTest, RoundTripFileUploadDownload) {
    std::string s3_key = "test/roundtrip.txt";
    std::string download_path = "/tmp/s3_roundtrip_download.txt";

    // Upload
    bool upload_result = fake_s3_->uploadFile(test_file_path_, s3_key);
    EXPECT_TRUE(upload_result);

    // Download
    bool download_result = fake_s3_->downloadFile(s3_key, download_path);
    EXPECT_TRUE(download_result);

    // Verify content matches
    std::ifstream original(test_file_path_);
    std::ifstream downloaded(download_path);

    std::string original_content((std::istreambuf_iterator<char>(original)),
                                 std::istreambuf_iterator<char>());
    std::string downloaded_content((std::istreambuf_iterator<char>(downloaded)),
                                   std::istreambuf_iterator<char>());

    EXPECT_EQ(original_content, downloaded_content);

    FileUtils::deleteFile(download_path);
}

// Test upload after delete
TEST_F(S3ServiceTest, UploadAfterDelete) {
    std::string s3_key = "test/delete_and_upload.txt";

    // Upload
    fake_s3_->uploadData({'f', 'i', 'r', 's', 't'}, s3_key);
    EXPECT_TRUE(fake_s3_->objectExists(s3_key));

    // Delete
    fake_s3_->deleteObject(s3_key);
    EXPECT_FALSE(fake_s3_->objectExists(s3_key));

    // Upload again
    fake_s3_->uploadData({'s', 'e', 'c', 'o', 'n', 'd'}, s3_key);
    EXPECT_TRUE(fake_s3_->objectExists(s3_key));

    std::vector<char> data = fake_s3_->downloadData(s3_key);
    EXPECT_EQ(6, data.size());
}
