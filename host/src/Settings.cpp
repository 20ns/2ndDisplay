#include "Settings.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <filesystem>
#include <Windows.h>
#include <ShlObj.h>

namespace TabDisplay {

Settings& Settings::instance()
{
    static Settings instance;
    return instance;
}

Settings::Settings()
    : width_(DEFAULT_WIDTH)
    , height_(DEFAULT_HEIGHT)
    , frameRate_(DEFAULT_FPS)
    , bitrate_(DEFAULT_BITRATE)
{
    // Load settings on construction
    load();
}

Settings::~Settings()
{
    // Save settings on destruction
    save();
}

bool Settings::load()
{
    try {
        // Get settings file path
        std::string path = getSettingsPath();
        
        // Check if file exists
        if (!std::filesystem::exists(path)) {
            spdlog::info("Settings file not found, using defaults");
            return false;
        }
        
        // Open file
        std::ifstream file(path);
        if (!file.is_open()) {
            spdlog::error("Failed to open settings file: {}", path);
            return false;
        }
        
        // Parse JSON
        nlohmann::json j;
        file >> j;
        file.close();
        
        // Load settings
        fromJson(j);
        
        spdlog::info("Settings loaded from {}", path);
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to load settings: {}", e.what());
        return false;
    }
}

bool Settings::save() const
{
    try {
        // Get settings file path
        std::string path = getSettingsPath();
        
        // Create directory if it doesn't exist
        ensureDirectoryExists(std::filesystem::path(path).parent_path().string());
        
        // Open file
        std::ofstream file(path);
        if (!file.is_open()) {
            spdlog::error("Failed to open settings file for writing: {}", path);
            return false;
        }
        
        // Convert to JSON
        nlohmann::json j = toJson();
        
        // Write JSON to file
        file << j.dump(2);
        file.close();
        
        spdlog::info("Settings saved to {}", path);
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to save settings: {}", e.what());
        return false;
    }
}

uint32_t Settings::getWidth() const
{
    return width_;
}

uint32_t Settings::getHeight() const
{
    return height_;
}

void Settings::setResolution(uint32_t width, uint32_t height)
{
    width_ = width;
    height_ = height;
}

uint32_t Settings::getFrameRate() const
{
    return frameRate_;
}

void Settings::setFrameRate(uint32_t fps)
{
    frameRate_ = fps;
}

uint32_t Settings::getBitrate() const
{
    return bitrate_;
}

void Settings::setBitrate(uint32_t bitrateInMbps)
{
    bitrate_ = bitrateInMbps;
}

std::string Settings::getLastConnectedDevice() const
{
    return lastConnectedDevice_;
}

void Settings::setLastConnectedDevice(const std::string& deviceIp)
{
    lastConnectedDevice_ = deviceIp;
}

std::string Settings::getSettingsPath() const
{
    PWSTR localAppDataPath = nullptr;
    std::string settingsDir;
    
    // Get local app data folder
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &localAppDataPath))) {
        std::wstring widePath(localAppDataPath);
        settingsDir = std::string(widePath.begin(), widePath.end());
        CoTaskMemFree(localAppDataPath);
    }
    else {
        // Fallback to environment variable
        char* envPath = nullptr;
        size_t len = 0;
        if (_dupenv_s(&envPath, &len, "LOCALAPPDATA") == 0 && envPath != nullptr) {
            settingsDir = envPath;
            free(envPath);
        }
        else {
            spdlog::error("Failed to get LocalAppData path");
            throw std::runtime_error("Failed to get LocalAppData path");
        }
    }
    
    // Combine paths
    return settingsDir + "\\TabDisplay\\settings.json";
}

void Settings::ensureDirectoryExists(const std::string& path) const
{
    std::error_code ec;
    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path, ec);
        if (ec) {
            spdlog::error("Failed to create directory: {}, error: {}", path, ec.message());
        }
    }
}

nlohmann::json Settings::toJson() const
{
    return {
        {"version", "0.1.0"},
        {"width", width_},
        {"height", height_},
        {"frameRate", frameRate_},
        {"bitrate", bitrate_},
        {"lastConnectedDevice", lastConnectedDevice_}
    };
}

void Settings::fromJson(const nlohmann::json& j)
{
    // Get settings with defaults if missing
    width_ = j.value("width", DEFAULT_WIDTH);
    height_ = j.value("height", DEFAULT_HEIGHT);
    frameRate_ = j.value("frameRate", DEFAULT_FPS);
    bitrate_ = j.value("bitrate", DEFAULT_BITRATE);
    lastConnectedDevice_ = j.value("lastConnectedDevice", "");
}

} // namespace TabDisplay