#include "image_controller.h"
#include "../utils/file_utils.h"
#include "../models/image_metadata.h"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace gara {

ImageController::ImageController(std::shared_ptr<S3Service> s3_service,
                                std::shared_ptr<ImageProcessor> image_processor,
                                std::shared_ptr<CacheManager> cache_manager)
    : s3_service_(s3_service),
      image_processor_(image_processor),
      cache_manager_(cache_manager) {
}

void ImageController::registerRoutes(crow::SimpleApp& app) {
    // Upload image
    CROW_ROUTE(app, "/api/images/upload").methods("POST"_method)
    ([this](const crow::request& req) {
        return handleUpload(req);
    });

    // Get/transform image
    CROW_ROUTE(app, "/api/images/<string>")
    ([this](const crow::request& req, const std::string& image_id) {
        return handleGetImage(req, image_id);
    });

    // Health check
    CROW_ROUTE(app, "/api/images/health")
    ([this](const crow::request& req) {
        return handleHealthCheck(req);
    });
}

crow::response ImageController::handleUpload(const crow::request& req) {
    try {
        std::vector<char> file_data;
        std::string filename;

        // Extract uploaded file from multipart form data
        if (!extractUploadedFile(req, file_data, filename)) {
            json error_response = {
                {"error", "Failed to extract file from request"},
                {"message", "Please upload a valid image file"}
            };
            crow::response resp(400, error_response.dump());
            resp.add_header("Content-Type", "application/json");
            return resp;
        }

        // Validate file size (100MB max)
        const size_t max_size = 100 * 1024 * 1024;
        if (file_data.size() > max_size) {
            json error_response = {
                {"error", "File too large"},
                {"message", "Maximum file size is 100MB"}
            };
            crow::response resp(413, error_response.dump());
            resp.add_header("Content-Type", "application/json");
            return resp;
        }

        // Process upload
        std::string image_id = processUpload(file_data, filename);

        if (image_id.empty()) {
            json error_response = {
                {"error", "Upload failed"},
                {"message", "Failed to process and upload image"}
            };
            crow::response resp(500, error_response.dump());
            resp.add_header("Content-Type", "application/json");
            return resp;
        }

        // Generate response
        json response = {
            {"image_id", image_id},
            {"original_filename", filename},
            {"size", file_data.size()},
            {"upload_timestamp", std::time(nullptr)},
            {"message", "Image uploaded successfully"}
        };

        crow::response resp(201, response.dump());
        resp.add_header("Content-Type", "application/json");
        return resp;

    } catch (const std::exception& e) {
        std::cerr << "Upload error: " << e.what() << std::endl;  // Log to server
        json error_response = {
            {"error", "Internal server error"},
            {"message", "An error occurred while processing your request"}  // Generic message to user
        };
        crow::response resp(500, error_response.dump());
        resp.add_header("Content-Type", "application/json");
        return resp;
    }
}

crow::response ImageController::handleGetImage(const crow::request& req,
                                              const std::string& image_id) {
    try {
        // Parse transformation parameters
        TransformRequest transform_req = parseTransformParams(req, image_id);

        // Get or create transformed image
        std::string s3_key = getOrCreateTransformed(transform_req);

        if (s3_key.empty()) {
            json error_response = {
                {"error", "Image not found or transformation failed"},
                {"image_id", image_id}
            };
            crow::response resp(404, error_response.dump());
            resp.add_header("Content-Type", "application/json");
            return resp;
        }

        // Generate presigned URL
        std::string presigned_url = s3_service_->generatePresignedUrl(s3_key, 3600);

        json response = {
            {"image_id", image_id},
            {"format", transform_req.target_format},
            {"width", transform_req.width},
            {"height", transform_req.height},
            {"url", presigned_url},
            {"expires_in", 3600}
        };

        crow::response resp(200, response.dump());
        resp.add_header("Content-Type", "application/json");
        return resp;

    } catch (const std::exception& e) {
        std::cerr << "Get image error: " << e.what() << std::endl;  // Log to server
        json error_response = {
            {"error", "Internal server error"},
            {"message", "An error occurred while processing your request"}  // Generic message to user
        };
        crow::response resp(500, error_response.dump());
        resp.add_header("Content-Type", "application/json");
        return resp;
    }
}

crow::response ImageController::handleHealthCheck(const crow::request& req) {
    json response = {
        {"status", "healthy"},
        {"service", "image-service"},
        {"s3_bucket", s3_service_->getBucketName()}
    };
    crow::response resp(200, response.dump());
    resp.add_header("Content-Type", "application/json");
    return resp;
}

bool ImageController::extractUploadedFile(const crow::request& req,
                                         std::vector<char>& file_data,
                                         std::string& filename) {
    // Parse multipart form data
    crow::multipart::message msg(req);

    for (const auto& part : msg.parts) {
        auto it = part.headers.find("Content-Disposition");
        if (it != part.headers.end()) {
            const auto& disposition = it->second;

            // Check if this part contains a file
            auto filename_it = disposition.params.find("filename");
            if (filename_it != disposition.params.end()) {
                filename = filename_it->second;
                file_data.assign(part.body.begin(), part.body.end());

                // Validate file extension
                std::string ext = utils::FileUtils::getFileExtension(filename);
                if (!utils::FileUtils::isValidImageFormat(ext)) {
                    return false;
                }

                return true;
            }
        }
    }

    return false;
}

