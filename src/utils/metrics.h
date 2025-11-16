#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <memory>

namespace gara {

/**
 * CloudWatch Embedded Metric Format (EMF) metrics publisher
 * Outputs metrics embedded in JSON logs that CloudWatch automatically extracts
 *
 * Reference: https://docs.aws.amazon.com/AmazonCloudWatch/latest/monitoring/CloudWatch_Embedded_Metric_Format.html
 */
class Metrics {
public:
    using DimensionMap = std::map<std::string, std::string>;

    /**
     * Initialize metrics with service configuration
     * @param namespace CloudWatch namespace (e.g., "GaraImage")
     * @param service_name Service name for default dimension
     * @param environment Environment name (e.g., "production")
     * @param enabled Whether metrics are enabled
     */
    static void initialize(
        const std::string& namespace_name,
        const std::string& service_name,
        const std::string& environment = "production",
        bool enabled = true
    );

    /**
     * Get the singleton metrics instance
     */
    static std::shared_ptr<Metrics> get();

    /**
     * Publish a counter metric
     * @param name Metric name (e.g., "CacheHits")
     * @param value Counter value (default 1)
     * @param unit Metric unit (None, Count, Percent, etc.)
     * @param dimensions Additional dimensions beyond default
     */
    void publish_count(
        const std::string& name,
        double value = 1.0,
        const std::string& unit = "Count",
        const DimensionMap& dimensions = {}
    );

    /**
     * Publish a duration/timing metric (in milliseconds)
     * @param name Metric name (e.g., "ImageProcessingDuration")
     * @param duration_ms Duration in milliseconds
     * @param dimensions Additional dimensions
     */
    void publish_duration(
        const std::string& name,
        double duration_ms,
        const DimensionMap& dimensions = {}
    );

    /**
     * Publish a gauge metric (current value)
     * @param name Metric name (e.g., "ActiveRequests")
     * @param value Current value
     * @param unit Metric unit
     * @param dimensions Additional dimensions
     */
    void publish_gauge(
        const std::string& name,
        double value,
        const std::string& unit = "None",
        const DimensionMap& dimensions = {}
    );

    /**
     * Helper class for timing operations
     * Automatically publishes duration when destroyed
     */
    class Timer {
    public:
        Timer(const std::string& metric_name, const DimensionMap& dimensions = {});
        ~Timer();

        // Delete copy constructors
        Timer(const Timer&) = delete;
        Timer& operator=(const Timer&) = delete;

        // Get elapsed time in milliseconds
        double elapsed_ms() const;

    private:
        std::string metric_name_;
        DimensionMap dimensions_;
        std::chrono::steady_clock::time_point start_time_;
    };

    /**
     * Create a timer for automatic duration tracking
     * Usage:
     *   {
     *     auto timer = Metrics::get()->start_timer("ImageProcessing");
     *     // ... do work ...
     *   } // Timer publishes duration when it goes out of scope
     */
    std::unique_ptr<Timer> start_timer(
        const std::string& metric_name,
        const DimensionMap& dimensions = {}
    );

    /**
     * Check if metrics are enabled
     */
    bool is_enabled() const { return enabled_; }

private:
    Metrics(
        const std::string& namespace_name,
        const std::string& service_name,
        const std::string& environment,
        bool enabled
    );

    void publish_metric(
        const std::string& name,
        double value,
        const std::string& unit,
        const DimensionMap& dimensions
    );

    nlohmann::json create_emf_log(
        const std::string& name,
        double value,
        const std::string& unit,
        const DimensionMap& dimensions
    );

    static std::shared_ptr<Metrics> instance_;

    std::string namespace_;
    std::string service_name_;
    std::string environment_;
    bool enabled_;
};

// Convenience macros for metrics
#define METRICS_COUNT(name, ...) \
    if (auto m = gara::Metrics::get(); m && m->is_enabled()) { \
        m->publish_count(name, ##__VA_ARGS__); \
    }

#define METRICS_DURATION(name, duration_ms, ...) \
    if (auto m = gara::Metrics::get(); m && m->is_enabled()) { \
        m->publish_duration(name, duration_ms, ##__VA_ARGS__); \
    }

#define METRICS_GAUGE(name, value, ...) \
    if (auto m = gara::Metrics::get(); m && m->is_enabled()) { \
        m->publish_gauge(name, value, ##__VA_ARGS__); \
    }

#define METRICS_TIMER(name, ...) \
    std::unique_ptr<gara::Metrics::Timer> _timer_##__LINE__; \
    if (auto m = gara::Metrics::get(); m && m->is_enabled()) { \
        _timer_##__LINE__ = m->start_timer(name, ##__VA_ARGS__); \
    }

} // namespace gara
