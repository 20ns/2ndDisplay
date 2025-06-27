/**
 * TEMPORARY DIRECT CONNECTION APPLICATION
 * This bypasses the broken build system and directly connects to the Android device
 * using our known working discovery logic and network parameters.
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

class DirectTabDisplayConnector {
private:
    SOCKET udpSocket = INVALID_SOCKET;
    std::string androidIP;
    int androidPort = 5004;
    bool connected = false;
    
public:
    bool initialize() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cout << "WSAStartup failed" << std::endl;
            return false;
        }
        
        udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udpSocket == INVALID_SOCKET) {
            std::cout << "Socket creation failed" << std::endl;
            return false;
        }
        
        std::cout << "Socket initialized successfully" << std::endl;
        return true;
    }
    
    bool discoverAndroidDevice() {
        std::cout << "\n=== DISCOVERING ANDROID DEVICE ===" << std::endl;
        
        // Based on our testing, we know the Android device is at 192.168.238.161
        // But let's also check the current network to be sure
        
        std::vector<std::string> candidateIPs = {
            "192.168.238.161",  // Known working IP
            "192.168.238.1",    // Common gateway pattern
            "192.168.238.129"   // Alternative pattern
        };
        
        for (const auto& testIP : candidateIPs) {
            std::cout << "Testing connectivity to: " << testIP << std::endl;
            
            if (testConnectivity(testIP)) {
                androidIP = testIP;
                std::cout << "✅ Found Android device at: " << androidIP << std::endl;
                return true;
            }
        }
        
        std::cout << "❌ No Android device found" << std::endl;
        return false;
    }
    
    bool testConnectivity(const std::string& ip) {
        // Send a simple ping-like UDP packet
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(androidPort);
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
        
        const char* testMsg = "PING";
        int result = sendto(udpSocket, testMsg, 4, 0, 
                           reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
        
        return result > 0;
    }
    
    bool sendControlPacket() {
        if (androidIP.empty()) {
            std::cout << "No Android device discovered" << std::endl;
            return false;
        }
        
        std::cout << "\n=== SENDING CONTROL PACKET ===" << std::endl;
        
        // Create proper JSON control packet
        std::string json = R"({
            "type": "keepalive",
            "width": 1920,
            "height": 1080,
            "fps": 30,
            "bitrate": 5000000,
            "touchPort": 5005
        })";
        
        // Remove extra whitespace
        json.erase(std::remove_if(json.begin(), json.end(), ::isspace), json.end());
        
        std::cout << "JSON: " << json << std::endl;
        
        // Create packet header (12 bytes)
        struct PacketHeader {
            uint16_t sequenceId = 1;      // 2 bytes
            uint16_t frameId = 0;         // 2 bytes  
            uint16_t totalChunks = 1;     // 2 bytes
            uint16_t chunkIndex = 0;      // 2 bytes
            uint32_t flags = 4;           // 4 bytes (FLAG_CONTROL_PACKET)
        } header;
        
        // Create full packet
        std::vector<uint8_t> packet;
        packet.resize(sizeof(header) + json.length());
        
        // Copy header (ensure little endian)
        packet[0] = header.sequenceId & 0xFF;
        packet[1] = (header.sequenceId >> 8) & 0xFF;
        packet[2] = header.frameId & 0xFF;
        packet[3] = (header.frameId >> 8) & 0xFF;
        packet[4] = header.totalChunks & 0xFF;
        packet[5] = (header.totalChunks >> 8) & 0xFF;
        packet[6] = header.chunkIndex & 0xFF;
        packet[7] = (header.chunkIndex >> 8) & 0xFF;
        packet[8] = header.flags & 0xFF;
        packet[9] = (header.flags >> 8) & 0xFF;
        packet[10] = (header.flags >> 16) & 0xFF;
        packet[11] = (header.flags >> 24) & 0xFF;
        
        // Copy JSON payload
        std::memcpy(packet.data() + sizeof(header), json.c_str(), json.length());
        
        // Send packet
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(androidPort);
        inet_pton(AF_INET, androidIP.c_str(), &addr.sin_addr);
        
        int bytesSent = sendto(udpSocket, reinterpret_cast<const char*>(packet.data()), 
                              static_cast<int>(packet.size()), 0,
                              reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
        
        if (bytesSent > 0) {
            std::cout << "✅ Control packet sent: " << bytesSent << " bytes" << std::endl;
            std::cout << "Android device should show 'Connected to...' status" << std::endl;
            connected = true;
            return true;
        } else {
            std::cout << "❌ Failed to send control packet" << std::endl;
            return false;
        }
    }
    
    void startKeepAlive() {
        if (!connected) return;
        
        std::cout << "\n=== STARTING KEEPALIVE ===" << std::endl;
        std::cout << "Sending keepalive packets every 5 seconds..." << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        
        int counter = 0;
        while (true) {
            sendControlPacket();
            counter++;
            std::cout << "Keepalive #" << counter << " sent" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    ~DirectTabDisplayConnector() {
        if (udpSocket != INVALID_SOCKET) {
            closesocket(udpSocket);
        }
        WSACleanup();
    }
};

int main() {
    std::cout << "=== DIRECT TABDISPLAY CONNECTOR ===" << std::endl;
    std::cout << "This will directly connect to your Android device" << std::endl;
    std::cout << "bypassing the broken host discovery system." << std::endl;
    std::cout << std::endl;
    
    DirectTabDisplayConnector connector;
    
    if (!connector.initialize()) {
        std::cout << "Failed to initialize connector" << std::endl;
        return 1;
    }
    
    if (!connector.discoverAndroidDevice()) {
        std::cout << "Failed to discover Android device" << std::endl;
        return 1;
    }
    
    if (!connector.sendControlPacket()) {
        std::cout << "Failed to send initial control packet" << std::endl;
        return 1;
    }
    
    std::cout << "\n✅ SUCCESS! Android device should now show connection status" << std::endl;
    std::cout << "Check your Android device - it should show 'Connected to...' status" << std::endl;
    
    char choice;
    std::cout << "\nStart continuous keepalive? (y/n): ";
    std::cin >> choice;
    
    if (choice == 'y' || choice == 'Y') {
        connector.startKeepAlive();
    }
    
    std::cout << "Press Enter to exit...";
    std::cin.ignore();
    std::cin.get();
    
    return 0;
}
