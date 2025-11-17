#ifndef GARA_LOCAL_FILE_SERVICE_H
#define GARA_LOCAL_FILE_SERVICE_H

#include "../interfaces/file_service_interface.h"
#include <filesystem>

namespace gara {

/**
 * @brief Local filesystem implementation of file storage service
 *
 * This service stores files on the local filesystem instead of S3.
 * It maintains the same directory structure as S3 for compatibility.
 */
class LocalFileService : public FileServiceInterface {
public:
    /**
     * @brief Constructor
     * @param storage_path Root directory for file storage (replaces bucket_name)
     */
    explicit LocalFileService(const std::string& storage_path);

    virtual ~LocalFileService() = default;

    // Override S3Service methods
    bool uploadFile(const std::string& local_path, const std::string& key,
                   const std::string& content_type = "application/octet-stream") override;

    bool uploadData(const std::vector<char>& data, const std::string& key,
                   const std::string& content_type = "application/octet-stream") override;

    bool downloadFile(const std::string& key, const std::string& local_path) override;

    std::vector<char> downloadData(const std::string& key) override;

    bool objectExists(const std::string& key) override;

    bool deleteObject(const std::string& key) override;

    std::string generatePresignedUrl(const std::string& key, int expiration_seconds = 3600) override;

    const std::string& getBucketName() const override { return storage_path_; }

private:
    std::string storage_path_;

    /**
     * @brief Get the full filesystem path for a key
     */
    std::filesystem::path getFilePath(const std::string& key) const;

    /**
     * @brief Ensure directory exists for a file path
     */
    bool ensureDirectoryExists(const std::filesystem::path& file_path);
};

} // namespace gara

#endif // GARA_LOCAL_FILE_SERVICE_H
