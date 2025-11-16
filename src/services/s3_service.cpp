#include "s3_service.h"
#include "../utils/logger.h"
#include "../utils/metrics.h"
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
        gara::Logger::log_structured(spdlog::level::err, "Failed to open file for S3 upload", {
            {"local_path", local_path},
            {"s3_key", s3_key},
            {"operation", "s3_upload"}
        });
        METRICS_COUNT("S3Errors", 1.0, "Count", {{"error_type", "file_not_found"}});
        return false;
    }

    request.SetBody(input_data);

    // Start timing S3 upload
    auto timer = gara::Metrics::get()->start_timer("S3UploadDuration", {{"operation", "put_object"}});

    auto outcome = s3_client_->PutObject(request);

    if (!outcome.IsSuccess()) {
        gara::Logger::log_structured(spdlog::level::err, "S3 upload failed", {
            {"s3_key", s3_key},
            {"bucket", bucket_name_},
            {"error_code", outcome.GetError().GetExceptionName()},
            {"error_message", outcome.GetError().GetMessage()},
            {"operation", "s3_upload"}
        });
        METRICS_COUNT("S3Errors", 1.0, "Count", {{"error_type", "upload_failed"}});
        return false;
    }

    // Track successful upload
    METRICS_COUNT("S3Operations", 1.0, "Count", {{"operation", "upload"}, {"status", "success"}});

    return true;
}

bool S3Service::uploadData(const std::vector<char>& data, const std::string& s3_key,
                          const std::string& content_type) {
    auto timer = gara::Metrics::get()->start_timer("S3UploadDuration", {{"operation", "put_data"}});

    Aws::S3::Model::PutObjectRequest request;
    request.SetBucket(bucket_name_);
    request.SetKey(s3_key);
    request.SetContentType(content_type);

    auto input_data = Aws::MakeShared<Aws::StringStream>("PutObjectInputStream");
    input_data->write(data.data(), data.size());

    request.SetBody(input_data);

    auto outcome = s3_client_->PutObject(request);

    if (!outcome.IsSuccess()) {
        gara::Logger::log_structured(spdlog::level::err, "S3 upload data failed", {
            {"s3_key", s3_key},
            {"bucket", bucket_name_},
            {"size_bytes", data.size()},
            {"error_code", outcome.GetError().GetExceptionName()},
            {"error_message", outcome.GetError().GetMessage()},
            {"operation", "upload_data"}
        });
        METRICS_COUNT("S3Errors", 1.0, "Count", {{"error_type", "upload_data_failed"}});
        return false;
    }

    METRICS_COUNT("S3Operations", 1.0, "Count", {{"operation", "upload_data"}, {"status", "success"}});
    return true;
}

bool S3Service::downloadFile(const std::string& s3_key, const std::string& local_path) {
    auto timer = gara::Metrics::get()->start_timer("S3DownloadDuration", {{"operation", "get_object"}});

    Aws::S3::Model::GetObjectRequest request;
    request.SetBucket(bucket_name_);
    request.SetKey(s3_key);

    auto outcome = s3_client_->GetObject(request);

    if (!outcome.IsSuccess()) {
        gara::Logger::log_structured(spdlog::level::err, "S3 download failed", {
            {"s3_key", s3_key},
            {"bucket", bucket_name_},
            {"local_path", local_path},
            {"error_code", outcome.GetError().GetExceptionName()},
            {"error_message", outcome.GetError().GetMessage()},
            {"operation", "download_file"}
        });
        METRICS_COUNT("S3Errors", 1.0, "Count", {{"error_type", "download_failed"}});
        return false;
    }

    auto& retrieved_file = outcome.GetResultWithOwnership().GetBody();
    std::ofstream output_file(local_path, std::ios::binary);

    if (!output_file.is_open()) {
        gara::Logger::log_structured(spdlog::level::err, "Failed to open output file for S3 download", {
            {"local_path", local_path},
            {"s3_key", s3_key},
            {"operation", "download_file"}
        });
        METRICS_COUNT("S3Errors", 1.0, "Count", {{"error_type", "file_open_failed"}});
        return false;
    }

    output_file << retrieved_file.rdbuf();
    METRICS_COUNT("S3Operations", 1.0, "Count", {{"operation", "download_file"}, {"status", "success"}});
    return true;
}

std::vector<char> S3Service::downloadData(const std::string& s3_key) {
    auto timer = gara::Metrics::get()->start_timer("S3DownloadDuration", {{"operation", "get_data"}});

    Aws::S3::Model::GetObjectRequest request;
    request.SetBucket(bucket_name_);
    request.SetKey(s3_key);

    auto outcome = s3_client_->GetObject(request);

    if (!outcome.IsSuccess()) {
        gara::Logger::log_structured(spdlog::level::err, "S3 download data failed", {
            {"s3_key", s3_key},
            {"bucket", bucket_name_},
            {"error_code", outcome.GetError().GetExceptionName()},
            {"error_message", outcome.GetError().GetMessage()},
            {"operation", "download_data"}
        });
        METRICS_COUNT("S3Errors", 1.0, "Count", {{"error_type", "download_data_failed"}});
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

    METRICS_COUNT("S3Operations", 1.0, "Count", {{"operation", "download_data"}, {"status", "success"}});
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
    auto timer = gara::Metrics::get()->start_timer("S3DeleteDuration", {{"operation", "delete_object"}});

    Aws::S3::Model::DeleteObjectRequest request;
    request.SetBucket(bucket_name_);
    request.SetKey(s3_key);

    auto outcome = s3_client_->DeleteObject(request);

    if (!outcome.IsSuccess()) {
        gara::Logger::log_structured(spdlog::level::err, "S3 delete failed", {
            {"s3_key", s3_key},
            {"bucket", bucket_name_},
            {"error_code", outcome.GetError().GetExceptionName()},
            {"error_message", outcome.GetError().GetMessage()},
            {"operation", "delete_object"}
        });
        METRICS_COUNT("S3Errors", 1.0, "Count", {{"error_type", "delete_failed"}});
        return false;
    }

    METRICS_COUNT("S3Operations", 1.0, "Count", {{"operation", "delete"}, {"status", "success"}});
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
