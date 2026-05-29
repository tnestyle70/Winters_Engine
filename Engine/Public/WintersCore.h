#pragma once

// Winters Engine Core Header
// ── 모든 엔진 코드가 필요로 하는 엔진 전역 타입 ──
// ── PCH에서 STL/Windows가 이미 포함되어 있음을 전제 ──

#include "WintersAPI.h"  // WINTERS_API dllexport/import
#include "WintersTypes.h"  // int32, float32, uint32, WString...
#include "WintersMath.h"  // Vec2, Vec3, Vec4, Mat4

//HRESULT 체크(RHI 코드용)
#define WINTERS_HR_CHECK(hr, msg)           \
    do {                                     \
        if (FAILED(hr)) {                    \
            OutputDebugStringA(msg);         \
            __debugbreak();                  \
            return false;                    \
        }                                    \
    } while(0)

//안전한 Com 객체의 Release(ComPtr 레거시 호환용으로 권장)
#define WINTERS_SAFE_RELEASE(p) \
 do {if (p) {(p)->Release(); (p) = nullptr; }} while(0)

// 디버그 로그
#if defined(_DEBUG)
#define WINTERS_LOG(fmt, ...) \
    do { char buf[512]; sprintf_s(buf, "[WINTERS] " fmt "\n", ##__VA_ARGS__); OutputDebugStringA(buf); } while(0)
#else
#define WINTERS_LOG(fmt, ...) ((void)0)
#endif