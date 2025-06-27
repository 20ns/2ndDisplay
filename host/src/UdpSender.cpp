#include "UdpSender.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <algorithm>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

namespace TabDisplay {

UdpSender::UdpSender()
    : socket_(INVALID_SOCKET)
    , nextSequenceId_(0)
    , nextFrameId_(0)
    , framesSinceKeepAlive_(0)
    , running_(false)
    , currentBandwidthMbps_(0.0)
    , currentLatencyMs_(0.0)
    , listeningPort_(0)
{
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        spdlog::error("WSAStartup failed: {}", result);
    }
}

UdpSender::~UdpSender()
{
    stopReceiving();
    
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    
    WSACleanup();
}

bool UdpSender::initialize(const std::string& remoteIp, uint16_t remotePort)
{
    // Create UDP socket
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == INVALID_SOCKET) {
        spdlog::error("Failed to create socket: {}", WSAGetLastError());
        return false;
    }
    
    // Enable broadcast
    int broadcastEnable = 1;
    if (setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, (char*)&broadcastEnable, sizeof(broadcastEnable)) < 0) {
        spdlog::error("Failed to set SO_BROADCAST: {}", WSAGetLastError());
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
        return false;
    }

    // Set receive buffer size
    int recvBufSize = 1024 * 1024; // 1MB buffer
    if (setsockopt(socket_, SOL_SOCKET, SO_RCVBUF, (char*)&recvBufSize, sizeof(recvBufSize)) < 0) {
        spdlog::error("Failed to set receive buffer size: {}", WSAGetLastError());
    }

    // Set send buffer size
    int sendBufSize = 1024 * 1024; // 1MB buffer
    if (setsockopt(socket_, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufSize, sizeof(sendBufSize)) < 0) {
        spdlog::error("Failed to set send buffer size: {}", WSAGetLastError());
    }

    // Set remote address
    memset(&remoteAddr_, 0, sizeof(remoteAddr_));
    remoteAddr_.sin_family = AF_INET;
    remoteAddr_.sin_port = htons(remotePort);
    remoteAddr_.sin_addr.s_addr = inet_addr(remoteIp.c_str());
    
    spdlog::info("UDP sender initialized, target: {}:{}", remoteIp, remotePort);
    return true;
}

