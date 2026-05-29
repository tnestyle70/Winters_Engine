# 11. Animation System — UE5-Style AnimInstance / StateMachine / Montage / Notify

> **UE5 대응**: `UAnimInstance`, `UAnimStateMachine`, `FAnimMontage`, `UAnimNotify`, `FAnimNode_StateMachine`
> **현재 Winters**: `CAnimator` (Play/Stop/Update), `PlayAnimationByName(string)`, 수동 lock timer, 수동 castFrame/recoveryFrame 감지
> **목표**: 상태 머신 기반 애니메이션 + Montage 인터럽트 + Notify 프레임 콜백 — 수동 타이머/프레임 추적 제거

---

## 1. Architecture Overview

### 1.1 현재 문제점

```
현재 흐름:
1. Scene_InGame 에서 PlayAnimationByName("irelia_spell1") 직접 호출
2. m_fLastActionTimer = lockDurationSec 설정 (수동)
3. 매 프레임 CAnimator::GetCurrentFrame() 으로 castFrame 비교 (수동)
4. HasFramePassed(castFrame, prevFrame) → 데미지/FX 발동 (수동)
5. lockDurationSec 만료 → idle/run 복귀 (수동)

문제:
- 상태 전환 로직 없음 (Idle→Run→Attack→Skill 전부 if/else 분기)
- PlayAnimationByName 호출 시 m_dCurrentTime=0 리셋 → castFrame 재발동 위험
- lockDurationSec ↔ 애니 길이 수동 매칭 (불일치 시 중간 커트)
- castFrame / recoveryFrame 감지 코드가 Scene_InGame 에 300줄+
- 애니 블렌딩 없음 (하드 커트 전환)
```

### 1.2 목표 아키텍처

```
CAnimInstance (per-character)
├── CAnimStateMachine
│   ├── CAnimState "Idle"      ← 기본
│   │   └── CAnimation* idle1
│   ├── CAnimState "Run"
│   │   └── CAnimation* run
│   ├── CAnimState "Attack"
│   │   └── CAnimation* attack_01
│   └── CAnimState "Death"
│       └── CAnimation* death
│
├── CAnimMontage (interrupt)   ← 스킬/이모트 (PlayAnimationByName 대체)
│   ├── CAnimation* spell1
│   ├── CAnimNotify[castFrame]   → 콜백 (데미지/FX)
│   ├── CAnimNotify[recoveryFrame] → 콜백 (락 해제)
│   └── lockDurationSec 자동 관리
│
└── Output: vector<XMFLOAT4X4> finalBoneMatrices
```

---

## 2. 파일 구조

```
Engine/
├── Public/Animation/
│   ├── CAnimInstance.h       ← 캐릭터별 애니메이션 런타임
│   ├── CAnimStateMachine.h   ← 상태 그래프
│   ├── CAnimState.h          ← 단일 상태 (애니 + 블렌드 트리)
│   ├── CAnimTransition.h     ← 조건 기반 상태 전환
│   ├── CAnimMontage.h        ← 인터럽트 원샷 (스킬/이모트)
│   ├── CAnimNotify.h         ← 프레임 정확 콜백
│   └── AnimTypes.h           ← enum / 타입 정의
├── Private/Animation/
│   ├── CAnimInstance.cpp
│   ├── CAnimStateMachine.cpp
│   ├── CAnimState.cpp
│   ├── CAnimTransition.cpp
│   ├── CAnimMontage.cpp
│   └── CAnimNotify.cpp
```

---

## 3. 코드 전문

### 3.1 `Engine/Public/Animation/AnimTypes.h`

```cpp
#pragma once

#include "WintersTypes.h"
#include <cstdint>

// ── Animation State IDs ──
enum class eAnimStateId : u8_t
{
    Idle = 0,
    Run,
    Attack,
    Skill_Q,
    Skill_W,
    Skill_E,
    Skill_R,
    Death,
    Custom_0,
    Custom_1,
    Custom_2,
    Custom_3,
    END
};

// ── Notify Event Type ──
enum class eAnimNotifyType : u8_t
{
    CastFrame = 0,       // 데미지/투사체 발사 시점
    RecoveryFrame,       // 액션 락 해제 시점
    FxSpawn,             // 이펙트 스폰
    SoundPlay,           // 사운드 재생
    RootMotionStart,     // 루트 모션 활성화
    RootMotionEnd,       // 루트 모션 비활성화
    Custom,              // 사용자 정의
    END
};

// ── Montage Priority ──
// 높은 값 = 높은 우선순위. 같은 우선순위면 후자가 이김.
enum class eMontageSlot : u8_t
{
    DefaultSlot = 0,     // 일반 스킬
    FullBody,            // 전신 (R 등)
    Additive,            // 부가 (이모트 등)
    END
};

// ── Blend Mode ──
enum class eAnimBlendMode : u8_t
{
    Override = 0,        // 현재 상태 완전 대체
    Additive,            // 현재 상태 위에 더하기
    END
};
```

### 3.2 `Engine/Public/Animation/CAnimNotify.h`

