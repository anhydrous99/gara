#ifndef GARA_LOCAL_CONFIG_SERVICE_H
#define GARA_LOCAL_CONFIG_SERVICE_H

#include "secrets_service.h"
#include <string>
#include <mutex>

namespace gara {

/**
 * @brief Local configuration service for retrieving secrets
 *
 * This service reads configuration from environment variables
 * instead of AWS Secrets Manager for local development.
 * Inherits from SecretsService for compatibility.
 */
class LocalConfigService : public SecretsService {
public:
    /**
     * @brief Constructor
     * @param api_key_env_var Environment variable name for API key (default: "API_KEY")
     */
    explicit LocalConfigService(const std::string& api_key_env_var = "API_KEY");

    /**
     * @brief Get the API key from environment variable
     * @return API key string, or empty string if not set
     */
    std::string getApiKey() override;

    /**
     * @brief Force refresh the API key (re-reads from environment)
     * @return true if API key is available, false otherwise
     */
    bool refreshApiKey() override;

    /**
     * @brief Check if the service has a valid API key
     * @return true if API key is set
     */
    bool isInitialized() const override;

private:
    std::string api_key_env_var_;
    mutable std::mutex cache_mutex_;
    std::string cached_api_key_;
    bool initialized_;

    /**
     * @brief Read API key from environment variable
     * @return API key value or empty string
     */
    std::string readApiKeyFromEnv();
};

} // namespace gara

#endif // GARA_LOCAL_CONFIG_SERVICE_H
