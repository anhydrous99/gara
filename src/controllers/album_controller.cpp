#include "album_controller.h"
#include "../middleware/auth_middleware.h"
#include "../models/image_metadata.h"
#include "../constants/album_constants.h"
#include "../exceptions/album_exceptions.h"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace gara {

AlbumController::AlbumController(
    std::shared_ptr<AlbumService> album_service,
    std::shared_ptr<S3Service> s3_service,
    std::shared_ptr<SecretsService> secrets_service)
    : album_service_(album_service),
      s3_service_(s3_service),
      secrets_service_(secrets_service) {
}

// registerRoutes is now a template method in the header

crow::response AlbumController::handleCreateAlbum(const crow::request& req) {
    return handleAuthenticatedJsonRequest<CreateAlbumRequest>(req, [this](const CreateAlbumRequest& request) {
        auto album = album_service_->createAlbum(request);
        return buildJsonResponse(201, album.toJson());
    });
}

crow::response AlbumController::handleListAlbums(const crow::request& req) {
    return handleJsonRequest(req, [this, &req]() {
        // Check if filtering by published
        bool published_only = false;
        auto published_param = req.url_params.get("published");
        if (published_param && std::string(published_param) == constants::PARAM_TRUE) {
            published_only = true;
        }

        auto albums = album_service_->listAlbums(published_only);

        json response;
        response["albums"] = json::array();
        for (const auto& album : albums) {
            response["albums"].push_back(album.toJson());
        }

        return buildJsonResponse(200, response);
    });
}

crow::response AlbumController::handleGetAlbum(const std::string& album_id, const crow::request& req) {
    return handleJsonRequest(req, [this, &album_id, &req]() {
        auto album = album_service_->getAlbum(album_id);

        // Check if non-authenticated users can view unpublished albums
        bool is_authenticated = validateAuth(req);
        if (!album.published && !is_authenticated) {
            return buildErrorResponse(404, "Not Found", "Album not found or not published");
        }

        // Generate presigned URLs for all images
        json response = album.toJson();
        json images_with_urls = json::array();

        for (const auto& image_id : album.image_ids) {
            std::string url = generatePresignedUrlForImage(image_id);
            if (!url.empty()) {
                images_with_urls.push_back({
                    {"id", image_id},
                    {"url", url}
                });
            }
        }

        response["images"] = images_with_urls;

        // Add cover image URL if exists
        if (!album.cover_image_id.empty()) {
            response["cover_image_url"] = generatePresignedUrlForImage(album.cover_image_id);
        }

        return buildJsonResponse(200, response);
    });
}

crow::response AlbumController::handleUpdateAlbum(const std::string& album_id, const crow::request& req) {
    return handleAuthenticatedJsonRequest<UpdateAlbumRequest>(req, [this, &album_id](const UpdateAlbumRequest& request) {
        auto album = album_service_->updateAlbum(album_id, request);
        return buildJsonResponse(200, album.toJson());
    });
}

crow::response AlbumController::handleDeleteAlbum(const std::string& album_id, const crow::request& req) {
    // Validate authentication
    if (!validateAuth(req)) {
        return buildAuthErrorResponse();
    }

    return handleJsonRequest(req, [this, &album_id]() {
        album_service_->deleteAlbum(album_id);
        json response = {{"success", true}};
        return buildJsonResponse(200, response);
    });
}

crow::response AlbumController::handleAddImages(const std::string& album_id, const crow::request& req) {
    return handleAuthenticatedJsonRequest<AddImagesRequest>(req, [this, &album_id](const AddImagesRequest& request) {
        auto album = album_service_->addImages(album_id, request);
        return buildJsonResponse(200, album.toJson());
    });
}

crow::response AlbumController::handleRemoveImage(const std::string& album_id,
                                                   const std::string& image_id,
                                                   const crow::request& req) {
    // Validate authentication
    if (!validateAuth(req)) {
        return buildAuthErrorResponse();
    }

    return handleJsonRequest(req, [this, &album_id, &image_id]() {
        auto album = album_service_->removeImage(album_id, image_id);
        return buildJsonResponse(200, album.toJson());
    }, 400);
}

