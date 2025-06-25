#pragma once

// Minimal stub of AMD Advanced Media Framework (AMF) headers so the project
// can compile when the real SDK is not available.  Hardware encoding will be
// disabled at runtime; EncoderAMF.cpp dynamically loads amfrt64.dll, so if the
// DLL is present the real symbols will be used instead.

#include <cstdint>

#define AMF_OK 0
#define AMF_FAIL 1
#define AMF_INPUT_FULL 2
#define AMF_RESULT int
#define AMF_CDECL_CALL

typedef uint64_t amf_uint64;

// Simple numeric ratio type used by some AMF properties (global version)
typedef struct { int num; int den; } amf_ratio_t;

// Forward declaration of generic data interface
namespace amf { class AMFData { public: virtual void Release() {} }; }

namespace amf {
class AMFDebug {};
class AMFTrace {};
class AMFFactory {
public:
    int CreateContext(void**) { return AMF_FAIL; }
    int CreateComponent(void*, const wchar_t*, void**) { return AMF_FAIL; }
    void GetTrace(AMFTrace**) {}
    void GetDebug(AMFDebug**) {}
};
class AMFContext {
public:
    int InitDX11(void*) { return AMF_FAIL; }
    int Terminate() { return 0; }
    int CreateSurfaceFromDX11Native(void*, void**, void*) { return AMF_FAIL; }
};
class AMFComponent : public AMFData {
public:
    int Terminate() { return 0; }
    // Removed the plain 'int' overload to avoid ambiguous conversions with other integral types
    //int SetProperty(const wchar_t*, int) { return 0; }
    int SetProperty(const wchar_t*, amf_uint64) { return 0; }
    int SetProperty(const wchar_t*, ::amf_ratio_t) { return 0; }
    int SubmitInput(void*) { return AMF_FAIL; }
    int QueryOutput(AMFData**) { return AMF_FAIL; }
    int Init(int, int, int) { return AMF_FAIL; }
};
class AMFBuffer : public AMFData { public: void* GetNative() { return nullptr; } size_t GetSize() { return 0; } void Release() {} };
class AMFSurface : public AMFBuffer {
public:
    // Removed the plain 'int' overload to avoid ambiguous conversions with other integral types
    //int SetProperty(const wchar_t*, int) { return 0; }
    int GetProperty(const wchar_t*, bool*) { return 0; }
    int GetProperty(const wchar_t*, uint64_t*) { return 0; }
    void Release() {}
    int SetProperty(const wchar_t*, amf_uint64) { return 0; }
};
}

// Common string constants used in EncoderAMF – keep them as wchar_t* so the
// real values can be resolved at runtime if the actual AMF headers are later
// included earlier in the include path.

#define AMF_VIDEO_ENCODER_USAGE                L"Usage"
#define AMF_VIDEO_ENCODER_USAGE_LOW_LATENCY    1
#define AMF_VIDEO_ENCODER_QUALITY_PRESET       L"QualityPreset"
#define AMF_VIDEO_ENCODER_QUALITY_PRESET_SPEED 1
#define AMF_VIDEO_ENCODER_WIDTH                L"EncoderWidth"
#define AMF_VIDEO_ENCODER_HEIGHT               L"EncoderHeight"
#define AMF_VIDEO_ENCODER_FRAMERATE            L"FrameRate"
#define AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD  L"RateControlMethod"
#define AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CBR 1
#define AMF_VIDEO_ENCODER_TARGET_BITRATE       L"TargetBitrate"
#define AMF_VIDEO_ENCODER_PEAK_BITRATE         L"PeakBitrate"
#define AMF_VIDEO_ENCODER_B_PIC_PATTERN        L"BPicPattern"
#define AMF_VIDEO_ENCODER_B_REFERENCE_ENABLE   L"BRefEnable"
#define AMF_VIDEO_ENCODER_LOW_LATENCY_MODE     L"LowLatencyMode"
#define AMF_VIDEO_ENCODER_RATE_CONTROL_SKIP_FRAME_ENABLE L"SkipFrameEnable"
#define AMF_VIDEO_ENCODER_INTRA_REFRESH_NUM_MBS_PER_SLOT L"IntraRefreshMbsPerSlot"
#define AMF_VIDEO_ENCODER_IDR_PERIOD           L"IDRPeriod"
#define AMF_VIDEO_ENCODER_FORCE_PICTURE_TYPE   L"ForcePicType"
#define AMF_VIDEO_ENCODER_PICTURE_TYPE_IDR     2
#define AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE     L"OutDataType"
#define AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_IDR 2

#define AMF_SURFACE_BGRA 0
#define AMF_SURFACE_FORMAT int

#define AMF_PTS L"Pts"

#define AMFVideoEncoderVCE_AVC L"AMFVideoEncoderVCE_AVC"

// Provide a stub for min/max macros interference if Windows headers got included before this stub.
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif 