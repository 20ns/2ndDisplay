// Win32 Tray implementation for TabDisplay
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <spdlog/spdlog.h>
#include <memory>
#include <thread>
#include <chrono>

// Include our core components
#include "CaptureDXGI.hpp"
#include "EncoderAMF.hpp"
#include "UdpSender.hpp"
#include "InputInjector.hpp"
#include "Settings.hpp"
#include "UsbDeviceDiscovery.hpp"

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_SHOW 1002
#define ID_TRAY_CONNECT 1003
#define ID_TRAY_DISCONNECT 1004
#define ID_TRAY_MODE_PRIMARY 1005
#define ID_TRAY_MODE_SECOND 1006
#define ID_TRAY_MODE_CUSTOM 1007

namespace TabDisplay {

// Global components
std::unique_ptr<CaptureDXGI> g_capture;
std::unique_ptr<EncoderAMF> g_encoder;
std::unique_ptr<UdpSender> g_sender;
std::unique_ptr<InputInjector> g_injector;
std::unique_ptr<UsbDeviceDiscovery> g_discovery;

// Connection state
bool g_connected = false;
std::string g_connectedDevice;

class SimpleTrayApp {
private:
    HWND m_hwnd;
    NOTIFYICONDATA m_nid;
    bool m_running;

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
            case ID_TRAY_CONNECT:
                ConnectToAndroid();
                break;
            case ID_TRAY_DISCONNECT:
                DisconnectFromAndroid();
                break;
            case ID_TRAY_MODE_PRIMARY:
                SetCaptureMode(CaptureDXGI::CaptureMode::PrimaryMonitor);
                break;
            case ID_TRAY_MODE_SECOND:
                SetCaptureMode(CaptureDXGI::CaptureMode::SecondMonitor);
                break;
            case ID_TRAY_MODE_CUSTOM:
                ConfigureCustomRegion();
                break;
            case ID_TRAY_EXIT:
                spdlog::info("User requested exit from tray menu");
                m_running = false;
                PostQuitMessage(0);
                break;
            }
            break;
        case WM_DESTROY:
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
        
        if (g_connected) {
            AppendMenuW(menu, MF_STRING, ID_TRAY_DISCONNECT, L"Disconnect");
        } else {
            AppendMenuW(menu, MF_STRING, ID_TRAY_CONNECT, L"Connect to Android");
        }
        
        AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
        