crow::response AlbumController::handleReorderImages(const std::string& album_id, const crow::request& req) {
    return handleAuthenticatedJsonRequest<ReorderImagesRequest>(req, [this, &album_id](const ReorderImagesRequest& request) {
        auto album = album_service_->reorderImages(album_id, request);
        return buildJsonResponse(200, album.toJson());
    });
}

void AlbumController::addCorsHeaders(crow::response& resp) {
    resp.add_header("Access-Control-Allow-Origin", "*");
    resp.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    resp.add_header("Access-Control-Allow-Headers", "Content-Type, X-API-Key, Authorization");
    resp.add_header("Access-Control-Max-Age", std::to_string(constants::CORS_MAX_AGE_SECONDS));
}

bool AlbumController::validateAuth(const crow::request& req) {
    std::string api_key = secrets_service_->getApiKey();
    return middleware::AuthMiddleware::validateApiKey(req, api_key);
}

std::string AlbumController::generatePresignedUrlForImage(const std::string& image_id) {
    // Try common image formats to find the image in S3
    for (const auto& format : constants::SUPPORTED_IMAGE_FORMATS) {
        std::string key = ImageMetadata::generateRawKey(image_id, format);
        if (s3_service_->objectExists(key)) {
            return s3_service_->generatePresignedUrl(key, constants::PRESIGNED_URL_EXPIRATION_SECONDS);
        }
    }

    return ""; // Image not found
}

// Helper method implementations
crow::response AlbumController::buildJsonResponse(int status_code, const json& body) {
    crow::response resp(status_code, body.dump());
    resp.add_header("Content-Type", "application/json");
    addCorsHeaders(resp);
    return resp;
}

crow::response AlbumController::buildErrorResponse(int status_code, const std::string& error, const std::string& details) {
    json error_response = {
        {"error", error},
        {"details", details}
    };
    return buildJsonResponse(status_code, error_response);
}

crow::response AlbumController::buildAuthErrorResponse() {
    crow::response resp = middleware::AuthMiddleware::unauthorizedResponse("Invalid or missing API key");
    addCorsHeaders(resp);
    return resp;
}

// Template implementations must be in header or same translation unit
template<typename RequestType, typename HandlerFunc>
crow::response AlbumController::handleAuthenticatedJsonRequest(const crow::request& req, HandlerFunc handler, int default_error_code) {
    // Validate authentication
    if (!validateAuth(req)) {
        return buildAuthErrorResponse();
    }

    try {
        auto body = json::parse(req.body);
        auto request = RequestType::fromJson(body);
        return handler(request);
    } catch (const json::exception& e) {
        return buildErrorResponse(400, constants::ERROR_INVALID_JSON, e.what());
    } catch (const exceptions::NotFoundException& e) {
        return buildErrorResponse(404, "Not Found", e.what());
    } catch (const exceptions::ValidationException& e) {
        return buildErrorResponse(400, constants::ERROR_VALIDATION, e.what());
    } catch (const exceptions::ConflictException& e) {
        return buildErrorResponse(409, "Conflict", e.what());
    } catch (const std::exception& e) {
        return buildErrorResponse(default_error_code,
            default_error_code == 400 ? "Bad Request" : "Internal Server Error",
            e.what());
    }
}

template<typename HandlerFunc>
crow::response AlbumController::handleJsonRequest(const crow::request& req, HandlerFunc handler, int default_error_code) {
    try {
        return handler();
    } catch (const exceptions::NotFoundException& e) {
        return buildErrorResponse(404, "Not Found", e.what());
    } catch (const exceptions::ValidationException& e) {
        return buildErrorResponse(400, constants::ERROR_VALIDATION, e.what());
    } catch (const std::exception& e) {
        return buildErrorResponse(default_error_code,
            default_error_code == 500 ? "Internal Server Error" : "Bad Request",
            e.what());
    }
}

} // namespace gara
