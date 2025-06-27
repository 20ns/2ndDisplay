#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

bool testConnectivity(const std::string& ipAddress, int port = 5004, int timeoutMs = 1000) {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cout << "WSAStartup failed: " << result << std::endl;
        return false;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        std::cout << "Socket creation failed" << std::endl;
        WSACleanup();
        return false;
    }
    
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ipAddress.c_str(), &addr.sin_addr);
    
    // Send a simple ping packet
    const char* testMessage = "PING";
    int sendResult = sendto(sock, testMessage, strlen(testMessage), 0, 
                           reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    
    bool success = (sendResult != SOCKET_ERROR);
    
    closesocket(sock);
    WSACleanup();
    
    return success;
}

int main() {
    std::cout << "TabDisplay Android Connectivity Test" << std::endl;
    std::cout << "====================================" << std::endl;
    
    // Test potential Android device IPs based on our network analysis
    std::vector<std::string> testIPs = {
        "192.168.238.1",     // Most likely gateway for 192.168.238.134
        "192.168.238.129",   // Common Android pattern
        "192.168.42.129",    // Common Android hotspot IP
        "192.168.43.1",      // Another common Android gateway
        "169.254.1.1",       // Link-local gateway
        "169.254.208.1",     // Based on our earlier 169.254.208.21
    };
    
    std::cout << "\\nTesting connectivity to potential Android device IPs..." << std::endl;
    
    bool foundDevice = false;
    for (const auto& ip : testIPs) {
        std::cout << "Testing " << ip << ":5004... ";
        
        if (testConnectivity(ip)) {
            std::cout << "✅ SUCCESS - Android device may be at this IP!" << std::endl;
            foundDevice = true;
        } else {
            std::cout << "❌ No response" << std::endl;
        }
    }
    
    std::cout << std::endl;
    
    if (foundDevice) {
        std::cout << "🎉 Found potential Android device!" << std::endl;
        std::cout << "The TabDisplay discovery should be able to connect." << std::endl;
    } else {
        std::cout << "⚠️  No Android device responded to ping." << std::endl;
        std::cout << "This could mean:" << std::endl;
        std::cout << "1. Android app is not running" << std::endl;
        std::cout << "2. Android firewall is blocking UDP port 5004" << std::endl;
        std::cout << "3. IP address is different than expected" << std::endl;
    }
    
    std::cout << "\\nTry connecting through TabDisplay tray icon to see detailed logs." << std::endl;
    
    return 0;
}
