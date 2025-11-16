#ifndef GARA_IMAGE_CONTROLLER_H
#define GARA_IMAGE_CONTROLLER_H

#include <crow.h>
#include <memory>
#include "../services/s3_service.h"
#include "../services/image_processor.h"
#include "../services/cache_manager.h"
#include "../services/secrets_service.h"
#include "../services/watermark_service.h"

namespace gara {

class ImageController {
public:
    ImageController(std::shared_ptr<S3Service> s3_service,
                   std::shared_ptr<ImageProcessor> image_processor,
                   std::shared_ptr<CacheManager> cache_manager,
                   std::shared_ptr<SecretsService> secrets_service,
                   std::shared_ptr<WatermarkService> watermark_service);

    // Register routes with Crow app (templated to support middleware)
    template<typename App>
    void registerRoutes(App& app);

private:
    std::shared_ptr<S3Service> s3_service_;
    std::shared_ptr<ImageProcessor> image_processor_;
    std::shared_ptr<CacheManager> cache_manager_;
    std::shared_ptr<SecretsService> secrets_service_;
    std::shared_ptr<WatermarkService> watermark_service_;

    // Upload endpoint handler
    crow::response handleUpload(const crow::request& req);

    // Get/transform image endpoint handler
    crow::response handleGetImage(const crow::request& req, const std::string& image_id);

    // Health check for image service
    crow::response handleHealthCheck(const crow::request& req);

    // Helper: Parse multipart form data and extract file
    bool extractUploadedFile(const crow::request& req,
                            std::vector<char>& file_data,
                            std::string& filename);

    // Helper: Parse query parameters for transformation
    TransformRequest parseTransformParams(const crow::request& req,
                                         const std::string& image_id);

    // Helper: Process and upload raw image
    std::string processUpload(const std::vector<char>& file_data,
                             const std::string& filename);

    // Helper: Get or create transformed image
    std::string getOrCreateTransformed(const TransformRequest& request);

    // Helper: Add CORS headers to response
    void addCorsHeaders(crow::response& resp);
};

// Template implementation must be in header
template<typename App>
void ImageController::registerRoutes(App& app) {
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

} // namespace gara

#endif // GARA_IMAGE_CONTROLLER_H
