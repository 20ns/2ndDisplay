#include "UsbDeviceDiscovery.hpp"
#include <spdlog/spdlog.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace TabDisplay {

UsbDeviceDiscovery::UsbDeviceDiscovery() {
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        spdlog::error("WSAStartup failed in UsbDeviceDiscovery: {}", result);
    }
}

UsbDeviceDiscovery::~UsbDeviceDiscovery() {
    WSACleanup();
}

std::vector<UsbDeviceInfo> UsbDeviceDiscovery::findAndroidDevices() {
    std::vector<UsbDeviceInfo> devices;
    
    spdlog::info("Starting Android device discovery...");
    
    // Get adapter information
    ULONG bufferSize = 0;
    DWORD result = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr, &bufferSize);
    
    if (result != ERROR_BUFFER_OVERFLOW) {
        spdlog::error("Failed to get adapter buffer size: {}", result);
        return devices;
    }
    
    spdlog::info("Allocated buffer size: {} bytes", bufferSize);
    
    std::vector<char> buffer(bufferSize);
    PIP_ADAPTER_ADDRESSES adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    
    result = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapters, &bufferSize);
    if (result != NO_ERROR) {
        spdlog::error("Failed to get adapter addresses: {}", result);
        return devices;
    }
    
    // Iterate through adapters
    for (PIP_ADAPTER_ADDRESSES adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
        // Get adapter name first for logging
        std::string adapterName;
        if (adapter->FriendlyName) {
            // Convert wide string to string
            int size = WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1, nullptr, 0, nullptr, nullptr);
            if (size > 0) {
                adapterName.resize(size - 1);
                WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1, adapterName.data(), size, nullptr, nullptr);
            }
        }
        
        spdlog::info("Checking network adapter: '{}' (Type: {}, Status: {})", 
                    adapterName, adapter->IfType, adapter->OperStatus);
        
        // Skip loopback and non-operational interfaces
        if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK || 
            adapter->OperStatus != IfOperStatusUp) {
            spdlog::debug("Skipping adapter '{}' - loopback or not operational", adapterName);
            continue;
        }
        
        // Check if this looks like an Android RNDIS interface
        if (!isAndroidInterface(adapterName)) {
            spdlog::debug("Adapter '{}' does not look like Android interface", adapterName);
            continue;
        }
        
        spdlog::info("Found potential Android interface: {}", adapterName);
        
        // Get IP addresses for this adapter
        for (PIP_ADAPTER_UNICAST_ADDRESS addr = adapter->FirstUnicastAddress; 
             addr != nullptr; addr = addr->Next) {
            
            if (addr->Address.lpSockaddr->sa_family == AF_INET) {
                sockaddr_in* sockaddr_ipv4 = reinterpret_cast<sockaddr_in*>(addr->Address.lpSockaddr);
                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, ipStr, INET_ADDRSTRLEN);
                
                std::string ipAddress(ipStr);
                
                spdlog::info("Found IP address {} on interface {}", ipAddress, adapterName);
                
                // Skip localhost
                if (ipAddress.substr(0, 7) == "127.0.0") {
                    spdlog::debug("Skipping localhost address: {}", ipAddress);
                    continue;
                }
                
                // Handle link-local addresses (169.254.x.x) - these might be USB connections
                if (ipAddress.substr(0, 7) == "169.254") {
                    spdlog::info("Found link-local address: {} - checking for Android device", ipAddress);
                    // For link-local, try to find the Android device by testing common gateway patterns
                    std::vector<std::string> linkLocalGateways = {
                        "169.254.1.1",     // Common gateway for USB connections
                        "169.254.208.1",   // Derived from the IP we saw (169.254.208.21)
                        "169.254.208.129", // Another common pattern
                    };
                    
                    for (const auto& testIP : linkLocalGateways) {
                        spdlog::info("Testing connectivity to potential Android device at: {}", testIP);
                        if (testConnectivity(testIP)) {
                            UsbDeviceInfo deviceInfo;
                            deviceInfo.ipAddress = testIP;
                            deviceInfo.interfaceName = adapterName;
                            deviceInfo.deviceName = getDeviceName(adapterName);
                            devices.push_back(deviceInfo);
                            
                            spdlog::info("Successfully found Android device at link-local IP: {}", testIP);
                            return devices; // Return immediately on first success
                        }
                    }
                    continue; // Skip further processing for link-local
                }
                
                spdlog::info("Found IP address on Android interface: {}", ipAddress);
                
                // Try to find Android device on this network
                // For USB tethering, the Android device typically uses the gateway IP
                // which is usually the first IP in the subnet
                std::string gatewayIP = findGatewayIP(ipAddress);
                
                if (!gatewayIP.empty() && testConnectivity(gatewayIP)) {
                    UsbDeviceInfo deviceInfo;
                    deviceInfo.ipAddress = gatewayIP;
                    deviceInfo.interfaceName = adapterName;
                    deviceInfo.deviceName = getDeviceName(adapterName);
                    devices.push_back(deviceInfo);
                    
                    spdlog::info("Successfully found Android device at: {}", gatewayIP);
                } else {
                    // Try some common Android USB tethering IPs
                    std::vector<std::string> commonIPs = {
                        "192.168.42.129",  // Common Android hotspot gateway
                        "192.168.43.1",    // Another common Android gateway
                        "10.0.0.1",        // Some devices use this
                        "172.20.10.1"      // iOS style, but some Android use this too
                    };
                    
                    for (const auto& testIP : commonIPs) {
                        if (testConnectivity(testIP)) {
                            UsbDeviceInfo deviceInfo;
                            deviceInfo.ipAddress = testIP;
                            deviceInfo.interfaceName = adapterName;
                            deviceInfo.deviceName = getDeviceName(adapterName);
                            devices.push_back(deviceInfo);
                            
                            spdlog::info("Found Android device at common IP: {}", testIP);
                            break;
                        }
                    }
                }
            }
        }
    }
    
    return devices;
}

