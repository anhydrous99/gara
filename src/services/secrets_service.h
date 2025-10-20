#ifndef GARA_SECRETS_SERVICE_H
#define GARA_SECRETS_SERVICE_H

#include <string>
#include <memory>
#include <mutex>
#include <chrono>
#include <aws/secretsmanager/SecretsManagerClient.h>

namespace gara {

/**
 * Service for retrieving secrets from AWS Secrets Manager
 * Implements caching with TTL to reduce AWS API calls and improve performance
 */
class SecretsService {
public:
    /**
     * Constructor
     * @param secret_name Name of the secret in AWS Secrets Manager
     * @param region AWS region where the secret is stored
     * @param cache_ttl_seconds Cache TTL in seconds (default: 300 = 5 minutes)
     * @param skip_aws_init Skip AWS client initialization (for testing)
     */
    explicit SecretsService(
        const std::string& secret_name,
        const std::string& region = "us-east-1",
        int cache_ttl_seconds = 300,
        bool skip_aws_init = false
    );

    /**
     * Get the API key from AWS Secrets Manager (with caching)
     * @return API key string, or empty string on error
     */
    std::string getApiKey();

    /**
     * Force refresh the cached API key from AWS Secrets Manager
     * @return true if refresh successful, false otherwise
     */
    bool refreshApiKey();

    /**
     * Check if the service is initialized and has retrieved a key at least once
     * @return true if initialized with a valid key
     */
    bool isInitialized() const;

    /**
     * Get the secret name being used
     * @return secret name
     */
    const std::string& getSecretName() const { return secret_name_; }

private:
    /**
     * Fetch secret from AWS Secrets Manager
     * @return secret value, or empty string on error
     */
    std::string fetchSecretFromAWS();

    /**
     * Check if cached value is still valid
     * @return true if cache is valid
     */
    bool isCacheValid() const;

    std::string secret_name_;
    std::string region_;
    int cache_ttl_seconds_;

    std::shared_ptr<Aws::SecretsManager::SecretsManagerClient> sm_client_;

    // Thread-safe cache
    mutable std::mutex cache_mutex_;
    std::string cached_api_key_;
    std::chrono::steady_clock::time_point cache_timestamp_;
    bool initialized_;
    bool skip_aws_init_;
};

} // namespace gara

#endif // GARA_SECRETS_SERVICE_H