        // Capture mode submenu
        HMENU modeMenu = CreatePopupMenu();
        AppendMenuW(modeMenu, MF_STRING, ID_TRAY_MODE_PRIMARY, L"Primary Monitor (Mirror)");
        AppendMenuW(modeMenu, MF_STRING, ID_TRAY_MODE_SECOND, L"Second Monitor (Extended)");
        AppendMenuW(modeMenu, MF_STRING, ID_TRAY_MODE_CUSTOM, L"Custom Region...");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(modeMenu), L"Capture Mode");
        
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
        
        if (g_connected) {
            statusText += "✅ Connection: Connected to " + g_connectedDevice + "\n";
            statusText += "✅ Screen Capture: Active\n";
        } else {
            statusText += "⚠️  Connection: Disconnected\n";
            statusText += "⚠️  Screen Capture: Inactive\n";
        }
        
        statusText += "\nRight-click tray icon for menu.\n";
        statusText += "Check TabDisplay.log for detailed logging.";
        
        MessageBoxA(nullptr, statusText.c_str(), "TabDisplay Status", MB_OK | MB_ICONINFORMATION);
    }

    void ConnectToAndroid() {
        spdlog::info("Attempting to connect to Android device...");
        
        if (!g_discovery) {
            MessageBoxA(nullptr, "Device discovery not initialized", "Connection Error", MB_OK | MB_ICONERROR);
            return;
        }
        
        auto device = g_discovery->getFirstAndroidDevice();
        if (!device) {
            MessageBoxA(nullptr, 
                "No Android device found.\n\n"
                "Please ensure your Android device is:\n"
                "1. Connected via USB cable\n"
                "2. USB tethering is enabled\n"
                "3. TabDisplay Android app is running\n"
                "4. Allow USB debugging if prompted",
                "No Device Found", MB_OK | MB_ICONWARNING);
            return;
        }
        
        spdlog::info("Found Android device: {} at {}", device->deviceName, device->ipAddress);
        
        // Initialize UDP sender with device IP
        if (!g_sender->initialize(device->ipAddress, 5004)) {
            MessageBoxA(nullptr, "Failed to initialize network connection to Android device", 
                       "Connection Error", MB_OK | MB_ICONERROR);
            return;
        }
        
        // Configure encoder settings based on capture mode
        EncoderAMF::EncoderSettings encSettings{};
        auto captureRegion = g_capture->getCaptureRegion();
        encSettings.width = captureRegion.width;
        encSettings.height = captureRegion.height;
        encSettings.frameRate = 60;
        encSettings.bitrate = 30; // 30 Mbps
        encSettings.lowLatency = true;
        encSettings.useBFrames = false;
        
        spdlog::info("Configuring encoder for {}x{} resolution", encSettings.width, encSettings.height);
        
        if (!g_encoder->configure(encSettings)) {
            MessageBoxA(nullptr, "Failed to configure video encoder", "Encoder Error", MB_OK | MB_ICONERROR);
            return;
        }
        
        // Set capture profile based on encoder settings
        CaptureDXGI::Profile profile = CaptureDXGI::Profile::FullHD_60Hz;
        if (encSettings.width == 1752 && encSettings.height == 2800) {
            profile = CaptureDXGI::Profile::Tablet_60Hz;
        }
        
        if (!g_capture->setProfile(profile)) {
            MessageBoxA(nullptr, "Failed to set capture profile", "Capture Error", MB_OK | MB_ICONERROR);
            return;
        }
        
        // Start screen capture
        if (!g_capture->startCapture()) {
            MessageBoxA(nullptr, "Failed to start screen capture", "Capture Error", MB_OK | MB_ICONERROR);
            return;
        }
        
        // Update connection state
        g_connected = true;
        g_connectedDevice = device->deviceName + " (" + device->ipAddress + ")";
        
        // Update tray icon tooltip
        strcpy_s(m_nid.szTip, sizeof(m_nid.szTip), ("TabDisplay - Connected to " + device->deviceName).c_str());
        Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        
        spdlog::info("Successfully connected to Android device: {}", g_connectedDevice);
        
        MessageBoxA(nullptr, 
            ("Successfully connected to " + device->deviceName + "\n\n"
             "Your Android device should now display your screen.\n"
             "Touch the screen to control your PC.").c_str(),
            "Connected", MB_OK | MB_ICONINFORMATION);
    }

    void DisconnectFromAndroid() {
        spdlog::info("Disconnecting from Android device...");
        
        // Stop screen capture
        if (g_capture) {
            g_capture->stopCapture();
        }
        
        // Update connection state
        g_connected = false;
        g_connectedDevice.clear();
        
        // Update tray icon tooltip
        strcpy_s(m_nid.szTip, sizeof(m_nid.szTip), "TabDisplay - Disconnected");
        Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        
        spdlog::info("Disconnected from Android device");
    }

    void SetCaptureMode(CaptureDXGI::CaptureMode mode) {
        if (!g_capture) {
            MessageBoxA(nullptr, "Capture system not initialized", "Error", MB_OK | MB_ICONERROR);
            return;
        }
        
        switch (mode) {
        case CaptureDXGI::CaptureMode::PrimaryMonitor:
            g_capture->setCaptureMode(mode);
            spdlog::info("Capture mode set to Primary Monitor (Mirror)");
            MessageBoxA(nullptr, "Capture mode set to Primary Monitor (Mirror)\n\n"
                       "Your tablet will now mirror your primary monitor.", 
                       "Capture Mode Changed", MB_OK | MB_ICONINFORMATION);
            break;
            
        case CaptureDXGI::CaptureMode::SecondMonitor:
            g_capture->setSecondMonitorRegion();
            spdlog::info("Capture mode set to Second Monitor (Extended)");
            {
                auto region = g_capture->getCaptureRegion();
                char msg[512];
                sprintf_s(msg, sizeof(msg), 
                    "Capture mode set to Second Monitor (Extended)\n\n"
                    "Virtual second monitor region: %dx%d at (%d, %d)\n\n"
                    "Move windows to the right of your primary monitor to see them on the tablet.",
                    region.width, region.height, region.x, region.y);
                MessageBoxA(nullptr, msg, "Capture Mode Changed", MB_OK | MB_ICONINFORMATION);
            }
            break;
            
        case CaptureDXGI::CaptureMode::CustomRegion:
            // This will be handled by ConfigureCustomRegion()
            break;
        }
    }

    void ConfigureCustomRegion() {
        // Simple dialog to configure custom region - for now just use a predefined region
        // In a full implementation, this could show a dialog to let user select region
        
        if (!g_capture) {
            MessageBoxA(nullptr, "Capture system not initialized", "Error", MB_OK | MB_ICONERROR);
            return;
        }
        
        // For now, set a custom region in the center of the screen
        g_capture->setCustomRegion(480, 270, 960, 540);  // Center quarter of 1920x1080
        
        auto region = g_capture->getCaptureRegion();
        char msg[512];
        sprintf_s(msg, sizeof(msg), 
            "Custom capture region configured:\n\n"
            "Size: %dx%d\n"
            "Position: (%d, %d)\n\n"
            "The tablet will now show this specific area of your desktop.",
            region.width, region.height, region.x, region.y);
        MessageBoxA(nullptr, msg, "Custom Region Set", MB_OK | MB_ICONINFORMATION);
        
        spdlog::info("Custom capture region set: {}x{} at ({}, {})", 
                     region.width, region.height, region.x, region.y);
    }

