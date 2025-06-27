#pragma once

#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>

// Forward declarations for OpenH264
typedef struct TagEncParamExt SEncParamExt;
typedef struct ISVCEncoderVtbl ISVCEncoder;

namespace TabDisplay {

class EncoderSoftware {
public:
    struct EncodedFrame {
        std::vector<uint8_t> data;
        bool isKeyFrame;
        uint64_t timestamp;
    };

    struct EncoderSettings {
        uint32_t width;
        uint32_t height;
        uint32_t frameRate;
        uint32_t bitrate;  // in Kbps
        bool lowLatency;
    };

    EncoderSoftware();
    ~EncoderSoftware();

    // Initialize software encoder
    bool initialize(const EncoderSettings& settings);

    // Encode raw RGB frame data
    bool encodeFrame(const uint8_t* rgbData, uint32_t width, uint32_t height, uint32_t stride);

    // Set frame callback for encoded frames
    using FrameCallback = std::function<void(const EncodedFrame&)>;
    void setFrameCallback(FrameCallback callback);

    // Force IDR/keyframe on next frame
    void forceKeyFrame();

    // Get current settings
    const EncoderSettings& getSettings() const { return settings_; }

private:
    // Convert RGB to YUV420
    void convertRGBToYUV420(const uint8_t* rgbData, uint32_t width, uint32_t height, uint32_t stride,
                           uint8_t* yuvData);

    // OpenH264 encoder instance
    ISVCEncoder* encoder_;
    SEncParamExt* encoderParams_;
    
    // Settings and state
    EncoderSettings settings_;
    FrameCallback frameCallback_;
    std::atomic<bool> forceKeyFrame_;
    std::atomic<uint32_t> frameNumber_;
    
    // Thread safety
    std::mutex encoderMutex_;
    
    // YUV conversion buffer
    std::vector<uint8_t> yuvBuffer_;
    
    bool initialized_;
};

} // namespace TabDisplay