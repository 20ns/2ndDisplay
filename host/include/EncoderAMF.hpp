#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <wrl/client.h>

// Forward declarations for AMF types
typedef void* AMFContext;
typedef void* AMFComponent;
namespace amf { class AMFSurface; }

// Alias to use amf::AMFSurface without needing full AMF headers
using AMFSurface = amf::AMFSurface;

namespace TabDisplay {

class EncoderAMF {
public:
    struct EncodedFrame {
        std::vector<uint8_t> data;
        bool isKeyFrame;
        uint64_t timestamp;
    };

    // Encoding settings
    struct EncoderSettings {
        uint32_t width;
        uint32_t height;
        uint32_t frameRate;
        uint32_t bitrate;  // in Mbps
        bool lowLatency;
        bool useBFrames;
    };

    EncoderAMF();
    ~EncoderAMF();

    // Initialize AMF library and create encoder
    bool initialize(const Microsoft::WRL::ComPtr<ID3D11Device>& device);

    // Configure encoder settings
    bool configure(const EncoderSettings& settings);

    // Encode a frame from DXGI surface
    bool encodeFrame(const Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture);

    // Set encoding bitrate (can be changed during encoding)
    bool setBitrate(uint32_t bitrateInMbps);

    // Set frame callback for encoded frames
    using FrameCallback = std::function<void(const EncodedFrame&)>;
    void setFrameCallback(FrameCallback callback);

    // Get current bitrate
    uint32_t getCurrentBitrate() const;

    // Force IDR/keyframe on next frame
    void forceKeyFrame();

private:
    // AMF library handling
    bool loadAMFLibrary();
    void unloadAMFLibrary();
    bool createAMFContext(const Microsoft::WRL::ComPtr<ID3D11Device>& device);
    bool createEncoder();
    bool configureEncoder(const EncoderSettings& settings);

    // Process encoded data from AMF
    void processEncodedData(AMFSurface* surface);

    // AMF components and state
    void* amfModule_;
    AMFContext amfContext_;
    AMFComponent amfEncoder_;
    EncoderSettings currentSettings_;

    // Thread safety
    std::mutex encoderMutex_;
    std::atomic<bool> forceIDR_;

    // Frame callback
    FrameCallback frameCallback_;

    // Metrics
    std::atomic<uint32_t> currentBitrate_;
};

} // namespace TabDisplay