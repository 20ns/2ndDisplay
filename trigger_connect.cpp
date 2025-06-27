#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>

// Message IDs from TrayApp_win32.cpp
#define ID_TRAY_CONNECT 1003

int main() {
    std::cout << "TabDisplay Connection Trigger" << std::endl;
    std::cout << "==============================" << std::endl;
    
    // Find the TabDisplay window
    HWND hwnd = FindWindowA(NULL, "TabDisplay");
    if (!hwnd) {
        // Try to find by class name or any window with TabDisplay in title
        hwnd = FindWindowA("TabDisplayWindowClass", NULL);
    }
    
    if (!hwnd) {
        std::cout << "❌ Could not find TabDisplay window" << std::endl;
        std::cout << "Make sure TabDisplay.exe is running with tray icon" << std::endl;
        return 1;
    }
    
    std::cout << "✅ Found TabDisplay window: " << hwnd << std::endl;
    
    // Send the connect message
    std::cout << "Sending connect command..." << std::endl;
    LRESULT result = SendMessage(hwnd, WM_COMMAND, ID_TRAY_CONNECT, 0);
    
    std::cout << "Message sent (result: " << result << ")" << std::endl;
    std::cout << "Check the TabDisplay.log file for discovery and connection attempts" << std::endl;
    
    return 0;
}
