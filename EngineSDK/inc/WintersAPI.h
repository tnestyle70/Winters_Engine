#pragma once

// ─────────────────────────────────────────────────────────────────
//  WintersAPI.h  |  DLL Export/Import 매크로
//
//  Engine 프로젝트: WINTERS_ENGINE_EXPORTS 가 정의됨 → dllexport
//  Client 프로젝트: 미정의                         → dllimport
// ─────────────────────────────────────────────────────────────────

// WINTERS_STATIC_BUILD: Tools/컨버터 등 DLL 아닌 EXE 에서 정의 — export/import 제거
#ifdef WINTERS_STATIC_BUILD
    #define WINTERS_ENGINE
#elif defined(WINTERS_ENGINE_EXPORTS)
    #define WINTERS_ENGINE __declspec(dllexport)
#else
    #define WINTERS_ENGINE __declspec(dllimport)
#endif
