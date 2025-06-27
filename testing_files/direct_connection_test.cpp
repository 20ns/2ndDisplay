/*
 * TEMPORARY DIRECT CONNECTION PATCH
 * This bypasses the USB discovery and directly connects to the Android device
 * at the known IP address: 192.168.238.161
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// Simple UDP sender for testing
class DirectConnector {
private:
    SOCKET socket_ = INVALID_SOCKET;
    sockaddr_in targetAddr_;
    
public:
    bool initialize(const std::string& targetIP, int targetPort) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cout << "WSAStartup failed" << std::endl;
            return false;
        }
        
        socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ == INVALID_SOCKET) {
            std::cout << "Socket creation failed" << std::endl;
            return false;
        }
        
        targetAddr_.sin_family = AF_INET;
        targetAddr_.sin_port = htons(targetPort);
        inet_pton(AF_INET, targetIP.c_str(), &targetAddr_.sin_addr);
        
        std::cout << "Direct connector initialized for " << targetIP << ":" << targetPort << std::endl;
        return true;
    }
    
    bool sendControlPacket() {
        // Create JSON control packet
        std::string json = R"({
            "type": "keepalive",
            "width": 1920,
            "height": 1080,
            "fps": 30,
            "bitrate": 5000000,
            "touchPort": 5005
        })";
        
        // Create packet header (12 bytes)
        struct PacketHeader {
            uint32_t packetId = 0;
            uint32_t payloadSize;
            uint32_t flags = 4; // FLAG_CONTROL_PACKET
        } header;
        
        header.payloadSize = static_cast<uint32_t>(json.length());
        
        // Send header
        int headerSent = sendto(socket_, reinterpret_cast<const char*>(&header), 
                               sizeof(header), 0, 
                               reinterpret_cast<const sockaddr*>(&targetAddr_), 
                               sizeof(targetAddr_));
        
        // Send payload
        int payloadSent = sendto(socket_, json.c_str(), static_cast<int>(json.length()), 0,
                                reinterpret_cast<const sockaddr*>(&targetAddr_), 
                                sizeof(targetAddr_));
        
        std::cout << "Sent header: " << headerSent << " bytes, payload: " << payloadSent << " bytes" << std::endl;
        return (headerSent > 0 && payloadSent > 0);
    }
    
    ~DirectConnector() {
        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
        }
        WSACleanup();
    }
};

int main() {
    std::cout << "=== DIRECT CONNECTION TEST ===" << std::endl;
    std::cout << "This will directly connect to your Android device" << std::endl;
    std::cout << "Expected Android IP: 192.168.238.161" << std::endl;
    std::cout << std::endl;
    
    DirectConnector connector;
    
    if (!connector.initialize("192.168.238.161", 5004)) {
        std::cout << "Failed to initialize direct connector" << std::endl;
        return 1;
    }
    
    std::cout << "Sending control packet to Android device..." << std::endl;
    
    if (connector.sendControlPacket()) {
        std::cout << "SUCCESS: Control packet sent!" << std::endl;
        std::cout << "Check your Android device - it should show 'Connected to...' status" << std::endl;
    } else {
        std::cout << "FAILED: Could not send control packet" << std::endl;
    }
    
    std::cout << "\nPress Enter to exit...";
    std::cin.get();
    
    return 0;
}
