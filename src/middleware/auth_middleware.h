#ifndef GARA_AUTH_MIDDLEWARE_H
#define GARA_AUTH_MIDDLEWARE_H

#include <string>
#include <crow.h>

namespace gara {
namespace middleware {

/**
 * Authentication middleware for validating API keys
 */
class AuthMiddleware {
public:
    /**
     * Validate API key from request headers
     * @param req Crow HTTP request
     * @param expected_key The valid API key to compare against
     * @return true if authentication succeeds, false otherwise
     */
    static bool validateApiKey(const crow::request& req, const std::string& expected_key);

    /**
     * Extract X-API-Key header from request
     * @param req Crow HTTP request
     * @return API key value, or empty string if not present
     */
    static std::string extractApiKey(const crow::request& req);

    /**
     * Compare two strings in constant time to prevent timing attacks
     * @param a First string
     * @param b Second string
     * @return true if strings are equal
     */
    static bool constantTimeCompare(const std::string& a, const std::string& b);

    /**
     * Generate 401 Unauthorized JSON response
     * @param message Error message
     * @return Crow response with 401 status
     */
    static crow::response unauthorizedResponse(const std::string& message);
};

} // namespace middleware
} // namespace gara

#endif // GARA_AUTH_MIDDLEWARE_H
