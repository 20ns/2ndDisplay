#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

bool isAndroidInterface(const std::string& interfaceName) {
    std::string lowerName = interfaceName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    
    std::cout << "Checking interface: '" << interfaceName << "' (lower: '" << lowerName << "')" << std::endl;
    
    std::vector<std::string> androidKeywords = {
        "android", "adb", "rndis", "remote ndis", "usb ethernet", "samsung", "galaxy", "pixel", "nexus"
    };
    
    for (const auto& keyword : androidKeywords) {
        if (lowerName.find(keyword) != std::string::npos) {
            std::cout << "  ✅ MATCH: keyword '" << keyword << "'" << std::endl;
            return true;
        }
    }
    
    // Check for secondary ethernet interfaces
    if (lowerName.find("ethernet") != std::string::npos && 
        (lowerName.find("2") != std::string::npos || lowerName.find("3") != std::string::npos || lowerName.find("usb") != std::string::npos)) {
        std::cout << "  ✅ MATCH: secondary ethernet pattern" << std::endl;
        return true;
    }
    
    std::cout << "  ❌ No match" << std::endl;
    return false;
}

int main() {
    std::cout << "TabDisplay Discovery Debug Test" << std::endl;
    std::cout << "===============================" << std::endl;
    
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cout << "WSAStartup failed: " << result << std::endl;
        return 1;
    }
    
    // Get adapter information
    ULONG bufferSize = 0;
    DWORD getResult = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr, &bufferSize);
    
    if (getResult != ERROR_BUFFER_OVERFLOW) {
        std::cout << "Failed to get adapter buffer size: " << getResult << std::endl;
        WSACleanup();
        return 1;
    }
    
    std::cout << "Buffer size needed: " << bufferSize << " bytes" << std::endl;
    
    std::vector<char> buffer(bufferSize);
    PIP_ADAPTER_ADDRESSES adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    
    getResult = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapters, &bufferSize);
    if (getResult != NO_ERROR) {
        std::cout << "Failed to get adapter addresses: " << getResult << std::endl;
        WSACleanup();
        return 1;
    }
    
    std::cout << "\nScanning network adapters..." << std::endl;
    
    int androidDevices = 0;
    
    for (PIP_ADAPTER_ADDRESSES adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
        // Get adapter name
        std::string adapterName;
        if (adapter->FriendlyName) {
            int size = WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1, nullptr, 0, nullptr, nullptr);
            if (size > 0) {
                adapterName.resize(size - 1);
                WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1, adapterName.data(), size, nullptr, nullptr);
            }
        }
        
        std::cout << "\n--- Adapter: " << adapterName << " ---" << std::endl;
        std::cout << "Type: " << adapter->IfType << ", Status: " << adapter->OperStatus << std::endl;
        
        // Skip loopback and non-operational interfaces
        if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK || adapter->OperStatus != IfOperStatusUp) {
            std::cout << "Skipping: loopback or not operational" << std::endl;
            continue;
        }
        
        // Check if this looks like an Android RNDIS interface
        if (!isAndroidInterface(adapterName)) {
            continue;
        }
        
        std::cout << "🎯 POTENTIAL ANDROID INTERFACE FOUND!" << std::endl;
        
        // Get IP addresses for this adapter
        bool foundIP = false;
        for (PIP_ADAPTER_UNICAST_ADDRESS addr = adapter->FirstUnicastAddress; addr != nullptr; addr = addr->Next) {
            if (addr->Address.lpSockaddr->sa_family == AF_INET) {
                sockaddr_in* sockaddr_ipv4 = reinterpret_cast<sockaddr_in*>(addr->Address.lpSockaddr);
                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, ipStr, INET_ADDRSTRLEN);
                
                std::string ipAddress(ipStr);
                std::cout << "  IP Address: " << ipAddress << std::endl;
                foundIP = true;
                
                // Skip localhost
                if (ipAddress.substr(0, 7) == "127.0.0") {
                    std::cout << "    Skipping localhost" << std::endl;
                    continue;
                }
                
                // Calculate potential gateway IP (usually .1)
                size_t lastDot = ipAddress.find_last_of('.');
                if (lastDot != std::string::npos) {
                    std::string gatewayIP = ipAddress.substr(0, lastDot + 1) + "1";
                    std::cout << "  Potential Android Gateway: " << gatewayIP << std::endl;
                    
                    androidDevices++;
                    std::cout << "  ✅ Would attempt connection to: " << gatewayIP << ":5004" << std::endl;
                }
            }
        }
        
        if (!foundIP) {
            std::cout << "  ⚠️  No IPv4 addresses found on this interface" << std::endl;
        }
    }
    
    WSACleanup();
    
    std::cout << "\n===============================" << std::endl;
    std::cout << "Summary: Found " << androidDevices << " potential Android device(s)" << std::endl;
    
    if (androidDevices == 0) {
        std::cout << "❌ No Android devices detected - this matches TabDisplay's behavior" << std::endl;
    } else {
        std::cout << "✅ Android devices should be detectable - there may be a bug in TabDisplay" << std::endl;
    }
    
    return 0;
}
