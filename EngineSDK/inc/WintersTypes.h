#pragma once
#include <cstdint>
#include <string>

// ─────────────────────────────────────────────────────────────────
//  WintersTypes.h  |  엔진 공통 타입 별칭
//
//  목표:
//    - 플랫폼 의존적 타입 크기를 고정 (int32는 항상 32비트)
//    - 코드 가독성 향상 (_float 대신 float32로 의도 명확화)
//    - 향후 다른 플랫폼 포팅 시 이 파일만 수정
// ─────────────────────────────────────────────────────────────────

// ── 정수 타입 ──────────────────────────────────────────────────
using int8   = int8_t;
using int16  = int16_t;
using int32  = int32_t;
using int64  = int64_t;
using uint8  = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

// ── 실수 타입 ──────────────────────────────────────────────────
using float32 = float;
using float64 = double;

// ── 선생님 컨벤션 별칭 (_t suffix) ─────────────────────────────
using f32_t = float;
using f64_t = double;
using bool_t = bool;
using i8_t = int8_t;
using i16_t = int16_t;
using i32_t = int32_t;
using i64_t = int64_t;
using u8_t = uint8_t;
using u16_t = uint16_t;
using u32_t = uint32_t;
using u64_t = uint64_t;

// ── 문자열 타입 ────────────────────────────────────────────────
using WString = std::wstring;
using WChar   = wchar_t;
using WStr    = const wchar_t*;
using String  = std::string;
using CStr    = const char*;
using wstring_t = std::wstring;
using tchar_t = wchar_t;
