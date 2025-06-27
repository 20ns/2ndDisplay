#include "CaptureDXGI.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <string>
#include <windows.h>
#include "EncoderAMF.hpp"

namespace TabDisplay {

// Profile settings definition
const CaptureDXGI::ProfileSettings CaptureDXGI::PROFILE_SETTINGS[] = {
    { 1920, 1080, 60 },    // FullHD_60Hz
    { 1752, 2800, 60 },    // Tablet_60Hz
    { 1752, 2800, 120 }    // Tablet_120Hz
};

CaptureDXGI::CaptureDXGI()
    : currentProfile_(Profile::FullHD_60Hz)
    , running_(false)
    , currentFrameRate_(0)
    , encoder_(nullptr)
    , captureMode_(CaptureMode::PrimaryMonitor)
    , captureRegion_{0, 0, 1920, 1080}
{
}

CaptureDXGI::~CaptureDXGI() {
    stopCapture();
}

bool CaptureDXGI::initialize() {
    // Create D3D11 device
    D3D_FEATURE_LEVEL featureLevel;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        device_.GetAddressOf(),
        &featureLevel,
        context_.GetAddressOf()
    );

    if (FAILED(hr)) {
        spdlog::error("Failed to create D3D11 device, error: {0:x}", hr);
        return false;
    }

    spdlog::info("D3D11 device created successfully with feature level: {0:x}", featureLevel);

    // Find primary display output
    if (!findPrimaryOutput()) {
        spdlog::error("Failed to find primary display output");
        return false;
    }

    return true;
}

