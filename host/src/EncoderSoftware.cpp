#include "EncoderSoftware.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <cstring>

// OpenH264 includes
#include <wels/codec_api.h>

namespace TabDisplay {

EncoderSoftware::EncoderSoftware()
    : encoder_(nullptr)
    , encoderParams_(nullptr)
    , forceKeyFrame_(false)
    , frameNumber_(0)
    , initialized_(false)
{
}

EncoderSoftware::~EncoderSoftware() {
    if (encoder_) {
        encoder_->Uninitialize();
        WelsDestroySVCEncoder(encoder_);
        encoder_ = nullptr;
    }
    
    if (encoderParams_) {
        delete encoderParams_;
        encoderParams_ = nullptr;
    }
}

bool EncoderSoftware::initialize(const EncoderSettings& settings) {
    std::lock_guard<std::mutex> lock(encoderMutex_);
    
    if (initialized_) {
        spdlog::warn("Software encoder already initialized");
        return true;
    }
    
    settings_ = settings;
    
    // Create OpenH264 encoder
    int result = WelsCreateSVCEncoder(&encoder_);
    if (result != 0 || !encoder_) {
        spdlog::error("Failed to create OpenH264 encoder, error: {}", result);
        return false;
    }
    
    // Allocate encoder parameters
    encoderParams_ = new SEncParamExt;
    if (!encoderParams_) {
        spdlog::error("Failed to allocate encoder parameters");
        return false;
    }
    
    // Initialize parameters with defaults
    encoder_->GetDefaultParams(encoderParams_);
    
    // Configure encoder parameters
    encoderParams_->iUsageType = CAMERA_VIDEO_REAL_TIME;
    encoderParams_->fMaxFrameRate = static_cast<float>(settings.frameRate);
    encoderParams_->iPicWidth = settings.width;
    encoderParams_->iPicHeight = settings.height;
    encoderParams_->iTargetBitrate = settings.bitrate * 1000; // Convert Kbps to bps
    encoderParams_->iMaxBitrate = settings.bitrate * 1200; // 20% overhead
    encoderParams_->iRCMode = RC_BITRATE_MODE;
    encoderParams_->iTemporalLayerNum = 1;
    encoderParams_->iSpatialLayerNum = 1;
    encoderParams_->bEnableDenoise = false;
    encoderParams_->bEnableBackgroundDetection = false;
    encoderParams_->bEnableAdaptiveQuant = true;
    encoderParams_->bEnableFrameSkip = false;
    encoderParams_->bEnableLongTermReference = false;
    encoderParams_->iLookaheadDepth = 0; // Disable lookahead for low latency
    
    // Configure spatial layer
    encoderParams_->sSpatialLayers[0].iVideoWidth = settings.width;
    encoderParams_->sSpatialLayers[0].iVideoHeight = settings.height;
    encoderParams_->sSpatialLayers[0].fFrameRate = static_cast<float>(settings.frameRate);
    encoderParams_->sSpatialLayers[0].iSpatialBitrate = settings.bitrate * 1000;
    encoderParams_->sSpatialLayers[0].iMaxSpatialBitrate = settings.bitrate * 1200;
    encoderParams_->sSpatialLayers[0].uiProfileIdc = PRO_BASELINE;
    
    if (settings.lowLatency) {
        // Low latency settings
        encoderParams_->sSpatialLayers[0].uiLevelIdc = LEVEL_3_1;
        encoderParams_->iComplexityMode = LOW_COMPLEXITY;
        encoderParams_->bEnableFrameSkip = false;
        encoderParams_->iMultipleThreadIdc = 1; // Single thread for consistency
    } else {
        encoderParams_->sSpatialLayers[0].uiLevelIdc = LEVEL_4_0;
        encoderParams_->iComplexityMode = MEDIUM_COMPLEXITY;
        encoderParams_->iMultipleThreadIdc = 0; // Auto thread count
    }
    
    // Initialize encoder
    result = encoder_->InitializeExt(encoderParams_);
    if (result != cmResultSuccess) {
        spdlog::error("Failed to initialize OpenH264 encoder, error: {}", result);
        return false;
    }
    
    // Set encoding options
    int videoFormat = videoFormatI420;
    encoder_->SetOption(ENCODER_OPTION_DATAFORMAT, &videoFormat);
    
    // Allocate YUV conversion buffer
    size_t yuvSize = settings.width * settings.height * 3 / 2; // YUV420 size
    yuvBuffer_.resize(yuvSize);
    
    initialized_ = true;
    spdlog::info("Software H.264 encoder initialized: {}x{} @ {}fps, {} Kbps", 
                settings.width, settings.height, settings.frameRate, settings.bitrate);
    
    return true;
}

void EncoderSoftware::convertRGBToYUV420(const uint8_t* rgbData, uint32_t width, uint32_t height, uint32_t stride,
                                        uint8_t* yuvData) {
    // Simple RGB to YUV420 conversion
    // Note: This assumes RGBA input (4 bytes per pixel)
    
    uint8_t* yPlane = yuvData;
    uint8_t* uPlane = yuvData + (width * height);
    uint8_t* vPlane = yuvData + (width * height) + (width * height / 4);
    
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            const uint8_t* pixel = rgbData + (y * stride) + (x * 4);
            uint8_t r = pixel[0];
            uint8_t g = pixel[1];
            uint8_t b = pixel[2];
            
            // Convert to YUV
            int Y = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
            int U = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
            int V = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
            
            // Clamp values
            Y = std::max(0, std::min(255, Y));
            U = std::max(0, std::min(255, U));
            V = std::max(0, std::min(255, V));
            
            yPlane[y * width + x] = static_cast<uint8_t>(Y);
            
            // Subsample UV (4:2:0)
            if ((y % 2 == 0) && (x % 2 == 0)) {
                uint32_t uvIndex = (y / 2) * (width / 2) + (x / 2);
                uPlane[uvIndex] = static_cast<uint8_t>(U);
                vPlane[uvIndex] = static_cast<uint8_t>(V);
            }
        }
    }
}

