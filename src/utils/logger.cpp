#include "utils/logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/pattern_formatter.h>
#include <iomanip>
#include <sstream>

namespace gara {

std::shared_ptr<spdlog::logger> Logger::logger_;
std::string Logger::service_name_;
std::string Logger::environment_;
Logger::Format Logger::format_;

void Logger::initialize(
    const std::string& service_name,
    const std::string& log_level,
    Format format,
    const std::string& environment
) {
    service_name_ = service_name;
    environment_ = environment;
    format_ = format;

    // Create console sink (stdout for info+, stderr for errors)
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    logger_ = std::make_shared<spdlog::logger>("gara", console_sink);

    // Set log level
    logger_->set_level(parse_log_level(log_level));

    // Set pattern based on format
    if (format == Format::TEXT) {
        // Human-readable format: [2025-11-16 10:30:45.123] [info] Message
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    } else {
        // JSON format - we'll construct JSON manually in log_structured
        // For now, use a simple pattern as fallback
        logger_->set_pattern("%v");
    }

    // Flush on every log (important for CloudWatch)
    logger_->flush_on(spdlog::level::info);

    // Register as default logger
    spdlog::set_default_logger(logger_);

    LOG_INFO("Logger initialized: service={}, level={}, format={}, environment={}",
             service_name, log_level,
             (format == Format::JSON ? "json" : "text"),
             environment);
}

std::shared_ptr<spdlog::logger> Logger::get() {
    if (!logger_) {
        // Auto-initialize with defaults if not explicitly initialized
        initialize("gara-image", "info", Format::JSON, "production");
    }
    return logger_;
}

void Logger::log_structured(
    spdlog::level::level_enum level,
    const std::string& message,
    const nlohmann::json& fields
) {
    if (format_ == Format::TEXT) {
        // Simple text format
        logger_->log(level, message);
        return;
    }

    // JSON structured format for CloudWatch
    nlohmann::json log_entry = {
        {"timestamp", get_timestamp()},
        {"level", spdlog::level::to_string_view(level).data()},
        {"service", service_name_},
        {"environment", environment_},
        {"message", message}
    };

    // Merge additional fields
    for (auto& [key, value] : fields.items()) {
        log_entry[key] = value;
    }

    // Output as compact JSON on single line
    logger_->log(level, log_entry.dump());
}

void Logger::log_with_request(
    spdlog::level::level_enum level,
    const std::string& message,
    const std::string& request_id,
    const std::string& endpoint,
    const nlohmann::json& fields
) {
    nlohmann::json context = fields;
    context["request_id"] = request_id;

    if (!endpoint.empty()) {
        context["endpoint"] = endpoint;
    }

    log_structured(level, message, context);
}

void Logger::log_error(
    const std::string& message,
    const std::exception& exception,
    const std::string& request_id
) {
    nlohmann::json fields = {
        {"error_type", "exception"},
        {"error_message", exception.what()}
    };

    if (!request_id.empty()) {
        fields["request_id"] = request_id;
    }

    log_structured(spdlog::level::err, message, fields);
}

std::string Logger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::gmtime(&now_time_t), "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << now_ms.count() << 'Z';

    return ss.str();
}

spdlog::level::level_enum Logger::parse_log_level(const std::string& level) {
    if (level == "trace") return spdlog::level::trace;
    if (level == "debug") return spdlog::level::debug;
    if (level == "info") return spdlog::level::info;
    if (level == "warn" || level == "warning") return spdlog::level::warn;
    if (level == "error") return spdlog::level::err;
    if (level == "critical") return spdlog::level::critical;
    if (level == "off") return spdlog::level::off;

    // Default to info
    return spdlog::level::info;
}

} // namespace gara
