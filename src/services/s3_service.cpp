#include "s3_service.h"
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/HeadObjectRequest.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/utils/memory/stl/AWSStringStream.h>
#include <fstream>
#include <iostream>

namespace gara {

S3Service::S3Service(const std::string& bucket_name, const std::string& region)
    : bucket_name_(bucket_name), region_(region) {
    initializeClient();
}

void S3Service::initializeClient() {
    Aws::Client::ClientConfiguration config;
    config.region = region_;

    s3_client_ = std::make_shared<Aws::S3::S3Client>(config);
}

bool S3Service::uploadFile(const std::string& local_path, const std::string& s3_key,
                          const std::string& content_type) {
    Aws::S3::Model::PutObjectRequest request;
    request.SetBucket(bucket_name_);
    request.SetKey(s3_key);
    request.SetContentType(content_type);

    auto input_data = Aws::MakeShared<Aws::FStream>("PutObjectInputStream",
                                                     local_path.c_str(),
                                                     std::ios_base::in | std::ios_base::binary);

    if (!input_data->is_open()) {
        std::cerr << "Failed to open file: " << local_path << std::endl;
        return false;
    }

    request.SetBody(input_data);

    auto outcome = s3_client_->PutObject(request);

    if (!outcome.IsSuccess()) {
        std::cerr << "S3 Upload Error: " << outcome.GetError().GetMessage() << std::endl;
        return false;
    }

    return true;
}

bool S3Service::uploadData(const std::vector<char>& data, const std::string& s3_key,
                          const std::string& content_type) {
    Aws::S3::Model::PutObjectRequest request;
    request.SetBucket(bucket_name_);
    request.SetKey(s3_key);
    request.SetContentType(content_type);

    auto input_data = Aws::MakeShared<Aws::StringStream>("PutObjectInputStream");
    input_data->write(data.data(), data.size());

    request.SetBody(input_data);

    auto outcome = s3_client_->PutObject(request);

    if (!outcome.IsSuccess()) {
        std::cerr << "S3 Upload Error: " << outcome.GetError().GetMessage() << std::endl;
        return false;
    }

    return true;
}

bool S3Service::downloadFile(const std::string& s3_key, const std::string& local_path) {
    Aws::S3::Model::GetObjectRequest request;
    request.SetBucket(bucket_name_);
    request.SetKey(s3_key);

    auto outcome = s3_client_->GetObject(request);

    if (!outcome.IsSuccess()) {
        std::cerr << "S3 Download Error: " << outcome.GetError().GetMessage() << std::endl;
        return false;
    }

    auto& retrieved_file = outcome.GetResultWithOwnership().GetBody();
    std::ofstream output_file(local_path, std::ios::binary);

    if (!output_file.is_open()) {
        std::cerr << "Failed to open output file: " << local_path << std::endl;
        return false;
    }

    output_file << retrieved_file.rdbuf();
    return true;
}

std::vector<char> S3Service::downloadData(const std::string& s3_key) {
    Aws::S3::Model::GetObjectRequest request;
    request.SetBucket(bucket_name_);
    request.SetKey(s3_key);

    auto outcome = s3_client_->GetObject(request);

    if (!outcome.IsSuccess()) {
        std::cerr << "S3 Download Error: " << outcome.GetError().GetMessage() << std::endl;
        return {};
    }

    auto& retrieved_file = outcome.GetResultWithOwnership().GetBody();
    std::vector<char> data;

    // Get the size
    retrieved_file.seekg(0, std::ios::end);
    size_t size = retrieved_file.tellg();
    retrieved_file.seekg(0, std::ios::beg);

    data.resize(size);
    retrieved_file.read(data.data(), size);

    return data;
}

bool S3Service::objectExists(const std::string& s3_key) {
    Aws::S3::Model::HeadObjectRequest request;
    request.SetBucket(bucket_name_);
    request.SetKey(s3_key);

    auto outcome = s3_client_->HeadObject(request);
    return outcome.IsSuccess();
}

bool S3Service::deleteObject(const std::string& s3_key) {
    Aws::S3::Model::DeleteObjectRequest request;
    request.SetBucket(bucket_name_);
    request.SetKey(s3_key);

    auto outcome = s3_client_->DeleteObject(request);

    if (!outcome.IsSuccess()) {
        std::cerr << "S3 Delete Error: " << outcome.GetError().GetMessage() << std::endl;
        return false;
    }

    return true;
}

std::string S3Service::generatePresignedUrl(const std::string& s3_key, int expiration_seconds) {
    // For presigned URLs, we need to use the request object
    Aws::S3::Model::GetObjectRequest request;
    request.SetBucket(bucket_name_);
    request.SetKey(s3_key);

    auto url = s3_client_->GeneratePresignedUrl(bucket_name_, s3_key, Aws::Http::HttpMethod::HTTP_GET, expiration_seconds);

    return url;
}

} // namespace gara