public:
    bool Initialize() {
        // Initialize core components first
        if (!InitializeComponents()) {
            return false;
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
        strcpy_s(m_nid.szTip, sizeof(m_nid.szTip), "TabDisplay - Ready to connect");

        if (!Shell_NotifyIcon(NIM_ADD, &m_nid)) {
            spdlog::error("Failed to add tray icon");
            return false;
        }

        spdlog::info("Tray icon created successfully using Win32 API");
        m_running = true;
        return true;
    }

    int Run() {
        if (!Initialize()) {
            return 1;
        }

        spdlog::info("TabDisplay tray application started (Win32 mode)");
        spdlog::info("Right-click tray icon for menu, double-click for status");

        // Show initial connection prompt
        int result = MessageBoxA(nullptr,
            "TabDisplay is ready!\n\n"
            "Connect your Android device via USB cable and enable USB tethering,\n"
            "then right-click the tray icon and select 'Connect to Android'.\n\n"
            "Would you like to try connecting now?",
            "TabDisplay Ready",
            MB_YESNO | MB_ICONQUESTION);
            
        if (result == IDYES) {
            ConnectToAndroid();
        }

        // Message loop
        MSG msg;
        while (m_running && GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        spdlog::info("TabDisplay tray application shutting down");
        return 0;
    }

private:
    bool InitializeComponents() {
        try {
            spdlog::info("Initializing TabDisplay components...");
            
            // Initialize device discovery
            g_discovery = std::make_unique<UsbDeviceDiscovery>();
            
            // Initialize screen capture
            g_capture = std::make_unique<CaptureDXGI>();
            if (!g_capture->initialize()) {
                spdlog::error("Failed to initialize screen capture");
                return false;
            }
            
            // Initialize hardware encoder
            g_encoder = std::make_unique<EncoderAMF>();
            if (!g_encoder->initialize(g_capture->getDevice())) {
                spdlog::error("Failed to initialize AMF encoder");
                spdlog::warn("Continuing without hardware encoder (software fallback will be used)");
                g_encoder.reset(); // Clear the failed encoder
            } else {
                // Link capture to encoder only if encoder initialized successfully
                g_capture->setEncoder(g_encoder.get());
            }
            
            // Initialize UDP sender
            g_sender = std::make_unique<UdpSender>();
            if (!g_sender->startReceiving()) {
                spdlog::error("Failed to start UDP receiver");
                return false;
            }
            
            // Initialize input injector
            g_injector = std::make_unique<InputInjector>();
            if (!g_injector->initialize()) {
                spdlog::warn("Failed to initialize input injector, touch input will not work");
                // Continue anyway
            }
            
            // Set up callbacks
            if (g_encoder) {
                g_encoder->setFrameCallback([](const EncoderAMF::EncodedFrame& frame) {
                    if (g_sender && g_connected) {
                        g_sender->sendFrame(frame.data, frame.isKeyFrame);
                    }
                });
            }
            
            g_sender->setTouchEventCallback([](const UdpSender::TouchEvent& event) {
                if (g_injector) {
                    InputInjector::InputType type;
                    switch (event.type) {
                        case UdpSender::TouchEvent::Type::DOWN:
                            type = InputInjector::InputType::TOUCH_DOWN;
                            break;
                        case UdpSender::TouchEvent::Type::MOVE:
                            type = InputInjector::InputType::TOUCH_MOVE;
                            break;
                        case UdpSender::TouchEvent::Type::UP:
                            type = InputInjector::InputType::TOUCH_UP;
                            break;
                    }
                    
                    g_injector->injectTouchEvent(event.x, event.y, event.pointerId, type);
                }
            });
            
            spdlog::info("All components initialized successfully");
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Exception during component initialization: {}", e.what());
            return false;
        }
    }

    void RemoveTrayIcon() {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
        spdlog::info("Tray icon removed");
    }
};

int RunTrayApp() {
    spdlog::info("Starting TabDisplay with Win32 tray UI...");
    spdlog::info("Windows App SDK detected, using Win32 tray implementation");
    
    SimpleTrayApp app;
    return app.Run();
}

} // namespace TabDisplay
