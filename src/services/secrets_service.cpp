#include "secrets_service.h"
#include <aws/secretsmanager/model/GetSecretValueRequest.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <iostream>

namespace gara {

SecretsService::SecretsService(
    const std::string& secret_name,
    const std::string& region,
    int cache_ttl_seconds,
    bool skip_aws_init
)
    : secret_name_(secret_name),
      region_(region),
      cache_ttl_seconds_(cache_ttl_seconds),
      initialized_(false),
      skip_aws_init_(skip_aws_init) {

    if (!skip_aws_init_) {
        // Initialize AWS Secrets Manager client
        Aws::Client::ClientConfiguration config;
        config.region = region_;
        sm_client_ = std::make_shared<Aws::SecretsManager::SecretsManagerClient>(config);

        // Attempt initial fetch
        std::string initial_key = fetchSecretFromAWS();
        if (!initial_key.empty()) {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            cached_api_key_ = initial_key;
            cache_timestamp_ = std::chrono::steady_clock::now();
            initialized_ = true;
        } else {
            std::cerr << "Warning: Failed to initialize SecretsService with secret: "
                     << secret_name_ << std::endl;
        }
    }
}

std::string SecretsService::getApiKey() {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    // Return cached value if still valid
    if (isCacheValid()) {
        return cached_api_key_;
    }

    // Cache expired or not initialized, fetch from AWS
    if (!skip_aws_init_) {
        std::string new_key = fetchSecretFromAWS();
        if (!new_key.empty()) {
            cached_api_key_ = new_key;
            cache_timestamp_ = std::chrono::steady_clock::now();
            initialized_ = true;
            return cached_api_key_;
        } else {
            // Fetch failed, but return cached value if available (degraded mode)
            std::cerr << "Warning: Failed to refresh API key from Secrets Manager, "
                     << "using cached value" << std::endl;
            return cached_api_key_;
        }
    }

    return cached_api_key_;
}

bool SecretsService::refreshApiKey() {
    if (skip_aws_init_) {
        return false;
    }

    std::string new_key = fetchSecretFromAWS();
    if (new_key.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(cache_mutex_);
    cached_api_key_ = new_key;
    cache_timestamp_ = std::chrono::steady_clock::now();
    initialized_ = true;
    return true;
}

bool SecretsService::isInitialized() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return initialized_ && !cached_api_key_.empty();
}

std::string SecretsService::fetchSecretFromAWS() {
    if (!sm_client_) {
        std::cerr << "Error: Secrets Manager client not initialized" << std::endl;
        return "";
    }

    try {
        Aws::SecretsManager::Model::GetSecretValueRequest request;
        request.SetSecretId(secret_name_);

        auto outcome = sm_client_->GetSecretValue(request);

        if (outcome.IsSuccess()) {
            const auto& result = outcome.GetResult();

            // Check if secret is stored as string (most common for API keys)
            const Aws::String& secret_string = result.GetSecretString();
            if (!secret_string.empty()) {
                return secret_string;
            }

            // Check if secret is binary
            const auto& secret_binary = result.GetSecretBinary();
            if (secret_binary.GetLength() > 0) {
                std::cerr << "Error: Secret is stored as binary, expected string" << std::endl;
                return "";
            }

            // No secret data found
            std::cerr << "Error: Secret exists but contains no data" << std::endl;
            return "";
        } else {
            const auto& error = outcome.GetError();
            std::cerr << "Error fetching secret '" << secret_name_ << "': "
                     << error.GetExceptionName() << " - "
                     << error.GetMessage() << std::endl;
            return "";
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception while fetching secret: " << e.what() << std::endl;
        return "";
    }
}

bool SecretsService::isCacheValid() const {
    if (!initialized_ || cached_api_key_.empty()) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - cache_timestamp_
    ).count();

    return elapsed < cache_ttl_seconds_;
}

} // namespace gara
