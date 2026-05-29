#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"

// ─────────────────────────────────────────────────────────────────
//  IWintersApp  |  게임 로직 인터페이스
//
//  Client가 이 인터페이스를 상속받아 게임 로직을 구현한다.
//  엔진은 이 인터페이스만 알고, 게임 구현을 모른다.
//
//  현재 구조 (DX9 엔진 대응):
//    OnInit()     ← CMainApp::Ready_MainApp()
//    OnUpdate()   ← CMainApp::Update_MainApp()
//    OnRender()   ← CMainApp::Render_MainApp()
//    OnShutdown() ← 소멸자/정리 코드
//
//  향후:
//    OnUpdate() 내부가 Job System으로 교체될 것.
//    "내가 직접 Update한다" → "System들이 dependency graph로 실행된다"
// ─────────────────────────────────────────────────────────────────

//이것도 export로 client 쪽에 공개를 하는 방향이 맞는 건가?
class WINTERS_ENGINE IWintersApp
{
public:
    virtual ~IWintersApp() = default;

    // 엔진 초기화 완료 후 게임 초기화
    // false 반환 시 엔진이 즉시 종료
    virtual bool    OnInit()                      = 0;

    // 매 프레임 게임 로직 실행
    // deltaTime: 이전 프레임과의 시간 간격 (초 단위)
    // SceneManager가 처리하므로 기본 빈 구현 (override 선택)
    virtual void    OnUpdate(float32 deltaTime)    {}
    virtual void    OnRender()                     {}
    virtual void    OnImGui()                      {}

    // 게임 종료 전 리소스 정리
    virtual void    OnShutdown()                   = 0;

};
