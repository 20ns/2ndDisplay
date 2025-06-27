#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Dispatching.h>

#include <shellapi.h>

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include "Version.hpp"
#include "Settings.hpp"
#include "UdpSender.hpp"
#include "CaptureDXGI.hpp"
#include "EncoderAMF.hpp"
#include "InputInjector.hpp"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace TabDisplay {

// Tray icon data and constants
constexpr UINT WM_TRAYICON = WM_USER + 1;
constexpr UINT TRAYICON_ID = 1;
constexpr UINT IDM_EXIT = 100;
constexpr UINT IDM_SETTINGS = 101;
constexpr UINT IDM_PROFILE_1080P_60HZ = 102;
constexpr UINT IDM_PROFILE_TABLET_60HZ = 103;
constexpr UINT IDM_PROFILE_TABLET_120HZ = 104;

// Forward declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateTrayIcon(HWND hwnd);
void RemoveTrayIcon(HWND hwnd);
void ShowTrayMenu(HWND hwnd);

// Global state
HWND g_hwnd = NULL;
NOTIFYICONDATA g_nid = {0};
bool g_isRunning = true;

// Application components
std::unique_ptr<CaptureDXGI> g_capture;
std::unique_ptr<EncoderAMF> g_encoder;
std::unique_ptr<UdpSender> g_sender;
std::unique_ptr<InputInjector> g_injector;

// UI window
struct TrayAppWindow : implements<TrayAppWindow, IInspectable>
{
    Window window{nullptr};
    TextBlock statusText{nullptr};
    TextBlock bandwidthText{nullptr};
    TextBlock latencyText{nullptr};
    ComboBox deviceCombo{nullptr};
    ComboBox resolutionCombo{nullptr};
    
    void InitializeComponent()
    {
        window = Window();
        window.Title(winrt::hstring(L"TabDisplay Settings"));
        
        auto content = StackPanel();
        content.Padding({12, 12, 12, 12});
        content.Spacing(10);
        
        // Devices section
        auto deviceLabel = TextBlock();
        deviceLabel.Text(winrt::hstring(L"Discovered Devices:"));
        deviceLabel.Margin({0, 0, 0, 5});
        content.Children().Append(deviceLabel);
        
        deviceCombo = ComboBox();
        deviceCombo.Width(300);
        deviceCombo.PlaceholderText(winrt::hstring(L"No devices found"));
        content.Children().Append(deviceCombo);
        
        auto refreshButton = Button();
        refreshButton.Content(box_value(L"Refresh Devices"));
        refreshButton.Click([this](IInspectable const&, RoutedEventArgs const&) {
            RefreshDevices();
        });
        content.Children().Append(refreshButton);
        
        // Resolution/FPS section
        auto resolutionLabel = TextBlock();
        resolutionLabel.Text(winrt::hstring(L"Resolution Profile:"));
        resolutionLabel.Margin({0, 10, 0, 5});
        content.Children().Append(resolutionLabel);
        
        resolutionCombo = ComboBox();
        resolutionCombo.Width(300);
        
        resolutionCombo.Items().Append(box_value(L"1080p 60Hz"));
        resolutionCombo.Items().Append(box_value(L"1752x2800 60Hz"));
        resolutionCombo.Items().Append(box_value(L"1752x2800 120Hz"));
        resolutionCombo.SelectedIndex(0);
        content.Children().Append(resolutionCombo);
        
        // Connect button
        auto connectButton = Button();
        connectButton.Content(box_value(L"Connect"));
        connectButton.Click([this](IInspectable const&, RoutedEventArgs const&) {
            Connect();
        });
        content.Children().Append(connectButton);
        
        // Status section
        auto statusPanel = StackPanel();
        statusPanel.Margin({0, 20, 0, 0});
        
        auto statusLabel = TextBlock();
        statusLabel.Text(winrt::hstring(L"Status:"));
        statusPanel.Children().Append(statusLabel);
        
        statusText = TextBlock();
        statusText.Text(winrt::hstring(L"Disconnected"));
        statusText.Margin({10, 5, 0, 0});
        statusPanel.Children().Append(statusText);
        
        auto bandwidthLabel = TextBlock();
        bandwidthLabel.Text(winrt::hstring(L"Bandwidth:"));
        bandwidthLabel.Margin({0, 10, 0, 0});
        statusPanel.Children().Append(bandwidthLabel);
        
        bandwidthText = TextBlock();
        bandwidthText.Text(winrt::hstring(L"0 Mbps"));
        bandwidthText.Margin({10, 5, 0, 0});
        statusPanel.Children().Append(bandwidthText);
        
        auto latencyLabel = TextBlock();
        latencyLabel.Text(winrt::hstring(L"Latency:"));
        latencyLabel.Margin({0, 10, 0, 0});
        statusPanel.Children().Append(latencyLabel);
        
        latencyText = TextBlock();
        latencyText.Text(winrt::hstring(L"0 ms"));
        latencyText.Margin({10, 5, 0, 0});
        statusPanel.Children().Append(latencyText);
        
        content.Children().Append(statusPanel);
        
        // Set content
        window.Content(content);
        window.Activate();
    }
    
