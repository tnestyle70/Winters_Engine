#pragma once
#include "WintersTypes.h"

enum class eEngineRHIBackend : u32_t
{
    Auto = 0,
    DX12,
    DX11,
    Null,
    Vulkan,
    Metal,
    Xbox,
    PS5,
};

// ─────────────────────────────────────────────────────────────────
//  EngineConfig.h  |  엔진 초기화 설정
//
//  Client의 main.cpp 에서 설정을 채운 뒤
//  WintersRun()에 넘긴다.
//
//  향후 확장 예정:
//    - RHI 백엔드 선택 (DX11 / DX12 / Vulkan)
//    - MSAA 설정
//    - HDR 출력 여부
// ─────────────────────────────────────────────────────────────────

struct EngineConfig
{
    // ── 윈도우 ───────────────────────────────────────────────
    WStr     windowTitle  = L"Winters Engine";
    uint32   windowWidth  = 1280;
    uint32   windowHeight = 720;
    bool     fullscreen   = false;

    // 씬 개수 - 왜 16?
    uint32_t iNumScenes = 16;
    eEngineRHIBackend rhiBackend = eEngineRHIBackend::Auto;
    bool_t allowRHIFallback = true;

    // ── 렌더링 ───────────────────────────────────────────────
    bool     vsync        = true;    // Present VSync 사용 여부
    uint32   targetFPS    = 60;      // CPU frame limiter. 0이면 제한 없음

    // ── 프로파일링 하네스 ────────────────────────────────────
    uint32   runSeconds   = 0;       // 0이면 무제한. N초 경과 시 자동 종료
    bool     profileCaptureOnExit = false; // 자동 종료 직전 프로파일러 타임라인 JSON 캡처
};
