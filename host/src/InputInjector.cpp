#include "InputInjector.hpp"
#include <spdlog/spdlog.h>

// Include ViGEmBus headers
#include <ViGEm/Client.h>

namespace TabDisplay {

InputInjector::InputInjector()
    : client_(nullptr)
    , target_(nullptr)
    , screenWidth_(1920)
    , screenHeight_(1080)
{
}

InputInjector::~InputInjector()
{
    if (target_) {
        vigem_target_remove(client_, target_);
        vigem_target_free(target_);
        target_ = nullptr;
    }
    
    if (client_) {
        vigem_disconnect(client_);
        vigem_free(client_);
        client_ = nullptr;
    }
}

bool InputInjector::initialize()
{
    // Check if ViGEmBus is available
    if (!isViGEmBusAvailable()) {
        spdlog::error("ViGEmBus is not available");
        return false;
    }
    
    // Create client
    client_ = vigem_alloc();
    if (!client_) {
        spdlog::error("Failed to allocate ViGEm client");
        return false;
    }
    
    // Connect to driver
    const auto result = vigem_connect(client_);
    if (!VIGEM_SUCCESS(result)) {
        spdlog::error("Failed to connect to ViGEm Bus, error: {}", result);
        vigem_free(client_);
        client_ = nullptr;
        return false;
    }
    
    spdlog::info("Connected to ViGEm Bus");
    
    // Create virtual controller (we'll use an Xbox pad since it has analog support)
    target_ = vigem_target_x360_alloc();
    if (!target_) {
        spdlog::error("Failed to allocate Xbox 360 Controller");
        vigem_disconnect(client_);
        vigem_free(client_);
        client_ = nullptr;
        return false;
    }
    
    // Add controller to bus
    const auto addResult = vigem_target_add(client_, target_);
    if (!VIGEM_SUCCESS(addResult)) {
        spdlog::error("Failed to add Xbox 360 Controller to bus, error: {}", addResult);
        vigem_target_free(target_);
        target_ = nullptr;
        vigem_disconnect(client_);
        vigem_free(client_);
        client_ = nullptr;
        return false;
    }
    
    spdlog::info("Virtual controller initialized");
    return true;
}

bool InputInjector::injectMouseEvent(uint32_t x, uint32_t y, InputType type)
{
    LONG absoluteX, absoluteY;
    convertCoordinates(x, y, absoluteX, absoluteY);
    
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dx = absoluteX;
    input.mi.dy = absoluteY;
    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    
    if (type == InputType::MOUSE_MOVE || type == InputType::TOUCH_MOVE) {
        input.mi.dwFlags |= MOUSEEVENTF_MOVE;
    }
    else if (type == InputType::MOUSE_DOWN || type == InputType::TOUCH_DOWN) {
        input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
    }
    else if (type == InputType::MOUSE_UP || type == InputType::TOUCH_UP) {
        input.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
    }
    
    UINT result = SendInput(1, &input, sizeof(INPUT));
    if (result != 1) {
        DWORD error = GetLastError();
        spdlog::error("Failed to inject mouse event, error: {}", error);
        
        if (statusCallback_) {
            statusCallback_(false, "Failed to inject mouse event");
        }
        
        return false;
    }
    
    return true;
}

bool InputInjector::injectTouchEvent(uint32_t x, uint32_t y, uint32_t pointerId, InputType type)
{
    // Store touch point for tracking
    if (type == InputType::TOUCH_DOWN || type == InputType::TOUCH_MOVE) {
        POINT pt = { static_cast<LONG>(x), static_cast<LONG>(y) };
        activeTouchPoints_[pointerId] = pt;
    }
    else if (type == InputType::TOUCH_UP) {
        activeTouchPoints_.erase(pointerId);
    }
    
    // For single touch, we can use mouse injection
    if (activeTouchPoints_.size() <= 1) {
        return injectMouseEvent(x, y, type);
    }
    
    // TODO: For multi-touch we would need to use Windows Touch Injection API
    // But this requires more complex setup and is out of scope for this initial implementation
    
    // Just inject the primary touch point as mouse
    if (!activeTouchPoints_.empty() && type == InputType::TOUCH_MOVE) {
        const auto& primaryPoint = activeTouchPoints_.begin()->second;
        return injectMouseEvent(primaryPoint.x, primaryPoint.y, type);
    }
    
    return true;
}

bool InputInjector::injectKeyEvent(uint32_t keyCode, bool isKeyDown)
{
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = static_cast<WORD>(keyCode);
    input.ki.dwFlags = isKeyDown ? 0 : KEYEVENTF_KEYUP;
    
    UINT result = SendInput(1, &input, sizeof(INPUT));
    if (result != 1) {
        DWORD error = GetLastError();
        spdlog::error("Failed to inject key event, error: {}", error);
        
        if (statusCallback_) {
            statusCallback_(false, "Failed to inject key event");
        }
        
        return false;
    }
    
    return true;
}

void InputInjector::setScreenResolution(uint32_t width, uint32_t height)
{
    screenWidth_ = width;
    screenHeight_ = height;
    spdlog::debug("Screen resolution set to {}x{}", width, height);
}

void InputInjector::setStatusCallback(StatusCallback callback)
{
    statusCallback_ = callback;
}

bool InputInjector::isViGEmBusAvailable()
{
    // Try to load the DLL (explicit wide-char overload to avoid char/wchar mismatch)
    HMODULE vigemDll = LoadLibraryW(L"ViGEmClient.dll");
    if (!vigemDll) {
        return false;
    }
    
    FreeLibrary(vigemDll);
    return true;
}

void InputInjector::convertCoordinates(uint32_t x, uint32_t y, LONG& outX, LONG& outY)
{
    // Convert screen coordinates to absolute coordinates (0-65535)
    outX = static_cast<LONG>((x * 65535) / screenWidth_);
    outY = static_cast<LONG>((y * 65535) / screenHeight_);
}

} // namespace TabDisplay