# 이렐리아 Phase D — 전수조사 결과 + 조치 계획 (2026-04-25)

> **원본 설계서**: `.md/plan/Champion/02_IRELIA_PHASE_D_EFFECTS_PLAN.md` (변경 없음, 참조용)
> **이 문서**: 3-PASS 전수조사 결과 기반 구체 조치 플랜
> **빌드 상태**: Engine.dll ✅ / Client.exe ❌ (링크 + 컴파일 + 파일명 오류 총 13건)

---

## 0. 현 상태 스코어카드

| 영역 | 항목 수 | 완료 | 부분 | 미구현 | 스코어 |
|------|---------|------|------|--------|--------|
| **Engine 시스템** | 5 | 5 | 0 | 0 | ✅ 100% |
| **Client 신규파일** | 7 | 4 | 2 | 1 | ⚠️ 57% (FxSystem.cpp 스텁, IreliaBlade 버그, UltWave 파일명) |
| **Scene_InGame.h 멤버** | 17 | 17 | 0 | 0 | ✅ 100% |
| **Scene_InGame.cpp 통합** | 7 | 0 | 0 | 7 | ❌ 0% ⭐ 최대 차단 |
| **SkillTable E 2-stage** | 1 | 0 | 0 | 1 | ❌ 0% |
| **ChampionTuner 슬라이더** | 1 | 0 | 0 | 1 | ❌ 0% |
| **vcxproj 등록** | 7+ | 5 | 1 | 1 | ⚠️ |

**종합**: Engine 완성 · Client 절반 · Scene_InGame 통합 전무

---

## 1. 🔴 BLOCKING (우선순위 1 — 컴파일/링크 실패 원인)

### 1.1 UltWaveSystem 파일명 오타 (`UIt` → `Ult`)

**현재 존재하는 오타 파일**:
- `Client/Public/GameObject/FX/UItWaveSystem.h` (소문자 L 이 아닌 대문자 I)
- `Client/Private/GameObject/FX/UItWaveSystem.cpp`

**조치**: VS 솔루션 탐색기에서 각 파일 **우클릭 → 이름 바꾸기**:
- `UItWaveSystem.h` → `UltWaveSystem.h`
- `UItWaveSystem.cpp` → `UltWaveSystem.cpp`

파일 시스템 이름 변경 + vcxproj/filters 자동 갱신. 내부 `#include "GameObject/FX/UItWaveSystem.h"` 도 전수 검색해서 `Ult` 로 치환. 헤더 `#pragma once` 아래 클래스명은 이미 `CUltWaveSystem` 이므로 내용은 변경 불필요.

### 1.2 FxSystem.cpp 중복 (Public 에 있어야 할 게 아님)

**현재 존재**:
- ✅ `Client/Private/GameObject/FX/FxSystem.cpp` (정상 위치)
- ❌ `Client/Public/GameObject/FX/FxSystem.cpp` (잘못된 위치, 삭제 대상)

**조치**: VS 솔루션 탐색기에서 `Public/GameObject/FX/FxSystem.cpp` 우클릭 → **"프로젝트에서 제외"** + 파일시스템에서도 삭제.

vcxproj 에 `<ClInclude Include="..\Public\GameObject\FX\FxSystem.cpp" />` 같은 잘못된 엔트리 있으면 함께 제거 (ClInclude 는 헤더만).

### 1.3 FxSystem.cpp 스텁 → 본체 구현 필요

**현재 상태**: `Private/GameObject/FX/FxSystem.cpp` 의 Create/Update/Render 가 전부 빈 바디 또는 null 반환.

**조치**: `.md/plan/Champion/02_IRELIA_PHASE_D_EFFECTS_PLAN.md` §3.5 전체 코드 그대로 붙여넣기. 특히 아래 4 메서드 전체:

```cpp
std::unique_ptr<CFxSystem> CFxSystem::Create(
    CDX11Device* pDevice, DX11Shader* pShader, DX11Pipeline* pPipeline,
    CBlendStateCache* pBlendCache)
{
    if (!pDevice || !pShader || !pPipeline || !pBlendCache)
        return nullptr;

    auto p = std::unique_ptr<CFxSystem>(new CFxSystem());
    p->m_pDevice     = pDevice;
    p->m_pBlendCache = pBlendCache;
    p->m_pPlane      = CPlaneRenderer::Create(pDevice->GetDevice(), pShader, pPipeline);
    if (!p->m_pPlane) return nullptr;
    p->m_pPlane->SetBlendCache(pBlendCache, eBlendPreset::AlphaBlend);
    return p;
}

EntityID CFxSystem::Spawn(CWorld& world, const FxBillboardComponent& tmpl)
{
    EntityID e = world.CreateEntity();
    world.AddComponent<FxBillboardComponent>(e, tmpl);
    return e;
}

// Update: lifetime 감소 + attachTo 추종 + 지연삭제
// (원본 계획서 §3.5 의 Update 전체)

// Render: bBillboard true → 카메라 right/-fwd/up 회전 / false → XZ 지면 퀘드
// (원본 계획서 §3.5 의 Render 전체)

// GetOrLoadTexture + Shutdown
```

