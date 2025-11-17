#ifndef GARA_IMAGE_CONTROLLER_H
#define GARA_IMAGE_CONTROLLER_H

#include <crow.h>
#include <memory>
#include "../interfaces/file_service_interface.h"
#include "../interfaces/database_client_interface.h"
#include "../services/image_processor.h"
#include "../services/cache_manager.h"
#include "../interfaces/config_service_interface.h"
#include "../services/watermark_service.h"

namespace gara {

// Constants for image listing API
namespace ImageListingConfig {
    constexpr int DEFAULT_LIMIT = 100;
    constexpr int MAX_LIMIT = 1000;
    constexpr int DEFAULT_OFFSET = 0;
}

// Structure to hold parsed list parameters
struct ListImageParams {
    int limit = ImageListingConfig::DEFAULT_LIMIT;
    int offset = ImageListingConfig::DEFAULT_OFFSET;
    ImageSortOrder sort_order = ImageSortOrder::NEWEST;
};

class ImageController {
public:
    ImageController(std::shared_ptr<FileServiceInterface> file_service,
                   std::shared_ptr<ImageProcessor> image_processor,
                   std::shared_ptr<CacheManager> cache_manager,
                   std::shared_ptr<ConfigServiceInterface> config_service,
                   std::shared_ptr<WatermarkService> watermark_service,
                   std::shared_ptr<DatabaseClientInterface> db_client);

    // Register routes with Crow app (templated to support middleware)
    template<typename App>
    void registerRoutes(App& app);

private:
    std::shared_ptr<FileServiceInterface> file_service_;
    std::shared_ptr<ImageProcessor> image_processor_;
    std::shared_ptr<CacheManager> cache_manager_;
    std::shared_ptr<ConfigServiceInterface> config_service_;
    std::shared_ptr<WatermarkService> watermark_service_;
    std::shared_ptr<DatabaseClientInterface> db_client_;

    // Upload endpoint handler
    crow::response handleUpload(const crow::request& req);

    // Get/transform image endpoint handler
    crow::response handleGetImage(const crow::request& req, const std::string& image_id);

    // List images endpoint handler
    crow::response handleListImages(const crow::request& req);

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

    // Helper: Create JSON error response with CORS headers
    crow::response createJsonError(int status_code, const std::string& error_message);

    // Helper: Parse and validate list image parameters
    std::optional<ListImageParams> parseListParams(const crow::request& req, std::string& error_message);

    // Helper: Store image metadata in database
    bool storeImageMetadata(const std::string& image_id, const std::string& filename,
                           const std::string& extension, size_t file_size,
                           const ImageInfo& img_info);
};

// Template implementation must be in header
template<typename App>
void ImageController::registerRoutes(App& app) {
    // Upload image
    CROW_ROUTE(app, "/api/images/upload").methods("POST"_method)
    ([this](const crow::request& req) {
        return handleUpload(req);
    });

    // List images (must come before /<string> route)
    CROW_ROUTE(app, "/api/images").methods("GET"_method)
    ([this](const crow::request& req) {
        return handleListImages(req);
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