bool UdpSender::sendFrame(const std::vector<uint8_t>& frameData, bool isKeyFrame)
{
    if (socket_ == INVALID_SOCKET) {
        spdlog::error("Socket not initialized");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(sendMutex_);
    
    // Get next frame ID
    uint16_t frameId = nextFrameId_++;
    
    // Split frame into packets
    auto packets = createPackets(frameData, frameId, nextSequenceId_, isKeyFrame);
    
    // Update next sequence ID
    nextSequenceId_ += static_cast<uint16_t>(packets.size() + 2); // +2 for parity packets
    
    // Create parity packets
    auto parityPackets = createParityChunks(packets, frameId, nextSequenceId_ - 2);
    
    // Track bandwidth for statistics
    size_t totalBytes = 0;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Send all packets
    for (const auto& packet : packets) {
        int bytesSent = sendto(socket_, 
                              reinterpret_cast<const char*>(packet.data()), 
                              static_cast<int>(packet.size()), 
                              0, 
                              reinterpret_cast<const sockaddr*>(&remoteAddr_), 
                              sizeof(remoteAddr_));
        
        if (bytesSent != packet.size()) {
            spdlog::error("Failed to send packet: {}", WSAGetLastError());
        }
        
        totalBytes += packet.size();
    }
    
    // Send parity packets
    for (const auto& packet : parityPackets) {
        int bytesSent = sendto(socket_, 
                              reinterpret_cast<const char*>(packet.data()), 
                              static_cast<int>(packet.size()), 
                              0, 
                              reinterpret_cast<const sockaddr*>(&remoteAddr_), 
                              sizeof(remoteAddr_));
        
        if (bytesSent != packet.size()) {
            spdlog::error("Failed to send parity packet: {}", WSAGetLastError());
        }
        
        totalBytes += packet.size();
    }
    
    // Update bandwidth calculation
    auto endTime = std::chrono::high_resolution_clock::now();
    double seconds = std::chrono::duration<double>(endTime - startTime).count();
    if (seconds > 0) {
        double mbps = (totalBytes * 8.0 / 1000000.0) / seconds;
        // Update with moving average
        currentBandwidthMbps_ = currentBandwidthMbps_ * 0.8 + mbps * 0.2;
    }
    
    // Check if we need to send a keepalive
    framesSinceKeepAlive_++;
    if (framesSinceKeepAlive_ >= KEEPALIVE_INTERVAL) {
        framesSinceKeepAlive_ = 0;
        // We'll get these parameters from elsewhere in practice
        sendKeepAlive(1920, 1080, 60, static_cast<uint32_t>(currentBandwidthMbps_));
    }
    
    return true;
}

bool UdpSender::sendKeepAlive(uint32_t width, uint32_t height, uint32_t fps, uint32_t bitrate)
{
    if (socket_ == INVALID_SOCKET) {
        spdlog::error("Socket not initialized");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(sendMutex_);
    
    // Create control packet
    auto packet = createControlPacket(nextSequenceId_++, width, height, fps, bitrate, listeningPort_);
    
    // Send packet
    int bytesSent = sendto(socket_, 
                          reinterpret_cast<const char*>(packet.data()), 
                          static_cast<int>(packet.size()), 
                          0, 
                          reinterpret_cast<const sockaddr*>(&remoteAddr_), 
                          sizeof(remoteAddr_));
    
    if (bytesSent != packet.size()) {
        spdlog::error("Failed to send keepalive packet: {}", WSAGetLastError());
        return false;
    }
    
    spdlog::debug("Sent keepalive packet: {}x{} at {}fps, {}Mbps, touchPort: {}", 
                 width, height, fps, bitrate, listeningPort_.load());
    return true;
}

bool UdpSender::startReceiving()
{
    if (socket_ == INVALID_SOCKET) {
        spdlog::error("Socket not initialized");
        return false;
    }
    
    if (running_) {
        spdlog::warn("Receiver already running");
        return true;
    }
    
    // Bind socket to any available port
    sockaddr_in localAddr;
    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = 0;  // Let the OS choose a port
    
    if (bind(socket_, reinterpret_cast<const sockaddr*>(&localAddr), sizeof(localAddr)) == SOCKET_ERROR) {
        spdlog::error("Failed to bind socket: {}", WSAGetLastError());
        return false;
    }
    
    // Get the actual port that was assigned
    sockaddr_in assignedAddr;
    int addrLen = sizeof(assignedAddr);
    if (getsockname(socket_, reinterpret_cast<sockaddr*>(&assignedAddr), &addrLen) == 0) {
        listeningPort_ = ntohs(assignedAddr.sin_port);
        spdlog::info("UDP receiver listening on port: {}", listeningPort_.load());
    } else {
        spdlog::warn("Failed to get assigned port: {}", WSAGetLastError());
    }
    
    // Start receive thread
    running_ = true;
    receiveThreadPtr_ = std::make_unique<std::thread>(&UdpSender::receiveThread, this);
    
    spdlog::info("UDP receiver started");
    return true;
}

void UdpSender::stopReceiving()
{
    if (!running_) return;
    
    running_ = false;
    if (receiveThreadPtr_ && receiveThreadPtr_->joinable()) {
        receiveThreadPtr_->join();
        receiveThreadPtr_.reset();
    }
    
    spdlog::info("UDP receiver stopped");
}

bool UdpSender::sendDiscoveryPacket()
{
    if (socket_ == INVALID_SOCKET) {
        spdlog::error("Socket not initialized");
        return false;
    }
    
    // Create broadcast address
    sockaddr_in broadcastAddr;
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(45678);  // Default discovery port
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;
    
    // Create discovery message
    const char* message = "HELLO";
    
    // Send packet
    int bytesSent = sendto(socket_, 
                          message, 
                          static_cast<int>(strlen(message)), 
                          0, 
                          reinterpret_cast<const sockaddr*>(&broadcastAddr), 
                          sizeof(broadcastAddr));
    
    if (bytesSent != strlen(message)) {
        spdlog::error("Failed to send discovery packet: {}", WSAGetLastError());
        return false;
    }
    
    spdlog::info("Sent discovery packet");
    return true;
}

std::vector<std::string> UdpSender::getDiscoveredDevices() const
{
    std::lock_guard<std::mutex> lock(discoveredDevicesMutex_);
    return discoveredDevices_;
}

void UdpSender::setTouchEventCallback(TouchEventCallback callback)
{
    touchEventCallback_ = callback;
}

double UdpSender::getCurrentBandwidthMbps() const
{
    return currentBandwidthMbps_;
}

double UdpSender::getCurrentLatencyMs() const
{
    return currentLatencyMs_;
}

uint16_t UdpSender::getListeningPort() const
{
    return listeningPort_;
}

void UdpSender::receiveThread()
{
    std::vector<char> buffer(4096);
    sockaddr_in senderAddr;
    int senderAddrLen = sizeof(senderAddr);
    
    while (running_) {
        // Receive data
        int bytesReceived = recvfrom(socket_, 
                                    buffer.data(), 
                                    static_cast<int>(buffer.size()), 
                                    0, 
                                    reinterpret_cast<sockaddr*>(&senderAddr), 
                                    &senderAddrLen);
        
        if (bytesReceived == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
                spdlog::error("Failed to receive data: {}", error);
            }
            continue;
        }
        
        if (bytesReceived <= 0) continue;
        
        // Ensure null termination
        buffer[bytesReceived] = '\0';
        
        // Check if it's a discovery response
        if (bytesReceived > 5 && strncmp(buffer.data(), "HELLO", 5) == 0) {
            handleDiscoveryResponse(std::string(buffer.data(), bytesReceived), senderAddr);
            continue;
        }
        
        // Try to parse as JSON for touch events
        try {
            std::string jsonStr(buffer.data(), bytesReceived);
            auto json = nlohmann::json::parse(jsonStr);
            
            if (json.contains("action") && json.contains("x") && json.contains("y")) {
                auto touchPacket = TouchInputPacket::fromJson(json);
                
                // Convert to internal touch event
                TouchEvent event;
                switch (touchPacket.action) {
                    case TouchInputPacket::Action::DOWN:
                        event.type = TouchEvent::Type::DOWN;
                        break;
                    case TouchInputPacket::Action::MOVE:
                        event.type = TouchEvent::Type::MOVE;
                        break;
                    case TouchInputPacket::Action::UP:
                        event.type = TouchEvent::Type::UP;
                        break;
                }
                
                event.x = touchPacket.x;
                event.y = touchPacket.y;
                event.pointerId = touchPacket.pointerId;
                
                // Call the callback
                if (touchEventCallback_) {
                    touchEventCallback_(event);
                }
                
                // Update latency calculation
                if (json.contains("timestamp")) {
                    uint64_t timestamp = json["timestamp"];
                    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    
                    double latency = static_cast<double>(now - timestamp);
                    // Update with moving average
                    currentLatencyMs_ = currentLatencyMs_ * 0.8 + latency * 0.2;
                }
            }
        } catch (const std::exception& e) {
            spdlog::warn("Failed to parse received data as JSON: {}", e.what());
        }
    }
}

void UdpSender::handleDiscoveryResponse(const std::string& data, const sockaddr_in& fromAddr)
{
    char ipAddr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &fromAddr.sin_addr, ipAddr, sizeof(ipAddr));
    
    std::string deviceId = ipAddr;
    
    // Extract device name if available
    size_t colonPos = data.find(':');
    if (colonPos != std::string::npos && colonPos + 1 < data.size()) {
        std::string deviceName = data.substr(colonPos + 1);
        deviceId += " (" + deviceName + ")";
    }
    
    spdlog::info("Discovered device: {}", deviceId);
    
    // Add to discovered devices list
    {
        std::lock_guard<std::mutex> lock(discoveredDevicesMutex_);
        if (std::find(discoveredDevices_.begin(), discoveredDevices_.end(), deviceId) == discoveredDevices_.end()) {
            discoveredDevices_.push_back(deviceId);
        }
    }
}

} // namespace TabDisplay