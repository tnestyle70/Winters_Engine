# Phase 7D-6A Irelia Q SkillHook 반영 보고

작성일: 2026-05-06

## 1. 반영 범위

```txt
Irelia SkillRegistry self-registration 추가
Irelia Q onCastAcceptedHook 추가
Scene_InGame의 Irelia Q dash fallback 제거
```

R / W / E stage hook은 아직 Scene 의존이 크므로 다음 단계로 남긴다.

---

## 2. 신규 파일

```txt
Client/Public/GameObject/Champion/Irelia/Irelia_Skills.h
Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp
Client/Private/GameObject/Champion/Irelia/Irelia_Registration.cpp
```

`Client.vcxproj` / `Client.vcxproj.filters`에도 Irelia 필터로 매핑했다.

---

## 3. Hook 등록

```cpp
constexpr u32_t kIrelia_Q_OnCastAccepted =
    MakeHookId(eChampion::IRELIA, HookVariant::Q_OnCastAccepted);

s.onCastAcceptedHookId = kIrelia_Q_OnCastAccepted;

CSkillHookRegistry::Instance().Register(
    kIrelia_Q_OnCastAccepted, &Irelia::OnCastAccepted_Q);
```

Irelia도 다른 순수 ECS 챔피언과 동일하게 keep-alive 경로를 가진다.

```cpp
extern void Irelia_KeepAlive();
Irelia_KeepAlive();
```

---

## 4. Q Dash 이전/이후

기존:

```txt
Scene_InGame::ApplyLocalPrediction
  -> if champ == IRELIA && slot == Q
  -> m_bDashActive / m_vDashStart / m_vDashEnd 직접 세팅
  -> IreliaFx::SpawnQTrail 직접 호출
```

변경:

```txt
ApplyLocalPrediction
  -> CSkillHookRegistry::Dispatch(def.onCastAcceptedHookId, ctx)
  -> Irelia::OnCastAccepted_Q
  -> Scene은 startPointDash callback으로 dash state만 실행
```

핵심 코드:

```cpp
void OnCastAccepted_Q(SkillHookContext& ctx)
{
    const Vec3 pStart =
        ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
    const Vec3 pTarget =
        ctx.pWorld->GetComponent<TransformComponent>(target).m_LocalPosition;

    Vec3 pEnd = pTarget;
    pEnd.y = pStart.y;

    const f32_t duration = ctx.getLocalDashDuration ? ctx.getLocalDashDuration() : 0.3f;
    ctx.startPointDash(pStart, pEnd, duration, target);
    IreliaFx::SpawnQTrail(*ctx.pWorld, ctx.casterEntity, duration);
}
```

Scene callback:

```cpp
ctx.startPointDash = [this](const Vec3& start, const Vec3& end,
    f32_t duration, EntityID target)
    {
        m_bDashActive = true;
        m_fDashElapsed = 0.f;
        m_fDashDuration = duration;
        m_vDashStart = start;
        m_vDashEnd = end;
        m_DashTargetEntity = target;
    };
```

---

## 5. 남은 작업

```txt
7D-6B Irelia tuning 분리
  - Dash duration / blade / beam / R wave / W release layer 값 이동

7D-6C Irelia E stage hook 분리
  - sword1 / sword2 id와 beam spawn state를 Scene 밖으로 빼야 함

7D-6D Irelia W stage hook 분리
  - W spin id와 W2 release layer 처리 이동

7D-6E Irelia R accepted hook 분리
  - UltWave spawn 파라미터를 Irelia tuning으로 이동한 뒤 분리
```
