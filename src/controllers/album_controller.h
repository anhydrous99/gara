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

    // Register routes with Crow app (templated to support middleware)
    template<typename App>
    void registerRoutes(App& app);

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

// Template implementation must be in header
template<typename App>
void AlbumController::registerRoutes(App& app) {
    // Create album
    CROW_ROUTE(app, "/api/albums").methods("POST"_method)
    ([this](const crow::request& req) {
        return handleCreateAlbum(req);
    });

    // List albums
    CROW_ROUTE(app, "/api/albums").methods("GET"_method)
    ([this](const crow::request& req) {
        return handleListAlbums(req);
    });

    // Get single album
    CROW_ROUTE(app, "/api/albums/<string>").methods("GET"_method)
    ([this](const crow::request& req, const std::string& album_id) {
        return handleGetAlbum(album_id, req);
    });

    // Update album
    CROW_ROUTE(app, "/api/albums/<string>").methods("PUT"_method)
    ([this](const crow::request& req, const std::string& album_id) {
        return handleUpdateAlbum(album_id, req);
    });

    // Delete album
    CROW_ROUTE(app, "/api/albums/<string>").methods("DELETE"_method)
    ([this](const crow::request& req, const std::string& album_id) {
        return handleDeleteAlbum(album_id, req);
    });

    // Add images to album
    CROW_ROUTE(app, "/api/albums/<string>/images").methods("POST"_method)
    ([this](const crow::request& req, const std::string& album_id) {
        return handleAddImages(album_id, req);
    });

    // Remove image from album
    CROW_ROUTE(app, "/api/albums/<string>/images/<string>").methods("DELETE"_method)
    ([this](const crow::request& req, const std::string& album_id, const std::string& image_id) {
        return handleRemoveImage(album_id, image_id, req);
    });

    // Reorder images in album
    CROW_ROUTE(app, "/api/albums/<string>/reorder").methods("PUT"_method)
    ([this](const crow::request& req, const std::string& album_id) {
        return handleReorderImages(album_id, req);
    });

    // OPTIONS handlers for CORS preflight
    CROW_ROUTE(app, "/api/albums").methods("OPTIONS"_method)
    ([this](const crow::request&) {
        crow::response resp(204);
        addCorsHeaders(resp);
        return resp;
    });

    CROW_ROUTE(app, "/api/albums/<string>").methods("OPTIONS"_method)
    ([this](const crow::request&, const std::string&) {
        crow::response resp(204);
        addCorsHeaders(resp);
        return resp;
    });

    CROW_ROUTE(app, "/api/albums/<string>/images").methods("OPTIONS"_method)
    ([this](const crow::request&, const std::string&) {
        crow::response resp(204);
        addCorsHeaders(resp);
        return resp;
    });

    CROW_ROUTE(app, "/api/albums/<string>/images/<string>").methods("OPTIONS"_method)
    ([this](const crow::request&, const std::string&, const std::string&) {
        crow::response resp(204);
        addCorsHeaders(resp);
        return resp;
    });

    CROW_ROUTE(app, "/api/albums/<string>/reorder").methods("OPTIONS"_method)
    ([this](const crow::request&, const std::string&) {
        crow::response resp(204);
        addCorsHeaders(resp);
        return resp;
    });
}

} // namespace gara

#endif // GARA_ALBUM_CONTROLLER_H
