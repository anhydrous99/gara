#ifndef GARA_WATERMARK_CONFIG_H
#define GARA_WATERMARK_CONFIG_H

#include <string>
#include <cstdlib>

namespace gara {

struct WatermarkConfig {
    bool enabled;
    std::string text;
    std::string position;      // "bottom-right", "top-left", "top-right", "bottom-left"
    int base_font_size;        // Base font size (will scale with image)
    std::string font_color;    // "white", "black", or hex color
    double opacity;            // 0.0 to 1.0
    int margin;                // Pixels from edge

    // Default constructor with sensible defaults
    WatermarkConfig()
        : enabled(true),
          text("© 2025 Armando Herrera"),
          position("bottom-right"),
          base_font_size(24),
          font_color("white"),
          opacity(0.9),
          margin(20) {}

    // Factory method to create config from environment variables
    static WatermarkConfig fromEnvironment() {
        WatermarkConfig config;

        // Read from environment variables with defaults
        const char* enabled_env = std::getenv("WATERMARK_ENABLED");
        if (enabled_env) {
            std::string enabled_str(enabled_env);
            config.enabled = (enabled_str == "true" || enabled_str == "1");
        }

        const char* text_env = std::getenv("WATERMARK_TEXT");
        if (text_env) {
            config.text = text_env;
        }

        const char* position_env = std::getenv("WATERMARK_POSITION");
        if (position_env) {
            config.position = position_env;
        }

        const char* font_size_env = std::getenv("WATERMARK_FONT_SIZE");
        if (font_size_env) {
            config.base_font_size = std::atoi(font_size_env);
        }

        const char* color_env = std::getenv("WATERMARK_COLOR");
        if (color_env) {
            config.font_color = color_env;
        }

        const char* opacity_env = std::getenv("WATERMARK_OPACITY");
        if (opacity_env) {
            config.opacity = std::atof(opacity_env);
        }

        const char* margin_env = std::getenv("WATERMARK_MARGIN");
        if (margin_env) {
            config.margin = std::atoi(margin_env);
        }

        return config;
    }

    // Validate configuration
    bool isValid() const {
        if (text.empty()) return false;
        if (base_font_size <= 0 || base_font_size > 200) return false;
        if (opacity < 0.0 || opacity > 1.0) return false;
        if (margin < 0 || margin > 500) return false;

        // Validate position
        if (position != "bottom-right" && position != "bottom-left" &&
            position != "top-right" && position != "top-left") {
            return false;
        }

        return true;
    }
};

} // namespace gara

#endif // GARA_WATERMARK_CONFIG_H