TransformRequest ImageController::parseTransformParams(const crow::request& req,
                                                      const std::string& image_id) {
    TransformRequest transform_req;
    transform_req.image_id = image_id;

    // Parse query parameters
    auto format_param = req.url_params.get("format");
    auto width_param = req.url_params.get("width");
    auto height_param = req.url_params.get("height");

    // Set format (default: jpeg)
    transform_req.target_format = format_param ? std::string(format_param) : "jpeg";

    // Set dimensions (default: 0 for original)
    const int MAX_DIMENSION = 10000;  // Reasonable maximum to prevent resource exhaustion
    try {
        transform_req.width = width_param ? std::stoi(width_param) : 0;
        transform_req.height = height_param ? std::stoi(height_param) : 0;

        // Validate dimensions
        if (transform_req.width < 0 || transform_req.height < 0) {
            transform_req.width = 0;
            transform_req.height = 0;
        }

        // Cap at maximum to prevent resource exhaustion
        if (transform_req.width > MAX_DIMENSION) {
            transform_req.width = MAX_DIMENSION;
        }
        if (transform_req.height > MAX_DIMENSION) {
            transform_req.height = MAX_DIMENSION;
        }
    } catch (const std::exception& e) {
        // Invalid parameters, use defaults (original dimensions)
        transform_req.width = 0;
        transform_req.height = 0;
    }

    return transform_req;
}

std::string ImageController::processUpload(const std::vector<char>& file_data,
                                          const std::string& filename) {
    // Calculate hash (image ID)
    std::string image_id = utils::FileUtils::calculateSHA256(file_data);

    // Get file extension
    std::string extension = utils::FileUtils::getFileExtension(filename);

    // Generate S3 key for raw image
    std::string s3_key = ImageMetadata::generateRawKey(image_id, extension);

    // Check if already uploaded (deduplication)
    if (s3_service_->objectExists(s3_key)) {
        std::cout << "Image already exists: " << image_id << std::endl;
        return image_id;
    }

    // Create temporary file
    utils::TempFile temp_file("upload_");
    if (!temp_file.write(file_data)) {
        std::cerr << "Failed to write temporary file" << std::endl;
        return "";
    }

    // Validate image
    if (!image_processor_->isValidImage(temp_file.getPath())) {
        std::cerr << "Invalid image file" << std::endl;
        return "";
    }

    // Upload to S3
    std::string content_type = utils::FileUtils::getMimeType(extension);
    if (!s3_service_->uploadFile(temp_file.getPath(), s3_key, content_type)) {
        std::cerr << "Failed to upload to S3" << std::endl;
        return "";
    }

    std::cout << "Uploaded image: " << image_id << " (" << s3_key << ")" << std::endl;
    return image_id;
}

std::string ImageController::getOrCreateTransformed(const TransformRequest& request) {
    // Check cache first
    std::string cached_key = cache_manager_->getCachedImage(request);
    if (!cached_key.empty()) {
        std::cout << "Cache hit for: " << cached_key << std::endl;
        return cached_key;
    }

    std::cout << "Cache miss - creating transformation" << std::endl;

    // Download raw image
    std::string raw_key = "raw/" + request.image_id + ".*";

    // We need to find the actual raw image (we stored it with original extension)
    // For now, try common extensions
    std::vector<std::string> extensions = {"jpg", "jpeg", "png", "gif", "bmp", "tiff", "webp"};
    std::string found_raw_key;

    for (const auto& ext : extensions) {
        std::string test_key = ImageMetadata::generateRawKey(request.image_id, ext);
        if (s3_service_->objectExists(test_key)) {
            found_raw_key = test_key;
            break;
        }
    }

    if (found_raw_key.empty()) {
        std::cerr << "Raw image not found: " << request.image_id << std::endl;
        return "";
    }

    // Download raw image to temp file
    utils::TempFile raw_temp("raw_");
    if (!s3_service_->downloadFile(found_raw_key, raw_temp.getPath())) {
        std::cerr << "Failed to download raw image" << std::endl;
        return "";
    }

    // Create temp file for transformed image
    utils::TempFile transformed_temp("transformed_");

    // Transform image
    bool success = image_processor_->transform(
        raw_temp.getPath(),
        transformed_temp.getPath(),
        request.target_format,
        request.width,
        request.height
    );

    if (!success) {
        std::cerr << "Image transformation failed" << std::endl;
        return "";
    }

    // Store in cache (S3)
    if (!cache_manager_->storeInCache(request, transformed_temp.getPath())) {
        std::cerr << "Failed to cache transformed image" << std::endl;
        return "";
    }

    // Return the S3 key
    return request.getCacheKey();
}

} // namespace gara
