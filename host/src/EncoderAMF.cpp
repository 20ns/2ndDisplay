#include "EncoderAMF.hpp"
#include <spdlog/spdlog.h>
#include <mutex>

// AMD AMF headers
#if __has_include(<components/Component.h>)
#  include <components/Component.h>
#  include <components/VideoEncoderVCE.h>
#  include <components/VideoEncoderHEVC.h>
#  include <core/Factory.h>
#  include <core/Context.h>
#  include <core/Surface.h>
#else
#  include "../third_party/amf_stub/amf_stub.hpp"  // provided in third_party/amf_stub when the real SDK is missing
#endif

namespace TabDisplay {

// AMF functions
typedef AMF_RESULT (AMF_CDECL_CALL *AMFInit_Fn)(amf_uint64 version, amf::AMFFactory **ppFactory);
typedef AMF_RESULT (AMF_CDECL_CALL *AMFQueryVersion_Fn)(amf_uint64 *pVersion);

// Static AMF variables
static amf::AMFFactory* g_AMFFactory = nullptr;
static amf::AMFDebug* g_AMFDebug = nullptr;
static amf::AMFTrace* g_AMFTrace = nullptr;

// If we compiled with the stub headers we need minimal type aliases in the amf namespace
#ifndef AMF_SURFACE_BGRA
namespace amf { enum AMF_SURFACE_FORMAT { AMF_SURFACE_BGRA = 0 }; }
#endif

EncoderAMF::EncoderAMF()
    : amfModule_(nullptr)
    , amfContext_(nullptr)
    , amfEncoder_(nullptr)
    , forceIDR_(false)
    , currentBitrate_(0)
{
}

EncoderAMF::~EncoderAMF()
{
    // Release AMF components in reverse order
    if (amfEncoder_) {
        reinterpret_cast<amf::AMFComponent*>(amfEncoder_)->Terminate();
        amfEncoder_ = nullptr;
    }

    if (amfContext_) {
        reinterpret_cast<amf::AMFContext*>(amfContext_)->Terminate();
        amfContext_ = nullptr;
    }

    unloadAMFLibrary();

    spdlog::info("AMF encoder destroyed");
}

bool EncoderAMF::loadAMFLibrary()
{
    // Load AMF Runtime DLL
    amfModule_ = LoadLibraryW(L"amfrt64.dll");
    if (amfModule_ == nullptr) {
        spdlog::error("Failed to load AMF Runtime DLL");
        return false;
    }

    // Get functions
    AMFInit_Fn initFn = reinterpret_cast<AMFInit_Fn>(GetProcAddress(reinterpret_cast<HMODULE>(amfModule_), "AMFInit"));
    AMFQueryVersion_Fn versionFn = reinterpret_cast<AMFQueryVersion_Fn>(GetProcAddress(reinterpret_cast<HMODULE>(amfModule_), "AMFQueryVersion"));

    if (!initFn || !versionFn) {
        spdlog::error("Failed to get AMF functions");
        FreeLibrary(reinterpret_cast<HMODULE>(amfModule_));
        amfModule_ = nullptr;
        return false;
    }

    // Query version
    amf_uint64 version = 0;
    AMF_RESULT result = versionFn(&version);
    if (result != AMF_OK) {
        spdlog::error("Failed to query AMF version, error: {}", result);
        FreeLibrary(reinterpret_cast<HMODULE>(amfModule_));
        amfModule_ = nullptr;
        return false;
    }

    // Initialize AMF
    result = initFn(version, &g_AMFFactory);
    if (result != AMF_OK) {
        spdlog::error("Failed to initialize AMF, error: {}", result);
        FreeLibrary(reinterpret_cast<HMODULE>(amfModule_));
        amfModule_ = nullptr;
        return false;
    }

    // Get debug interfaces
    g_AMFFactory->GetTrace(&g_AMFTrace);
    g_AMFFactory->GetDebug(&g_AMFDebug);

    spdlog::info("AMF library loaded successfully, version: {}", version);
    return true;
}

void EncoderAMF::unloadAMFLibrary()
{
    if (amfModule_) {
        FreeLibrary(reinterpret_cast<HMODULE>(amfModule_));
        amfModule_ = nullptr;
        g_AMFFactory = nullptr;
        g_AMFTrace = nullptr;
        g_AMFDebug = nullptr;
    }
}

bool EncoderAMF::createAMFContext(const Microsoft::WRL::ComPtr<ID3D11Device>& device)
{
    if (!g_AMFFactory) {
        spdlog::error("AMF factory not initialized");
        return false;
    }

    // Create AMF context
    void* ctx = nullptr;
    AMF_RESULT result = g_AMFFactory->CreateContext(&ctx);
    amfContext_ = ctx;
    if (result != AMF_OK) {
        spdlog::error("Failed to create AMF context, error: {} ({})", result, getAMFErrorString(result));
        return false;
    }

    spdlog::info("AMF context created, initializing with D3D11 device...");

    // Initialize with D3D11 device
    result = reinterpret_cast<amf::AMFContext*>(amfContext_)->InitDX11(device.Get());
    if (result != AMF_OK) {
        spdlog::error("Failed to initialize AMF context with D3D11, error: {} ({})", result, getAMFErrorString(result));
        
        // Try alternative initialization methods
        spdlog::info("Trying CPU-based AMF initialization as fallback...");
        result = reinterpret_cast<amf::AMFContext*>(amfContext_)->InitDX11(nullptr);
        if (result != AMF_OK) {
            spdlog::error("CPU fallback also failed, error: {} ({})", result, getAMFErrorString(result));
            reinterpret_cast<amf::AMFContext*>(amfContext_)->Terminate();
            amfContext_ = nullptr;
            return false;
        }
        spdlog::warn("Using CPU-based AMF initialization (performance may be reduced)");
    }

    spdlog::info("AMF context created and initialized successfully");
    return true;
}

bool EncoderAMF::createEncoder()
{
    if (!amfContext_) {
        spdlog::error("AMF context not initialized");
        return false;
    }

    // Create H.264 encoder component
    void* enc = nullptr;
    AMF_RESULT result = g_AMFFactory->CreateComponent(reinterpret_cast<amf::AMFContext*>(amfContext_),
                                                     AMFVideoEncoderVCE_AVC,
                                                     &enc);
    amfEncoder_ = enc;
    if (result != AMF_OK) {
        spdlog::error("Failed to create AMF encoder component, error: {}", result);
        return false;
    }

    spdlog::info("AMF encoder component created successfully");
    return true;
}

bool EncoderAMF::configureEncoder(const EncoderSettings& settings)
{
    if (!amfEncoder_) {
        spdlog::error("AMF encoder not created");
        return false;
    }

    auto encoder = reinterpret_cast<amf::AMFComponent*>(amfEncoder_);
    AMF_RESULT result;

    // Set common properties
    result = encoder->SetProperty(AMF_VIDEO_ENCODER_USAGE, AMF_VIDEO_ENCODER_USAGE_LOW_LATENCY);
    result = encoder->SetProperty(AMF_VIDEO_ENCODER_QUALITY_PRESET, AMF_VIDEO_ENCODER_QUALITY_PRESET_SPEED);
    
    // Set resolution
    result = encoder->SetProperty(AMF_VIDEO_ENCODER_WIDTH, static_cast<amf_uint64>(settings.width));
    result = encoder->SetProperty(AMF_VIDEO_ENCODER_HEIGHT, static_cast<amf_uint64>(settings.height));
    
    // Set frame rate (use a named variable to avoid potential macro interference)
    amf_ratio_t framerateRatio;
    framerateRatio.num = static_cast<int>(settings.frameRate);
    framerateRatio.den = 1;
    result = encoder->SetProperty(AMF_VIDEO_ENCODER_FRAMERATE, framerateRatio);
    
    // Set rate control mode and bitrate
    result = encoder->SetProperty(AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD, AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CBR);
    result = encoder->SetProperty(AMF_VIDEO_ENCODER_TARGET_BITRATE, static_cast<amf_uint64>(settings.bitrate) * 1000000ULL); // Mbps to bps
    result = encoder->SetProperty(AMF_VIDEO_ENCODER_PEAK_BITRATE, static_cast<amf_uint64>(settings.bitrate) * 1000000ULL);
    
    // B-frames setting
    result = encoder->SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, settings.useBFrames ? 1 : 0);
    result = encoder->SetProperty(AMF_VIDEO_ENCODER_B_REFERENCE_ENABLE, false);
    
