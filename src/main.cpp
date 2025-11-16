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
#include "middleware/request_context_middleware.h"
#include "utils/logger.h"
#include "utils/metrics.h"
#include <aws/dynamodb/DynamoDBClient.h>

int main() {
    // Get logging configuration from environment
    const char* log_level_env = std::getenv("LOG_LEVEL");
    const char* log_format_env = std::getenv("LOG_FORMAT");
    const char* environment_env = std::getenv("ENVIRONMENT");

    std::string log_level = log_level_env ? log_level_env : "info";
    std::string log_format_str = log_format_env ? log_format_env : "json";
    std::string environment = environment_env ? environment_env : "production";

    // Initialize logging
    auto log_format = (log_format_str == "text")
        ? gara::Logger::Format::TEXT
        : gara::Logger::Format::JSON;

    gara::Logger::initialize("gara-image", log_level, log_format, environment);

    // Initialize metrics
    const char* metrics_enabled_env = std::getenv("METRICS_ENABLED");
    const char* metrics_namespace_env = std::getenv("METRICS_NAMESPACE");

    bool metrics_enabled = metrics_enabled_env
        ? (std::string(metrics_enabled_env) == "true")
        : true;
    std::string metrics_namespace = metrics_namespace_env ? metrics_namespace_env : "GaraImage";

    gara::Metrics::initialize(metrics_namespace, "gara-image", environment, metrics_enabled);

    // Initialize AWS SDK
    Aws::SDKOptions options;
    Aws::InitAPI(options);

    // Initialize libvips
    if (!gara::ImageProcessor::initialize()) {
        LOG_CRITICAL("Failed to initialize image processor");
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

    LOG_INFO("Starting Gara Image Service");
    gara::Logger::log_structured(spdlog::level::info, "Service configuration", {
        {"s3_bucket", bucket_name},
        {"aws_region", region},
        {"api_key_secret", secret_name},
        {"dynamodb_table", dynamodb_table}
    });

    // Initialize services
    auto s3_service = std::make_shared<gara::S3Service>(bucket_name, region);
    auto image_processor = std::make_shared<gara::ImageProcessor>();
    auto cache_manager = std::make_shared<gara::CacheManager>(s3_service);
    auto secrets_service = std::make_shared<gara::SecretsService>(secret_name, region);

    // Initialize watermark service
    auto watermark_config = gara::WatermarkConfig::fromEnvironment();
    auto watermark_service = std::make_shared<gara::WatermarkService>(watermark_config);

    gara::Logger::log_structured(spdlog::level::info, "Watermark configuration", {
        {"enabled", watermark_config.enabled},
        {"text", watermark_config.text}
    });

    // Check if secrets service is initialized
    if (!secrets_service->isInitialized()) {
        LOG_WARN("Failed to retrieve API key from Secrets Manager - authentication will not work");
    } else {
        LOG_INFO("API key authentication enabled");
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

    // Startup App with middleware
    using App = crow::App<gara::RequestContextMiddleware>;
    App app;

    // Basic routes
    CROW_ROUTE(app, "/")([](){
        return "Gara Image Service - Upload and transform images on AWS";
    });

    CROW_ROUTE(app, "/health")([s3_service, secrets_service]() {
        nlohmann::json health_status = {
            {"status", "healthy"},
            {"timestamp", gara::Logger::get_timestamp()},
            {"services", {
                {"s3", s3_service != nullptr ? "ok" : "unavailable"},
                {"secrets_manager", secrets_service && secrets_service->isInitialized() ? "ok" : "unavailable"}
            }}
        };

        // Simple health check - return 200 if core services are available
        int status_code = (s3_service != nullptr) ? 200 : 503;
        if (status_code != 200) {
            health_status["status"] = "degraded";
        }

        crow::response resp(status_code, health_status.dump());
        resp.add_header("Content-Type", "application/json");
        return resp;
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
    int port = 8080;
    if (port_env) {
        try {
            port = std::stoi(port_env);
            if (port <= 0 || port > 65535) {
                LOG_WARN("Invalid port number, using default 8080");
                port = 8080;
            }
        } catch (const std::exception& e) {
            LOG_WARN("Invalid PORT value, using default 8080");
        }
    }

    gara::Logger::log_structured(spdlog::level::info, "Starting server", {
        {"port", port},
        {"log_level", log_level},
        {"log_format", log_format_str},
        {"metrics_enabled", metrics_enabled}
    });

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