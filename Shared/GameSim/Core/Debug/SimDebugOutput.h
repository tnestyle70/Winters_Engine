#pragma once

// Shared/GameSim 소유 sim 진단 출력 게이트.
// Engine_Defines.h(<dinput.h>, using namespace 오염)를 Shared TU로 끌어오지 않고
// WintersOutputAIDebugStringA 트레이스를 유지하기 위한 최소 정의다.
// Engine_Defines.h의 동일 블록과 같은 가드(WINTERS_DEBUG_STRING_GATE_DEFINED)를
// 공유하므로 두 헤더가 한 TU에서 만나도 먼저 온 쪽 정의가 사용된다.
// 게이트 의미: NON_AI(기본 0) = 일반 트레이스 차단, AI(기본 1) = sim/AI 진단 허용.

#ifndef WINTERS_ENABLE_NON_AI_DEBUG_STRING
#define WINTERS_ENABLE_NON_AI_DEBUG_STRING 0
#endif

#ifndef WINTERS_ENABLE_AI_DEBUG_STRING
#define WINTERS_ENABLE_AI_DEBUG_STRING 1
#endif

#ifndef WINTERS_DEBUG_STRING_GATE_DEFINED
#define WINTERS_DEBUG_STRING_GATE_DEFINED

#if defined(_WIN32)
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char* pText);
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringW(const wchar_t* pText);
#endif

inline void WintersOutputDebugStringA(const char* pText)
{
#if WINTERS_ENABLE_NON_AI_DEBUG_STRING && defined(_WIN32)
    ::OutputDebugStringA(pText);
#else
    (void)pText;
#endif
}

inline void WintersOutputDebugStringW(const wchar_t* pText)
{
#if WINTERS_ENABLE_NON_AI_DEBUG_STRING && defined(_WIN32)
    ::OutputDebugStringW(pText);
#else
    (void)pText;
#endif
}

inline void WintersOutputAIDebugStringA(const char* pText)
{
#if WINTERS_ENABLE_AI_DEBUG_STRING && defined(_WIN32)
    ::OutputDebugStringA(pText);
#else
    (void)pText;
#endif
}

inline void WintersOutputAIDebugStringW(const wchar_t* pText)
{
#if WINTERS_ENABLE_AI_DEBUG_STRING && defined(_WIN32)
    ::OutputDebugStringW(pText);
#else
    (void)pText;
#endif
}

#endif // WINTERS_DEBUG_STRING_GATE_DEFINED