```cpp
#pragma once

#include "Animation/AnimTypes.h"
#include "WintersTypes.h"
#include <functional>
#include <string>

// ─────────────────────────────────────────────────────────────────
//  CAnimNotify — 프레임 정확 콜백
//
//  UE5 UAnimNotify / UAnimNotifyState 대응.
//  특정 프레임에 도달하면 콜백 발동 (castFrame, recoveryFrame, FX spawn).
//
//  현재 CAnimator::HasFramePassed + Scene_InGame 의 m_fActivePrevFrame
//  수동 추적을 대체. 1회 발동 보장 (m_bFired).
//
//  SkillDef.castFrame / recoveryFrame 와 직접 매핑:
//    CAnimNotify(eAnimNotifyType::CastFrame, def.castFrame, callback)
// ─────────────────────────────────────────────────────────────────
class CAnimNotify
{
public:
    using Callback = std::function<void()>;

    CAnimNotify() = default;
    CAnimNotify(eAnimNotifyType eType, f32_t fFrame, Callback cb,
                const std::string& strLabel = "");
    ~CAnimNotify() = default;

    // ── Query ──
    eAnimNotifyType GetType()    const { return m_eType; }
    f32_t           GetFrame()   const { return m_fFrame; }
    bool            HasFired()   const { return m_bFired; }
    const std::string& GetLabel() const { return m_strLabel; }

    // ── Update (Montage/State 가 매 프레임 호출) ──
    // prevFrame < m_fFrame <= curFrame 이면 발동 (CLAUDE.md 5.6 규칙 준수)
    bool TryFire(f32_t fPrevFrame, f32_t fCurFrame);

    // ── Reset (몽타주 재시작 시) ──
    void Reset() { m_bFired = false; }

private:
    eAnimNotifyType m_eType  = eAnimNotifyType::Custom;
    f32_t           m_fFrame = 0.f;
    bool            m_bFired = false;
    Callback        m_cb;
    std::string     m_strLabel;
};
```

### 3.3 `Engine/Private/Animation/CAnimNotify.cpp`

```cpp
#include "Animation/CAnimNotify.h"

CAnimNotify::CAnimNotify(eAnimNotifyType eType, f32_t fFrame, Callback cb,
                         const std::string& strLabel)
    : m_eType(eType)
    , m_fFrame(fFrame)
    , m_cb(std::move(cb))
    , m_strLabel(strLabel)
{
}

bool CAnimNotify::TryFire(f32_t fPrevFrame, f32_t fCurFrame)
{
    if (m_bFired) return false;
    if (m_fFrame <= 0.f) return false;

    // HasFramePassed 로직 (CAnimator::HasFramePassed 동일)
    // prevFrame < frame <= curFrame
    if (fPrevFrame < m_fFrame && m_fFrame <= fCurFrame)
    {
        m_bFired = true;
        if (m_cb) m_cb();
        return true;
    }
    return false;
}
```

### 3.4 `Engine/Public/Animation/CAnimTransition.h`

```cpp
#pragma once

#include "Animation/AnimTypes.h"
#include "WintersTypes.h"
#include <functional>

// ─────────────────────────────────────────────────────────────────
//  CAnimTransition — 조건 기반 상태 전환
//
//  UE5 FAnimTransitionRule 대응.
//  조건 함수가 true 를 반환하면 From→To 전환 발동.
//  블렌드 시간 지정 가능 (현재는 하드 커트, 향후 crossfade).
// ─────────────────────────────────────────────────────────────────
class CAnimTransition
{
public:
    using ConditionFunc = std::function<bool()>;

    CAnimTransition() = default;
    CAnimTransition(eAnimStateId eFrom, eAnimStateId eTo,
                    ConditionFunc condition, f32_t fBlendTime = 0.f);
    ~CAnimTransition() = default;

    eAnimStateId  GetFrom()      const { return m_eFrom; }
    eAnimStateId  GetTo()        const { return m_eTo; }
    f32_t         GetBlendTime() const { return m_fBlendTime; }

    bool Evaluate() const { return m_condition ? m_condition() : false; }

private:
    eAnimStateId  m_eFrom      = eAnimStateId::END;
    eAnimStateId  m_eTo        = eAnimStateId::END;
    f32_t         m_fBlendTime = 0.f;
    ConditionFunc m_condition;
};
```

### 3.5 `Engine/Private/Animation/CAnimTransition.cpp`

```cpp
#include "Animation/CAnimTransition.h"

CAnimTransition::CAnimTransition(eAnimStateId eFrom, eAnimStateId eTo,
                                 ConditionFunc condition, f32_t fBlendTime)
    : m_eFrom(eFrom)
    , m_eTo(eTo)
    , m_fBlendTime(fBlendTime)
    , m_condition(std::move(condition))
{
}
```

### 3.6 `Engine/Public/Animation/CAnimState.h`

```cpp
#pragma once

#include "Animation/AnimTypes.h"
#include "WintersTypes.h"
#include <string>
#include <vector>
#include <memory>

namespace Engine { class CAnimation; }

// ─────────────────────────────────────────────────────────────────
//  CAnimState — 상태 머신의 단일 상태
//
//  UE5 FAnimNode_StateMachine 내부 State 대응.
//  하나의 CAnimation 을 재생 (향후: 블렌드 트리).
//  루프 / 원샷 설정 가능.
// ─────────────────────────────────────────────────────────────────
class CAnimState
{
public:
    CAnimState() = default;
    CAnimState(eAnimStateId eId, const std::string& strName, bool bLoop = true);
    ~CAnimState() = default;

    // ── Setup ──
    void SetAnimation(Engine::CAnimation* pAnim) { m_pAnimation = pAnim; }
    void SetLoop(bool bLoop)                      { m_bLoop = bLoop; }
    void SetPlaySpeed(f32_t fSpeed)               { m_fPlaySpeed = fSpeed; }

    // ── Query ──
    eAnimStateId         GetId()        const { return m_eId; }
    const std::string&   GetName()      const { return m_strName; }
    Engine::CAnimation*  GetAnimation() const { return m_pAnimation; }
    bool                 IsLoop()       const { return m_bLoop; }
    f32_t                GetPlaySpeed() const { return m_fPlaySpeed; }

    // ── State Events ──
    void OnEnter();
    void OnExit();

private:
    eAnimStateId        m_eId     = eAnimStateId::END;
    std::string         m_strName;
    Engine::CAnimation* m_pAnimation = nullptr;
    bool                m_bLoop      = true;
    f32_t               m_fPlaySpeed = 1.f;
};
```

