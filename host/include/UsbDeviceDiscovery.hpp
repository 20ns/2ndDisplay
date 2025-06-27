#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <string>
#include <vector>
#include <optional>

namespace TabDisplay {

struct UsbDeviceInfo {
    std::string ipAddress;
    std::string interfaceName;
    std::string deviceName;
};

class UsbDeviceDiscovery {
public:
    UsbDeviceDiscovery();
    ~UsbDeviceDiscovery();

    // Find Android devices connected via USB tethering (RNDIS)
    std::vector<UsbDeviceInfo> findAndroidDevices();
    
    // Test connectivity to a device on port 5004
    bool testConnectivity(const std::string& ipAddress, int timeoutMs = 2000);
    
    // Get the first available Android device
    std::optional<UsbDeviceInfo> getFirstAndroidDevice();

private:
    // Helper to enumerate network interfaces
    std::vector<std::string> getNetworkInterfaces();
    
    // Helper to get IP addresses for an interface
    std::vector<std::string> getInterfaceIPs(const std::string& interfaceName);
    
    // Check if an interface name suggests it's an Android USB connection
    bool isAndroidInterface(const std::string& interfaceName);
    
    // Get device name from interface
    std::string getDeviceName(const std::string& interfaceName);
    
    // Find gateway IP for a given local IP
    std::string findGatewayIP(const std::string& localIP);
};

} // namespace TabDisplay