    // Low latency specific settings
    if (settings.lowLatency) {
        result = encoder->SetProperty(AMF_VIDEO_ENCODER_LOW_LATENCY_MODE, true);
        result = encoder->SetProperty(AMF_VIDEO_ENCODER_RATE_CONTROL_SKIP_FRAME_ENABLE, false);
        result = encoder->SetProperty(AMF_VIDEO_ENCODER_INTRA_REFRESH_NUM_MBS_PER_SLOT, 0);
    }
    
    // GOP settings - keyframe interval
    result = encoder->SetProperty(AMF_VIDEO_ENCODER_IDR_PERIOD, 60);
    
    // Initialize the encoder after setting all properties
    result = encoder->Init(AMF_SURFACE_BGRA, settings.width, settings.height);
    if (result != AMF_OK) {
        spdlog::error("Failed to initialize AMF encoder, error: {}", result);
        return false;
    }

    currentSettings_ = settings;
    currentBitrate_ = settings.bitrate;
    
    spdlog::info("AMF encoder configured: {}x{} at {}fps, {}Mbps", 
                settings.width, settings.height, settings.frameRate, settings.bitrate);
    return true;
}

bool EncoderAMF::initialize(const Microsoft::WRL::ComPtr<ID3D11Device>& device)
{
    // Load AMF library
    if (!loadAMFLibrary()) {
        return false;
    }

    // Create AMF context with D3D11 device
    if (!createAMFContext(device)) {
        unloadAMFLibrary();
        return false;
    }

    // Create encoder component
    if (!createEncoder()) {
        reinterpret_cast<amf::AMFContext*>(amfContext_)->Terminate();
        amfContext_ = nullptr;
        unloadAMFLibrary();
        return false;
    }

    spdlog::info("AMF encoder initialized successfully");
    return true;
}