### 3.7 `Engine/Private/Animation/CAnimState.cpp`

```cpp
#include "Animation/CAnimState.h"

CAnimState::CAnimState(eAnimStateId eId, const std::string& strName, bool bLoop)
    : m_eId(eId)
    , m_strName(strName)
    , m_bLoop(bLoop)
{
}

void CAnimState::OnEnter()
{
    // 상태 진입 시 처리 (향후: 블렌드 트리 초기화)
}

void CAnimState::OnExit()
{
    // 상태 퇴장 시 처리
}
```

### 3.8 `Engine/Public/Animation/CAnimStateMachine.h`

```cpp
#pragma once

#include "Animation/AnimTypes.h"
#include "Animation/CAnimState.h"
#include "Animation/CAnimTransition.h"
#include "WintersTypes.h"
#include <vector>
#include <unordered_map>

namespace Engine { class CAnimator; class CAnimation; }

// ─────────────────────────────────────────────────────────────────
//  CAnimStateMachine — 상태 그래프 (Idle→Run→Attack→Death)
//
//  UE5 FAnimNode_StateMachine 대응.
//  매 프레임 현재 상태에서 나가는 Transition 조건 평가 → 전환.
//  CAnimator 의 Play/Stop 을 제어.
//
//  현재 Scene_InGame 의 idle/run/attack if-else 분기:
//    if (m_bMoving) PlayAnimationByName(run);
//    else PlayAnimationByName(idle);
//  → CAnimTransition 조건으로 선언적 변환.
// ─────────────────────────────────────────────────────────────────
class CAnimStateMachine
{
public:
    CAnimStateMachine();
    ~CAnimStateMachine();

    // ── Setup ──
    void AddState(const CAnimState& state);
    void AddTransition(const CAnimTransition& transition);
    void SetDefaultState(eAnimStateId eId);

    // ── Bind ──
    void BindAnimator(Engine::CAnimator* pAnimator) { m_pAnimator = pAnimator; }

    // ── Update ──
    void Update(f32_t fDeltaTime);

    // ── Query ──
    eAnimStateId   GetCurrentStateId()   const { return m_eCurrentState; }
    const CAnimState* GetCurrentState()  const;
    bool           IsInState(eAnimStateId id) const { return m_eCurrentState == id; }

    // ── Override (Montage 에서 상태 머신 일시 정지/복구) ──
    void Pause()  { m_bPaused = true; }
    void Resume() { m_bPaused = false; }
    bool IsPaused() const { return m_bPaused; }

    // ── ImGui ──
    void OnImGui();

private:
    void TransitionTo(eAnimStateId eNewState);

    Engine::CAnimator* m_pAnimator = nullptr;

    std::unordered_map<eAnimStateId, CAnimState> m_mapStates;
    std::vector<CAnimTransition>                 m_vecTransitions;

    eAnimStateId m_eCurrentState = eAnimStateId::Idle;
    bool         m_bPaused       = false;
};
```

### 3.9 `Engine/Private/Animation/CAnimStateMachine.cpp`

```cpp
#include "Animation/CAnimStateMachine.h"
#include "Resource/Animator.h"
#include "Resource/Animation.h"

#ifdef WINTERS_EDITOR
#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")
#endif

CAnimStateMachine::CAnimStateMachine()  = default;
CAnimStateMachine::~CAnimStateMachine() = default;

void CAnimStateMachine::AddState(const CAnimState& state)
{
    m_mapStates[state.GetId()] = state;
}

void CAnimStateMachine::AddTransition(const CAnimTransition& transition)
{
    m_vecTransitions.push_back(transition);
}

void CAnimStateMachine::SetDefaultState(eAnimStateId eId)
{
    m_eCurrentState = eId;
    TransitionTo(eId);
}

void CAnimStateMachine::Update(f32_t fDeltaTime)
{
    if (m_bPaused) return;

    // 현재 상태에서 나가는 전환 조건 평가
    for (const auto& tr : m_vecTransitions)
    {
        if (tr.GetFrom() != m_eCurrentState) continue;
        if (tr.Evaluate())
        {
            TransitionTo(tr.GetTo());
            break;   // 1 프레임 1 전환
        }
    }
}

const CAnimState* CAnimStateMachine::GetCurrentState() const
{
    auto it = m_mapStates.find(m_eCurrentState);
    return (it != m_mapStates.end()) ? &it->second : nullptr;
}

void CAnimStateMachine::TransitionTo(eAnimStateId eNewState)
{
    // 이전 상태 퇴장
    auto itOld = m_mapStates.find(m_eCurrentState);
    if (itOld != m_mapStates.end())
        itOld->second.OnExit();

    m_eCurrentState = eNewState;

    // 새 상태 진입
    auto itNew = m_mapStates.find(eNewState);
    if (itNew != m_mapStates.end())
    {
        itNew->second.OnEnter();

        // CAnimator 에 애니 설정
        if (m_pAnimator && itNew->second.GetAnimation())
        {
            m_pAnimator->PlayAnimation(
                itNew->second.GetAnimation(),
                itNew->second.IsLoop());
            m_pAnimator->SetPlaySpeed(itNew->second.GetPlaySpeed());
        }
    }
}

void CAnimStateMachine::OnImGui()
{
#ifdef WINTERS_EDITOR
    if (ImGui::CollapsingHeader("AnimStateMachine"))
    {
        const CAnimState* pCur = GetCurrentState();
        ImGui::Text("Current: %s (id=%d)",
            pCur ? pCur->GetName().c_str() : "NONE",
            static_cast<i32_t>(m_eCurrentState));
        ImGui::Text("Paused: %s", m_bPaused ? "Yes" : "No");

        if (ImGui::TreeNode("States"))
        {
            for (const auto& [id, state] : m_mapStates)
            {
                bool bCurrent = (id == m_eCurrentState);
                ImGui::Text("%s %s (loop=%s, speed=%.2f)",
                    bCurrent ? ">>>" : "   ",
                    state.GetName().c_str(),
                    state.IsLoop() ? "Y" : "N",
                    state.GetPlaySpeed());
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Transitions"))
        {
            for (u32_t i = 0; i < m_vecTransitions.size(); ++i)
            {
                const auto& tr = m_vecTransitions[i];
                ImGui::Text("[%u] %d -> %d (blend=%.2fs)",
                    i,
                    static_cast<i32_t>(tr.GetFrom()),
                    static_cast<i32_t>(tr.GetTo()),
                    tr.GetBlendTime());
            }
            ImGui::TreePop();
        }
    }
#endif
}
```