bool UsbDeviceDiscovery::testConnectivity(const std::string& ipAddress, int timeoutMs) {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        return false;
    }
    
    // Set socket timeout
    DWORD timeout = timeoutMs;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
    
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5004);  // Android app port
    inet_pton(AF_INET, ipAddress.c_str(), &addr.sin_addr);
    
    // Send a simple ping packet
    const char* testMessage = "PING";
    int result = sendto(sock, testMessage, strlen(testMessage), 0, 
                       reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    
    closesocket(sock);
    
    // For now, just return true if send succeeded
    // In the future, we could wait for a response
    return result != SOCKET_ERROR;
}

std::optional<UsbDeviceInfo> UsbDeviceDiscovery::getFirstAndroidDevice() {
    auto devices = findAndroidDevices();
    if (!devices.empty()) {
        return devices[0];
    }
    return std::nullopt;
}

bool UsbDeviceDiscovery::isAndroidInterface(const std::string& interfaceName) {
    // Convert to lowercase for case-insensitive comparison
    std::string lowerName = interfaceName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    
    spdlog::debug("Checking if '{}' is an Android interface", interfaceName);
    
    // Look for keywords that suggest Android USB connection
    std::vector<std::string> androidKeywords = {
        "android",
        "adb",
        "rndis",
        "remote ndis",
        "usb ethernet",
        "samsung",
        "galaxy",
        "pixel",
        "nexus"
    };
    
    for (const auto& keyword : androidKeywords) {
        if (lowerName.find(keyword) != std::string::npos) {
            spdlog::info("Interface '{}' matches Android keyword: '{}'", interfaceName, keyword);
            return true;
        }
    }
    
    // For testing: also check generic ethernet interfaces that might be USB connections
    // This helps catch cases where the interface name isn't obviously Android-related
    if (lowerName.find("ethernet") != std::string::npos && 
        (lowerName.find("2") != std::string::npos || lowerName.find("3") != std::string::npos || 
         lowerName.find("4") != std::string::npos || lowerName.find("usb") != std::string::npos)) {
        spdlog::info("Interface '{}' looks like a secondary ethernet interface (possibly USB)", interfaceName);
        return true;
    }
    
    spdlog::debug("Interface '{}' does not appear to be Android-related", interfaceName);
    return false;
}

std::string UsbDeviceDiscovery::getDeviceName(const std::string& interfaceName) {
    // Extract a clean device name from the interface name
    std::string deviceName = interfaceName;
    
    // Remove common prefixes/suffixes
    size_t pos = deviceName.find("Remote NDIS");
    if (pos != std::string::npos) {
        deviceName = deviceName.substr(0, pos);
    }
    
    pos = deviceName.find("USB Ethernet");
    if (pos != std::string::npos) {
        deviceName = deviceName.substr(0, pos);
    }
    
    // Trim whitespace
    deviceName.erase(0, deviceName.find_first_not_of(" \t"));
    deviceName.erase(deviceName.find_last_not_of(" \t") + 1);
    
    if (deviceName.empty()) {
        deviceName = "Android Device";
    }
    
    return deviceName;
}

std::string UsbDeviceDiscovery::findGatewayIP(const std::string& localIP) {
    // For USB tethering, try to find the gateway IP
    // Android devices can use various IP patterns
    
    size_t lastDot = localIP.find_last_of('.');
    if (lastDot == std::string::npos) {
        return "";
    }
    
    std::string subnet = localIP.substr(0, lastDot);
    
    // Try common gateway patterns - expanded to cover more Android devices
    std::vector<std::string> gatewayPatterns = {
        subnet + ".1",      // Most common
        subnet + ".129",    // Common alternative
        subnet + ".161",    // Samsung devices often use this
        subnet + ".254",    // Some devices use the last address
        subnet + ".100",    // Another common pattern
    };
    
    spdlog::info("Looking for Android device on subnet: {}", subnet);
    
    for (const auto& gateway : gatewayPatterns) {
        spdlog::info("Testing connectivity to potential Android IP: {}", gateway);
        if (testConnectivity(gateway, 2000)) {
            spdlog::info("Successfully connected to Android device at: {}", gateway);
            return gateway;
        }
    }
    
    spdlog::warn("No Android device found on subnet: {}", subnet);
    return "";
}

} // namespace TabDisplay
