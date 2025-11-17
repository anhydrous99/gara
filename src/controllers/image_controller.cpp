#include "image_controller.h"
#include "../utils/file_utils.h"
#include "../utils/logger.h"
#include "../utils/metrics.h"
#include "../models/image_metadata.h"
#include "../middleware/auth_middleware.h"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace gara {

ImageController::ImageController(std::shared_ptr<FileServiceInterface> file_service,
                                std::shared_ptr<ImageProcessor> image_processor,
                                std::shared_ptr<CacheManager> cache_manager,
                                std::shared_ptr<ConfigServiceInterface> config_service,
                                std::shared_ptr<WatermarkService> watermark_service,
                                std::shared_ptr<DatabaseClientInterface> db_client)
    : file_service_(file_service),
      image_processor_(image_processor),
      cache_manager_(cache_manager),
      config_service_(config_service),
      watermark_service_(watermark_service),
      db_client_(db_client) {
}

// registerRoutes is now a template method in the header

crow::response ImageController::handleUpload(const crow::request& req) {
    try {
        // Authenticate request using API key
        std::string api_key = config_service_->getApiKey();

        if (!middleware::AuthMiddleware::validateApiKey(req, api_key)) {
            std::string provided_key = middleware::AuthMiddleware::extractApiKey(req);

            crow::response auth_resp = [this, &provided_key]() {
                if (provided_key.empty()) {
                    return middleware::AuthMiddleware::unauthorizedResponse(
                        "Missing X-API-Key header"
                    );
                } else {
                    return middleware::AuthMiddleware::unauthorizedResponse(
                        "Invalid API key"
                    );
                }
            }();
            addCorsHeaders(auth_resp);
            return auth_resp;
        }

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
            addCorsHeaders(resp);
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
            addCorsHeaders(resp);
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
            addCorsHeaders(resp);
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
        addCorsHeaders(resp);
        return resp;

    } catch (const std::exception& e) {
        gara::Logger::log_structured(spdlog::level::err, "Image upload error", {
            {"endpoint", "/api/images/upload"},
            {"error", e.what()}
        });
        METRICS_COUNT("APIRequests", 1.0, "Count", {{"endpoint", "/upload"}, {"status", "error"}});
        json error_response = {
            {"error", "Internal server error"},
            {"message", "An error occurred while processing your request"}  // Generic message to user
        };
        crow::response resp(500, error_response.dump());
        resp.add_header("Content-Type", "application/json");
        addCorsHeaders(resp);
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
            addCorsHeaders(resp);
            return resp;
        }

        // Generate presigned URL
        std::string presigned_url = file_service_->generatePresignedUrl(s3_key, 3600);

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
        addCorsHeaders(resp);
        return resp;

    } catch (const std::exception& e) {
        gara::Logger::log_structured(spdlog::level::err, "Get image error", {
            {"endpoint", "/api/images/:id"},
            {"image_id", image_id},
            {"error", e.what()}
        });
        METRICS_COUNT("APIRequests", 1.0, "Count", {{"endpoint", "/get"}, {"status", "error"}});
        json error_response = {
            {"error", "Internal server error"},
            {"message", "An error occurred while processing your request"}  // Generic message to user
        };
        crow::response resp(500, error_response.dump());
        resp.add_header("Content-Type", "application/json");
        addCorsHeaders(resp);
        return resp;
    }
}

