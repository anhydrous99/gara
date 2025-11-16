#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <nlohmann/json.hpp>
#include <string>
#include <memory>
#include <chrono>

namespace gara {

/**
 * Structured logger for ECS/CloudWatch integration
 * Outputs JSON-formatted logs to stdout/stderr for CloudWatch Logs
 */
class Logger {
public:
    enum class Format {
        JSON,    // Structured JSON for CloudWatch Logs Insights
        TEXT     // Human-readable text format
    };

    /**
     * Initialize the global logger
     * @param service_name Name of the service (e.g., "gara-image")
     * @param log_level Minimum log level (trace, debug, info, warn, error, critical)
     * @param format Output format (JSON or TEXT)
     * @param environment Environment name (e.g., "production", "staging")
     */
    static void initialize(
        const std::string& service_name,
        const std::string& log_level = "info",
        Format format = Format::JSON,
        const std::string& environment = "production"
    );

    /**
     * Get the singleton logger instance
     */
    static std::shared_ptr<spdlog::logger> get();

    /**
     * Log structured data with additional context fields
     * @param level Log level
     * @param message Log message
     * @param fields Additional JSON fields (e.g., {"request_id": "123", "duration_ms": 45})
     */
    static void log_structured(
        spdlog::level::level_enum level,
        const std::string& message,
        const nlohmann::json& fields = {}
    );

    /**
     * Log with request context
     * @param level Log level
     * @param message Log message
     * @param request_id Request correlation ID
     * @param endpoint Endpoint being accessed
     * @param fields Additional JSON fields
     */
    static void log_with_request(
        spdlog::level::level_enum level,
        const std::string& message,
        const std::string& request_id,
        const std::string& endpoint = "",
        const nlohmann::json& fields = {}
    );

    /**
     * Log error with exception details
     * @param message Error message
     * @param exception Exception object
     * @param request_id Optional request correlation ID
     */
    static void log_error(
        const std::string& message,
        const std::exception& exception,
        const std::string& request_id = ""
    );

    /**
     * Get current timestamp in ISO 8601 format
     */
    static std::string get_timestamp();

    /**
     * Convert log level string to spdlog level enum
     */
    static spdlog::level::level_enum parse_log_level(const std::string& level);

private:
    static std::shared_ptr<spdlog::logger> logger_;
    static std::string service_name_;
    static std::string environment_;
    static Format format_;
};

// Convenience macros for structured logging
#define LOG_TRACE(...) gara::Logger::get()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) gara::Logger::get()->debug(__VA_ARGS__)
#define LOG_INFO(...) gara::Logger::get()->info(__VA_ARGS__)
#define LOG_WARN(...) gara::Logger::get()->warn(__VA_ARGS__)
#define LOG_ERROR(...) gara::Logger::get()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) gara::Logger::get()->critical(__VA_ARGS__)

} // namespace gara
