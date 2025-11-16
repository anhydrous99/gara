#include "auth_middleware.h"
#include "../utils/logger.h"
#include "../utils/metrics.h"
#include <nlohmann/json.hpp>
#include <algorithm>

using json = nlohmann::json;

namespace gara {
namespace middleware {

bool AuthMiddleware::validateApiKey(const crow::request& req, const std::string& expected_key) {
    // If expected key is empty, authentication is not configured
    if (expected_key.empty()) {
        LOG_WARN("Authentication attempted but API key not configured");
        METRICS_COUNT("AuthAttempts", 1.0, "Count", {{"status", "unconfigured"}});
        return false;
    }

    std::string provided_key = extractApiKey(req);

    if (provided_key.empty()) {
        gara::Logger::log_structured(spdlog::level::warn, "Authentication failed: missing API key", {
            {"reason", "missing_key"},
            {"endpoint", std::string(req.url)}
        });
        METRICS_COUNT("AuthAttempts", 1.0, "Count", {{"status", "missing_key"}});
        return false;
    }

    // Use constant-time comparison to prevent timing attacks
    bool valid = constantTimeCompare(provided_key, expected_key);

    if (!valid) {
        // Security: Log failed auth without revealing the actual key
        gara::Logger::log_structured(spdlog::level::warn, "Authentication failed: invalid API key", {
            {"reason", "invalid_key"},
            {"endpoint", std::string(req.url)}
        });
        METRICS_COUNT("AuthAttempts", 1.0, "Count", {{"status", "invalid_key"}});
    } else {
        // Track successful authentication
        METRICS_COUNT("AuthAttempts", 1.0, "Count", {{"status", "success"}});
    }

    return valid;
}

std::string AuthMiddleware::extractApiKey(const crow::request& req) {
    // Check for X-API-Key header
    auto api_key_header = req.get_header_value("X-API-Key");

    if (api_key_header.empty()) {
        // Also check lowercase variant (HTTP headers are case-insensitive)
        api_key_header = req.get_header_value("x-api-key");
    }

    return api_key_header;
}

bool AuthMiddleware::constantTimeCompare(const std::string& a, const std::string& b) {
    // If lengths differ, still compare to prevent timing attacks revealing length
    size_t len_a = a.length();
    size_t len_b = b.length();

    // Use the longer length for comparison
    size_t max_len = std::max(len_a, len_b);

    unsigned char result = 0;

    // Compare byte by byte
    for (size_t i = 0; i < max_len; ++i) {
        unsigned char byte_a = (i < len_a) ? static_cast<unsigned char>(a[i]) : 0;
        unsigned char byte_b = (i < len_b) ? static_cast<unsigned char>(b[i]) : 0;
        result |= byte_a ^ byte_b;
    }

    // Also XOR the length difference to account for different lengths
    result |= len_a ^ len_b;

    return result == 0;
}

crow::response AuthMiddleware::unauthorizedResponse(const std::string& message) {
    json error_json = {
        {"error", "Unauthorized"},
        {"message", message}
    };

    crow::response res(401);
    res.set_header("Content-Type", "application/json");
    res.write(error_json.dump());
    return res;
}

} // namespace middleware
} // namespace gara