bool CaptureDXGI::findPrimaryOutput() {
    // Get DXGI device
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = device_.As(&dxgiDevice);
    if (FAILED(hr)) {
        spdlog::error("Failed to get DXGI device, error: {0:x}", hr);
        return false;
    }

    // Get DXGI adapter
    hr = dxgiDevice->GetAdapter(reinterpret_cast<IDXGIAdapter**>(adapter_.GetAddressOf()));
    if (FAILED(hr)) {
        spdlog::error("Failed to get DXGI adapter, error: {0:x}", hr);
        return false;
    }

    // Enumerate outputs to find the primary one
    UINT i = 0;
    Microsoft::WRL::ComPtr<IDXGIOutput> currentOutput;
    while (adapter_->EnumOutputs(i, currentOutput.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND) {
        DXGI_OUTPUT_DESC desc;
        hr = currentOutput->GetDesc(&desc);
        if (SUCCEEDED(hr)) {
            // Convert wide char device name to UTF-8 string for logging
            std::wstring wname(desc.DeviceName);
            std::string name(wname.begin(), wname.end());
            spdlog::info("Found display output: {} ({}x{})", 
                        name, 
                        desc.DesktopCoordinates.right - desc.DesktopCoordinates.left,
                        desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top);

            if (desc.AttachedToDesktop) {
                // Take the first attached output as primary
                hr = currentOutput.As(&output_);
                if (SUCCEEDED(hr)) {
                    std::wstring wprim(desc.DeviceName);
                    std::string prim(wprim.begin(), wprim.end());
                    spdlog::info("Selected primary display: {}", prim);
                    return true;
                }
            }
        }
        i++;
    }

    spdlog::error("No suitable display output found");
    return false;
}

bool CaptureDXGI::startCapture() {
    if (running_) {
        spdlog::warn("Capture already running");
        return true;
    }

    // Get duplication interface
    HRESULT hr = output_->DuplicateOutput(device_.Get(), outputDuplication_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        spdlog::error("Failed to duplicate output, error: {0:x}", hr);
        return false;
    }

    // Start capture thread
    running_ = true;
    captureThreadPtr_ = std::make_unique<std::thread>(&CaptureDXGI::captureThread, this);
    
    spdlog::info("Screen capture started");
    return true;
}

void CaptureDXGI::stopCapture() {
    if (!running_) return;

    running_ = false;
    if (captureThreadPtr_ && captureThreadPtr_->joinable()) {
        captureThreadPtr_->join();
        captureThreadPtr_.reset();
    }

    outputDuplication_.Reset();
    spdlog::info("Screen capture stopped");
}

bool CaptureDXGI::setProfile(Profile profile) {
    if (profile < Profile::FullHD_60Hz || profile > Profile::Tablet_120Hz) {
        spdlog::error("Invalid profile");
        return false;
    }

    currentProfile_ = profile;
    spdlog::info("Profile set to {}x{} at {} fps",
               PROFILE_SETTINGS[static_cast<int>(currentProfile_)].width,
               PROFILE_SETTINGS[static_cast<int>(currentProfile_)].height,
               PROFILE_SETTINGS[static_cast<int>(currentProfile_)].frameRate);
    return true;
}

CaptureDXGI::Profile CaptureDXGI::getCurrentProfile() const {
    return currentProfile_;
}

void CaptureDXGI::setFrameCallback(FrameCallback callback) {
    std::lock_guard<std::mutex> lock(frameMutex_);
    frameCallback_ = callback;
}

uint32_t CaptureDXGI::getCurrentFrameRate() const {
    return currentFrameRate_;
}

void CaptureDXGI::setEncoder(EncoderAMF* encoder) {
    encoder_ = encoder;
}

void CaptureDXGI::captureThread() {
    DXGI_OUTDUPL_DESC outputDesc;
    outputDuplication_->GetDesc(&outputDesc);

    spdlog::info("Output duplication format: {}", outputDesc.ModeDesc.Format);
    spdlog::info("Output duplication size: {}x{}", 
                 outputDesc.ModeDesc.Width, 
                 outputDesc.ModeDesc.Height);

    // FPS calculation variables
    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    int frameCount = 0;
    auto fpsUpdateTime = lastFrameTime;

    // Target frame duration based on current profile
    const auto& profile = PROFILE_SETTINGS[static_cast<int>(currentProfile_)];
    const auto targetFrameDuration = std::chrono::nanoseconds(1000000000 / profile.frameRate);

    bool keyFrame = true; // First frame should be a key frame
    uint32_t frameSinceLastIDR = 0;

    while (running_) {
        // Throttle to target frame rate
        const auto now = std::chrono::high_resolution_clock::now();
        const auto elapsed = now - lastFrameTime;
        
        if (elapsed < targetFrameDuration) {
            std::this_thread::sleep_for(targetFrameDuration - elapsed);
        }

        // Update FPS counter
        frameCount++;
        const auto fpsElapsed = std::chrono::high_resolution_clock::now() - fpsUpdateTime;
        if (fpsElapsed >= std::chrono::seconds(1)) {
            currentFrameRate_ = static_cast<uint32_t>(frameCount / 
                std::chrono::duration<float>(fpsElapsed).count());
            frameCount = 0;
            fpsUpdateTime = std::chrono::high_resolution_clock::now();
        }

        // Capture frame
        Microsoft::WRL::ComPtr<IDXGIResource> desktopResource;
        DXGI_OUTDUPL_FRAME_INFO frameInfo;
        
        HRESULT hr = outputDuplication_->AcquireNextFrame(100, &frameInfo, desktopResource.GetAddressOf());
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            // No new frame, continue
            continue;
        } else if (FAILED(hr)) {
            spdlog::error("Failed to acquire next frame, error: {0:x}", hr);
            break;
        }

        // Get texture from resource
        Microsoft::WRL::ComPtr<ID3D11Texture2D> desktopTexture;
        hr = desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), 
                                            reinterpret_cast<void**>(desktopTexture.GetAddressOf()));
        if (FAILED(hr)) {
            spdlog::error("Failed to get texture from resource, error: {0:x}", hr);
            outputDuplication_->ReleaseFrame();
            continue;
        }

        // Get texture description
        D3D11_TEXTURE2D_DESC textureDesc;
        desktopTexture->GetDesc(&textureDesc);

        // Handle region-based capture
        Microsoft::WRL::ComPtr<ID3D11Texture2D> processedTexture = desktopTexture;
        D3D11_TEXTURE2D_DESC processedDesc = textureDesc;
        
        if (captureMode_ != CaptureMode::PrimaryMonitor) {
            // Crop the texture to the specified region
            Microsoft::WRL::ComPtr<ID3D11Texture2D> croppedTexture;
            if (cropFrame(desktopTexture, croppedTexture, captureRegion_)) {
                processedTexture = croppedTexture;
                processedTexture->GetDesc(&processedDesc);
                spdlog::debug("Frame cropped to region: {}x{} at ({}, {})", 
                             captureRegion_.width, captureRegion_.height,
                             captureRegion_.x, captureRegion_.y);
            } else {
                spdlog::warn("Failed to crop frame, using full frame");
            }
        }

        // Create staging texture for CPU access
        Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTexture;
        D3D11_TEXTURE2D_DESC stagingDesc = processedDesc;
        stagingDesc.BindFlags = 0;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        stagingDesc.Usage = D3D11_USAGE_STAGING;
        stagingDesc.MiscFlags = 0;

        hr = device_->CreateTexture2D(&stagingDesc, nullptr, stagingTexture.GetAddressOf());
        if (FAILED(hr)) {
            spdlog::error("Failed to create staging texture, error: {0:x}", hr);
            outputDuplication_->ReleaseFrame();
            continue;
        }

        // Copy to staging texture (use processed texture which may be cropped)
        context_->CopyResource(stagingTexture.Get(), processedTexture.Get());

        // --- Forward frame to hardware encoder ---
        if (encoder_) {
            // Force an IDR every 60 frames
            if (frameSinceLastIDR >= 60) {
                encoder_->forceKeyFrame();
                frameSinceLastIDR = 0;
            }
            // Use the processed texture (potentially cropped) for encoding
            encoder_->encodeFrame(processedTexture);
            ++frameSinceLastIDR;
        }

        // Map texture to get data
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        hr = context_->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
        if (FAILED(hr)) {
            spdlog::error("Failed to map staging texture, error: {0:x}", hr);
            outputDuplication_->ReleaseFrame();
            continue;
        }

        // Create frame data
        CaptureFrame frame;
        frame.width = processedDesc.Width;   // Use processed texture dimensions
        frame.height = processedDesc.Height; // Use processed texture dimensions
        frame.rowPitch = mappedResource.RowPitch;
        frame.keyFrame = keyFrame;
        
        // Allocate buffer and copy data
        frame.data.resize(mappedResource.RowPitch * processedDesc.Height);
        memcpy(frame.data.data(), mappedResource.pData, frame.data.size());
        
        // Unmap
        context_->Unmap(stagingTexture.Get(), 0);

        // Release frame
        outputDuplication_->ReleaseFrame();

        // Track keyframe flag for CPU frame callback every 60 frames
        keyFrame = (frameSinceLastIDR == 0);

        // Call callback
        if (frameCallback_) {
            std::lock_guard<std::mutex> lock(frameMutex_);
            frameCallback_(frame);
        }

        // Update last frame time for next iteration
        lastFrameTime = std::chrono::high_resolution_clock::now();
    }
}