crow::response ImageController::handleHealthCheck(const crow::request& req) {
    json response = {
        {"status", "healthy"},
        {"service", "image-service"},
        {"s3_bucket", file_service_->getBucketName()}
    };
    crow::response resp(200, response.dump());
    resp.add_header("Content-Type", "application/json");
    addCorsHeaders(resp);
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
    if (file_service_->objectExists(s3_key)) {
        gara::Logger::log_structured(spdlog::level::info, "Image already exists (deduplicated)", {
            {"image_id", image_id},
            {"s3_key", s3_key},
            {"size_bytes", file_data.size()}
        });
        METRICS_COUNT("UploadOperations", 1.0, "Count", {{"status", "deduplicated"}});
        return image_id;
    }

    // Create temporary file
    utils::TempFile temp_file("upload_");
    if (!temp_file.write(file_data)) {
        gara::Logger::log_structured(spdlog::level::err, "Failed to write temporary file for upload", {
            {"image_id", image_id},
            {"size_bytes", file_data.size()}
        });
        METRICS_COUNT("UploadOperations", 1.0, "Count", {{"status", "temp_file_error"}});
        return "";
    }

    // Validate image
    if (!image_processor_->isValidImage(temp_file.getPath())) {
        gara::Logger::log_structured(spdlog::level::err, "Invalid image file uploaded", {
            {"image_id", image_id},
            {"filename", filename},
            {"extension", extension}
        });
        METRICS_COUNT("UploadOperations", 1.0, "Count", {{"status", "invalid_image"}});
        return "";
    }

    // Get image information (dimensions)
    ImageInfo img_info = image_processor_->getImageInfo(temp_file.getPath());

    // Upload to S3
    std::string content_type = utils::FileUtils::getMimeType(extension);
    if (!file_service_->uploadFile(temp_file.getPath(), s3_key, content_type)) {
        gara::Logger::log_structured(spdlog::level::err, "Failed to upload image to S3", {
            {"image_id", image_id},
            {"s3_key", s3_key},
            {"content_type", content_type}
        });
        METRICS_COUNT("UploadOperations", 1.0, "Count", {{"status", "s3_upload_error"}});
        return "";
    }

    // Store metadata in database
    ImageMetadata metadata;
    metadata.image_id = image_id;
    metadata.original_format = extension;
    metadata.s3_raw_key = s3_key;
    metadata.original_size = file_data.size();
    metadata.upload_timestamp = std::time(nullptr);

    // Extract name from filename (remove extension)
    size_t last_dot = filename.find_last_of('.');
    metadata.name = (last_dot != std::string::npos) ? filename.substr(0, last_dot) : filename;

    // Set dimensions from image info
    metadata.width = img_info.width;
    metadata.height = img_info.height;

    if (!db_client_->putImageMetadata(metadata)) {
        gara::Logger::log_structured(spdlog::level::warn, "Failed to store image metadata in database", {
            {"image_id", image_id},
            {"name", metadata.name}
        });
        // Don't fail upload if database storage fails
    }

    gara::Logger::log_structured(spdlog::level::info, "Image uploaded successfully", {
        {"image_id", image_id},
        {"s3_key", s3_key},
        {"size_bytes", file_data.size()},
        {"content_type", content_type},
        {"width", img_info.width},
        {"height", img_info.height}
    });
    METRICS_COUNT("UploadOperations", 1.0, "Count", {{"status", "success"}});
    return image_id;
}

std::string ImageController::getOrCreateTransformed(const TransformRequest& request) {
    // Start timing the operation
    auto timer = gara::Metrics::get()->start_timer("ImageTransformDuration");

    // Check cache first
    std::string cached_key = cache_manager_->getCachedImage(request);
    if (!cached_key.empty()) {
        gara::Logger::log_structured(spdlog::level::info, "Cache hit", {
            {"cache_key", cached_key},
            {"image_id", request.image_id},
            {"operation", "transform"}
        });
        METRICS_COUNT("CacheHits", 1.0, "Count", {{"operation", "transform"}});
        return cached_key;
    }

    gara::Logger::log_structured(spdlog::level::info, "Cache miss - transforming image", {
        {"image_id", request.image_id},
        {"width", request.width},
        {"height", request.height},
        {"format", request.target_format}
    });
    METRICS_COUNT("CacheMisses", 1.0, "Count", {{"operation", "transform"}});

    // Download raw image
    std::string raw_key = "raw/" + request.image_id + ".*";

    // We need to find the actual raw image (we stored it with original extension)
    // For now, try common extensions
    std::vector<std::string> extensions = {"jpg", "jpeg", "png", "gif", "bmp", "tiff", "webp"};
    std::string found_raw_key;

    for (const auto& ext : extensions) {
        std::string test_key = ImageMetadata::generateRawKey(request.image_id, ext);
        if (file_service_->objectExists(test_key)) {
            found_raw_key = test_key;
            break;
        }
    }

    if (found_raw_key.empty()) {
        gara::Logger::log_structured(spdlog::level::err, "Raw image not found in S3", {
            {"image_id", request.image_id},
            {"operation", "transform"}
        });
        METRICS_COUNT("TransformOperations", 1.0, "Count", {{"status", "raw_not_found"}});
        return "";
    }

    // Download raw image to temp file
    utils::TempFile raw_temp("raw_");
    if (!file_service_->downloadFile(found_raw_key, raw_temp.getPath())) {
        gara::Logger::log_structured(spdlog::level::err, "Failed to download raw image from S3", {
            {"image_id", request.image_id},
            {"s3_key", found_raw_key}
        });
        METRICS_COUNT("TransformOperations", 1.0, "Count", {{"status", "download_error"}});
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
        gara::Logger::log_structured(spdlog::level::err, "Image transformation failed", {
            {"image_id", request.image_id},
            {"format", request.target_format},
            {"width", request.width},
            {"height", request.height}
        });
        METRICS_COUNT("TransformOperations", 1.0, "Count", {{"status", "transform_error"}});
        return "";
    }

    // Apply watermark if enabled
    if (watermark_service_->isEnabled()) {
        try {
            // Load transformed image
            vips::VImage transformed_image = vips::VImage::new_from_file(
                transformed_temp.getPath().c_str()
            );

            // Apply watermark
            vips::VImage watermarked = watermark_service_->applyWatermark(transformed_image);

            // Save watermarked image back to temp file
            watermarked.write_to_file(transformed_temp.getPath().c_str());

            gara::Logger::log_structured(spdlog::level::info, "Watermark applied successfully", {
                {"image_id", request.image_id},
                {"format", request.target_format}
            });

        } catch (const vips::VError& e) {
            // Log error but continue (graceful degradation)
            gara::Logger::log_structured(spdlog::level::warn, "Watermark failed, using non-watermarked image", {
                {"image_id", request.image_id},
                {"error", e.what()},
                {"graceful_degradation", true}
            });
        }
    }

    // Store in cache (S3)
    if (!cache_manager_->storeInCache(request, transformed_temp.getPath())) {
        gara::Logger::log_structured(spdlog::level::err, "Failed to cache transformed image", {
            {"image_id", request.image_id},
            {"cache_key", request.getCacheKey()}
        });
        METRICS_COUNT("TransformOperations", 1.0, "Count", {{"status", "cache_error"}});
        return "";
    }

    // Return the S3 key
    return request.getCacheKey();
}