    void RefreshDevices()
    {
        if (g_sender) {
            g_sender->sendDiscoveryPacket();
            
            // Clear and repopulate device combo
            deviceCombo.Items().Clear();
            
            auto devices = g_sender->getDiscoveredDevices();
            for (const auto& device : devices) {
                deviceCombo.Items().Append(box_value(winrt::to_hstring(device)));
            }
            
            if (!devices.empty()) {
                deviceCombo.SelectedIndex(0);
            }
        }
    }
    
    void Connect()
    {
        if (!g_sender || !g_capture || !g_encoder) {
            statusText.Text(winrt::hstring(L"Error: Components not initialized"));
            return;
        }
        
        // Get selected device
        int deviceIndex = deviceCombo.SelectedIndex();
        if (deviceIndex < 0) {
            statusText.Text(winrt::hstring(L"Error: No device selected"));
            return;
        }
        
        auto deviceName = winrt::unbox_value<winrt::hstring>(deviceCombo.SelectedItem());
        std::string device = winrt::to_string(deviceName);
        
        // Extract IP address from device string
        std::string ipAddress = device;
        size_t spacePos = device.find(' ');
        if (spacePos != std::string::npos) {
            ipAddress = device.substr(0, spacePos);
        }
        
        // Initialize sender with device IP
        if (!g_sender->initialize(ipAddress, 5004)) {
            statusText.Text(winrt::hstring(L"Error: Failed to connect to device"));
            return;
        }
        
        // Set profile based on selection
        int profileIndex = resolutionCombo.SelectedIndex();
        CaptureDXGI::Profile profile;
        
        switch (profileIndex) {
            case 0:
                profile = CaptureDXGI::Profile::FullHD_60Hz;
                break;
            case 1:
                profile = CaptureDXGI::Profile::Tablet_60Hz;
                break;
            case 2:
                profile = CaptureDXGI::Profile::Tablet_120Hz;
                break;
            default:
                profile = CaptureDXGI::Profile::FullHD_60Hz;
        }
        
        g_capture->setProfile(profile);
        
        // Map profile to encoder settings
        EncoderAMF::EncoderSettings encSettings{};
        switch (profile) {
            case CaptureDXGI::Profile::FullHD_60Hz:
                encSettings.width = 1920;
                encSettings.height = 1080;
                encSettings.frameRate = 60;
                encSettings.bitrate = Settings::instance().getBitrate();
                break;
            case CaptureDXGI::Profile::Tablet_60Hz:
                encSettings.width = 1752;
                encSettings.height = 2800;
                encSettings.frameRate = 60;
                encSettings.bitrate = Settings::instance().getBitrate();
                break;
            case CaptureDXGI::Profile::Tablet_120Hz:
                encSettings.width = 1752;
                encSettings.height = 2800;
                encSettings.frameRate = 120;
                encSettings.bitrate = Settings::instance().getBitrate();
                break;
        }
        encSettings.lowLatency = true;
        encSettings.useBFrames = false;

        if (!g_encoder->configure(encSettings)) {
            statusText.Text(winrt::hstring(L"Error: Failed to configure encoder"));
            return;
        }

        // Start the capture loop if not already running
        if (!g_capture->startCapture()) {
            statusText.Text(winrt::hstring(L"Error: Failed to start capture"));
            return;
        }

        // Update input injector resolution so absolute coordinates map correctly
        if (g_injector) {
            g_injector->setScreenResolution(encSettings.width, encSettings.height);
        }

        // Update status
        statusText.Text(winrt::hstring(L"Connected to " + deviceName));

        // Save settings
        Settings::instance().setLastConnectedDevice(ipAddress);
        Settings::instance().save();
    }
    
    void UpdateStats(double bandwidth, double latency)
    {
        bandwidthText.Text(winrt::hstring(std::to_wstring(static_cast<int>(bandwidth)) + L" Mbps"));
        latencyText.Text(winrt::hstring(std::to_wstring(static_cast<int>(latency)) + L" ms"));
    }
};

// Global UI window
winrt::com_ptr<TrayAppWindow> g_appWindow = nullptr;

// Initialize the application
bool InitializeApp()
{
    try {
        // Initialize components
        g_capture = std::make_unique<CaptureDXGI>();
        if (!g_capture->initialize()) {
            spdlog::error("Failed to initialize screen capture");
            return false;
        }
        
        // Create and initialize the hardware encoder using the D3D11 device from capture
        g_encoder = std::make_unique<EncoderAMF>();
        if (!g_encoder->initialize(g_capture->getDevice())) {
            spdlog::error("Failed to initialize AMF encoder");
            return false;
        }
        
        // Let capture module push frames directly into the encoder
        g_capture->setEncoder(g_encoder.get());
        
        g_sender = std::make_unique<UdpSender>();
        if (!g_sender->startReceiving()) {
            spdlog::error("Failed to start UDP receiver");
            return false;
        }
        
        g_injector = std::make_unique<InputInjector>();
        if (!g_injector->initialize()) {
            spdlog::warn("Failed to initialize input injector, touch input will not work");
            // Continue anyway
        }
        
        // Set up callbacks
        g_capture->setFrameCallback([](const CaptureDXGI::CaptureFrame&){});
        
        g_encoder->setFrameCallback([](const EncoderAMF::EncodedFrame& frame) {
            if (g_sender) {
                g_sender->sendFrame(frame.data, frame.isKeyFrame);
            }
        });
        
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
        
        // Load settings
        Settings& settings = Settings::instance();
        
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("Exception during initialization: {}", e.what());
        return false;
    }
}