### 3.10 `Engine/Public/Animation/CAnimMontage.h`

```cpp
#pragma once

#include "Animation/AnimTypes.h"
#include "Animation/CAnimNotify.h"
#include "WintersTypes.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace Engine { class CAnimation; class CAnimator; }

// ─────────────────────────────────────────────────────────────────
//  CAnimMontage — 인터럽트 기반 원샷 애니메이션
//
//  UE5 UAnimMontage 대응.
//  PlayAnimationByName 을 완전 대체:
//    기존: m_pPlayerRenderer->PlayAnimationByName("irelia_spell1")
//    신규: m_pAnimInstance->PlayMontage(pIreliaQMontage)
//
//  스킬 / 이모트 / 죽음 등 상태 머신을 일시 중단하고 1회 재생.
//  재생 완료 시 상태 머신 자동 복귀.
//
//  Notify 연결:
//    pMontage->AddNotify(CAnimNotify(CastFrame, 6.f, [](){ DealDamage(); }));
//    pMontage->AddNotify(CAnimNotify(RecoveryFrame, 12.f, [](){ Unlock(); }));
//
//  SkillDef 연동:
//    lockDurationSec → m_fDuration (자동 계산 옵션)
//    animPlaySpeed   → m_fPlaySpeed
//    castFrame       → CAnimNotify(CastFrame, ...)
//    recoveryFrame   → CAnimNotify(RecoveryFrame, ...)
// ─────────────────────────────────────────────────────────────────
class CAnimMontage
{
public:
    CAnimMontage();
    ~CAnimMontage();

    // ── Factory (SkillDef 에서 자동 생성) ──
    static std::unique_ptr<CAnimMontage> CreateFromSkillDef(
        const struct SkillDef& def,
        Engine::CAnimation* pAnim,
        std::function<void()> onCastFrame = nullptr,
        std::function<void()> onRecoveryFrame = nullptr);

    // ── Setup ──
    void SetAnimation(Engine::CAnimation* pAnim) { m_pAnimation = pAnim; }
    void SetDuration(f32_t sec)                   { m_fDuration = sec; }
    void SetPlaySpeed(f32_t speed)                { m_fPlaySpeed = speed; }
    void SetSlot(eMontageSlot slot)               { m_eSlot = slot; }
    void SetName(const std::string& name)         { m_strName = name; }
    void AddNotify(const CAnimNotify& notify);

    // ── Playback ──
    void Play(Engine::CAnimator* pAnimator);
    void Stop();
    void Update(f32_t fDeltaTime);

    // ── Query ──
    bool IsPlaying()       const { return m_bPlaying; }
    bool IsFinished()      const { return m_bFinished; }
    f32_t GetElapsedTime() const { return m_fElapsed; }
    f32_t GetDuration()    const { return m_fDuration; }
    f32_t GetPlaySpeed()   const { return m_fPlaySpeed; }
    eMontageSlot GetSlot() const { return m_eSlot; }
    const std::string& GetName() const { return m_strName; }

    // ── Completion Callback ──
    using OnCompleteFunc = std::function<void()>;
    void SetOnComplete(OnCompleteFunc cb) { m_cbOnComplete = std::move(cb); }

    // ── ImGui ──
    void OnImGui();

private:
    Engine::CAnimation* m_pAnimation = nullptr;
    Engine::CAnimator*  m_pAnimator  = nullptr;

    std::string  m_strName;
    eMontageSlot m_eSlot     = eMontageSlot::DefaultSlot;
    f32_t        m_fDuration  = 0.f;    // lockDurationSec
    f32_t        m_fPlaySpeed = 1.f;    // animPlaySpeed
    f32_t        m_fElapsed   = 0.f;
    f32_t        m_fPrevFrame = 0.f;
    bool         m_bPlaying   = false;
    bool         m_bFinished  = false;

    std::vector<CAnimNotify> m_vecNotifies;
    OnCompleteFunc           m_cbOnComplete;
};
```

### 3.11 `Engine/Private/Animation/CAnimMontage.cpp`

