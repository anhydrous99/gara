#pragma once

#include <crow.h>
#include <string>
#include "utils/id_generator.h"

namespace gara {

/**
 * Request context middleware for correlation tracking
 * Adds a unique request ID to each incoming request for distributed tracing
 */
struct RequestContextMiddleware {
    struct context {
        std::string request_id;
        std::string endpoint;
        std::chrono::steady_clock::time_point start_time;
    };

    void before_handle(crow::request& req, crow::response& res, context& ctx) {
        // Generate unique request ID (or use existing from X-Request-ID header)
        auto header_id = req.get_header_value("X-Request-ID");
        if (!header_id.empty()) {
            ctx.request_id = header_id;
        } else {
            ctx.request_id = utils::IdGenerator::generateAlbumId();
        }

        // Store endpoint path
        ctx.endpoint = req.url;

        // Record start time for request duration tracking
        ctx.start_time = std::chrono::steady_clock::now();

        // Add request ID to response headers for client correlation
        res.add_header("X-Request-ID", ctx.request_id);
    }

    void after_handle(crow::request& req, crow::response& res, context& ctx) {
        // Calculate request duration
        auto end_time = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - ctx.start_time
        ).count();

        // This can be used to log request completion metrics
        // Will be integrated with logging/metrics in controllers
        (void)duration_ms; // Unused for now, but available for metrics
    }

    // Helper to get request ID from context
    static std::string get_request_id(const context& ctx) {
        return ctx.request_id;
    }

    // Helper to get endpoint from context
    static std::string get_endpoint(const context& ctx) {
        return ctx.endpoint;
    }

    // Helper to get elapsed time since request start
    static double get_elapsed_ms(const context& ctx) {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now - ctx.start_time
        ).count();
    }
};

} // namespace gara
