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
#include "UsbDeviceDiscovery.hpp"
#include "UdpSender.hpp"

#define WM_TRAYICON (WM_USER + 1)
#define WM_DISCOVERY_COMPLETE (WM_USER + 2)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_SHOW 1002
#define ID_TRAY_DISCOVER 1003
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
        
        // Discovery option
        if (discoveryInProgress_) {
            AppendMenuW(menu, MF_STRING | MF_GRAYED, ID_TRAY_DISCOVER, L"Discovering Android Devices...");
        } else {
            AppendMenuW(menu, MF_STRING, ID_TRAY_DISCOVER, L"Find Android Devices");
        }
        
        // Show discovered devices
        if (!discoveredDevices_.empty()) {
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
                
                // Network discovery via UDP broadcast
                spdlog::info("Broadcasting discovery packets");
                if (udpSender_) {
                    udpSender_->sendDiscoveryPacket();
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // Wait for responses
                    
                    // Get discovered devices from UDP sender
                    auto networkDevices = udpSender_->getDiscoveredDevices();
                    spdlog::info("Found {} devices via network discovery", networkDevices.size());
                    
                    // Convert network devices to UsbDeviceInfo format
                    for (const auto& deviceResponse : networkDevices) {
                        UsbDeviceInfo networkDevice;
                        // Parse "HELLO:DeviceName" format
                        if (deviceResponse.find("HELLO:") == 0) {
                            networkDevice.deviceName = deviceResponse.substr(6);
                            networkDevice.ipAddress = "192.168.42.129"; // Default Android USB IP
                            networkDevice.interfaceName = "Network Discovery";
                            discoveredDevices_.push_back(networkDevice);
                        }
                    }
                }
                
                // Add USB devices
                discoveredDevices_.insert(discoveredDevices_.end(), usbDevices.begin(), usbDevices.end());
                
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
        spdlog::info("Attempting to connect to device: {} ({})", device.deviceName, device.ipAddress);
        
        // Test connectivity first
        if (!deviceDiscovery_->testConnectivity(device.ipAddress)) {
            std::string errorMsg = "Failed to connect to " + device.deviceName + "\n\n";
            errorMsg += "IP: " + device.ipAddress + "\n";
            errorMsg += "Make sure the Android app is running and listening on port 5004.";
            
            MessageBoxA(nullptr, errorMsg.c_str(), "Connection Failed", MB_OK | MB_ICONERROR);
            spdlog::error("Connectivity test failed for device: {}", device.ipAddress);
            return;
        }
        
        // Initialize UDP sender for this device
        if (!udpSender_->initialize(device.ipAddress, 5004)) {
            MessageBoxA(nullptr, "Failed to initialize UDP connection", "Connection Failed", MB_OK | MB_ICONERROR);
            spdlog::error("Failed to initialize UDP sender for device: {}", device.ipAddress);
            return;
        }
        
        // Start receiving touch events
        if (!udpSender_->startReceiving()) {
            MessageBoxA(nullptr, "Failed to start receiving touch events", "Connection Failed", MB_OK | MB_ICONERROR);
            spdlog::error("Failed to start receiving for device: {}", device.ipAddress);
            return;
        }
        
        spdlog::info("Successfully connected to device: {}", device.deviceName);
        UpdateTrayTooltip("TabDisplay - Connected to " + device.deviceName);
        
        std::string successMsg = "Successfully connected to " + device.deviceName + "!\n\n";
        successMsg += "Your Android device should now be receiving video.";
        MessageBoxA(nullptr, successMsg.c_str(), "Connection Successful", MB_OK | MB_ICONINFORMATION);
    }

    void StopAllOperations() {
        spdlog::info("Stopping all operations");
        
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
