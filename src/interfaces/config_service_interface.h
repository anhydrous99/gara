#ifndef GARA_CONFIG_SERVICE_INTERFACE_H
#define GARA_CONFIG_SERVICE_INTERFACE_H

#include <string>

namespace gara {

/**
 * @brief Abstract interface for configuration/secrets services
 *
 * This interface abstracts configuration retrieval, allowing for
 * different implementations (AWS Secrets Manager, environment variables, etc.)
 */
class ConfigServiceInterface {
public:
    virtual ~ConfigServiceInterface() = default;

    /**
     * @brief Get the API key
     * @return API key string, or empty string if not available
     */
    virtual std::string getApiKey() = 0;

    /**
     * @brief Force refresh the API key
     * @return true if successful, false otherwise
     */
    virtual bool refreshApiKey() = 0;

    /**
     * @brief Check if the service is initialized
     * @return true if initialized with a valid key
     */
    virtual bool isInitialized() const = 0;

    /**
     * @brief Get the secret/config name being used
     * @return secret or environment variable name
     */
    virtual const std::string& getSecretName() const = 0;
};

} // namespace gara

#endif // GARA_CONFIG_SERVICE_INTERFACE_H
