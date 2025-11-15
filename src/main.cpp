#include <crow.h>
#include <crow/compression.h>
#include <aws/core/Aws.h>
#include <iostream>
#include <memory>
#include <fstream>
#include <sstream>

#include "services/s3_service.h"
#include "services/image_processor.h"
#include "services/cache_manager.h"
#include "services/secrets_service.h"
#include "services/watermark_service.h"
#include "services/album_service.h"
#include "services/dynamodb_client_wrapper.h"
#include "controllers/image_controller.h"
#include "controllers/album_controller.h"
#include "models/watermark_config.h"
#include <aws/dynamodb/DynamoDBClient.h>

int main() {
    // Initialize AWS SDK
    Aws::SDKOptions options;
    Aws::InitAPI(options);

    // Initialize libvips
    if (!gara::ImageProcessor::initialize()) {
        std::cerr << "Failed to initialize image processor" << std::endl;
        return 1;
    }

    // Get configuration from environment
    const char* bucket_env = std::getenv("S3_BUCKET_NAME");
    const char* region_env = std::getenv("AWS_REGION");
    const char* secret_name_env = std::getenv("SECRETS_MANAGER_API_KEY_NAME");
    const char* dynamodb_table_env = std::getenv("DYNAMODB_TABLE_NAME");

    std::string bucket_name = bucket_env ? bucket_env : "gara-images";
    std::string region = region_env ? region_env : "us-east-1";
    std::string secret_name = secret_name_env ? secret_name_env : "gara-api-key";
    std::string dynamodb_table = dynamodb_table_env ? dynamodb_table_env : "gara-albums";

    std::cout << "Starting Gara Image Service" << std::endl;
    std::cout << "S3 Bucket: " << bucket_name << std::endl;
    std::cout << "AWS Region: " << region << std::endl;
    std::cout << "API Key Secret: " << secret_name << std::endl;
    std::cout << "DynamoDB Table: " << dynamodb_table << std::endl;

    // Initialize services
    auto s3_service = std::make_shared<gara::S3Service>(bucket_name, region);
    auto image_processor = std::make_shared<gara::ImageProcessor>();
    auto cache_manager = std::make_shared<gara::CacheManager>(s3_service);
    auto secrets_service = std::make_shared<gara::SecretsService>(secret_name, region);

    // Initialize watermark service
    auto watermark_config = gara::WatermarkConfig::fromEnvironment();
    auto watermark_service = std::make_shared<gara::WatermarkService>(watermark_config);

    std::cout << "Watermark: " << (watermark_config.enabled ? "Enabled" : "Disabled") << std::endl;
    if (watermark_config.enabled) {
        std::cout << "Watermark text: " << watermark_config.text << std::endl;
    }

    // Check if secrets service is initialized
    if (!secrets_service->isInitialized()) {
        std::cerr << "Warning: Failed to retrieve API key from Secrets Manager" << std::endl;
        std::cerr << "Authentication will not work until secret is available" << std::endl;
    } else {
        std::cout << "API key authentication enabled" << std::endl;
    }

    // Initialize DynamoDB client
    Aws::Client::ClientConfiguration dynamodb_config;
    dynamodb_config.region = region.c_str();
    auto dynamodb_client = std::make_shared<Aws::DynamoDB::DynamoDBClient>(dynamodb_config);
    auto dynamodb_wrapper = std::make_shared<gara::DynamoDBClientWrapper>(dynamodb_client);

    // Initialize album service
    auto album_service = std::make_shared<gara::AlbumService>(dynamodb_table, dynamodb_wrapper, s3_service);

    // Initialize controllers
    gara::ImageController image_controller(s3_service, image_processor, cache_manager, secrets_service, watermark_service);
    gara::AlbumController album_controller(album_service, s3_service, secrets_service);

    // Startup App with compression middleware
    crow::SimpleApp app;

    // Basic routes
    CROW_ROUTE(app, "/")([](){
        return "Gara Image Service - Upload and transform images on AWS";
    });

    CROW_ROUTE(app, "/health")([]() {
        return crow::response(200, "OK");
    });

    // OpenAPI documentation endpoints
    CROW_ROUTE(app, "/api/openapi.yaml")([]() {
        std::ifstream file("openapi.yaml");
        if (!file) {
            return crow::response(404, "OpenAPI spec not found");
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        crow::response resp(200, buffer.str());
        resp.add_header("Content-Type", "application/x-yaml");
        return resp;
    });

    CROW_ROUTE(app, "/api/docs")([]() {
        std::ifstream file("docs/api.html");
        if (!file) {
            // Fallback: redirect to online Swagger Editor
            crow::response resp(307);
            resp.add_header("Location", "https://editor.swagger.io/?url=" +
                std::string("http://localhost:8080/api/openapi.yaml"));
            return resp;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        crow::response resp(200, buffer.str());
        resp.add_header("Content-Type", "text/html");
        return resp;
    });

    // Register image routes
    image_controller.registerRoutes(app);

    // Register album routes
    album_controller.registerRoutes(app);

    // Get port from environment
    char* port_env = std::getenv("PORT");
    int port = 80;
    if (port_env) {
        try {
            port = std::stoi(port_env);
            if (port <= 0 || port > 65535) {
                std::cerr << "Invalid port number, using default 80" << std::endl;
                port = 80;
            }
        } catch (const std::exception& e) {
            std::cerr << "Invalid PORT value, using default 80" << std::endl;
        }
    }

    std::cout << "Starting server on port " << port << std::endl;

    // Run app
    app
    .port(port)
    .loglevel(crow::LogLevel::Warning)
    .multithreaded().run();

    // Cleanup
    gara::ImageProcessor::shutdown();
    Aws::ShutdownAPI(options);

    return 0;
}