// Create and show the settings window
void ShowSettingsWindow()
{
    if (g_appWindow == nullptr) {
        g_appWindow = winrt::make_self<TrayAppWindow>();
        g_appWindow->InitializeComponent();
        
        // Refresh devices initially
        g_appWindow->RefreshDevices();
        
        // Start stats update timer
        auto window = g_appWindow->window;
        auto dispatcher = window.DispatcherQueue();
        
        auto timer = DispatcherQueueTimer();
        timer.Interval(std::chrono::milliseconds(1000));
        timer.Tick([](auto&, auto&) {
            if (g_sender && g_appWindow) {
                double bandwidth = g_sender->getCurrentBandwidthMbps();
                double latency = g_sender->getCurrentLatencyMs();
                g_appWindow->UpdateStats(bandwidth, latency);
            }
        });
        timer.Start();
    }
    else {
        // Just activate existing window
        g_appWindow->window.Activate();
    }
}

// Main entry point
int RunTrayApp()
{
    // Initialize WinUI
    winrt::init_apartment();
    
    // Create window class for message-only window
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"TabDisplayTrayWindow";
    
    if (!RegisterClassEx(&wc)) {
        spdlog::error("Failed to register window class");
        return 1;
    }
    
    // Create a message-only window
    g_hwnd = CreateWindowEx(
        0,
        L"TabDisplayTrayWindow",
        L"TabDisplay Tray",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!g_hwnd) {
        spdlog::error("Failed to create window");
        return 1;
    }
    
    // Create tray icon
    CreateTrayIcon(g_hwnd);
    
    // Initialize application components
    if (!InitializeApp()) {
        spdlog::error("Failed to initialize application");
        RemoveTrayIcon(g_hwnd);
        return 1;
    }
    
    // Message loop
    MSG msg;
    while (g_isRunning && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Cleanup
    RemoveTrayIcon(g_hwnd);
    
    return 0;
}

// Window procedure for the message-only window
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP && wParam == TRAYICON_ID) {
                ShowTrayMenu(hwnd);
            }
            else if (lParam == WM_LBUTTONUP && wParam == TRAYICON_ID) {
                ShowSettingsWindow();
            }
            return 0;
            
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDM_EXIT:
                    g_isRunning = false;
                    PostQuitMessage(0);
                    return 0;
                    
                case IDM_SETTINGS:
                    ShowSettingsWindow();
                    return 0;
                    
                case IDM_PROFILE_1080P_60HZ:
                    if (g_capture) {
                        g_capture->setProfile(CaptureDXGI::Profile::FullHD_60Hz);
                    }
                    return 0;
                    
                case IDM_PROFILE_TABLET_60HZ:
                    if (g_capture) {
                        g_capture->setProfile(CaptureDXGI::Profile::Tablet_60Hz);
                    }
                    return 0;
                    
                case IDM_PROFILE_TABLET_120HZ:
                    if (g_capture) {
                        g_capture->setProfile(CaptureDXGI::Profile::Tablet_120Hz);
                    }
                    return 0;
            }
            break;
            
        case WM_DESTROY:
            g_isRunning = false;
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Create the tray icon
void CreateTrayIcon(HWND hwnd)
{
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = hwnd;
    g_nid.uID = TRAYICON_ID;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    
    // Load icon from resources
    // We'll use LoadIcon for now, but ideally we'd load from resources
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    
    wcscpy_s(g_nid.szTip, L"TabDisplay");
    
    Shell_NotifyIcon(NIM_ADD, &g_nid);
}

// Remove the tray icon
void RemoveTrayIcon(HWND hwnd)
{
    Shell_NotifyIcon(NIM_DELETE, &g_nid);
}

// Show the tray menu
void ShowTrayMenu(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);
    
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;
    
    // Add menu items
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_SETTINGS, L"Settings");
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    
    // Add profile submenu
    HMENU hProfileMenu = CreatePopupMenu();
    if (hProfileMenu) {
        InsertMenu(hProfileMenu, -1, MF_BYPOSITION | MF_STRING, IDM_PROFILE_1080P_60HZ, L"1080p 60Hz");
        InsertMenu(hProfileMenu, -1, MF_BYPOSITION | MF_STRING, IDM_PROFILE_TABLET_60HZ, L"1752x2800 60Hz");
        InsertMenu(hProfileMenu, -1, MF_BYPOSITION | MF_STRING, IDM_PROFILE_TABLET_120HZ, L"1752x2800 120Hz");
        
        // Add submenu to main menu
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hProfileMenu, L"Profile");
    }
    
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_EXIT, L"Exit");
    
    // Track the popup menu
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

} // namespace TabDisplay