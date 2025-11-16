# Logging and Metrics Guide

## Overview

This document describes the logging and metrics infrastructure for the Gara Image Service, optimized for AWS ECS and CloudWatch integration.

## Architecture

### Logging
- **Framework**: spdlog with JSON structured logging
- **Output**: stdout/stderr (automatically captured by CloudWatch Logs in ECS)
- **Format**: JSON for CloudWatch Logs Insights queries
- **Features**:
  - Structured JSON logs with contextual fields
  - Request correlation IDs for distributed tracing
  - Multiple log levels (trace, debug, info, warn, error, critical)
  - ISO 8601 timestamps

### Metrics
- **Format**: CloudWatch Embedded Metric Format (EMF)
- **Output**: stdout (automatically extracted to CloudWatch Metrics)
- **Features**:
  - Zero additional infrastructure required
  - Automatic CloudWatch Metrics creation
  - Custom dimensions for filtering
  - Support for counters, gauges, and timers

## Configuration

### Environment Variables

Configure logging and metrics via environment variables in `.env`:

```bash
# Logging Configuration
LOG_LEVEL=info              # trace, debug, info, warn, error, critical
LOG_FORMAT=json             # json (CloudWatch) or text (local dev)
ENVIRONMENT=production      # Used for log grouping

# Metrics Configuration
METRICS_ENABLED=true        # Enable/disable metrics
METRICS_NAMESPACE=GaraImage # CloudWatch metrics namespace
```

### ECS Task Definition

Ensure your ECS task definition includes CloudWatch Logs configuration:

```json
{
  "logConfiguration": {
    "logDriver": "awslogs",
    "options": {
      "awslogs-group": "/ecs/gara-image",
      "awslogs-region": "us-east-1",
      "awslogs-stream-prefix": "ecs"
    }
  }
}
```

## Usage Examples

### Structured Logging

#### Basic Logging

```cpp
#include "utils/logger.h"

// Simple log messages
LOG_INFO("Server started successfully");
LOG_WARN("Cache miss for image: {}", image_id);
LOG_ERROR("Failed to upload file: {}", error_message);
```

#### Structured Logging with Context

```cpp
#include "utils/logger.h"

// Add contextual fields as JSON
gara::Logger::log_structured(spdlog::level::info, "Image uploaded", {
    {"image_id", "abc123"},
    {"size_bytes", 1024000},
    {"format", "jpeg"},
    {"duration_ms", 145}
});
```

Output (JSON to CloudWatch Logs):
```json
{
  "timestamp": "2025-11-16T10:30:45.123Z",
  "level": "INFO",
  "service": "gara-image",
  "environment": "production",
  "message": "Image uploaded",
  "image_id": "abc123",
  "size_bytes": 1024000,
  "format": "jpeg",
  "duration_ms": 145
}
```

#### Error Logging with Exception

```cpp
try {
    // ... some operation
} catch (const std::exception& e) {
    gara::Logger::log_error("Image processing failed", e);
}
```

### Metrics

#### Counters

Track occurrences of events:

```cpp
#include "utils/metrics.h"

// Simple counter
METRICS_COUNT("ImageUploads");

// Counter with custom value and dimensions
METRICS_COUNT("CacheHits", 1.0, "Count", {
    {"operation", "transform"},
    {"cache_type", "s3"}
});
```

#### Gauges

Track current values:

```cpp
// Track current queue size
METRICS_GAUGE("ActiveRequests", active_count, "Count");

// With dimensions
METRICS_GAUGE("CacheSize", cache_bytes, "Bytes", {
    {"cache_type", "transformed"}
});
```

#### Timers

Measure operation duration:

```cpp
// Automatic timing (publishes duration on scope exit)
{
    auto timer = gara::Metrics::get()->start_timer("ImageProcessingDuration");

    // ... perform image processing ...

} // Timer automatically publishes duration when destroyed

// With dimensions
auto timer = gara::Metrics::get()->start_timer("S3UploadDuration", {
    {"operation", "put_object"}
});
```

#### Manual Duration Tracking

```cpp
auto start = std::chrono::steady_clock::now();

// ... do work ...

auto end = std::chrono::steady_clock::now();
auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    end - start
).count();

METRICS_DURATION("CustomOperation", duration_ms);
```

## CloudWatch Logs Insights Queries

### Query Examples

**Find all errors in the last hour:**
```
fields @timestamp, message, error_message, image_id
| filter level = "ERROR"
| sort @timestamp desc
| limit 100
```

**Track cache hit rate:**
```
fields @timestamp
| filter message = "Cache hit" or message = "Cache miss - transforming image"
| stats count(*) as total by message
```

**Average image processing duration:**
```
fields duration_ms
| filter message = "Cache miss - transforming image"
| stats avg(duration_ms) as avg_duration, max(duration_ms) as max_duration
```

**Errors by type:**
```
fields @timestamp, error_code, error_message
| filter level = "ERROR"
| stats count() by error_code
| sort count() desc
```

## CloudWatch Metrics

### Available Metrics

The following metrics are automatically published to CloudWatch:

| Metric Name | Type | Description | Dimensions |
|------------|------|-------------|------------|
| `CacheHits` | Counter | Number of cache hits | operation |
| `CacheMisses` | Counter | Number of cache misses | operation |
| `ImageTransformDuration` | Timer | Time to transform images (ms) | - |
| `S3UploadDuration` | Timer | Time to upload to S3 (ms) | operation |
| `S3Operations` | Counter | S3 operation count | operation, status |
| `S3Errors` | Counter | S3 error count | error_type |