bool EncoderSoftware::encodeFrame(const uint8_t* rgbData, uint32_t width, uint32_t height, uint32_t stride) {
    std::lock_guard<std::mutex> lock(encoderMutex_);
    
    if (!initialized_ || !encoder_) {
        spdlog::error("Encoder not initialized");
        return false;
    }
    
    if (width != settings_.width || height != settings_.height) {
        spdlog::error("Frame dimensions {}x{} don't match encoder settings {}x{}", 
                     width, height, settings_.width, settings_.height);
        return false;
    }
    
    // Convert RGB to YUV420
    convertRGBToYUV420(rgbData, width, height, stride, yuvBuffer_.data());
    
    // Prepare source picture
    SSourcePicture sourcePicture;
    memset(&sourcePicture, 0, sizeof(SSourcePicture));
    
    sourcePicture.iPicWidth = width;
    sourcePicture.iPicHeight = height;
    sourcePicture.iColorFormat = videoFormatI420;
    sourcePicture.iStride[0] = width;
    sourcePicture.iStride[1] = width / 2;
    sourcePicture.iStride[2] = width / 2;
    sourcePicture.pData[0] = yuvBuffer_.data();
    sourcePicture.pData[1] = yuvBuffer_.data() + (width * height);
    sourcePicture.pData[2] = yuvBuffer_.data() + (width * height) + (width * height / 4);
    
    // Set timestamp
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    sourcePicture.uiTimeStamp = now;
    
    // Force keyframe if requested
    if (forceKeyFrame_.exchange(false)) {
        encoder_->ForceIntraFrame(true);
    }
    
    // Encode frame
    SFrameBSInfo frameInfo;
    memset(&frameInfo, 0, sizeof(SFrameBSInfo));
    
    int result = encoder_->EncodeFrame(&sourcePicture, &frameInfo);
    if (result != cmResultSuccess) {
        spdlog::error("Failed to encode frame, error: {}", result);
        return false;
    }
    
    frameNumber_++;
    
    // Check if we have encoded data
    if (frameInfo.eFrameType != videoFrameTypeSkip && frameInfo.iLayerNum > 0) {
        // Collect all NAL units into a single buffer
        std::vector<uint8_t> encodedData;
        bool isKeyFrame = (frameInfo.eFrameType == videoFrameTypeIDR);
        
        for (int layer = 0; layer < frameInfo.iLayerNum; layer++) {
            const SLayerBSInfo& layerInfo = frameInfo.sLayerInfo[layer];
            
            for (int nal = 0; nal < layerInfo.iNalCount; nal++) {
                const uint8_t* nalData = layerInfo.pBsBuf + layerInfo.pNalLengthInByte[nal];
                int nalSize = layerInfo.pNalLengthInByte[nal];
                
                if (nal + 1 < layerInfo.iNalCount) {
                    nalSize = layerInfo.pNalLengthInByte[nal + 1] - layerInfo.pNalLengthInByte[nal];
                } else {
                    nalSize = layerInfo.iNalLengthInByte[nal];
                }
                
                encodedData.insert(encodedData.end(), nalData, nalData + nalSize);
            }
        }
        
        // Call frame callback if we have data
        if (!encodedData.empty() && frameCallback_) {
            EncodedFrame frame;
            frame.data = std::move(encodedData);
            frame.isKeyFrame = isKeyFrame;
            frame.timestamp = now;
            
            frameCallback_(frame);
        }
    }
    
    return true;
}

void EncoderSoftware::setFrameCallback(FrameCallback callback) {
    frameCallback_ = callback;
}

void EncoderSoftware::forceKeyFrame() {
    forceKeyFrame_ = true;
}

} // namespace TabDisplay