#ifndef GARA_TESTS_MOCKS_FAKE_FILE_SERVICE_H
#define GARA_TESTS_MOCKS_FAKE_FILE_SERVICE_H

#include "../../src/interfaces/file_service_interface.h"
#include <map>
#include <vector>
#include <fstream>
#include <mutex>

namespace gara {
namespace testing {

/**
 * @brief In-memory fake file service for testing
 *
 * Simulates file storage without actual file system or cloud storage
 */
class FakeFileService : public FileServiceInterface {
public:
    explicit FakeFileService(const std::string& bucket_name = "test-bucket")
        : bucket_name_(bucket_name) {}

    bool uploadFile(const std::string& local_path, const std::string& key,
                   const std::string& content_type = "application/octet-stream") override {
        std::lock_guard<std::mutex> lock(mutex_);

        // Read file and store in memory
        std::ifstream file(local_path, std::ios::binary);
        if (!file) return false;

        std::vector<char> data((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());

        storage_[key] = data;
        content_types_[key] = content_type;
        return true;
    }

    bool uploadData(const std::vector<char>& data, const std::string& key,
                   const std::string& content_type = "application/octet-stream") override {
        std::lock_guard<std::mutex> lock(mutex_);
        storage_[key] = data;
        content_types_[key] = content_type;
        return true;
    }

    bool downloadFile(const std::string& key, const std::string& local_path) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (storage_.find(key) == storage_.end()) {
            return false;
        }

        std::ofstream file(local_path, std::ios::binary);
        if (!file) return false;

        file.write(storage_[key].data(), storage_[key].size());
        return true;
    }

    std::vector<char> downloadData(const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (storage_.find(key) == storage_.end()) {
            return {};
        }
        return storage_[key];
    }

    bool objectExists(const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return storage_.find(key) != storage_.end();
    }

    bool deleteObject(const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (storage_.find(key) == storage_.end()) {
            return false;
        }
        storage_.erase(key);
        content_types_.erase(key);
        return true;
    }

    std::string generatePresignedUrl(const std::string& key, int expiration_seconds = 3600) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (storage_.find(key) == storage_.end()) {
            return "";
        }
        return "https://fake-s3.amazonaws.com/" + bucket_name_ + "/" + key +
               "?expires=" + std::to_string(expiration_seconds);
    }

    const std::string& getBucketName() const override {
        return bucket_name_;
    }

    // Test helpers
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        storage_.clear();
        content_types_.clear();
    }

    size_t getObjectCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return storage_.size();
    }

private:
    std::string bucket_name_;
    mutable std::mutex mutex_;
    std::map<std::string, std::vector<char>> storage_;
    std::map<std::string, std::string> content_types_;
};

} // namespace testing
} // namespace gara

#endif // GARA_TESTS_MOCKS_FAKE_FILE_SERVICE_H