include 누락 체크:
```cpp
#include "GameObject/FX/FxSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "Renderer/PlaneRenderer.h"
#include "Resource/Texture.h"
#include "RHI/CDX11Device.h"
#include "RHI/DX11/BlendStateCache.h"
#include "DynamicCamera.h"
#include "ProfilerAPI.h"
#include <DirectXMath.h>
#include <vector>
```

### 1.4 IreliaBladeSystem.cpp 변수명 혼용 버그

**문제**: 람다 파라미터가 `blade` 로 선언됐는데 본문에서 `b.` 로 참조 → 정의되지 않은 식별자.

**조치**: 다음 중 하나로 통일 (원본 계획서와 일치시키려면 `b` 권장):
```cpp
world.ForEach<IreliaBladeComponent, TransformComponent>(
    std::function<void(EntityID, IreliaBladeComponent&, TransformComponent&)>(
        [&](EntityID e, IreliaBladeComponent& b, TransformComponent& tf)   // ← b 로 통일
        {
            switch (b.state)
            {
            case IreliaBladeComponent::eState::Placed:
                break;
            case IreliaBladeComponent::eState::Returning:
            {
                // b. 사용하는 모든 줄은 그대로
                ...
                break;   // ← Returning 블록 끝 break 확인
            }
            case IreliaBladeComponent::eState::Dead:
                if (b.fxBillboardId != NULL_ENTITY)
                    vecMarkFxDelete.push_back(b.fxBillboardId);
                vecDelete.push_back(e);
                break;   // ← break 누락 여부 재확인
            }
        }));
```

**추가 점검**:
- `if (dist < 0.5f)` 주석처리된 거리 체크 — **원복 권장**. 없으면 이렐리아가 정확히 검 위치에 있을 때 div-by-zero 위험 + 계속 Returning 상태로 유지되어 영원히 소멸 안 됨.
- switch 밖 `default:` 누락 여부 확인 (컴파일 경고만 남)

### 1.5 Engine StatusEffectSystem.cpp — `std::` prefix 누락 (minor)

**현재**:
```cpp
auto pInstance = unique_ptr<CStatusEffectSystem>(new CStatusEffectSystem());
```

**조치** (둘 중 하나):
- `std::unique_ptr<CStatusEffectSystem>(...)` 로 변경, 또는
- 파일 상단에 `using std::unique_ptr;` 추가 (Engine_Defines.h 의 관례 확인 필요)

`Engine_Defines.h` 에 `using namespace std;` 가 전역 적용되어 있다면 그대로 OK (확인 필요).

---

## 2. 🔴 Scene_InGame.cpp 통합 — 7개 섹션 전부 미구현 (가장 큰 차단)

### 2.1 OnEnter — StatusEffectSystem 등록 + FX/Blade/Ult 시스템 생성

**TransformSystem 등록 블록(L61-84) 뒤**에 추가:
```cpp
// [Phase T-8] Status Effect System 등록 (Phase 3)
{
    auto pStatus = CStatusEffectSystem::Create();
    m_pScheduler->RegisterSystem(std::move(pStatus));
}
```

**AttackRange Plane 생성 블록(L277-312) 뒤**에 추가:
```cpp
// [Phase T-8] FX / Blade / UltWave 시스템 생성
{
    CGameInstance* pGI  = CGameInstance::Get();
    CDX11Device*   pDev = pGI->Get_RHIDevice();

    m_pFxSystem = CFxSystem::Create(
        pDev,
        pGI->Get_MeshShader(),
        pGI->Get_MeshPipeline(),
        pGI->Get_BlendStateCache());

    m_pIreliaBladeSystem = CIreliaBladeSystem::Create();
    m_pUltWaveSystem     = CUltWaveSystem::Create();
}
```

**파일 상단 include**:
```cpp
#include "ECS/Systems/StatusEffectSystem.h"
#include "GameObject/FX/FxSystem.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/UltWaveSystem.h"
#include "GameObject/Champion/Irelia/IreliaBladeSystem.h"
#include <cmath>
```
(경로 점검 — 실제 폴더 구조가 `FX/` 와 `Champion/Irelia/` 서브폴더. Scene_InGame.h 의 기존 include 경로와 반드시 일치시킬 것)

