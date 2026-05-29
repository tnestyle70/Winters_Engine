#pragma once
#include "ECS/ISystem.h"
#include "WintersAPI.h"   // WINTERS_ENGINE (DLL export 매크로) — Client 에서 Initialize/RegisterSystem/Execute 직접 호출 링크

class CJobSystem;
class CWorld;

// Phase 5-A: CSystemSchedular 는 Scene 이 직접 소유 + 멤버 함수 직접 호출.
// 호출부(Client Scene_InGame) 가 구체 타입으로 쓰므로 WINTERS_ENGINE 마크 필요.
// (인터페이스 추상화 ISystemScheduler 는 Phase 5-B 또는 별도 리팩터에서)
class WINTERS_ENGINE CSystemSchedular
{
public:
    CSystemSchedular() = default;
    ~CSystemSchedular() = default;

    // ★ 중요: WINTERS_ENGINE dllexport 클래스가 unique_ptr 멤버를 포함할 때 필수.
    // MSVC 는 dllexport 클래스의 모든 특수 멤버 함수를 강제 인스턴스화/export 하려 함.
    // unique_ptr 의 copy 는 deleted → 암묵적 copy ctor 생성 실패 → construct_at 에러.
    // 명시적으로 copy = delete 선언하면 MSVC 가 copy 경로 인스턴스화를 스킵.
    // (CWorld.h:59~62 이 동일 패턴 사용)
    CSystemSchedular(const CSystemSchedular&) = delete;
    CSystemSchedular& operator=(const CSystemSchedular&) = delete;
    CSystemSchedular(CSystemSchedular&&) = default;
    CSystemSchedular& operator=(CSystemSchedular&&) = default;

    void Initialize(CJobSystem* pJobSystem);
    void RegisterSystem(unique_ptr<ISystem> system);
    void Execute(CWorld& world, float fTimeDelta);
private:
    // Phase = 시스템 실행 순서 그룹.
    // 같은 Phase 는 JobSystem 으로 병렬 실행 가능.
    CJobSystem* m_pJobSystem{ nullptr };
    map<uint32_t, vector<unique_ptr<ISystem>>> m_mapPhases;
};