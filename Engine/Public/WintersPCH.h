#pragma once
// Winters Engine Precompiled Header
// ── 절대 변경되지 않는 외부 헤더만 여기에 ──
// ── 엔진 내부 헤더(.h)는 절대 넣지 않는다 ──

//C++ Standard Library
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <cassert>

//Containers
#include <string>
#include <vector>
#include <array>
#include <map>
#include <unordered_map>
#include <queue>
#include <stack>

//Utilities
#include <memory>
#include <functional>
#include <algorithm>
#include <utility>
#include <type_traits>
#include <typeindex>
#include <optional>
#include <variant>
#include <span>

//Threading
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

//I/O
#include <fstream>
#include <sstream>
#include <filesystem>

//Windows 
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#ifndef WINTERS_ENABLE_NON_AI_DEBUG_STRING
#define WINTERS_ENABLE_NON_AI_DEBUG_STRING 0
#endif

#ifndef WINTERS_ENABLE_AI_DEBUG_STRING
#define WINTERS_ENABLE_AI_DEBUG_STRING 1
#endif

#ifndef WINTERS_DEBUG_STRING_GATE_DEFINED
#define WINTERS_DEBUG_STRING_GATE_DEFINED
inline void WintersOutputDebugStringA(const char* pText)
{
#if WINTERS_ENABLE_NON_AI_DEBUG_STRING
    ::OutputDebugStringA(pText);
#else
    (void)pText;
#endif
}

inline void WintersOutputDebugStringW(const wchar_t* pText)
{
#if WINTERS_ENABLE_NON_AI_DEBUG_STRING
    ::OutputDebugStringW(pText);
#else
    (void)pText;
#endif
}

inline void WintersOutputAIDebugStringA(const char* pText)
{
#if WINTERS_ENABLE_AI_DEBUG_STRING
    ::OutputDebugStringA(pText);
#else
    (void)pText;
#endif
}

inline void WintersOutputAIDebugStringW(const wchar_t* pText)
{
#if WINTERS_ENABLE_AI_DEBUG_STRING
    ::OutputDebugStringW(pText);
#else
    (void)pText;
#endif
}
#endif

#ifndef WINTERS_KEEP_RAW_OUTPUT_DEBUG_STRING
#ifndef OutputDebugStringA
#define OutputDebugStringA WintersOutputDebugStringA
#endif
#ifndef OutputDebugStringW
#define OutputDebugStringW WintersOutputDebugStringW
#endif
#endif

//DirectXMath(SIMD)
#include <DirectXMath.h>
#include <DirectXCollision.h>

//COM 객체 Smart Pointer
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

// ============================================================
// STL using 선언 — 엔진 전역에서 std:: 생략 가능
// PCH에서 선언하므로 모든 .h / .cpp에 자동 적용
// ============================================================
using std::vector;
using std::array;
using std::map;
using std::unordered_map;
using std::queue;
using std::stack;
using std::string;
using std::wstring;
using std::unique_ptr;
using std::shared_ptr;
using std::type_index;
using std::max;
using std::move;
using std::make_unique;
using std::make_shared;
using std::function;
using std::optional;
using std::span;
using std::thread;
using std::mutex;
using std::atomic;
using std::condition_variable;
using std::lock_guard;
using std::unique_lock;
using std::memory_order_relaxed;
using std::memory_order_acquire;
using std::memory_order_release;
using std::memory_order_acq_rel;
