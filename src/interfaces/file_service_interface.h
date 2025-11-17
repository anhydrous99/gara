#ifndef GARA_FILE_SERVICE_INTERFACE_H
#define GARA_FILE_SERVICE_INTERFACE_H

#include <string>
#include <vector>

namespace gara {

/**
 * @brief Abstract interface for file storage services
 *
 * This interface abstracts file storage operations, allowing for
 * different implementations (S3, local filesystem, etc.)
 */
class FileServiceInterface {
public:
    virtual ~FileServiceInterface() = default;

    // Upload file from local path
    virtual bool uploadFile(const std::string& local_path, const std::string& key,
                           const std::string& content_type = "application/octet-stream") = 0;

    // Upload binary data
    virtual bool uploadData(const std::vector<char>& data, const std::string& key,
                           const std::string& content_type = "application/octet-stream") = 0;

    // Download file to local path
    virtual bool downloadFile(const std::string& key, const std::string& local_path) = 0;

    // Download file to memory
    virtual std::vector<char> downloadData(const std::string& key) = 0;

    // Check if object exists
    virtual bool objectExists(const std::string& key) = 0;

    // Delete object
    virtual bool deleteObject(const std::string& key) = 0;

    // Generate presigned URL (or equivalent access path)
    virtual std::string generatePresignedUrl(const std::string& key, int expiration_seconds = 3600) = 0;

    // Get storage name (bucket name or storage path)
    virtual const std::string& getBucketName() const = 0;
};

} // namespace gara

#endif // GARA_FILE_SERVICE_INTERFACE_H
