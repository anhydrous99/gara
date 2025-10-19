#ifndef GARA_S3_SERVICE_H
#define GARA_S3_SERVICE_H

#include <string>
#include <vector>
#include <memory>
#include <aws/s3/S3Client.h>
#include <aws/core/Aws.h>

namespace gara {

class S3Service {
public:
    S3Service(const std::string& bucket_name, const std::string& region = "us-east-1");
    virtual ~S3Service() = default;

protected:
    // Constructor for derived classes that don't need AWS SDK initialization
    S3Service(const std::string& bucket_name, const std::string& region, bool skip_init)
        : bucket_name_(bucket_name), region_(region) {
        if (!skip_init) {
            initializeClient();
        }
    }

public:

    // Upload file to S3
    virtual bool uploadFile(const std::string& local_path, const std::string& s3_key,
                   const std::string& content_type = "application/octet-stream");

    // Upload binary data to S3
    virtual bool uploadData(const std::vector<char>& data, const std::string& s3_key,
                   const std::string& content_type = "application/octet-stream");

    // Download file from S3 to local path
    virtual bool downloadFile(const std::string& s3_key, const std::string& local_path);

    // Download file from S3 to memory
    virtual std::vector<char> downloadData(const std::string& s3_key);

    // Check if object exists in S3
    virtual bool objectExists(const std::string& s3_key);

    // Delete object from S3
    virtual bool deleteObject(const std::string& s3_key);

    // Generate presigned URL (valid for specified duration in seconds)
    virtual std::string generatePresignedUrl(const std::string& s3_key, int expiration_seconds = 3600);

    // Get bucket name
    virtual const std::string& getBucketName() const { return bucket_name_; }

protected:
    std::string bucket_name_;
    std::string region_;

    // Initialize S3 client
    void initializeClient();

private:
    std::shared_ptr<Aws::S3::S3Client> s3_client_;
};

} // namespace gara

#endif // GARA_S3_SERVICE_H