crow::response ImageController::handleListImages(const crow::request& req) {
    try {
        // Parse query parameters with defaults
        int limit = 100;
        int offset = 0;
        ImageSortOrder sort_order = ImageSortOrder::NEWEST;

        // Extract limit parameter
        auto limit_param = req.url_params.get("limit");
        if (limit_param) {
            try {
                limit = std::stoi(limit_param);
                if (limit < 1 || limit > 1000) {
                    json error_response = {
                        {"error", "Invalid limit parameter: must be between 1 and 1000"}
                    };
                    crow::response resp(400, error_response.dump());
                    resp.add_header("Content-Type", "application/json");
                    addCorsHeaders(resp);
                    return resp;
                }
            } catch (const std::exception& e) {
                json error_response = {
                    {"error", "Invalid limit parameter: must be a valid integer"}
                };
                crow::response resp(400, error_response.dump());
                resp.add_header("Content-Type", "application/json");
                addCorsHeaders(resp);
                return resp;
            }
        }

        // Extract offset parameter
        auto offset_param = req.url_params.get("offset");
        if (offset_param) {
            try {
                offset = std::stoi(offset_param);
                if (offset < 0) {
                    json error_response = {
                        {"error", "Invalid offset parameter: must be non-negative"}
                    };
                    crow::response resp(400, error_response.dump());
                    resp.add_header("Content-Type", "application/json");
                    addCorsHeaders(resp);
                    return resp;
                }
            } catch (const std::exception& e) {
                json error_response = {
                    {"error", "Invalid offset parameter: must be a valid integer"}
                };
                crow::response resp(400, error_response.dump());
                resp.add_header("Content-Type", "application/json");
                addCorsHeaders(resp);
                return resp;
            }
        }

        // Extract sort parameter
        auto sort_param = req.url_params.get("sort");
        if (sort_param) {
            std::string sort_str = sort_param;
            if (sort_str == "newest") {
                sort_order = ImageSortOrder::NEWEST;
            } else if (sort_str == "oldest") {
                sort_order = ImageSortOrder::OLDEST;
            } else if (sort_str == "name_asc") {
                sort_order = ImageSortOrder::NAME_ASC;
            } else if (sort_str == "name_desc") {
                sort_order = ImageSortOrder::NAME_DESC;
            } else {
                json error_response = {
                    {"error", "Invalid sort parameter: must be one of 'newest', 'oldest', 'name_asc', 'name_desc'"}
                };
                crow::response resp(400, error_response.dump());
                resp.add_header("Content-Type", "application/json");
                addCorsHeaders(resp);
                return resp;
            }
        }

        // Get total count
        int total = db_client_->getImageCount();

        // Get images with pagination
        std::vector<ImageMetadata> images = db_client_->listImages(limit, offset, sort_order);

        // Build JSON response
        json images_json = json::array();
        for (const auto& img : images) {
            images_json.push_back(img.toJson());
        }

        json response = {
            {"images", images_json},
            {"total", total},
            {"limit", limit},
            {"offset", offset}
        };

        gara::Logger::log_structured(spdlog::level::info, "Listed images successfully", {
            {"total", total},
            {"limit", limit},
            {"offset", offset},
            {"returned", images.size()}
        });

        crow::response resp(200, response.dump());
        resp.add_header("Content-Type", "application/json");
        addCorsHeaders(resp);
        return resp;

    } catch (const std::exception& e) {
        gara::Logger::log_structured(spdlog::level::err, "Failed to list images", {
            {"error", e.what()}
        });

        json error_response = {
            {"error", "Failed to retrieve image list"}
        };
        crow::response resp(500, error_response.dump());
        resp.add_header("Content-Type", "application/json");
        addCorsHeaders(resp);
        return resp;
    }
}

void ImageController::addCorsHeaders(crow::response& resp) {
    resp.add_header("Access-Control-Allow-Origin", "*");
    resp.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    resp.add_header("Access-Control-Allow-Headers", "Content-Type, X-API-Key, Authorization");
    resp.add_header("Access-Control-Max-Age", "3600");
}

} // namespace gara
