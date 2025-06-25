#pragma once

#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace TabDisplay {

class Settings {
public:
    // Singleton access
    static Settings& instance();

    // Load settings from file
    bool load();

    // Save settings to file
    bool save() const;

    // Get/set resolution
    uint32_t getWidth() const;
    uint32_t getHeight() const;
    void setResolution(uint32_t width, uint32_t height);

    // Get/set frame rate
    uint32_t getFrameRate() const;
    void setFrameRate(uint32_t fps);

    // Get/set bitrate
    uint32_t getBitrate() const;
    void setBitrate(uint32_t bitrateInMbps);

    // Get/set last connected device
    std::string getLastConnectedDevice() const;
    void setLastConnectedDevice(const std::string& deviceIp);

    // Default settings
    static constexpr uint32_t DEFAULT_WIDTH = 1920;
    static constexpr uint32_t DEFAULT_HEIGHT = 1080;
    static constexpr uint32_t DEFAULT_FPS = 60;
    static constexpr uint32_t DEFAULT_BITRATE = 30;  // Mbps

private:
    // Private constructor for singleton
    Settings();
    ~Settings();

    // Settings path
    std::string getSettingsPath() const;
    void ensureDirectoryExists(const std::string& path) const;

    // Settings data
    uint32_t width_;
    uint32_t height_;
    uint32_t frameRate_;
    uint32_t bitrate_;
    std::string lastConnectedDevice_;

    // Json serialization
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

} // namespace TabDisplay