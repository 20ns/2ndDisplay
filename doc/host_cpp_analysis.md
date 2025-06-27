# Host C++ Application Structure Analysis

## Project Overview
TabDisplay is a Windows C++ application that streams screen content to Android devices via USB tethering, turning tablets into second monitors.

## Key Components

### Core Source Files
```
host/src/
├── main.cpp - Entry point, logging setup
├── TrayApp_win32.cpp - Main application with system tray
├── UsbDeviceDiscovery.cpp - Android device discovery via USB
├── UdpSender.cpp - Network communication and packets
├── CaptureDXGI.cpp - Screen capture using DirectX
├── EncoderAMF.cpp - Hardware H.264 video encoding
├── InputInjector.cpp - Touch input processing
├── Settings.cpp - Configuration management
└── Packet.hpp - Protocol definitions
```

### Build System (CMakeLists.txt)
- **Standard:** C++20 with MSVC runtime
- **Dependencies:** spdlog, nlohmann/json, FEC library, AMD AMF
- **Targets:** WinUI3, Win32 tray, basic, stub implementations
- **Current Active:** Win32 tray implementation

## **CRITICAL DISCOVERY ISSUES IDENTIFIED**

### 1. Inconsistent Discovery Implementation
**Two conflicting approaches:**
- `UdpSender::sendDiscoveryPacket()` - broadcasts "HELLO" to port 45678 ✅
- `UsbDeviceDiscovery::testConnectivity()` - sends "PING" to port 5004 ❌

### 2. Build/Version Mismatch
- Current executable predates enhanced discovery code
- Missing improved USB RNDIS detection logic
- Debug logs show old version (27/06/2025 14:16:51)

### 3. Missing Integration
- Discovery and connection logic not coordinated
- Components initialized but discovery never triggered automatically
- Tray app doesn't properly invoke discovery mechanism

### 4. IP Pattern Detection Issues
- Hardcoded gateway patterns may not match all Android USB configs
- Link-local address handling (169.254.x.x) incomplete

## Network Architecture

### Protocol Structure (Packet.hpp)
```cpp
struct PacketHeader {
    uint16_t sequenceId;   // Unique sequence ID
    uint16_t frameId;      // Frame this chunk belongs to
    uint16_t totalChunks;  // Total chunks in frame
    uint16_t chunkIndex;   // Chunk index (0-based)
    uint32_t flags;        // Bit flags (keyframe, input, etc.)
};
```

### Communication Flow
1. Video frames split into 1350-byte chunks
2. XOR-based Forward Error Correction (FEC)
3. JSON control packets every 60 frames
4. Touch events received as JSON from Android

### Expected Discovery Protocol
```
Host → Android: "HELLO" broadcast to UDP port 45678
Android → Host: "HELLO:[device_name]" response
Host: Adds device to available connections list
```

## Screen Capture & Encoding

### DXGI Capture System
- **Profiles:** FullHD_60Hz (1920×1080), Tablet_60Hz/120Hz (1752×2800)
- **Modes:** Primary monitor, second monitor, custom region
- **Status:** ✅ DXGI initialized successfully

### AMF Encoder
- **Hardware:** AMD Advanced Media Framework for H.264
- **Fallback:** Software encoding when AMF unavailable
- **Status:** ❌ AMF failed to load (expected on non-AMD systems)

## USB Device Discovery Logic
```cpp
bool isAndroidInterface(const std::string& interfaceName) {
    // Searches for keywords: "android", "adb", "rndis", 
    // "remote ndis", "usb ethernet", "samsung", "galaxy", "pixel", "nexus"
}
```

## Root Cause Analysis

### Why Discovery Fails
1. **Port Confusion:** Uses port 5004 instead of 45678 for discovery
2. **Outdated Executable:** Running version predates critical fixes
3. **No Auto-Discovery:** Manual trigger required but not implemented
4. **Integration Gap:** Discovery logic exists but not called properly

## **IMMEDIATE FIXES REQUIRED**

### 1. Rebuild Application
Current executable needs to be rebuilt with latest code

### 2. Unify Discovery Logic
Use broadcast "HELLO" to port 45678 consistently

### 3. Integrate Discovery
Trigger automatic discovery when "Connect to Android" selected

### 4. Enhance IP Detection
Support broader Android USB IP configurations

### 5. Add Discovery Timeout
Implement proper timeout and retry logic

## System Architecture Strengths
- ✅ Well-structured modular design
- ✅ Comprehensive packet protocol
- ✅ Multiple UI implementation fallbacks
- ✅ Robust error handling and logging
- ✅ Hardware acceleration with software fallback

## Conclusion
The host application has solid architecture but suffers from integration issues between discovery and connection logic. The fundamental approach is sound but needs coordination between UDP broadcast discovery and USB device detection systems.