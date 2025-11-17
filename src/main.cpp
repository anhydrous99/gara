#include <crow.h>
#include <crow/compression.h>
#include <iostream>
#include <memory>
#include <fstream>
#include <sstream>
#include <filesystem>

#include "services/local_file_service.h"
#include "services/image_processor.h"
#include "services/cache_manager.h"
#include "services/local_config_service.h"
#include "services/watermark_service.h"
#include "services/album_service.h"
#include "db/sqlite_client.h"
#include "controllers/image_controller.h"
#include "controllers/album_controller.h"
#include "models/watermark_config.h"
#include "middleware/request_context_middleware.h"
#include "utils/logger.h"
#include "utils/metrics.h"

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

    // Initialize libvips
    if (!gara::ImageProcessor::initialize()) {
        LOG_CRITICAL("Failed to initialize image processor");
        return 1;
    }

    // Get configuration from environment
    const char* storage_path_env = std::getenv("STORAGE_PATH");
    const char* db_path_env = std::getenv("DATABASE_PATH");
    const char* api_key_env_var = std::getenv("API_KEY_ENV_VAR");

    std::string storage_path = storage_path_env ? storage_path_env : "./data/images";
    std::string db_path = db_path_env ? db_path_env : "./data/gara.db";
    std::string api_key_var = api_key_env_var ? api_key_env_var : "API_KEY";

    LOG_INFO("Starting Gara Image Service (Local Mode)");
    gara::Logger::log_structured(spdlog::level::info, "Service configuration", {
        {"storage_path", storage_path},
        {"database_path", db_path},
        {"api_key_env_var", api_key_var},
        {"mode", "local"}
    });

    // Create data directories if they don't exist
    try {
        std::filesystem::create_directories(storage_path);
        std::filesystem::create_directories(std::filesystem::path(db_path).parent_path());
        LOG_INFO("Data directories created/verified");
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create data directories: " + std::string(e.what()));
        return 1;
    }

    // Initialize SQLite database
    std::shared_ptr<gara::SQLiteClient> db_client;
    try {
        db_client = std::make_shared<gara::SQLiteClient>(db_path);
        if (!db_client->initialize()) {
            LOG_CRITICAL("Failed to initialize database schema");
            return 1;
        }
        LOG_INFO("Database initialized successfully");
    } catch (const std::exception& e) {
        LOG_CRITICAL("Failed to open database: " + std::string(e.what()));
        return 1;
    }

    // Initialize services
    auto file_service = std::make_shared<gara::LocalFileService>(storage_path);
    auto image_processor = std::make_shared<gara::ImageProcessor>();
    auto cache_manager = std::make_shared<gara::CacheManager>(file_service);
    auto config_service = std::make_shared<gara::LocalConfigService>(api_key_var);

    // Initialize watermark service
    auto watermark_config = gara::WatermarkConfig::fromEnvironment();
    auto watermark_service = std::make_shared<gara::WatermarkService>(watermark_config);

    gara::Logger::log_structured(spdlog::level::info, "Watermark configuration", {
        {"enabled", watermark_config.enabled},
        {"text", watermark_config.text}
    });

    // Check if config service has API key
    if (!config_service->isInitialized()) {
        LOG_WARN("API key not configured - authentication will not work");
        LOG_WARN("Set " + api_key_var + " environment variable to enable authentication");
    } else {
        LOG_INFO("API key authentication enabled");
    }

    // Initialize album service
    auto album_service = std::make_shared<gara::AlbumService>(db_client, file_service);

    // Initialize controllers
    gara::ImageController image_controller(file_service, image_processor, cache_manager, config_service, watermark_service);
    gara::AlbumController album_controller(album_service, file_service, config_service);

    // Startup App with middleware
    using App = crow::App<gara::RequestContextMiddleware>;
    App app;

    // Basic routes
    CROW_ROUTE(app, "/")([](){
        return "Gara Image Service - Local image storage and transformation";
    });

    CROW_ROUTE(app, "/health")([file_service, config_service]() {
        nlohmann::json health_status = {
            {"status", "healthy"},
            {"timestamp", gara::Logger::get_timestamp()},
            {"mode", "local"},
            {"services", {
                {"storage", file_service != nullptr ? "ok" : "unavailable"},
                {"config", config_service && config_service->isInitialized() ? "ok" : "unavailable"}
            }}
        };

        // Simple health check - return 200 if core services are available
        int status_code = (file_service != nullptr) ? 200 : 503;
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
        {"metrics_enabled", metrics_enabled},
        {"mode", "local"}
    });

    // Run app
    app
    .port(port)
    .loglevel(crow::LogLevel::Warning)
    .multithreaded().run();

    // Cleanup
    gara::ImageProcessor::shutdown();

    return 0;
}
