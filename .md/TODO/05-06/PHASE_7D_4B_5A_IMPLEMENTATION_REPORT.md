# Phase 7D-4B~5A 구현 보고

작성일: 2026-05-06

## 1. 이번 반영 범위

```txt
7D-4B Kalista 튜닝 단일화
7D-4C Kalista 패시브 recoveryHook 이관
7D-5A Yasuo 튜닝 단일화
```

목표는 `Scene_InGame`에 남아 있던 champion-specific 값/분기를 한 번에 다 밀어내기보다, 다음 훅 분리에서 값 불일치가 생기지 않도록 먼저 챔피언 모듈 쪽 단일 저장소를 만드는 것이다.

---

## 2. Kalista 튜닝 단일화

추가 파일:

```txt
Client/Public/GameObject/Champion/Kalista/Kalista_Tuning.h
Client/Private/GameObject/Champion/Kalista/Kalista_Tuning.cpp
```

핵심 구조:

```cpp
namespace Kalista
{
    struct KalistaTuning
    {
        f32_t baSpeed = 30.f;
        f32_t baMaxDist = 8.f;
        f32_t baRadius = 0.6f;
        f32_t baDamage = 70.f;
        f32_t baFlySpearScale = 0.02f;
        f32_t baStuckSpearScale = 0.02f;

        f32_t qSpeed = 30.f;
        f32_t qMaxDist = 11.f;
        f32_t qRadius = 0.6f;
        f32_t qDamage = 70.f;
        f32_t qFlySpearScale = 0.015f;
        f32_t qStuckSpearScale = 0.008f;

        f32_t passiveDashDist = 2.f;
        f32_t passiveDashDuration = 0.1f;
        f32_t passiveDashAnimSpeed = 1.f;

        f32_t rendBaseDamage = 20.f;
        f32_t rendStackDamage = 30.f;
    };

    KalistaTuning& GetTuning();
}
```

`Scene_InGame` getter/setter는 이제 Scene 멤버가 아니라 `Kalista::GetTuning()`을 경유한다.

```cpp
f32_t GetKalistaQSpeed() const { return Kalista::GetTuning().qSpeed; }
void  SetKalistaQSpeed(f32_t v) { Kalista::GetTuning().qSpeed = (v < 5.f) ? 5.f : v; }

f32_t GetKalistaPassiveDashDist() const { return Kalista::GetTuning().passiveDashDist; }
void  SetKalistaPassiveDashDist(f32_t v) { Kalista::GetTuning().passiveDashDist = (v < 0.1f) ? 0.1f : v; }
```

`Kalista_Skills.cpp`도 로컬 상수 대신 같은 튜닝 저장소를 본다.

```cpp
const KalistaTuning& tuning = GetTuning();
SpawnSpear(*ctx.pWorld, ctx.pFxMeshRenderer,
    ctx.casterEntity, ctx.casterTeam,
    origin, forward,
    tuning.qSpeed, tuning.qMaxDist, tuning.qRadius, tuning.qDamage,
    tuning.qFlySpearScale, tuning.qStuckSpearScale);
```

---

## 3. Kalista 패시브 recoveryHook 이관

기존 구조:

```txt
Scene_InGame recoveryFrame 도달
  -> if champ == KALISTA
  -> pending dash dir 확인
  -> dash anim 재생
  -> StartKalistaPassiveDash()
```

변경 구조:

```txt
Scene 입력
  -> Kalista::QueuePassiveDash(dir)

Scene recoveryFrame 도달
  -> CSkillHookRegistry::Dispatch(recoveryHookId)
  -> Kalista::OnRecoveryFrame_PassiveDash(ctx)
  -> Scene callback으로 StartKalistaPassiveDash 호출
```

등록 코드:

```cpp
constexpr u32_t kKal_BA_Recovery = MakeHookId(eChampion::KALISTA, HookVariant::BA_Recovery);
constexpr u32_t kKal_Q_Recovery = MakeHookId(eChampion::KALISTA, HookVariant::Q_Recovery);

s.recoveryHookId = kKal_BA_Recovery;
s.recoveryHookId = kKal_Q_Recovery;

CSkillHookRegistry::Instance().Register(kKal_BA_Recovery, &Kalista::OnRecoveryFrame_PassiveDash);
CSkillHookRegistry::Instance().Register(kKal_Q_Recovery, &Kalista::OnRecoveryFrame_PassiveDash);
```