bool EncoderAMF::configure(const EncoderSettings& settings)
{
    std::lock_guard<std::mutex> lock(encoderMutex_);

    // If encoder already configured, terminate it first
    if (amfEncoder_) {
        reinterpret_cast<amf::AMFComponent*>(amfEncoder_)->Terminate();
        amfEncoder_ = nullptr;
    }

    // Create new encoder
    if (!createEncoder()) {
        return false;
    }

    // Configure encoder with new settings
    if (!configureEncoder(settings)) {
        reinterpret_cast<amf::AMFComponent*>(amfEncoder_)->Terminate();
        amfEncoder_ = nullptr;
        return false;
    }

    return true;
}

bool EncoderAMF::encodeFrame(const Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture)
{
    if (!amfContext_ || !amfEncoder_) {
        spdlog::error("AMF not initialized");
        return false;
    }

    std::lock_guard<std::mutex> lock(encoderMutex_);
    auto context = reinterpret_cast<amf::AMFContext*>(amfContext_);
    auto encoder = reinterpret_cast<amf::AMFComponent*>(amfEncoder_);
    AMF_RESULT result;

    // Create surface from D3D11 texture
    amf::AMFSurface* surface = nullptr;
    result = context->CreateSurfaceFromDX11Native(texture.Get(), reinterpret_cast<void**>(&surface), nullptr);
    if (result != AMF_OK) {
        spdlog::error("Failed to create AMF surface from texture, error: {}", result);
        return false;
    }

    // Force keyframe if requested
    if (forceIDR_) {
        surface->SetProperty(AMF_VIDEO_ENCODER_FORCE_PICTURE_TYPE, AMF_VIDEO_ENCODER_PICTURE_TYPE_IDR);
        forceIDR_ = false;
    }

    // Submit frame for encoding
    result = encoder->SubmitInput(surface);
    if (result != AMF_OK) {
        if (result == AMF_INPUT_FULL) {
            // Input queue full, need to query output
            spdlog::debug("AMF encoder input queue full");
        } else {
            spdlog::error("Failed to submit input to AMF encoder, error: {}", result);
            surface->Release();
            return false;
        }
    }

    surface->Release();

    // Query encoded data
    amf::AMFData* outputData = nullptr;
    result = encoder->QueryOutput(&outputData);
    while (result == AMF_OK && outputData) {
        // Process encoded data
        auto buffer = reinterpret_cast<amf::AMFBuffer*>(outputData);
        if (buffer) {
            processEncodedData(reinterpret_cast<AMFSurface*>(outputData));
        }

        // Release this output
        outputData->Release();
        outputData = nullptr;

        // Query next output
        result = encoder->QueryOutput(&outputData);
    }

    return true;
}