### 2.2 OnUpdate — Q 마크 + R 스폰 + 3 시스템 tick

castFrame 블록(L602-631) 직후:
```cpp
// [Phase T-8] Q 마크 + R 칼날 벽 — castFrame 시점 1회 스폰
if (m_pActiveSkillDef && m_pPlayerRenderer)
{
    const Engine::CAnimator* pAnim = m_pPlayerRenderer->GetAnimator();
    if (pAnim)
    {
        const SkillDef& d = *m_pActiveSkillDef;
        if (d.castFrame > 0.f && pAnim->HasFramePassed(d.castFrame, m_fActivePrevFrame))
        {
            // Q 타겟 머리 위 마크
            if (d.champ == eChampion::IRELIA
                && d.slot == static_cast<uint8_t>(eSkillSlot::Q)
                && m_DashTargetEntity != NULL_ENTITY)
            {
                FxBillboardComponent fx{};
                fx.attachTo      = m_DashTargetEntity;
                fx.vAttachOffset = { 0.f, 3.5f, 0.f };
                fx.texturePath   = L"C:/Users/user/Desktop/Winters/Client/Bin/Resource/Texture/FX/Irelia/irelia_base_q_mark_pulse_erode.png";
                fx.fWidth        = 1.5f;
                fx.fHeight       = 1.5f;
                fx.fLifetime     = 3.f;
                fx.bBillboard    = true;
                CFxSystem::Spawn(m_World, fx);
            }

            // R 전방 칼날 벽 스폰
            if (d.champ == eChampion::IRELIA
                && d.slot == static_cast<uint8_t>(eSkillSlot::R)
                && m_pPlayerTransform)
            {
                const Vec3 origin = m_pPlayerTransform->GetPosition();
                const f32_t yaw = m_pPlayerTransform->GetRotation().y;
                const Vec3 fwd { std::sinf(yaw), 0.f, std::cosf(yaw) };

                CUltWaveSystem::Spawn(m_World, origin, fwd, m_PlayerEntity,
                    m_fWaveLength, m_fWaveWidth, m_fWaveSpeed, m_fWaveMaxDist, m_fWaveDamage);
            }
        }
    }
}

// [Phase T-8] 이펙트 / Blade / UltWave Tick
if (m_pFxSystem)          m_pFxSystem->Update(m_World, dt);
if (m_pIreliaBladeSystem) m_pIreliaBladeSystem->Execute(m_World, dt);
if (m_pUltWaveSystem)     m_pUltWaveSystem->Execute(m_World, dt);
```

### 2.3 OnRender — FX 렌더 호출

AttackRange Plane 렌더 블록(L1010-1029) 뒤:
```cpp
// [Phase T-8] FX 빌보드 전체 렌더 (알파 마지막 단계)
if (m_pFxSystem && m_pCamera)
    m_pFxSystem->Render(m_World, m_pCamera.get());
```

### 2.4 UpdateCombatInput — Stun 가드

함수 첫 줄 (`outSkipGroundMove = false;` 다음):
```cpp
// [Phase T-8] Stun 시 입력 전면 차단
if (m_PlayerEntity != NULL_ENTITY
    && m_World.HasComponent<StunComponent>(m_PlayerEntity))
    return;
```

### 2.5 DispatchSkillInput — Disarm 가드 + E stage 분기

**Disarm 가드** (`using namespace Engine;` 바로 전):
```cpp
// [Phase T-8] Disarm → 평타만 차단
if (slot == static_cast<uint8_t>(eSkillSlot::BasicAttack)
    && m_World.HasComponent<DisarmComponent>(m_PlayerEntity))
    return false;
```

**E stage2 true 분기** (기존 stage2 블록 내, `slotState.cooldownRemaining = def->cooldownSec;` 전):
```cpp
if (def->champ == eChampion::IRELIA
    && def->slot == static_cast<uint8_t>(eSkillSlot::E))
{
    CIreliaBladeSystem::TriggerReturn(m_World, m_IreliaActiveBladeId);
    m_IreliaActiveBladeId = NULL_ENTITY;
}
```

**E stage1 분기** (`slotState.stageWindow = def->stageWindowSec;` 전):
```cpp
if (def->champ == eChampion::IRELIA
    && def->slot == static_cast<uint8_t>(eSkillSlot::E)
    && cmd.resolvedTargetMode == static_cast<uint8_t>(eTargetMode::GroundTarget))
{
    m_IreliaActiveBladeId = CIreliaBladeSystem::SpawnPlaced(m_World, cmd.groundPos, m_PlayerEntity);
}
```