Scene은 칼리스타 모듈에 방향만 예약한다.

```cpp
const Vec3 dashDir{ dx / len, 0.f, dz / len };
Kalista::QueuePassiveDash(dashDir);
```

Hook 실행에 필요한 Scene 소유 동작은 `SkillHookContext` 콜백으로 넘긴다.

```cpp
ctx.pCasterRenderer = m_pPlayerRenderer;
ctx.fGlobalAnimSpeed = m_fGlobalAnimSpeed;
ctx.startLocalDash = [this](const Vec3& dir) { StartKalistaPassiveDash(dir); };
ctx.setLocalDashDuration = [this](f32_t duration) { SetKalistaPassiveDashDuration(duration); };
ctx.getLocalDashDuration = [this]() -> f32_t { return GetKalistaPassiveDashDuration(); };
ctx.setLocalActionAnimActive = [this](bool_t active) { m_bKalistaPassiveDashAnimActive = active; };
```

결과:

```txt
Scene_InGame 안의 champ == KALISTA recovery fallback 제거
m_bKalistaPassivePending 제거
m_bKalistaPassiveDashAnimFired 제거
m_vKalistaPassivePendingDir 제거
```

---

## 4. Yasuo 튜닝 단일화

추가 파일:

```txt
Client/Public/GameObject/Champion/Yasuo/Yasuo_Tuning.h
Client/Private/GameObject/Champion/Yasuo/Yasuo_Tuning.cpp
```

핵심 구조:

```cpp
namespace Yasuo
{
    struct YasuoTuning
    {
        f32_t qSpeed = 25.f;
        f32_t qLifetime = 0.5f;
        f32_t qTornadoSpeed = 18.f;
        f32_t qTornadoLifetime = 1.5f;
        f32_t qTornadoScale = 0.02f;
        f32_t wLifetime = 5.0f;
        f32_t wWidth = 6.0f;
        f32_t wHeight = 0.5f;
        f32_t eDashDuration = 0.25f;
        f32_t rSearchRadius = 8.f;
        f32_t rSequenceDuration = 1.0f;

        f32_t qDamage = 60.f;
        f32_t qTornadoDamage = 100.f;
        f32_t qTornadoStunSec = 1.0f;
        f32_t eDamage = 80.f;
        f32_t rPerHitDamage = 40.f;
        f32_t rHitInterval = 0.2f;
        Vec4 qTornadoColor{ 1.0f, 1.4f, 2.2f, 1.0f };
        f32_t wMeshScale = 0.01f;

        f32_t qHitDelay = 0.25f;
        f32_t eqDelay = 0.20f;
        f32_t eqRadius = 2.5f;
        f32_t eqDamage = 70.f;
    };
}
```

Scene의 Yasuo branch는 멤버 값 대신 getter를 호출한다.

```cpp
YasuoFx::SpawnQStraight(m_World, origin,
    cmd.direction, GetYasuoQSpeed(), GetYasuoQLifetime());

CPendingHitSystem::Schedule(m_World,
    m_PlayerEntity, m_PlayerTeam,
    cmd.direction,
    GetYasuoQHitDelay(),
    eYasuoProjectileKind::Wind,
    GetYasuoQSpeed(),
    GetYasuoQSpeed() * GetYasuoQLifetime(),
    0.8f,
    GetYasuoQDamage(), 0.f);
```

Scene에 남긴 Yasuo 값은 튜닝값이 아니라 런타임 상태다.

```txt
m_bYasuoDashActive
m_fYasuoDashElapsed
m_vYasuoDashStart / m_vYasuoDashEnd
m_YasuoDashTargetEntity
m_bYasuoRActive
m_fYasuoRElapsed
m_YasuoRTarget
m_iYasuoRHitsFired
```

---

## 5. 검증

```txt
PASS - Client Debug ClCompile
PASS - Scene_InGame legacy Kalista recovery fallback 제거 확인
PASS - Kalista/Yasuo 신규 튜닝 파일 vcxproj + filters 매핑 확인
```

다음 권장 순서:

```txt
7D-5B Yasuo keySwapHook 분리
7D-5C Yasuo Q/W accepted hook 분리
7D-5D Yasuo E/R accepted hook + Scene callback 축소
7D-6 Irelia Q/R fallback branch 모듈화
```