```cpp
#include "Animation/CAnimMontage.h"
#include "Resource/Animator.h"
#include "Resource/Animation.h"

#include "Shared/GameSim/Definitions/SkillDef.h"

#ifdef WINTERS_EDITOR
#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")
#endif

CAnimMontage::CAnimMontage()  = default;
CAnimMontage::~CAnimMontage() = default;

std::unique_ptr<CAnimMontage> CAnimMontage::CreateFromSkillDef(
    const SkillDef& def,
    Engine::CAnimation* pAnim,
    std::function<void()> onCastFrame,
    std::function<void()> onRecoveryFrame)
{
    auto pMontage = std::make_unique<CAnimMontage>();

    pMontage->SetAnimation(pAnim);
    pMontage->SetDuration(def.lockDurationSec);
    pMontage->SetPlaySpeed(def.animPlaySpeed);
    pMontage->SetName(def.animKey ? def.animKey : "unknown");

    // castFrame Notify
    if (def.castFrame > 0.f && onCastFrame)
    {
        pMontage->AddNotify(CAnimNotify(
            eAnimNotifyType::CastFrame,
            def.castFrame,
            std::move(onCastFrame),
            "CastFrame"));
    }

    // recoveryFrame Notify
    if (def.recoveryFrame > 0.f && onRecoveryFrame)
    {
        pMontage->AddNotify(CAnimNotify(
            eAnimNotifyType::RecoveryFrame,
            def.recoveryFrame,
            std::move(onRecoveryFrame),
            "RecoveryFrame"));
    }

    return pMontage;
}

void CAnimMontage::AddNotify(const CAnimNotify& notify)
{
    m_vecNotifies.push_back(notify);
}

void CAnimMontage::Play(Engine::CAnimator* pAnimator)
{
    m_pAnimator = pAnimator;
    m_fElapsed  = 0.f;
    m_fPrevFrame = 0.f;
    m_bPlaying  = true;
    m_bFinished = false;

    // Notify 리셋 (재발동 방지)
    for (auto& n : m_vecNotifies)
        n.Reset();

    // CAnimator 에 애니 재생
    if (m_pAnimator && m_pAnimation)
    {
        m_pAnimator->PlayAnimation(m_pAnimation, false);   // 원샷
        m_pAnimator->SetPlaySpeed(m_fPlaySpeed);
    }
}

void CAnimMontage::Stop()
{
    m_bPlaying  = false;
    m_bFinished = true;

    if (m_pAnimator)
        m_pAnimator->Stop();
}

void CAnimMontage::Update(f32_t fDeltaTime)
{
    if (!m_bPlaying) return;

    m_fElapsed += fDeltaTime;

    // ── Notify 발동 ──
    if (m_pAnimator)
    {
        const f32_t curFrame = m_pAnimator->GetCurrentFrame();

        // ★ 단일 블록에서 모든 Notify 판정 (castFrame 감지 블록 분리 금지 규칙)
        for (auto& notify : m_vecNotifies)
            notify.TryFire(m_fPrevFrame, curFrame);

        // ★ 맨 마지막에 prevFrame 갱신
        m_fPrevFrame = curFrame;
    }

    // ── Duration 만료 판정 ──
    if (m_fDuration > 0.f && m_fElapsed >= m_fDuration)
    {
        m_bPlaying  = false;
        m_bFinished = true;

        if (m_cbOnComplete)
            m_cbOnComplete();
    }

    // ── CAnimator 가 자체적으로 재생 완료 (원샷) ──
    if (m_pAnimator && !m_pAnimator->IsPlaying())
    {
        m_bPlaying  = false;
        m_bFinished = true;

        if (m_cbOnComplete)
            m_cbOnComplete();
    }
}

void CAnimMontage::OnImGui()
{
#ifdef WINTERS_EDITOR
    if (ImGui::TreeNode(m_strName.empty() ? "Montage" : m_strName.c_str()))
    {
        ImGui::Text("Playing: %s  Finished: %s", m_bPlaying ? "Y" : "N", m_bFinished ? "Y" : "N");
        ImGui::Text("Elapsed: %.2f / %.2f s", m_fElapsed, m_fDuration);
        ImGui::Text("PlaySpeed: %.2f", m_fPlaySpeed);

        if (ImGui::TreeNode("Notifies"))
        {
            for (u32_t i = 0; i < m_vecNotifies.size(); ++i)
            {
                const auto& n = m_vecNotifies[i];
                ImGui::Text("[%u] %s frame=%.1f fired=%s",
                    i,
                    n.GetLabel().c_str(),
                    n.GetFrame(),
                    n.HasFired() ? "Y" : "N");
            }
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
#endif
}
```

### 3.12 `Engine/Public/Animation/CAnimInstance.h`

```cpp
#pragma once

#include "Animation/AnimTypes.h"
#include "Animation/CAnimStateMachine.h"
#include "Animation/CAnimMontage.h"
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace Engine { class CAnimator; class CAnimation; class CSkeleton; }
class ModelRenderer;

// ─────────────────────────────────────────────────────────────────
//  CAnimInstance — 캐릭터별 애니메이션 런타임
//
//  UE5 UAnimInstance 대응. 캐릭터 1명당 1개.
//  StateMachine + Montage 계층을 관리.
//
//  현재 흐름:
//    ModelRenderer::PlayAnimationByName("irelia_idle_01")
//    → CAnimator::PlayAnimation(pAnim, bLoop)
//    → Scene_InGame 에서 수동 프레임 감시
//
//  신규 흐름:
//    CAnimInstance::Initialize(pRenderer)
//    CAnimInstance::SetupStateMachine(...)     // Idle/Run/Death
//    CAnimInstance::PlayMontage(pMontage)      // 스킬 인터럽트
//    CAnimInstance::Update(dt)                 // 자동 상태 전환 + Notify
//
//  ModelRenderer 를 래핑하지만 소유하지 않음 (Scene 이 소유).
//  CAnimator 접근은 ModelRenderer::GetAnimator() 경유.
// ─────────────────────────────────────────────────────────────────
class WINTERS_ENGINE CAnimInstance
{
public:
    CAnimInstance();
    ~CAnimInstance();

    static std::unique_ptr<CAnimInstance> Create(ModelRenderer* pRenderer);

    // ── Lifecycle ──
    void Initialize(ModelRenderer* pRenderer);
    void Update(f32_t fDeltaTime);

    // ── State Machine ──
    CAnimStateMachine& GetStateMachine() { return m_StateMachine; }
    void SetupDefaultStates(
        Engine::CAnimation* pIdle,
        Engine::CAnimation* pRun,
        Engine::CAnimation* pDeath,
        std::function<bool()> fnIsMoving,
        std::function<bool()> fnIsDead);

    // ── Montage ──
    void PlayMontage(CAnimMontage* pMontage);
    void StopMontage();
    bool IsMontageActive() const { return m_pActiveMontage && m_pActiveMontage->IsPlaying(); }
    CAnimMontage* GetActiveMontage() const { return m_pActiveMontage; }

    // ── Animation Query (편의) ──
    void PlayAnimationDirect(const std::string& strKey, bool bLoop = true);
    void SetPlaySpeed(f32_t fSpeed);
    f32_t GetCurrentFrame() const;
    bool  IsPlaying()       const;

    // ── Root Motion ──
    bool  HasRootMotion()   const { return m_bRootMotionEnabled; }
    Vec3  ConsumeRootMotionDelta();
    void  SetRootMotionEnabled(bool b) { m_bRootMotionEnabled = b; }

    // ── ImGui ──
    void OnImGui();

private:
    ModelRenderer*       m_pRenderer  = nullptr;
    Engine::CAnimator*   m_pAnimator  = nullptr;   // 캐시 (m_pRenderer->GetAnimator())

    CAnimStateMachine    m_StateMachine;
    CAnimMontage*        m_pActiveMontage = nullptr;   // 비소유 (외부 관리)

    // Root Motion
    bool  m_bRootMotionEnabled = false;
    Vec3  m_vRootMotionDelta{ 0.f, 0.f, 0.f };
};
```