void CaptureDXGI::setCaptureMode(CaptureMode mode) {
    captureMode_ = mode;
    spdlog::info("Capture mode set to: {}", static_cast<int>(mode));
}

void CaptureDXGI::setCustomRegion(int x, int y, int width, int height) {
    captureRegion_ = {x, y, width, height};
    captureMode_ = CaptureMode::CustomRegion;
    spdlog::info("Custom capture region set to: {}x{} at ({}, {})", width, height, x, y);
}

void CaptureDXGI::setSecondMonitorRegion() {
    // For now, configure a virtual second monitor to the right of the primary display
    // This creates a virtual extended desktop area
    
    // Get primary monitor resolution
    RECT primaryRect;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &primaryRect, 0);
    int primaryWidth = primaryRect.right - primaryRect.left;
    int primaryHeight = primaryRect.bottom - primaryRect.top;
    
    // Set second monitor region to the right of primary monitor
    // Use tablet resolution for the virtual second monitor
    const auto& profile = PROFILE_SETTINGS[static_cast<int>(currentProfile_)];
    
    captureRegion_ = {
        primaryWidth,  // Start to the right of primary monitor
        0,             // Top of screen
        static_cast<int>(profile.width),   // Use profile width
        static_cast<int>(profile.height)   // Use profile height
    };
    
    captureMode_ = CaptureMode::SecondMonitor;
    
    spdlog::info("Second monitor region configured: {}x{} at ({}, {})", 
                 captureRegion_.width, captureRegion_.height, 
                 captureRegion_.x, captureRegion_.y);
}

CaptureDXGI::CaptureMode CaptureDXGI::getCaptureMode() const {
    return captureMode_;
}

CaptureDXGI::CaptureRegion CaptureDXGI::getCaptureRegion() const {
    return captureRegion_;
}

bool CaptureDXGI::cropFrame(const Microsoft::WRL::ComPtr<ID3D11Texture2D>& sourceTexture,
                           Microsoft::WRL::ComPtr<ID3D11Texture2D>& croppedTexture,
                           const CaptureRegion& region) {
    
    // Get source texture description
    D3D11_TEXTURE2D_DESC sourceDesc;
    sourceTexture->GetDesc(&sourceDesc);
    
    // Validate region bounds
    if (region.x < 0 || region.y < 0 || 
        region.x + region.width > static_cast<int>(sourceDesc.Width) ||
        region.y + region.height > static_cast<int>(sourceDesc.Height)) {
        spdlog::error("Crop region is out of bounds");
        return false;
    }
    
    // Create cropped texture description
    D3D11_TEXTURE2D_DESC croppedDesc = sourceDesc;
    croppedDesc.Width = region.width;
    croppedDesc.Height = region.height;
    
    // Create the cropped texture
    HRESULT hr = device_->CreateTexture2D(&croppedDesc, nullptr, croppedTexture.GetAddressOf());
    if (FAILED(hr)) {
        spdlog::error("Failed to create cropped texture, error: {0:x}", hr);
        return false;
    }
    
    // Copy the region from source to cropped texture
    D3D11_BOX srcBox;
    srcBox.left = region.x;
    srcBox.top = region.y;
    srcBox.front = 0;
    srcBox.right = region.x + region.width;
    srcBox.bottom = region.y + region.height;
    srcBox.back = 1;
    
    context_->CopySubresourceRegion(
        croppedTexture.Get(), 0,  // Destination
        0, 0, 0,                  // Destination coordinates
        sourceTexture.Get(), 0,   // Source
        &srcBox                   // Source region
    );
    
    return true;
}

} // namespace TabDisplay