---

## 3. 🟠 SkillTable.cpp — Irelia E 2-stage 전환

현재(L54-64):
```cpp
{ eChampion::IRELIA, 3, eTargetMode::GroundTarget,
  0.6f, 9.0f, 80.f,
  "spell3", nullptr, nullptr,
  1.f, true, eRotateMode::TowardsCursor,
  1, eTargetMode::Self, nullptr, 0.f, eRotateMode::None, 0.f,   // stageCount=1
  10.f, 20.f, 0.f, 0.f,
  ...
```

변경(stageCount=1 → 2, stage2 필드 갱신):
```cpp
{ eChampion::IRELIA, 3, eTargetMode::GroundTarget,
  0.6f, 9.0f, 80.f,
  "spell3", nullptr, nullptr,
  1.f, true, eRotateMode::TowardsCursor,
  2, eTargetMode::Self, "spell3", 0.4f, eRotateMode::None, 4.f,  // stageCount=2
  10.f, 20.f, 6.f, 14.f,                                          // stage2CastFrame=6, recovery=14
  1.f, 1.f,
  "spell3_to_idle", "spell3_run", 0.05f },
```

---

## 4. 🟠 ChampionTuner.cpp — "(WIP)" 제거 + 슬라이더 추가

현재 L57:
```cpp
if (ImGui::CollapsingHeader("Skill Parameters (WIP)"))
{
    ImGui::Text("Q (Bladesurge / Dash)");
    ...
    ImGui::TextDisabled("E Bind Travel Speed — 구현 후 노출");
    ImGui::TextDisabled("R Wave Length/Width — 구현 후 노출");
}
```

변경:
```cpp
if (ImGui::CollapsingHeader("Skill Parameters"))
{
    ImGui::Text("Q (Bladesurge / Dash)");
    // 기존 Q 슬라이더 유지

    ImGui::Spacing();
    ImGui::Text("E (Blade / Bind)");
    f32_t eSpeed = pScene->GetBladeTravelSpeed();
    if (ImGui::SliderFloat("E Travel Speed", &eSpeed, 5.f, 40.f, "%.1f"))
        pScene->SetBladeTravelSpeed(eSpeed);
    f32_t eStun = pScene->GetBladeStunSec();
    if (ImGui::SliderFloat("E Stun Duration", &eStun, 0.5f, 3.f, "%.2f"))
        pScene->SetBladeStunSec(eStun);

    ImGui::Spacing();
    ImGui::Text("R (Wave)");
    f32_t rLen = pScene->GetWaveLength();
    f32_t rWid = pScene->GetWaveWidth();
    f32_t rSpd = pScene->GetWaveSpeed();
    f32_t rMax = pScene->GetWaveMaxDist();
    f32_t rDmg = pScene->GetWaveDamage();
    if (ImGui::SliderFloat("R Length",  &rLen, 6.f, 20.f, "%.1f")) pScene->SetWaveLength(rLen);
    if (ImGui::SliderFloat("R Width",   &rWid, 1.f,  8.f, "%.1f")) pScene->SetWaveWidth(rWid);
    if (ImGui::SliderFloat("R Speed",   &rSpd,10.f, 50.f, "%.1f")) pScene->SetWaveSpeed(rSpd);
    if (ImGui::SliderFloat("R MaxDist", &rMax, 6.f, 30.f, "%.1f")) pScene->SetWaveMaxDist(rMax);
    if (ImGui::SliderFloat("R Damage",  &rDmg,50.f,500.f, "%.0f")) pScene->SetWaveDamage(rDmg);
}
```

---

## 5. 🟡 마이너 폴리시 (컴파일 통과 후 정리)

- `FxBillboardComponent.bBillboard` / `bPendingDelete`: `bool` → `bool_t` (컨벤션 일관성)
- `UltWaveComponent.bInWallPhase`: `bool` → `bool_t`
- ISystem 스타일 vs StatusEffectSystem 스타일 (`float` vs `f32_t`): 동등 typedef — 선택적 정리
- `IreliaBladeSystem` 의 `// 아니 왜 가까이 가면 없애??` 주석 — 계획서 복원 권장 (div-by-zero 방지)

---

## 6. 실행 순서 (최적화)