### 3.13 `Engine/Private/Animation/CAnimInstance.cpp`

```cpp
#include "Animation/CAnimInstance.h"
#include "Renderer/ModelRenderer.h"
#include "Resource/Animator.h"
#include "Resource/Animation.h"

#ifdef WINTERS_EDITOR
#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")
#endif

CAnimInstance::CAnimInstance()  = default;
CAnimInstance::~CAnimInstance() = default;

std::unique_ptr<CAnimInstance> CAnimInstance::Create(ModelRenderer* pRenderer)
{
    auto p = std::unique_ptr<CAnimInstance>(new CAnimInstance());
    p->Initialize(pRenderer);
    return p;
}

void CAnimInstance::Initialize(ModelRenderer* pRenderer)
{
    m_pRenderer = pRenderer;
    m_pAnimator = pRenderer ? pRenderer->GetAnimator() : nullptr;

    m_StateMachine.BindAnimator(m_pAnimator);
}

void CAnimInstance::SetupDefaultStates(
    Engine::CAnimation* pIdle,
    Engine::CAnimation* pRun,
    Engine::CAnimation* pDeath,
    std::function<bool()> fnIsMoving,
    std::function<bool()> fnIsDead)
{
    // ── 기본 3상태: Idle ↔ Run, Any → Death ──
    //
    // 현재 Scene_InGame 의 if/else 분기:
    //   if (m_bMoving) PlayAnimationByName(run);
    //   else           PlayAnimationByName(idle);
    // → 선언적 전환으로 대체.

    CAnimState stIdle(eAnimStateId::Idle, "Idle", true);
    stIdle.SetAnimation(pIdle);
    m_StateMachine.AddState(stIdle);

    CAnimState stRun(eAnimStateId::Run, "Run", true);
    stRun.SetAnimation(pRun);
    m_StateMachine.AddState(stRun);

    CAnimState stDeath(eAnimStateId::Death, "Death", false);
    stDeath.SetAnimation(pDeath);
    m_StateMachine.AddState(stDeath);

    // Idle → Run (이동 중)
    m_StateMachine.AddTransition(CAnimTransition(
        eAnimStateId::Idle, eAnimStateId::Run,
        fnIsMoving));

    // Run → Idle (이동 정지)
    m_StateMachine.AddTransition(CAnimTransition(
        eAnimStateId::Run, eAnimStateId::Idle,
        [fnIsMoving]() { return !fnIsMoving(); }));

    // Any → Death
    m_StateMachine.AddTransition(CAnimTransition(
        eAnimStateId::Idle, eAnimStateId::Death, fnIsDead));
    m_StateMachine.AddTransition(CAnimTransition(
        eAnimStateId::Run, eAnimStateId::Death, fnIsDead));

    m_StateMachine.SetDefaultState(eAnimStateId::Idle);
}

void CAnimInstance::Update(f32_t fDeltaTime)
{
    // Animator 캐시 갱신 (지연 초기화 대응)
    if (!m_pAnimator && m_pRenderer)
    {
        m_pAnimator = m_pRenderer->GetAnimator();
        m_StateMachine.BindAnimator(m_pAnimator);
    }

    // ── Montage 가 재생 중이면 상태 머신 일시 정지 ──
    if (m_pActiveMontage && m_pActiveMontage->IsPlaying())
    {
        m_StateMachine.Pause();
        m_pActiveMontage->Update(fDeltaTime);

        if (m_pActiveMontage->IsFinished())
        {
            m_pActiveMontage = nullptr;
            m_StateMachine.Resume();
        }
    }
    else
    {
        // 상태 머신 업데이트 (전환 조건 평가 + 애니 재생)
        m_StateMachine.Update(fDeltaTime);
    }
}

void CAnimInstance::PlayMontage(CAnimMontage* pMontage)
{
    if (!pMontage || !m_pAnimator) return;

    // 기존 몽타주 정지
    if (m_pActiveMontage && m_pActiveMontage->IsPlaying())
        m_pActiveMontage->Stop();

    m_pActiveMontage = pMontage;
    m_pActiveMontage->Play(m_pAnimator);

    // 상태 머신 일시 정지
    m_StateMachine.Pause();
}

void CAnimInstance::StopMontage()
{
    if (m_pActiveMontage)
    {
        m_pActiveMontage->Stop();
        m_pActiveMontage = nullptr;
    }
    m_StateMachine.Resume();
}

void CAnimInstance::PlayAnimationDirect(const std::string& strKey, bool bLoop)
{
    // 하위 호환: 기존 PlayAnimationByName 래핑
    if (m_pRenderer)
        m_pRenderer->PlayAnimationByName(strKey, bLoop);
}

void CAnimInstance::SetPlaySpeed(f32_t fSpeed)
{
    if (m_pAnimator)
        m_pAnimator->SetPlaySpeed(fSpeed);
}

f32_t CAnimInstance::GetCurrentFrame() const
{
    return m_pAnimator ? m_pAnimator->GetCurrentFrame() : 0.f;
}

bool CAnimInstance::IsPlaying() const
{
    return m_pAnimator ? m_pAnimator->IsPlaying() : false;
}

Vec3 CAnimInstance::ConsumeRootMotionDelta()
{
    Vec3 delta = m_vRootMotionDelta;
    m_vRootMotionDelta = { 0.f, 0.f, 0.f };
    return delta;
}

void CAnimInstance::OnImGui()
{
#ifdef WINTERS_EDITOR
    if (ImGui::CollapsingHeader("AnimInstance"))
    {
        ImGui::Text("Renderer: %s  Animator: %s",
            m_pRenderer ? "bound" : "null",
            m_pAnimator ? "bound" : "null");

        if (m_pAnimator)
        {
            ImGui::Text("Frame: %.1f  Playing: %s",
                m_pAnimator->GetCurrentFrame(),
                m_pAnimator->IsPlaying() ? "Y" : "N");

            f32_t speed = m_pAnimator->GetPlaySpeed();
            if (ImGui::SliderFloat("PlaySpeed", &speed, 0.01f, 5.f))
                m_pAnimator->SetPlaySpeed(speed);
        }

        ImGui::Text("Montage Active: %s",
            (m_pActiveMontage && m_pActiveMontage->IsPlaying()) ? "Y" : "N");

        if (m_pActiveMontage)
            m_pActiveMontage->OnImGui();

        m_StateMachine.OnImGui();

        ImGui::Checkbox("Root Motion", &m_bRootMotionEnabled);
    }
#endif
}
```

