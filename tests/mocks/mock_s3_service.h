#ifndef GARA_MOCK_S3_SERVICE_H
#define GARA_MOCK_S3_SERVICE_H

#include <gmock/gmock.h>
#include "services/s3_service.h"
#include <map>
#include <vector>
#include <fstream>

namespace gara {
namespace testing {

// Mock S3 Service with in-memory storage for testing
class MockS3Service {
public:
    explicit MockS3Service(const std::string& bucket_name = "test-bucket",
                          const std::string& region = "us-east-1")
        : bucket_name_(bucket_name), region_(region) {}

    // Mock methods
    MOCK_METHOD(bool, uploadFile, (const std::string& local_path,
                                   const std::string& s3_key,
                                   const std::string& content_type), ());

    MOCK_METHOD(bool, uploadData, (const std::vector<char>& data,
                                   const std::string& s3_key,
                                   const std::string& content_type), ());

    MOCK_METHOD(bool, downloadFile, (const std::string& s3_key,
                                     const std::string& local_path), ());

    MOCK_METHOD(std::vector<char>, downloadData, (const std::string& s3_key), ());

    MOCK_METHOD(bool, objectExists, (const std::string& s3_key), ());

    MOCK_METHOD(bool, deleteObject, (const std::string& s3_key), ());

    MOCK_METHOD(std::string, generatePresignedUrl, (const std::string& s3_key,
                                                    int expiration_seconds), ());

    const std::string& getBucketName() const { return bucket_name_; }

private:
    std::string bucket_name_;
    std::string region_;
};

// Fake S3 Service with in-memory storage (for integration tests)
class FakeS3Service : public S3Service {
public:
    explicit FakeS3Service(const std::string& bucket_name = "test-bucket",
                          const std::string& region = "us-east-1")
        : S3Service(bucket_name, region, true) {}  // Skip AWS SDK initialization

    bool uploadFile(const std::string& local_path, const std::string& s3_key,
                   const std::string& content_type = "application/octet-stream") override {
        // Read file and store in memory
        std::ifstream file(local_path, std::ios::binary);
        if (!file) return false;

        std::vector<char> data((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());

        storage_[s3_key] = data;
        content_types_[s3_key] = content_type;
        return true;
    }

    bool uploadData(const std::vector<char>& data, const std::string& s3_key,
                   const std::string& content_type = "application/octet-stream") override {
        storage_[s3_key] = data;
        content_types_[s3_key] = content_type;
        return true;
    }

    bool downloadFile(const std::string& s3_key, const std::string& local_path) override {
        if (storage_.find(s3_key) == storage_.end()) {
            return false;
        }

        std::ofstream file(local_path, std::ios::binary);
        if (!file) return false;

        file.write(storage_[s3_key].data(), storage_[s3_key].size());
        return true;
    }

    std::vector<char> downloadData(const std::string& s3_key) override {
        if (storage_.find(s3_key) == storage_.end()) {
            return {};
        }
        return storage_[s3_key];
    }

    bool objectExists(const std::string& s3_key) override {
        return storage_.find(s3_key) != storage_.end();
    }

    bool deleteObject(const std::string& s3_key) override {
        if (storage_.find(s3_key) == storage_.end()) {
            return false;
        }
        storage_.erase(s3_key);
        content_types_.erase(s3_key);
        return true;
    }

    std::string generatePresignedUrl(const std::string& s3_key, int expiration_seconds = 3600) override {
        if (storage_.find(s3_key) == storage_.end()) {
            return "";
        }
        return "https://fake-s3.amazonaws.com/" + getBucketName() + "/" + s3_key +
               "?expires=" + std::to_string(expiration_seconds);
    }

    // Test helpers
    void clear() {
        storage_.clear();
        content_types_.clear();
    }

    size_t getObjectCount() const {
        return storage_.size();
    }

private:
    std::map<std::string, std::vector<char>> storage_;
    std::map<std::string, std::string> content_types_;
};

} // namespace testing
} // namespace gara

#endif // GARA_MOCK_S3_SERVICE_H
