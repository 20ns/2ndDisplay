#include <Windows.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include "Version.hpp"

// Forward declaration of the tray app function
namespace TabDisplay {
    int RunTrayApp();
}

// Setup logging for the application
void SetupLogging() {
    try {
        // Create console logger
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::debug);

        // Create rotating file logger with 5MB file size and 3 rotations
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "TabDisplay.log", 5 * 1024 * 1024, 3);
        file_sink->set_level(spdlog::level::trace);

        // Create logger with both sinks
        auto logger = std::make_shared<spdlog::logger>("TabDisplay", 
            spdlog::sinks_init_list{console_sink, file_sink});

#ifdef _DEBUG
        logger->set_level(spdlog::level::debug);
#else
        logger->set_level(spdlog::level::info);
#endif

        // Register as default logger
        spdlog::set_default_logger(logger);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

        spdlog::info("TabDisplay v{} starting up", TabDisplay::VERSION_STRING);
    }
    catch (const std::exception& e) {
        // If setup fails, use basic console logger as fallback
        spdlog::set_default_logger(std::make_shared<spdlog::logger>("console_logger", 
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>()));
        spdlog::error("Failed to initialize logging: {}", e.what());
    }
}

// Main entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Setup logging
    SetupLogging();

    // Run the application
    try {
        return TabDisplay::RunTrayApp();
    }
    catch (const std::exception& e) {
        spdlog::critical("Unhandled exception in main: {}", e.what());
        return 1;
    }
}