#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <wrl/client.h>

namespace TabDisplay {

class EncoderAMF; // forward declaration to avoid cyclic include

class CaptureDXGI {
public:
    struct CaptureFrame {
        std::vector<uint8_t> data;
        uint32_t width;
        uint32_t height;
        uint32_t rowPitch;
        bool keyFrame;
    };

    // Resolution profiles
    enum class Profile {
        FullHD_60Hz,         // 1080p 60Hz
        Tablet_60Hz,         // 1752�2800 60Hz
        Tablet_120Hz         // 1752�2800 120Hz
    };

    CaptureDXGI();
    ~CaptureDXGI();

    // Initialize DXGI and D3D11 resources
    bool initialize();
    
    // Start capture
    bool startCapture();
    
    // Stop capture
    void stopCapture();
    
    // Set profile and update settings
    bool setProfile(Profile profile);
    
    // Get current profile
    Profile getCurrentProfile() const;

    // Get frame callback
    using FrameCallback = std::function<void(const CaptureFrame&)>;
    void setFrameCallback(FrameCallback callback);

    // Get current frame rate
    uint32_t getCurrentFrameRate() const;

    // NEW: expose D3D11 device so external modules can initialize the encoder
    Microsoft::WRL::ComPtr<ID3D11Device> getDevice() const { return device_; }

    // NEW: provide encoder pointer so CaptureDXGI can push frames directly
    void setEncoder(EncoderAMF* encoder);

    // Set capture mode and region
    void setCaptureMode(CaptureMode mode);
    void setCustomRegion(int x, int y, int width, int height);
    void setSecondMonitorRegion(); // Auto-configure for virtual second monitor
    
    // Get current capture settings
    CaptureMode getCaptureMode() const;
    CaptureRegion getCaptureRegion() const;

private:
    // Resolution profile settings
    struct ProfileSettings {
        uint32_t width;
        uint32_t height;
        uint32_t frameRate;
    };

    // Capture thread function
    void captureThread();

    // Profile settings
    static const ProfileSettings PROFILE_SETTINGS[];
    Profile currentProfile_;

    // DXGI and D3D11 resources
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> outputDuplication_;
    Microsoft::WRL::ComPtr<IDXGIOutput1> output_;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter_;

    // Thread control
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> captureThreadPtr_;
    std::mutex frameMutex_;
    
    // Frame callback
    FrameCallback frameCallback_;
    
    // Performance metrics
    std::atomic<uint32_t> currentFrameRate_;
    
    // Get DXGI output for primary display
    bool findPrimaryOutput();

    // Pointer to the encoder instance (non-owning)
    EncoderAMF* encoder_{};

    // Second monitor capture region
    struct CaptureRegion {
        int x, y;           // Desktop coordinates
        int width, height;  // Capture dimensions
    };

    // Capture modes
    enum class CaptureMode {
        PrimaryMonitor,     // Original behavior - capture primary monitor
        SecondMonitor,      // Capture virtual second monitor area  
        CustomRegion        // Capture specific desktop region
    };

    // Current capture settings
    CaptureMode captureMode_;
    CaptureRegion captureRegion_;
    
    // Helper methods for region capture
    bool cropFrame(const Microsoft::WRL::ComPtr<ID3D11Texture2D>& sourceTexture,
                   Microsoft::WRL::ComPtr<ID3D11Texture2D>& croppedTexture,
                   const CaptureRegion& region);
};

} // namespace TabDisplay