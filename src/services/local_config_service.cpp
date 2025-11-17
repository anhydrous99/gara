#include "local_config_service.h"
#include "../utils/logger.h"
#include <cstdlib>

namespace gara {

LocalConfigService::LocalConfigService(const std::string& api_key_env_var)
    : api_key_env_var_(api_key_env_var), initialized_(false) {

    // Try to read API key on initialization
    cached_api_key_ = readApiKeyFromEnv();
    initialized_ = !cached_api_key_.empty();

    if (initialized_) {
        LOG_INFO("Local config service initialized with API key from: {}", api_key_env_var_);
    } else {
        LOG_WARN("Local config service initialized but API key not found in: {}", api_key_env_var_);
    }
}

std::string LocalConfigService::readApiKeyFromEnv() {
    const char* api_key = std::getenv(api_key_env_var_.c_str());

    if (api_key == nullptr || std::string(api_key).empty()) {
        LOG_WARN("API key not found in environment variable: {}", api_key_env_var_);
        return "";
    }

    return std::string(api_key);
}

std::string LocalConfigService::getApiKey() {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    if (cached_api_key_.empty()) {
        // Try to refresh if not cached
        cached_api_key_ = readApiKeyFromEnv();
        initialized_ = !cached_api_key_.empty();
    }

    return cached_api_key_;
}

bool LocalConfigService::refreshApiKey() {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    cached_api_key_ = readApiKeyFromEnv();
    initialized_ = !cached_api_key_.empty();

    if (initialized_) {
        LOG_DEBUG("API key refreshed from environment");
    } else {
        LOG_ERROR("Failed to refresh API key - not found in environment");
    }

    return initialized_;
}

bool LocalConfigService::isInitialized() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return initialized_;
}

} // namespace gara
