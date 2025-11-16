#include "utils/metrics.h"
#include "utils/logger.h"
#include <iostream>

namespace gara {

std::shared_ptr<Metrics> Metrics::instance_;

void Metrics::initialize(
    const std::string& namespace_name,
    const std::string& service_name,
    const std::string& environment,
    bool enabled
) {
    instance_ = std::shared_ptr<Metrics>(
        new Metrics(namespace_name, service_name, environment, enabled)
    );

    if (enabled) {
        LOG_INFO("Metrics initialized: namespace={}, service={}, environment={}",
                 namespace_name, service_name, environment);
    } else {
        LOG_INFO("Metrics disabled");
    }
}

std::shared_ptr<Metrics> Metrics::get() {
    if (!instance_) {
        // Auto-initialize with defaults if not explicitly initialized
        initialize("GaraImage", "gara-image", "production", true);
    }
    return instance_;
}

Metrics::Metrics(
    const std::string& namespace_name,
    const std::string& service_name,
    const std::string& environment,
    bool enabled
)
    : namespace_(namespace_name)
    , service_name_(service_name)
    , environment_(environment)
    , enabled_(enabled)
{}

void Metrics::publish_count(
    const std::string& name,
    double value,
    const std::string& unit,
    const DimensionMap& dimensions
) {
    if (!enabled_) return;
    publish_metric(name, value, unit, dimensions);
}

void Metrics::publish_duration(
    const std::string& name,
    double duration_ms,
    const DimensionMap& dimensions
) {
    if (!enabled_) return;
    publish_metric(name, duration_ms, "Milliseconds", dimensions);
}

void Metrics::publish_gauge(
    const std::string& name,
    double value,
    const std::string& unit,
    const DimensionMap& dimensions
) {
    if (!enabled_) return;
    publish_metric(name, value, unit, dimensions);
}

void Metrics::publish_metric(
    const std::string& name,
    double value,
    const std::string& unit,
    const DimensionMap& dimensions
) {
    if (!enabled_) return;

    nlohmann::json emf_log = create_emf_log(name, value, unit, dimensions);

    // Output to stdout as JSON (CloudWatch will parse this)
    std::cout << emf_log.dump() << std::endl;
}

nlohmann::json Metrics::create_emf_log(
    const std::string& name,
    double value,
    const std::string& unit,
    const DimensionMap& dimensions
) {
    // Build dimension sets
    std::vector<std::vector<std::string>> dimension_sets;
    std::vector<std::string> dimension_names = {"ServiceName", "Environment"};

    // Add custom dimensions
    for (const auto& [key, val] : dimensions) {
        dimension_names.push_back(key);
    }

    dimension_sets.push_back(dimension_names);

    // Build the EMF structure
    nlohmann::json emf_log = {
        {"_aws", {
            {"Timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()},
            {"CloudWatchMetrics", {
                {
                    {"Namespace", namespace_},
                    {"Dimensions", dimension_sets},
                    {"Metrics", {
                        {
                            {"Name", name},
                            {"Unit", unit}
                        }
                    }}
                }
            }}
        }},
        {"ServiceName", service_name_},
        {"Environment", environment_},
        {name, value}
    };

    // Add custom dimension values
    for (const auto& [key, val] : dimensions) {
        emf_log[key] = val;
    }

    return emf_log;
}

std::unique_ptr<Metrics::Timer> Metrics::start_timer(
    const std::string& metric_name,
    const DimensionMap& dimensions
) {
    if (!enabled_) return nullptr;
    return std::unique_ptr<Timer>(new Timer(metric_name, dimensions));
}

// Timer implementation
Metrics::Timer::Timer(const std::string& metric_name, const DimensionMap& dimensions)
    : metric_name_(metric_name)
    , dimensions_(dimensions)
    , start_time_(std::chrono::steady_clock::now())
{}

Metrics::Timer::~Timer() {
    try {
        double duration = elapsed_ms();
        if (auto metrics = Metrics::get(); metrics && metrics->is_enabled()) {
            metrics->publish_duration(metric_name_, duration, dimensions_);
        }
    } catch (...) {
        // Silently ignore exceptions in destructor
    }
}

double Metrics::Timer::elapsed_ms() const {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time_
    );
    return static_cast<double>(duration.count());
}

} // namespace gara
