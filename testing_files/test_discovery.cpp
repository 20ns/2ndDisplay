#include <iostream>
#include <spdlog/spdlog.h>
#include "UsbDeviceDiscovery.hpp"

int main() {
    // Set console logging
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    
    std::cout << "TabDisplay USB Discovery Test" << std::endl;
    std::cout << "==============================" << std::endl;
    
    try {
        TabDisplay::UsbDeviceDiscovery discovery;
        
        std::cout << "\nSearching for Android devices..." << std::endl;
        auto devices = discovery.findAndroidDevices();
        
        if (devices.empty()) {
            std::cout << "\nNo Android devices found." << std::endl;
            std::cout << "Make sure:" << std::endl;
            std::cout << "1. Android device is connected via USB" << std::endl;
            std::cout << "2. USB tethering is enabled" << std::endl;
            std::cout << "3. Device appears in network interfaces" << std::endl;
        } else {
            std::cout << "\nFound " << devices.size() << " Android device(s):" << std::endl;
            for (size_t i = 0; i < devices.size(); ++i) {
                const auto& device = devices[i];
                std::cout << "  " << (i+1) << ". " << device.deviceName << std::endl;
                std::cout << "     IP: " << device.ipAddress << std::endl;
                std::cout << "     Interface: " << device.interfaceName << std::endl;
                std::cout << std::endl;
            }
        }
        
        // Test getFirstAndroidDevice
        auto firstDevice = discovery.getFirstAndroidDevice();
        if (firstDevice) {
            std::cout << "First device for connection: " << firstDevice->deviceName 
                      << " (" << firstDevice->ipAddress << ")" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
    
    std::cout << "\nPress Enter to exit..." << std::endl;
    std::cin.get();
    
    return 0;
}
