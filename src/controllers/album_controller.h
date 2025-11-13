#ifndef GARA_ALBUM_CONTROLLER_H
#define GARA_ALBUM_CONTROLLER_H

#include <crow.h>
#include <memory>
#include "../services/album_service.h"
#include "../services/s3_service.h"
#include "../services/secrets_service.h"

namespace gara {

class AlbumController {
public:
    AlbumController(
        std::shared_ptr<AlbumService> album_service,
        std::shared_ptr<S3Service> s3_service,
        std::shared_ptr<SecretsService> secrets_service
    );

    // Register routes with Crow app
    void registerRoutes(crow::SimpleApp& app);

private:
    std::shared_ptr<AlbumService> album_service_;
    std::shared_ptr<S3Service> s3_service_;
    std::shared_ptr<SecretsService> secrets_service_;

    // Route handlers
    crow::response handleCreateAlbum(const crow::request& req);
    crow::response handleListAlbums(const crow::request& req);
    crow::response handleGetAlbum(const std::string& album_id, const crow::request& req);
    crow::response handleUpdateAlbum(const std::string& album_id, const crow::request& req);
    crow::response handleDeleteAlbum(const std::string& album_id, const crow::request& req);
    crow::response handleAddImages(const std::string& album_id, const crow::request& req);
    crow::response handleRemoveImage(const std::string& album_id, const std::string& image_id, const crow::request& req);
    crow::response handleReorderImages(const std::string& album_id, const crow::request& req);

    // Helper methods
    void addCorsHeaders(crow::response& resp);
    bool validateAuth(const crow::request& req);
    std::string generatePresignedUrlForImage(const std::string& image_id);

    // Response builder helpers
    crow::response buildJsonResponse(int status_code, const nlohmann::json& body);
    crow::response buildErrorResponse(int status_code, const std::string& error, const std::string& details);
    crow::response buildAuthErrorResponse();

    // Template for handling authenticated JSON requests
    template<typename RequestType, typename HandlerFunc>
    crow::response handleAuthenticatedJsonRequest(const crow::request& req, HandlerFunc handler, int default_error_code = 400);

    // Template for handling unauthenticated JSON requests (like GET)
    template<typename HandlerFunc>
    crow::response handleJsonRequest(const crow::request& req, HandlerFunc handler, int default_error_code = 500);
};

} // namespace gara

#endif // GARA_ALBUM_CONTROLLER_H
