#include "secrets_service.h"
#include "../utils/logger.h"
#include "../utils/metrics.h"
#include <aws/secretsmanager/model/GetSecretValueRequest.h>
#include <aws/core/utils/logging/LogMacros.h>

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
            gara::Logger::log_structured(spdlog::level::warn, "Failed to initialize SecretsService", {
                {"secret_name", secret_name_},
                {"region", region_},
                {"operation", "initial_fetch"}
            });
            METRICS_COUNT("SecretsManagerErrors", 1.0, "Count", {{"error_type", "init_failed"}});
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
            gara::Logger::log_structured(spdlog::level::warn, "Failed to refresh API key from Secrets Manager, using cached value", {
                {"secret_name", secret_name_},
                {"operation", "refresh"},
                {"degraded_mode", true}
            });
            METRICS_COUNT("SecretsManagerErrors", 1.0, "Count", {{"error_type", "refresh_failed"}});
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
    auto timer = gara::Metrics::get()->start_timer("SecretsManagerDuration", {{"operation", "fetch"}});

    if (!sm_client_) {
        gara::Logger::log_structured(spdlog::level::err, "Secrets Manager client not initialized", {
            {"secret_name", secret_name_},
            {"operation", "fetch"}
        });
        METRICS_COUNT("SecretsManagerErrors", 1.0, "Count", {{"error_type", "client_not_initialized"}});
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
                METRICS_COUNT("SecretsManagerOperations", 1.0, "Count", {{"operation", "fetch"}, {"status", "success"}});
                return secret_string;
            }

            // Check if secret is binary
            const auto& secret_binary = result.GetSecretBinary();
            if (secret_binary.GetLength() > 0) {
                gara::Logger::log_structured(spdlog::level::err, "Secret is stored as binary, expected string", {
                    {"secret_name", secret_name_},
                    {"secret_type", "binary"},
                    {"expected_type", "string"}
                });
                METRICS_COUNT("SecretsManagerErrors", 1.0, "Count", {{"error_type", "binary_secret"}});
                return "";
            }

            // No secret data found
            gara::Logger::log_structured(spdlog::level::err, "Secret exists but contains no data", {
                {"secret_name", secret_name_}
            });
            METRICS_COUNT("SecretsManagerErrors", 1.0, "Count", {{"error_type", "empty_secret"}});
            return "";
        } else {
            const auto& error = outcome.GetError();
            gara::Logger::log_structured(spdlog::level::err, "Failed to fetch secret from AWS Secrets Manager", {
                {"secret_name", secret_name_},
                {"error_code", error.GetExceptionName()},
                {"error_message", error.GetMessage()},
                {"operation", "GetSecretValue"}
            });
            METRICS_COUNT("SecretsManagerErrors", 1.0, "Count", {{"error_type", "aws_error"}});
            METRICS_COUNT("SecretsManagerOperations", 1.0, "Count", {{"operation", "fetch"}, {"status", "error"}});
            return "";
        }
    } catch (const std::exception& e) {
        gara::Logger::log_structured(spdlog::level::err, "Exception while fetching secret", {
            {"secret_name", secret_name_},
            {"error", e.what()},
            {"operation", "fetch"}
        });
        METRICS_COUNT("SecretsManagerErrors", 1.0, "Count", {{"error_type", "exception"}});
        METRICS_COUNT("SecretsManagerOperations", 1.0, "Count", {{"operation", "fetch"}, {"status", "error"}});
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