void EncoderAMF::processEncodedData(AMFSurface* surface)
{
    auto buffer = reinterpret_cast<amf::AMFBuffer*>(surface);
    if (!buffer) return;

    // Get buffer properties
    bool isKeyFrame = false;
    surface->GetProperty(AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE, &isKeyFrame);
    isKeyFrame = (isKeyFrame == AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_IDR);

    uint64_t timestamp = 0;
    surface->GetProperty(AMF_PTS, &timestamp);
    
    // Get data
    uint8_t* data = static_cast<uint8_t*>(buffer->GetNative());
    size_t size = buffer->GetSize();

    // Create a copy of the encoded data
    EncodedFrame frame;
    frame.data.resize(size);
    memcpy(frame.data.data(), data, size);
    frame.isKeyFrame = isKeyFrame;
    frame.timestamp = timestamp;

    // Call callback
    if (frameCallback_) {
        frameCallback_(frame);
    }
}

bool EncoderAMF::setBitrate(uint32_t bitrateInMbps)
{
    if (!amfEncoder_) {
        spdlog::error("AMF encoder not created");
        return false;
    }

    std::lock_guard<std::mutex> lock(encoderMutex_);
    auto encoder = reinterpret_cast<amf::AMFComponent*>(amfEncoder_);
    
    // Set new bitrate
    AMF_RESULT result = encoder->SetProperty(AMF_VIDEO_ENCODER_TARGET_BITRATE, static_cast<amf_uint64>(bitrateInMbps) * 1000000ULL);
    if (result != AMF_OK) {
        spdlog::error("Failed to set target bitrate, error: {}", result);
        return false;
    }
    
    result = encoder->SetProperty(AMF_VIDEO_ENCODER_PEAK_BITRATE, static_cast<amf_uint64>(bitrateInMbps) * 1000000ULL);
    if (result != AMF_OK) {
        spdlog::error("Failed to set peak bitrate, error: {}", result);
        return false;
    }

    currentBitrate_ = bitrateInMbps;
    spdlog::info("Encoder bitrate changed to {} Mbps", bitrateInMbps);
    
    return true;
}

void EncoderAMF::setFrameCallback(FrameCallback callback)
{
    frameCallback_ = callback;
}

uint32_t EncoderAMF::getCurrentBitrate() const
{
    return currentBitrate_;
}

void EncoderAMF::forceKeyFrame()
{
    forceIDR_ = true;
}

const char* EncoderAMF::getAMFErrorString(AMF_RESULT result)
{
    // Use simple if-else chain to avoid issues with AMF constants in stubs
    if (result == 0) return "OK";
    if (result == 1) return "General failure";
    if (result == 2) return "Unexpected error";
    if (result == 3) return "Access denied";
    if (result == 4) return "Invalid argument";
    if (result == 5) return "Out of range";
    if (result == 6) return "Out of memory";
    if (result == 7) return "Invalid pointer";
    if (result == 8) return "No interface";
    if (result == 9) return "Not implemented";
    if (result == 10) return "Not supported";
    if (result == 11) return "Not found";
    if (result == 12) return "Already initialized";
    if (result == 13) return "Not initialized";
    if (result == 14) return "Invalid format";
    if (result == 15) return "Wrong state";
    if (result == 16) return "File not open";
    return "Unknown error";
}

} // namespace TabDisplay