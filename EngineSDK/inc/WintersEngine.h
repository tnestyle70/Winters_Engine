#pragma once

// ─────────────────────────────────────────────────────────────────
//  WintersEngine.h  |  Client 통합 인클루드
//
//  Client 코드에서는 이 헤더 하나만 include 하면 된다.
//  #include "WintersEngine.h"
//
//  엔진 내부 구현(Header/)은 Client에 노출되지 않는다.
//  DX11 타입, 윈도우 핸들 등이 Client 코드로 오염되지 않음.
// ─────────────────────────────────────────────────────────────────

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "EngineConfig.h"
#include "IWintersApp.h"
#include "WintersPaths.h"

class IRHIDevice;

// ─────────────────────────────────────────────────────────────────
//  WintersRun()
//
//  엔진의 유일한 진입점.
//  내부 동작:
//    1. Win32 윈도우 생성
//    2. DX11 디바이스 초기화
//    3. app->OnInit() 호출
//    4. 게임 루프 실행 (Update → Render)
//    5. app->OnShutdown() 호출
//    6. 정리 후 종료 코드 반환
//
//  반환값: 0 = 정상 종료, 음수 = 오류
// ─────────────────────────────────────────────────────────────────
WINTERS_ENGINE int32 WintersRun(IWintersApp* app, const EngineConfig& config);
WINTERS_ENGINE IRHIDevice* WintersGetRHIDevice();
