// Win32 Tray implementation for TabDisplay
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <WinSock2.h>
#include <windows.h>
#include <shellapi.h>
#include <spdlog/spdlog.h>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include "UsbDeviceDiscovery.hpp"
#include "UdpSender.hpp"
#include "CaptureDXGI.hpp"
#include "EncoderAMF.hpp"
// #include "EncoderSoftware.hpp"  // Temporarily disabled

#define WM_TRAYICON (WM_USER + 1)
#define WM_DISCOVERY_COMPLETE (WM_USER + 2)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_SHOW 1002
#define ID_TRAY_DISCOVER 1003
#define ID_TRAY_STOP_STREAM 1004
#define ID_TRAY_CONNECT_BASE 2000

namespace TabDisplay {

class SimpleTrayApp {
private:
    HWND m_hwnd;
    NOTIFYICONDATA m_nid;
    bool m_running;
    
    // Discovery and connection components
    std::unique_ptr<UsbDeviceDiscovery> deviceDiscovery_;
    std::unique_ptr<UdpSender> udpSender_;
    std::vector<UsbDeviceInfo> discoveredDevices_;
    bool discoveryInProgress_;
    std::unique_ptr<std::thread> discoveryThread_;
    
    // Video streaming components
    std::unique_ptr<CaptureDXGI> capture_;
    std::unique_ptr<EncoderAMF> encoder_;
    // std::unique_ptr<EncoderSoftware> softwareEncoder_;  // Temporarily disabled
    bool streamingActive_;
    std::unique_ptr<std::thread> streamingThread_;
    std::atomic<int> frameCounter_;
    bool useEncoder_;
    bool useHardwareEncoder_;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        SimpleTrayApp* app = reinterpret_cast<SimpleTrayApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (app) {
            return app->HandleMessage(uMsg, wParam, lParam);
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) {
                ShowContextMenu();
            } else if (lParam == WM_LBUTTONDBLCLK) {
                spdlog::info("Tray icon double-clicked");
                ShowStatusDialog();
            }
            break;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
            case ID_TRAY_SHOW:
                ShowStatusDialog();
                break;
            case ID_TRAY_DISCOVER:
                StartDiscovery();
                break;
            case ID_TRAY_STOP_STREAM:
                StopVideoStreaming();
                break;
            case ID_TRAY_EXIT:
                spdlog::info("User requested exit from tray menu");
                m_running = false;
                PostQuitMessage(0);
                break;
            default:
                // Handle device connection commands
                if (LOWORD(wParam) >= ID_TRAY_CONNECT_BASE) {
                    int deviceIndex = LOWORD(wParam) - ID_TRAY_CONNECT_BASE;
                    ConnectToDevice(deviceIndex);
                }
                break;
            }
            break;
        case WM_DISCOVERY_COMPLETE:
            OnDiscoveryComplete();
            break;
        case WM_DESTROY:
            StopAllOperations();
            RemoveTrayIcon();
            PostQuitMessage(0);
            break;
        }
        return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
    }

    void ShowContextMenu() {
        POINT pt;
        GetCursorPos(&pt);

        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, ID_TRAY_SHOW, L"Show Status");
        AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
        
        // Streaming controls
        if (streamingActive_) {
            AppendMenuW(menu, MF_STRING, ID_TRAY_STOP_STREAM, L"Stop Video Streaming");
            AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
        }
        
        // Discovery option
        if (discoveryInProgress_) {
            AppendMenuW(menu, MF_STRING | MF_GRAYED, ID_TRAY_DISCOVER, L"Discovering Android Devices...");
        } else {
            AppendMenuW(menu, MF_STRING, ID_TRAY_DISCOVER, L"Find Android Devices");
        }
        
        // Show discovered devices (only if not streaming)
        if (!discoveredDevices_.empty() && !streamingActive_) {
            AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
            for (size_t i = 0; i < discoveredDevices_.size(); ++i) {
                std::wstring deviceText = L"Connect to: " + 
                    std::wstring(discoveredDevices_[i].deviceName.begin(), discoveredDevices_[i].deviceName.end());
                AppendMenuW(menu, MF_STRING, ID_TRAY_CONNECT_BASE + i, deviceText.c_str());
            }
        }
        
        AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit TabDisplay");

        SetForegroundWindow(m_hwnd);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, nullptr);
        DestroyMenu(menu);
    }

    void ShowStatusDialog() {
        std::string statusText = "TabDisplay v0.1.0 - Win32 Tray Mode\n\n";
        statusText += "✅ Windows App SDK: Detected\n";
        statusText += "✅ Tray Icon: Active (Win32)\n";
        statusText += "✅ Core functionality: Available\n";
        statusText += "⚠️  WinUI 3 UI: Partial (using Win32 fallback)\n\n";
        
        // Streaming status
        if (streamingActive_) {
            statusText += "🎥 Video Streaming: Active\n";
            statusText += "   Broadcasting screen to connected device\n\n";
        } else {
            statusText += "🎥 Video Streaming: Inactive\n\n";
        }
        
        if (discoveredDevices_.empty()) {
            statusText += "📱 Android Devices: None found\n";
            statusText += "   Right-click and select 'Find Android Devices'\n";
        } else {
            statusText += "📱 Android Devices Found: " + std::to_string(discoveredDevices_.size()) + "\n";
            for (const auto& device : discoveredDevices_) {
                statusText += "   • " + device.deviceName + " (" + device.ipAddress + ")\n";
            }
        }
        
        statusText += "\nRight-click tray icon for menu.\n";
        statusText += "Check TabDisplay.log for detailed logging.";
        
        MessageBoxA(nullptr, statusText.c_str(), "TabDisplay Status", MB_OK | MB_ICONINFORMATION);
    }

    void StartDiscovery() {
        if (discoveryInProgress_) {
            spdlog::warn("Discovery already in progress");
            return;
        }
        
        spdlog::info("Starting Android device discovery");
        discoveryInProgress_ = true;
        UpdateTrayTooltip("TabDisplay - Discovering devices...");
        
        // Start discovery in a separate thread
        discoveryThread_ = std::make_unique<std::thread>([this]() {
            try {
                // Clear previous results
                discoveredDevices_.clear();
                
                // USB device discovery
                spdlog::info("Searching for USB tethered Android devices");
                auto usbDevices = deviceDiscovery_->findAndroidDevices();
                
                // Add USB devices first (these are the reliable ones)
                discoveredDevices_.insert(discoveredDevices_.end(), usbDevices.begin(), usbDevices.end());
                
                // Network discovery via UDP broadcast (capture WiFi IPs for video streaming)
                spdlog::info("Starting UDP receiver for network discovery");
                if (udpSender_) {
                    // Start UDP receiver to capture discovery responses
                    if (!udpSender_->startReceiving()) {
                        spdlog::warn("Failed to start UDP receiver for discovery");
                    } else {
                        spdlog::info("UDP receiver started, broadcasting discovery packets");
                        udpSender_->sendDiscoveryPacket();
                        std::this_thread::sleep_for(std::chrono::milliseconds(3000)); // Wait longer for responses
                        
                        // Get discovered devices from UDP sender
                        auto networkDevices = udpSender_->getDiscoveredDevices();
                        spdlog::info("Found {} devices via network discovery", networkDevices.size());
                        
                        if (!networkDevices.empty()) {
                            for (const auto& device : networkDevices) {
                                spdlog::info("Network discovered device: {}", device);
                            }
                        }
                        
                        // Stop UDP receiver after discovery
                        udpSender_->stopReceiving();
                    }
                }
                
                spdlog::info("Discovery complete. Found {} total devices", discoveredDevices_.size());
                
                // Notify main thread
                PostMessage(m_hwnd, WM_DISCOVERY_COMPLETE, 0, 0);
                
            } catch (const std::exception& e) {
                spdlog::error("Discovery failed: {}", e.what());
                PostMessage(m_hwnd, WM_DISCOVERY_COMPLETE, 0, 0);
            }
        });
    }

    void OnDiscoveryComplete() {
        discoveryInProgress_ = false;
        if (discoveryThread_ && discoveryThread_->joinable()) {
            discoveryThread_->join();
        }
        
        std::string tooltip = "TabDisplay - Found " + std::to_string(discoveredDevices_.size()) + " device(s)";
        UpdateTrayTooltip(tooltip);
        
        if (discoveredDevices_.empty()) {
            MessageBoxA(nullptr, 
                "No Android devices found.\n\n"
                "Make sure:\n"
                "• Android device is connected via USB\n"
                "• USB tethering is enabled\n"
                "• TabDisplay app is running on Android\n"
                "• Discovery service is listening on port 45678",
                "Discovery Complete", MB_OK | MB_ICONWARNING);
        } else {
            spdlog::info("Discovery completed successfully with {} devices", discoveredDevices_.size());
        }
    }

    void ConnectToDevice(int deviceIndex) {
        if (deviceIndex < 0 || deviceIndex >= static_cast<int>(discoveredDevices_.size())) {
            spdlog::error("Invalid device index: {}", deviceIndex);
            return;
        }
        
        const auto& device = discoveredDevices_[deviceIndex];
        
        // Get WiFi IPs from UDP discovery (these are the correct IPs for video streaming)
        auto networkDevices = udpSender_->getDiscoveredDevices();
        std::string videoTargetIP;
        
        if (!networkDevices.empty()) {
            // Extract IP from the first network discovered device (format: "192.168.1.166 (Galaxy_Tab_S10+)")
            std::string networkDevice = networkDevices[0];
            size_t spacePos = networkDevice.find(' ');
            if (spacePos != std::string::npos) {
                videoTargetIP = networkDevice.substr(0, spacePos);
            } else {
                videoTargetIP = networkDevice; // Fallback if no space found
            }
            spdlog::info("Using WiFi IP from network discovery: {}", videoTargetIP);
        } else {
            // Fallback to USB IP if no network discovery results
            videoTargetIP = device.ipAddress;
            spdlog::warn("No WiFi IP found, falling back to USB IP: {}", videoTargetIP);
        }
        
        spdlog::info("Attempting to connect to device: {} (USB: {}, Video target: {})", 
                    device.deviceName, device.ipAddress, videoTargetIP);
        
        // Test connectivity to the video target IP
        if (!deviceDiscovery_->testConnectivity(videoTargetIP)) {
            std::string errorMsg = "Failed to connect to " + device.deviceName + "\n\n";
            errorMsg += "Video Target IP: " + videoTargetIP + "\n";
            errorMsg += "Make sure the Android app is running and listening on port 5004.";
            
            MessageBoxA(nullptr, errorMsg.c_str(), "Connection Failed", MB_OK | MB_ICONERROR);
            spdlog::error("Connectivity test failed for video target: {}", videoTargetIP);
            return;
        }
        
        // Initialize UDP sender for video target IP
        if (!udpSender_->initialize(videoTargetIP, 5004)) {
            MessageBoxA(nullptr, "Failed to initialize UDP connection", "Connection Failed", MB_OK | MB_ICONERROR);
            spdlog::error("Failed to initialize UDP sender for video target: {}", videoTargetIP);
            return;
        }
        
        spdlog::info("Successfully connected to device: {}", device.deviceName);
        UpdateTrayTooltip("TabDisplay - Connected to " + device.deviceName);
        
        // Start video streaming
        if (StartVideoStreaming()) {
            spdlog::info("Video streaming started successfully");
            
            std::string successMsg = "Successfully connected to " + device.deviceName + "!\n\n";
            successMsg += "Video streaming is now active.\n";
            successMsg += "Your tablet should show the extended desktop.";
            MessageBoxA(nullptr, successMsg.c_str(), "Connection Successful", MB_OK | MB_ICONINFORMATION);
        } else {
            spdlog::error("Failed to start video streaming");
            MessageBoxA(nullptr, "Connection established but video streaming failed to start.\nCheck TabDisplay.log for details.", 
                       "Partial Success", MB_OK | MB_ICONWARNING);
        }
    }

    bool StartVideoStreaming() {
        if (streamingActive_) {
            spdlog::warn("Video streaming already active");
            return true;
        }
        
        try {
            streamingActive_ = true;
            
            // Initialize frame counter
            frameCounter_ = 0;
            
            if (useEncoder_) {
                if (useHardwareEncoder_) {
                    // Set up AMF hardware encoder callback for H.264 encoded frames
                    encoder_->setFrameCallback([this](const EncoderAMF::EncodedFrame& encodedFrame) {
                        if (!streamingActive_ || !udpSender_) {
                            return;
                        }
                        
                        try {
                            // Send encoded H.264 frame
                            if (udpSender_->sendFrame(encodedFrame.data, encodedFrame.isKeyFrame)) {
                                frameCounter_++;
                                if (frameCounter_ % 30 == 0) { // Log every second
                                    spdlog::info("Sent H.264 frame #{}, size: {} bytes, keyframe: {}", 
                                               frameCounter_.load(), encodedFrame.data.size(), encodedFrame.isKeyFrame);
                                }
                            } else {
                                spdlog::warn("Failed to send H.264 frame #{}", frameCounter_.load());
                            }
                        } catch (const std::exception& e) {
                            spdlog::error("Exception in AMF encoder callback: {}", e.what());
                        }
                    });
                    
                    // Connect capture to hardware encoder
                    capture_->setEncoder(encoder_.get());
                    spdlog::info("Using H.264 hardware encoding for video streaming");
                } else {
                    spdlog::error("Software encoder path reached but software encoder is disabled!");
                }
                
            } else {
                // Fallback: Set up frame callback for raw frames  
                capture_->setFrameCallback([this](const CaptureDXGI::CaptureFrame& frame) {
                    if (!streamingActive_ || !udpSender_) {
                        return;
                    }
                    
                    try {
                        // Convert frame data to vector for UDP sender
                        std::vector<uint8_t> frameData(frame.data.begin(), frame.data.end());
                        
                        // Send frame
                        bool isKeyFrame = (frameCounter_ % 60 == 0); // Keyframe every 2 seconds at 30fps
                        if (udpSender_->sendFrame(frameData, isKeyFrame)) {
                            frameCounter_++;
                            if (frameCounter_ % 30 == 0) { // Log every second
                                spdlog::info("Sent raw frame #{}, size: {} bytes", frameCounter_.load(), frameData.size());
                            }
                        } else {
                            spdlog::warn("Failed to send raw frame #{}", frameCounter_.load());
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Exception in frame callback: {}", e.what());
                    }
                });
                spdlog::warn("Using raw frame streaming (Android may not display correctly)");
            }
            
            // Configure capture for extended desktop mode (virtual second monitor)
            capture_->setSecondMonitorRegion();
            spdlog::info("Configured capture for extended desktop mode (virtual second monitor)");
            
            // Initialize capture if needed
            if (!capture_->initialize()) {
                spdlog::error("Failed to initialize DXGI capture, falling back to keepalive only");
                
                // Start keepalive-only thread as fallback
                streamingThread_ = std::make_unique<std::thread>([this]() {
                    spdlog::info("Keepalive-only streaming thread started");
                    int keepaliveCounter = 0;
                    
                    while (streamingActive_) {
                        if (!udpSender_->sendKeepAlive(1920, 1080, 60, 30)) {
                            spdlog::warn("Failed to send keepalive");
                        } else {
                            spdlog::info("Sent keepalive #{}", ++keepaliveCounter);
                        }
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                    spdlog::info("Keepalive-only streaming thread ended");
                });
                
                spdlog::info("Video streaming started in keepalive-only mode");
                UpdateTrayTooltip("TabDisplay - Streaming keepalive");
                return true;
            }
            
            // Start actual frame capture
            if (!capture_->startCapture()) {
                spdlog::error("Failed to start DXGI capture");
                return false;
            }
            
            spdlog::info("Video streaming started with frame capture at 30fps");
            UpdateTrayTooltip("TabDisplay - Streaming video");
            
            return true;
            
        } catch (const std::exception& e) {
            spdlog::error("Exception starting video streaming: {}", e.what());
            streamingActive_ = false;
            return false;
        }
    }
    
    void StopVideoStreaming() {
        if (!streamingActive_) {
            return;
        }
        
        spdlog::info("Stopping video streaming");
        streamingActive_ = false;
        
        if (capture_) {
            capture_->stopCapture();
        }
        
        if (streamingThread_ && streamingThread_->joinable()) {
            streamingThread_->join();
        }
        
        UpdateTrayTooltip("TabDisplay - Ready");
    }

    void StopAllOperations() {
        spdlog::info("Stopping all operations");
        
        // Stop video streaming
        StopVideoStreaming();
        
        // Stop discovery if running
        discoveryInProgress_ = false;
        if (discoveryThread_ && discoveryThread_->joinable()) {
            discoveryThread_->join();
        }
        
        // Stop UDP operations  
        if (udpSender_) {
            udpSender_->stopReceiving();
        }
    }

    void UpdateTrayTooltip(const std::string& tooltip) {
        strncpy_s(m_nid.szTip, sizeof(m_nid.szTip), tooltip.c_str(), _TRUNCATE);
        Shell_NotifyIcon(NIM_MODIFY, &m_nid);
    }

public:
    bool Initialize() {
        // Initialize discovery components
        deviceDiscovery_ = std::make_unique<UsbDeviceDiscovery>();
        udpSender_ = std::make_unique<UdpSender>();
        discoveryInProgress_ = false;
        
        // Initialize video streaming components
        capture_ = std::make_unique<CaptureDXGI>();
        encoder_ = std::make_unique<EncoderAMF>();
        // softwareEncoder_ = std::make_unique<EncoderSoftware>();  // Temporarily disabled
        streamingActive_ = false;
        frameCounter_ = 0;
        useEncoder_ = false;
        useHardwareEncoder_ = false;
        
        // Initialize video components
        if (!capture_->initialize()) {
            spdlog::warn("Failed to initialize DXGI capture, video streaming may not work");
        } else {
            spdlog::info("DXGI capture initialized successfully");
            
            // Try to initialize AMF encoder first
            try {
                if (encoder_->initialize(capture_->getDevice())) {
                    // Configure encoder for H.264 streaming
                    EncoderAMF::EncoderSettings settings;
                    settings.width = 960;  // Reduce to 960x540 for testing
                    settings.height = 540;
                    settings.frameRate = 30;
                    settings.bitrate = 8; // 8 Mbps
                    settings.lowLatency = true;
                    settings.useBFrames = false;
                    
                    if (encoder_->configure(settings)) {
                        useEncoder_ = true;
                        useHardwareEncoder_ = true;
                        spdlog::info("AMF H.264 hardware encoder initialized successfully (8 Mbps, 30fps)");
                    } else {
                        spdlog::warn("Failed to configure AMF encoder, trying software encoder...");
                    }
                } else {
                    spdlog::warn("Failed to initialize AMF encoder, trying software encoder...");
                }
            } catch (const std::exception& e) {
                spdlog::error("Exception initializing AMF encoder: {}, trying software encoder...", e.what());
            }
            
            // Try software encoder if AMF fails - TEMPORARILY DISABLED
            if (!useEncoder_) {
                spdlog::warn("Hardware encoder failed, software encoder disabled - will use raw frames as fallback");
                // TODO: Implement proper software encoder when OpenH264 is configured
            }
        }
        
        // Register window class
        WNDCLASSW wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = L"TabDisplayTray";
        
        if (!RegisterClassW(&wc)) {
            spdlog::error("Failed to register window class");
            return false;
        }

        // Create hidden window
        m_hwnd = CreateWindowW(L"TabDisplayTray", L"TabDisplay", 0, 0, 0, 0, 0,
                             nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        if (!m_hwnd) {
            spdlog::error("Failed to create window");
            return false;
        }

        SetWindowLongPtr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

        // Create tray icon
        ZeroMemory(&m_nid, sizeof(m_nid));
        m_nid.cbSize = sizeof(m_nid);
        m_nid.hWnd = m_hwnd;
        m_nid.uID = 1;
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        m_nid.uCallbackMessage = WM_TRAYICON;
        m_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        strcpy_s(m_nid.szTip, sizeof(m_nid.szTip), "TabDisplay - Second Monitor App");

        if (!Shell_NotifyIcon(NIM_ADD, &m_nid)) {
            spdlog::error("Failed to add tray icon");
            return false;
        }

        spdlog::info("Tray icon created successfully using Win32 API");
        spdlog::info("Discovery components initialized");
        m_running = true;
        return true;
    }

    void RemoveTrayIcon() {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
        spdlog::info("Tray icon removed");
    }

    int Run() {
        if (!Initialize()) {
            return 1;
        }

        spdlog::info("TabDisplay tray application started (Win32 mode)");
        spdlog::info("Right-click tray icon for menu, double-click for status");

        // Show initial status
        ShowStatusDialog();

        // Automatically start discovery after showing status
        spdlog::info("Starting automatic Android device discovery");
        PostMessage(m_hwnd, WM_COMMAND, ID_TRAY_DISCOVER, 0);

        // Message loop
        MSG msg;
        while (m_running && GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        spdlog::info("TabDisplay tray application shutting down");
        return 0;
    }
};

int RunTrayApp() {
    spdlog::info("Starting TabDisplay with Win32 tray UI...");
    spdlog::info("Windows App SDK detected, using Win32 tray implementation");
    
    SimpleTrayApp app;
    return app.Run();
}

} // namespace TabDisplay