---

## 4. Example: Irelia Q (Bladesurge) — Before vs After

### 4.1 Before (Scene_InGame.cpp ~80줄)

```cpp
// Scene_InGame.cpp — Irelia Q 수동 처리

// 1. 스킬 발동 (DispatchSkillInput)
void CScene_InGame::ApplyLocalPrediction(const CastSkillCommand& cmd, const SkillDef& def)
{
    m_ActiveSkillDefStorage = def;            // 값 복사 (댕글링 방지)
    m_pActiveSkillDef = &m_ActiveSkillDefStorage;
    m_ActiveSkillCommandStorage = cmd;
    m_fActivePrevFrame = 0.f;
    m_bCastFrameFired = false;
    m_bRecoveryFrameFired = false;
    m_fLastActionTimer = def.lockDurationSec; // 수동 락
    m_pPlayerRenderer->PlayAnimationByName(def.animKey, def.bOneShot);  // 수동 재생
    m_pPlayerRenderer->GetAnimator()->SetPlaySpeed(def.animPlaySpeed);  // 수동 속도
}

// 2. 매 프레임 프레임 이벤트 감시 (OnUpdate ~150줄)
if (m_pActiveSkillDef && m_pPlayerRenderer)
{
    const CAnimator* pAnim = m_pPlayerRenderer->GetAnimator();
    const f32_t curF = pAnim->GetCurrentFrame();
    // castFrame
    if (!m_bCastFrameFired && pAnim->HasFramePassed(def.castFrame, m_fActivePrevFrame))
    {
        m_bCastFrameFired = true;
        // 데미지 + FX + 훅 dispatch 80줄...
    }
    // recoveryFrame
    if (!m_bRecoveryFrameFired && pAnim->HasFramePassed(def.recoveryFrame, m_fActivePrevFrame))
    {
        m_bRecoveryFrameFired = true;
        // 락 해제
    }
    m_fActivePrevFrame = curF;
}

// 3. 락 타이머 만료 (OnUpdate)
if (m_fLastActionTimer > 0.f)
{
    m_fLastActionTimer -= dt;
    if (m_fLastActionTimer <= 0.f)
        m_pPlayerRenderer->PlayAnimationByName(m_bMoving ? runAnim : idleAnim);
}
```

### 4.2 After (WChampionCharacter ~30줄)

```cpp
// WChampionCharacter.cpp — Irelia Q Montage 방식

void WChampionCharacter::CastSkill_Q(const CastSkillCommand& cmd)
{
    const SkillDef* pDef = FindSkillDef(eChampion::IRELIA, 1);
    if (!pDef) return;

    // 몽타주 자동 생성 (SkillDef 에서)
    Engine::CAnimation* pQAnim = FindAnimationByName(pDef->animKey);  // "irelia_spell1"
    m_pQMontage = CAnimMontage::CreateFromSkillDef(
        *pDef,
        pQAnim,
        // castFrame 콜백: 데미지 + FX
        [this, cmd]()
        {
            // 데미지 (GameplayHook dispatch)
            GameplayHookContext ctx{};
            ctx.pWorld       = GetWorld();
            ctx.casterEntity = GetEntityID();
            CGameplayHookRegistry::Instance().Dispatch(kIreliaQCast, ctx);

            // FX (VisualHook dispatch)
            VisualHookContext viz{};
            viz.pWorld       = GetWorld();
            viz.casterEntity = GetEntityID();
            CVisualHookRegistry::Instance().Dispatch(kIreliaQCast, viz);
        },
        // recoveryFrame 콜백: 이동 재개
        [this]()
        {
            // Q 대시 착지 → 이동 즉시 가능
            ClearActionLock();
        });

    // 몽타주 완료 → 상태 머신 복귀
    m_pQMontage->SetOnComplete([this]()
    {
        // 상태 머신이 Idle/Run 자동 결정
    });

    // 재생
    m_pAnimInstance->PlayMontage(m_pQMontage.get());
    SetActionLock(pDef->lockDurationSec);

    // Q 대시 시작
    StartDash(cmd.targetEntityId);
}
```

