#pragma once

#include <Windows.h>
#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include <unordered_map>
#include <ViGEm/Client.h>

namespace TabDisplay {

class InputInjector {
public:
    // Input event types
    enum class InputType {
        TOUCH_DOWN,
        TOUCH_MOVE,
        TOUCH_UP,
        MOUSE_MOVE,
        MOUSE_DOWN,
        MOUSE_UP,
        KEYBOARD
    };

    // Input event data
    struct InputEvent {
        InputType type;
        uint32_t x;
        uint32_t y;
        uint32_t pointerId;
        uint32_t keyCode;
    };

    InputInjector();
    ~InputInjector();

    // Initialize the injector with ViGEmBus
    bool initialize();

    // Inject mouse event
    bool injectMouseEvent(uint32_t x, uint32_t y, InputType type);

    // Inject touch event with multitouch support
    bool injectTouchEvent(uint32_t x, uint32_t y, uint32_t pointerId, InputType type);

    // Inject keyboard event
    bool injectKeyEvent(uint32_t keyCode, bool isKeyDown);

    // Set screen resolution for scaling calculations
    void setScreenResolution(uint32_t width, uint32_t height);

    // Set callback for injection status
    using StatusCallback = std::function<void(bool success, const std::string& message)>;
    void setStatusCallback(StatusCallback callback);

    // Check if ViGEmBus is available
    static bool isViGEmBusAvailable();

private:
    // ViGEmBus client and target
    PVIGEM_CLIENT client_;
    PVIGEM_TARGET target_;

    // Status callback
    StatusCallback statusCallback_;

    // Screen resolution for scaling
    uint32_t screenWidth_;
    uint32_t screenHeight_;

    // Track active touch points
    std::unordered_map<uint32_t, POINT> activeTouchPoints_;
    
    // Convert screen coordinates to absolute mouse coordinates
    void convertCoordinates(uint32_t x, uint32_t y, LONG& outX, LONG& outY);
};

} // namespace TabDisplay