| 순서 | 작업 | 빌드 단계 | 검증 |
|------|------|-----------|------|
| A | UltWave 파일명 2개 변경 + 중복 FxSystem.cpp 삭제 | 솔루션 탐색기 | 파일 시스템 확인 |
| B | FxSystem.cpp 본체 작성 (계획서 §3.5 복붙) | Client 컴파일 | Link 에러 소멸 |
| C | IreliaBladeSystem.cpp 변수명 통일 + break 확인 | Client 컴파일 | 컴파일 에러 소멸 |
| D | Scene_InGame.cpp §2.1 OnEnter 통합 | Client 컴파일 | 시스템 생성 로그 |
| E | Scene_InGame.cpp §2.2 OnUpdate tick + castFrame 분기 | Client 컴파일 | 실행 가능 상태 |
| F | Scene_InGame.cpp §2.3 OnRender FX 렌더 | 인게임 | 빌보드 표시 |
| G | Scene_InGame.cpp §2.4/2.5 Stun/Disarm 가드 + E 분기 | 인게임 | Stun 주입 테스트 |
| H | SkillTable E stageCount=2 | 인게임 | E 1타→2타 동작 |
| I | ChampionTuner 슬라이더 | 인게임 | ImGui 튜닝 |
| J | 마이너 폴리시 (bool_t 등) | 무관 | 코드 리뷰 |

**A+B+C+D 가 빌드 통과 최소 조건**. E 까지 해야 실행 + 테스트 가능.

---

## 7. 완료 판정 체크리스트

### Phase T-8 Step 1 (Engine)
- [x] StatusEffectSystem.h/.cpp
- [x] NavigationSystem Stun/Slow 가드
- [x] GameplayComponents (변경 없음 확인)
- [x] vcxproj + EngineSDK 동기화

### Phase T-8 Step 2 (FxSystem 기반)
- [ ] FxSystem.cpp 본체
- [ ] FxBillboardComponent `bool_t` 정리 (선택)
- [ ] Scene_InGame OnEnter + OnUpdate tick + OnRender 통합

### Phase T-8 Step 3 (Q 마크)
- [ ] castFrame Q 분기
- [ ] 인게임: 칼리스타 Q → 마크 펄스 3초

### Phase T-8 Step 4 (E 검)
- [ ] IreliaBladeSystem.cpp 수정 (변수명/break)
- [ ] SkillTable E stageCount=2
- [ ] DispatchSkillInput E stage1/stage2 분기
- [ ] 인게임: E 1타 지면 → E 2타 회수 → 스턴 1.25s

### Phase T-8 Step 5 (R 벽)
- [ ] UltWaveSystem 파일명 수정 + vcxproj 갱신
- [ ] castFrame R 분기
- [ ] 인게임: R → 벽 → Disarm 1.5s + Slow 2.5s

### Phase T-8 Step 6 (튜너)
- [ ] ChampionTuner 슬라이더
- [ ] 실시간 파라미터 반영

### 후속
- [ ] `CLAUDE.md` 인트로 "직전 완료" 갱신
- [ ] `CLAUDE.md` Gotcha 추가 (ForEach 중 Destroy / 빌보드 / FX 캐시)
- [ ] `memory/project_session_2026_04_25.md`

---

## 8. 가장 큰 리스크 (사전 경고)

1. **Scene_InGame.cpp include 경로**: 실제 폴더는 `FX/` 와 `Champion/Irelia/` — 계획서 §4.3 의 include 를 그대로 복사하면 경로 불일치. 반드시 `#include "GameObject/FX/FxSystem.h"` / `#include "GameObject/Champion/Irelia/IreliaBladeSystem.h"` 처럼 서브폴더 경로 포함.

2. **UltWave 파일명 변경 후** Client.vcxproj/filters 의 모든 "UItWave" 문자열 전수 검색 후 치환 — 빠뜨리면 사라진 파일 참조로 빌드 실패.

3. **FxSystem.cpp 가 Public 폴더에 있는 중복** — 솔루션 탐색기에서도, 파일 시스템에서도 확실히 제거. ClInclude 로 잘못 등록된 엔트리도 같이 제거.

4. **Scene_InGame OnUpdate 의 tick 호출 순서**: StatusEffectSystem 은 Scheduler (Phase 3) 에서 돈다. Fx/Blade/Ult 는 Scheduler 외부에서 직접 Execute. Scheduler 실행은 대개 OnUpdate 맨 앞. 그러므로 **castFrame 훅 → Spawn → Fx/Blade/Ult Execute** 순서 확인.

5. **CDX11Device::GetDevice() 반환값 확인** — `pDevice->GetDevice()` 인지 `pDev->GetDevice()` 인지 변수명 정리.