---

## 5. SkillDef ↔ AnimMontage 자동 매핑 규칙

| SkillDef 필드 | AnimMontage/Notify 필드 | 변환 |
|---|---|---|
| `animKey` | `CAnimMontage::m_strName` + `FindAnimationByName()` | 문자열 → CAnimation* |
| `lockDurationSec` | `CAnimMontage::m_fDuration` | 직접 대입 |
| `animPlaySpeed` | `CAnimMontage::m_fPlaySpeed` | 직접 대입 |
| `bOneShot` | `CAnimator::PlayAnimation(pAnim, false)` | 원샷 = Montage 기본 |
| `castFrame` | `CAnimNotify(CastFrame, castFrame, cb)` | 프레임 번호 → Notify |
| `recoveryFrame` | `CAnimNotify(RecoveryFrame, recoveryFrame, cb)` | 프레임 번호 → Notify |
| `castFrameHookId` | `cb` 내부에서 HookId dispatch | 콜백 클로저에 캡처 |
| `recoveryHookId` | `cb` 내부에서 HookId dispatch | 콜백 클로저에 캡처 |
| `stage2AnimKey` | 2번째 `CAnimMontage` (체인) | Stage2 = 별도 Montage |
| `endTransitionIdleAnim` | `CAnimMontage::SetOnComplete` 에서 전환 | 완료 콜백 |

### 부등식 검증 (CLAUDE.md 5.6):
```
lockDuration * animPlaySpeed >= recoveryFrame / FBX_FPS
```
`CAnimMontage::CreateFromSkillDef` 내부에서 assert 검증:
```cpp
assert(def.lockDurationSec * def.animPlaySpeed >=
       def.recoveryFrame / 25.f);  // 25 FPS default
```

---

## 6. Root Motion Support

### 6.1 설계

```
CAnimation::Evaluate() 시 루트 본(index=0) 의 위치 변화량 추출
→ CAnimInstance::m_vRootMotionDelta 에 누적
→ WCharacter::Tick 에서 ConsumeRootMotionDelta() 호출 → Transform 적용
→ NavAgent 와 충돌 시 NavAgent 비활성화 (Root Motion 우선)
```

### 6.2 적용 시점

Irelia Q 대시: 현재 수동 보간 (`UpdateDash` 선형 Lerp).
Root Motion 활성화 시: FBX 루트 본 이동 → 자연스러운 곡선 대시.
단, LoL 모작에서는 서버 권위 이동이므로 Root Motion 은 시각 보정용.

---

## 7. Verification Checklist

| # | 검증 항목 | 합격 조건 |
|---|----------|----------|
| 1 | CAnimStateMachine Idle↔Run 전환 | bMoving true/false 에 따라 애니 정상 전환 |
| 2 | CAnimMontage 재생 + 완료 복귀 | Montage 종료 후 StateMachine 의 현재 상태 애니로 복귀 |
| 3 | CAnimNotify castFrame 1회 발동 | HasFramePassed 교차 시 콜백 1회 + m_bFired=true |
| 4 | CAnimNotify recoveryFrame 1회 발동 | 동일 |
| 5 | CAnimMontage::CreateFromSkillDef | SkillDef 의 모든 필드 정확 매핑 |
| 6 | lockDuration 부등식 assert | 위반 시 assert 발동 |
| 7 | Montage 중 상태 머신 Pause | StateMachine::Update 미호출 확인 |
| 8 | 기존 PlayAnimationByName 호환 | PlayAnimationDirect 로 기존 코드 동작 |
| 9 | ImGui 실시간 표시 | 현재 상태 / Montage 진행 / Notify 발동 여부 표시 |
| 10 | 프레임 성능 | CAnimInstance::Update < 0.1ms per character |

---

## 8. Migration Strategy

### Phase 1: CAnimInstance 도입 (비파괴, 1주)
- Engine/Public/Animation/ 에 모든 헤더/소스 추가
- CAnimInstance::Create(pRenderer) 로 ModelRenderer 래핑
- PlayAnimationDirect 로 기존 PlayAnimationByName 동작 유지
- Scene_InGame 에서 m_pAnimInstance 멤버 추가, 기존 코드와 병행

### Phase 2: StateMachine 전환 (1주)
- SetupDefaultStates(idle, run, death, fnIsMoving, fnIsDead) 호출
- Scene_InGame 의 idle/run if-else 분기 제거
- m_bMoving 을 WCharacter 멤버로 이관

### Phase 3: Montage 전환 (2주)
- 챔피언별 스킬을 CAnimMontage::CreateFromSkillDef 로 생성
- Scene_InGame 의 castFrame/recoveryFrame 수동 감시 코드 300줄 제거
- m_fLastActionTimer / m_fActivePrevFrame / m_bCastFrameFired 멤버 제거
- WSkillComponent + CAnimMontage 조합으로 완전 대체

### Phase 4: Root Motion + 블렌딩 (선택, 2주)
- 루트 본 추출 + ConsumeRootMotionDelta
- 상태 전환 시 크로스페이드 블렌딩 (현재 하드 커트)
