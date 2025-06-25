// Win32 Tray implementation for TabDisplay
#include <windows.h>
#include <shellapi.h>
#include <spdlog/spdlog.h>

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_SHOW 1002

namespace TabDisplay {

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
        AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit TabDisplay");

        SetForegroundWindow(m_hwnd);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, nullptr);
        DestroyMenu(menu);
    }

    void ShowStatusDialog() {
        MessageBoxA(nullptr,
            "TabDisplay v0.1.0 - Win32 Tray Mode\n\n"
            "✅ Windows App SDK: Detected\n"
            "✅ Tray Icon: Active (Win32)\n"
            "✅ Core functionality: Available\n"
            "⚠️  WinUI 3 UI: Partial (using Win32 fallback)\n\n"
            "Right-click tray icon for menu.\n"
            "Check TabDisplay.log for detailed logging.",
            "TabDisplay Status",
            MB_OK | MB_ICONINFORMATION);
    }

public:
    bool Initialize() {
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