All metrics include default dimensions:
- `ServiceName`: gara-image
- `Environment`: production/staging/dev

### Creating CloudWatch Dashboards

Example dashboard widget for cache hit rate:

```json
{
  "type": "metric",
  "properties": {
    "metrics": [
      [ "GaraImage", "CacheHits", { "stat": "Sum" } ],
      [ ".", "CacheMisses", { "stat": "Sum" } ]
    ],
    "period": 300,
    "stat": "Sum",
    "region": "us-east-1",
    "title": "Cache Performance"
  }
}
```

### Setting Up Alarms

Example alarm for high error rate:

```bash
aws cloudwatch put-metric-alarm \
  --alarm-name gara-image-high-errors \
  --alarm-description "Alert when S3 errors exceed threshold" \
  --metric-name S3Errors \
  --namespace GaraImage \
  --statistic Sum \
  --period 300 \
  --evaluation-periods 2 \
  --threshold 10 \
  --comparison-operator GreaterThanThreshold
```

## Request Correlation

Every request automatically receives a unique request ID via the `RequestContextMiddleware`:

```cpp
// Request ID is automatically added to response headers
X-Request-ID: a1b2c3d4-e5f6-7890-...

// And available in logs
{
  "request_id": "a1b2c3d4-e5f6-7890",
  "message": "Processing image",
  ...
}
```

### Tracking Requests Across Services

Use the request ID to trace a request through the entire system:

```
fields @timestamp, message, request_id, operation
| filter request_id = "a1b2c3d4-e5f6-7890"
| sort @timestamp asc
```

## Health Checks

The `/health` endpoint provides service health status with dependency checks:

```bash
curl http://localhost:8080/health
```

Response:
```json
{
  "status": "healthy",
  "timestamp": "2025-11-16T10:30:45.123Z",
  "services": {
    "s3": "ok",
    "secrets_manager": "ok"
  }
}
```

Status codes:
- `200`: All services healthy
- `503`: Service degraded (some dependencies unavailable)

## Best Practices

### Logging

1. **Use appropriate log levels:**
   - `TRACE`: Very detailed debugging (disabled in production)
   - `DEBUG`: Debugging information
   - `INFO`: General information about application flow
   - `WARN`: Warning messages (non-critical issues)
   - `ERROR`: Error messages (operation failed)
   - `CRITICAL`: Critical errors (service cannot continue)

2. **Include contextual information:**
   ```cpp
   // Good - includes context
   gara::Logger::log_structured(spdlog::level::err, "Upload failed", {
       {"image_id", image_id},
       {"error_code", error_code},
       {"retry_count", retries}
   });

   // Bad - missing context
   LOG_ERROR("Upload failed");
   ```

3. **Don't log sensitive data:**
   - Never log API keys, passwords, or authentication tokens
   - Avoid logging full credit card numbers or PII
   - Use sanitized versions or hashes when necessary

4. **Use structured logging for machine parsing:**
   - Prefer `log_structured()` over simple string messages
   - Add dimensions that help with filtering and aggregation

### Metrics

1. **Use dimensions wisely:**
   - Add dimensions that are useful for filtering
   - Avoid high-cardinality dimensions (unique IDs)
   - Keep dimension count reasonable (< 10 per metric)

2. **Use timers for performance tracking:**
   ```cpp
   // Automatic timing
   auto timer = Metrics::get()->start_timer("OperationName");
   ```

3. **Track both successes and failures:**
   ```cpp
   if (success) {
       METRICS_COUNT("Operations", 1.0, "Count", {{"status", "success"}});
   } else {
       METRICS_COUNT("Operations", 1.0, "Count", {{"status", "failure"}});
   }
   ```

4. **Use meaningful metric names:**
   - Use PascalCase (e.g., `ImageProcessingDuration`)
   - Be specific (e.g., `S3UploadDuration` not `UploadTime`)
   - Include units in description

## Migrating Existing Code

To migrate existing `std::cout` / `std::cerr` logging:

### Before:
```cpp
std::cout << "Processing image: " << image_id << std::endl;
std::cerr << "Error: " << error_message << std::endl;
```

### After:
```cpp
#include "utils/logger.h"

gara::Logger::log_structured(spdlog::level::info, "Processing image", {
    {"image_id", image_id}
});

gara::Logger::log_structured(spdlog::level::err, "Processing failed", {
    {"error_message", error_message},
    {"image_id", image_id}
});
```

## Troubleshooting

### Logs Not Appearing in CloudWatch

1. Check ECS task definition has correct `logConfiguration`
2. Verify IAM role has `logs:CreateLogGroup`, `logs:CreateLogStream`, `logs:PutLogEvents` permissions
3. Check log group exists: `/ecs/gara-image`

### Metrics Not Showing in CloudWatch

1. Ensure `METRICS_ENABLED=true` in environment variables
2. Verify EMF logs are being output (check CloudWatch Logs)
3. Check metrics namespace matches: `GaraImage`
4. Wait up to 5 minutes for metrics to appear

### Performance Issues

1. Reduce log level in production (use `info` or `warn`)
2. Limit high-frequency metrics (consider sampling)
3. Use async logging if needed (spdlog supports this)

## Additional Resources

- [CloudWatch Logs Insights Query Syntax](https://docs.aws.amazon.com/AmazonCloudWatch/latest/logs/CWL_QuerySyntax.html)
- [CloudWatch Embedded Metric Format](https://docs.aws.amazon.com/AmazonCloudWatch/latest/monitoring/CloudWatch_Embedded_Metric_Format.html)
- [spdlog Documentation](https://github.com/gabime/spdlog)
