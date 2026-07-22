# 2026-07-18 IRELIA E 유닛 확대 + 표식/Q 초기화 + 귀환 6초 계획서

```text
Session - 이렐리아 E 닫힘 타격을 미니언/정글까지 확대 + E/R 표식→Q 쿨타임 초기화 신설 + 귀환 6초 상향 (귀환 중단 로직은 기존 구현 확인, 코드 무변경)
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성, C8 검증이 병목
관련: 2026-07-18_IRELIA_VIEGO_EZREAL_STAGE_INPUT_REGRESSION_RECOVERY_PLAN/_RESULT (미커밋 병행 세션 — 충돌 회피 대상)
```

## 1. 결정 기록

```text
① 문제·제약: E 닫힘이 ForEach<ChampionComponent> 수동 루프(IreliaGameSim.cpp:636-669)라 미니언/정글 타격 0건. 표식·Q초기화 인프라 부재. 귀환 2.0s(EconomyGameplayDefs.json:81). 병행 세션이 CommandExecutor.cpp(+262줄)·SkillRegistry·JSON 2종을 미커밋 편집 중 — 그 파일들과의 충돌 최소화가 제약.
② 순진한 해법의 실패: (a) E 루프 옆에 Minion/Jungle ForEach 2개 추가 → 생존/타겟터블/팀 필터 3중 재작성, 기존 CollectEnemyMobileUnitsInSegment(리븐 W/R·요네·애니가 사용) 무시 = 제2 경로 금지 위반. (b) Q초기화를 CommandExecutor의 Riven recast 분기(2737행) 옆 인라인 → 병행 세션 최대 충돌 파일 편집.
③ 메커니즘: E 타깃 수집을 공용 CollectEnemyMobileUnitsInSegment로 교체(스턴은 CanReceiveCrowdControl이 미니언/정글 허용 확인됨). 표식은 LeeSinQMarkComponent 선례의 per-target IreliaMarkComponent — E 히트·R 웨이브 히트·R 벽 포착 시 부여, OnQ에서 소모→bQMarkCooldownReset 플래그, Phase_ExecuteCommands 직후 도는 IreliaGameSim::Tick(GameRoomTick.cpp:322)이 같은 틱에 slots[Q] 쿨다운 0. 귀환은 JSON 6.0 + kRecallDurationSec 6.f(클라 귀환 액션락이 이 상수를 직접 사용: Scene_InGameNetwork.cpp:432).
④ 대조: LoL 원본 = E/R 표식을 Q가 소모하며 쿨다운 환급. 리븐 Q recast는 CommandExecutor 인라인 0 처리지만, 여기선 챔피언 파일 격리(충돌 회피)를 위해 Tick 후처리를 선택 — 틱 순서(커맨드→챔피언 Tick→스냅샷)가 같은 틱 반영을 보장. 귀환 중단(이동:CommandExecutor.cpp:2114·피격:DamagePipeline.cpp:438-440·스턴/에어본:StatusEffectSystem.cpp:181-192·CC상태:RecallSystem.cpp:89-95)은 전부 기존 구현 — 신규 코드 없음.
⑤ 대가: Q 쿨다운 쓰기 지점이 CommandExecutor(시작)와 IreliaGameSim::Tick(리셋) 두 곳으로 갈라져 발견성이 낮다 — CommandExecutor가 안정화되면 인라인 통합이 옳아진다. E가 공용 필터를 얻으며 사망/언타겟터블 대상 스턴 엣지가 사라짐(의도적 정합). 표식 시각 표시(target_mark.wfx 재생)는 이번 슬라이스 제외 — 서버 cue 이벤트 신설이 필요해 후속.
```

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/IreliaSimComponent.h

`bool_t bRWallActive = false;` 멤버 (private 구조체 말미), 기존 코드:

```cpp
	bool_t bDashActive = false;
	bool_t bRWaveActive = false;
	bool_t bRWallActive = false;
};
```

아래로 교체:

```cpp
	bool_t bDashActive = false;
	bool_t bRWaveActive = false;
	bool_t bRWallActive = false;
	bool_t bQMarkCooldownReset = false;
};
```

파일 말미 `static_assert(std::is_trivially_copyable_v<IreliaSimComponent>);` 기존 코드 아래에 추가:

```cpp
struct IreliaMarkComponent
{
	EntityID sourceEntity = NULL_ENTITY;
	f32_t fRemainingSec = 0.f;
};

static_assert(std::is_trivially_copyable_v<IreliaMarkComponent>);
```

### 2-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Irelia/IreliaGameSim.cpp

(a) 익명 네임스페이스 `GetIreliaState` 함수(93-99행) 기존 코드 아래에 추가:

```cpp
    void ApplyIreliaMark(CWorld& world, const TickContext& tc, EntityID source, EntityID target, eSkillSlot slot)
    {
        if (source == NULL_ENTITY || target == NULL_ENTITY || !world.IsAlive(target))
            return;

        const f32_t markDurationSec = ResolveIreliaSkillEffectParam(
            world, tc, source, slot, eSkillEffectParamId::MarkDurationSec);
        if (markDurationSec <= 0.f)
            return;

        IreliaMarkComponent mark{};
        mark.sourceEntity = source;
        mark.fRemainingSec = markDurationSec;

        if (world.HasComponent<IreliaMarkComponent>(target))
            world.GetComponent<IreliaMarkComponent>(target) = mark;
        else
            world.AddComponent<IreliaMarkComponent>(target, mark);
    }
```

(b) `OnE` 수동 세그먼트 루프 — 기존 코드(624-669행, `world.ForEach<ChampionComponent, ...>` 블록 포함 전체):

```cpp
        const f32_t dx = b.x - a.x;
        const f32_t dz = b.z - a.z;
        const f32_t segLenSq = dx * dx + dz * dz + 0.000001f;
        // ... (beamRadius/stunSec/eBaseDamage/eDamagePerRank 4개 파라미터 해석은 유지) ...
        world.ForEach<ChampionComponent, TransformComponent>( /* 수동 투영 + 팀 필터 + ApplyStun + EnqueuePhysicalDamage */ );
```

아래로 교체(파라미터 해석 4줄은 그대로 두고, dx/dz/segLenSq 3줄과 ForEach 블록만 교체):

```cpp
        const std::vector<EntityID> targets =
            GameplayStateQuery::CollectEnemyMobileUnitsInSegment(
                world, ctx.casterEntity, a, b, beamRadius);
        for (EntityID target : targets)
        {
            GameplayStatus::ApplyStun(
                world,
                *ctx.pTickCtx,
                target,
                ctx.casterEntity,
                eChampion::IRELIA,
                eSkillSlot::E,
                stunSec);

            EnqueuePhysicalDamage(
                world,
                ctx.casterEntity,
                target,
                ctx.casterTeam,
                eBaseDamage + eDamagePerRank * static_cast<f32_t>(ctx.skillRank),
                static_cast<u16_t>((static_cast<u32_t>(eChampion::IRELIA) << 8) | 3u),
                ctx.skillRank);

            ApplyIreliaMark(world, *ctx.pTickCtx, ctx.casterEntity, target, eSkillSlot::E);
        }
```

(c) `OnQ` — 기존 코드 `ClearMove(world, ctx.casterEntity);`(475행) 아래에 추가:

```cpp
        if (world.HasComponent<IreliaMarkComponent>(cmd.targetEntity))
        {
            const IreliaMarkComponent& mark =
                world.GetComponent<IreliaMarkComponent>(cmd.targetEntity);
            if (mark.sourceEntity == ctx.casterEntity && mark.fRemainingSec > 0.f)
            {
                world.RemoveComponent<IreliaMarkComponent>(cmd.targetEntity);
                state.bQMarkCooldownReset = true;
            }
        }
```

(d) `TickRWave` 웨이브 히트 분기 — 기존 코드 `StartRWall(world, tc, caster, state, false, rWallDurationSec);`(382행) 바로 위 `EmitIreliaREffect(... kIreliaREffectStageHit ...)` 호출 아래에 추가:

```cpp
                ApplyIreliaMark(world, tc, caster, hitTarget, eSkillSlot::R);
```

(e) `TickRWave` 벽 포착 람다 — 기존 코드 `EmitIreliaREffect(... kIreliaREffectStageWallMark ...)` 호출(432-440행) 아래에 추가:

```cpp
                    ApplyIreliaMark(world, tc, caster, target, eSkillSlot::R);
```

(f) `IreliaGameSim::Tick` per-entity 람다 진입부 — 기존 코드 `[&](EntityID entity, IreliaSimComponent& state, TransformComponent& transform)` `{`(708행) 아래에 추가:

```cpp
                    if (state.bQMarkCooldownReset)
                    {
                        state.bQMarkCooldownReset = false;
                        if (world.HasComponent<SkillStateComponent>(entity))
                        {
                            auto& qSlot = world.GetComponent<SkillStateComponent>(entity)
                                .slots[static_cast<u8_t>(eSkillSlot::Q)];
                            qSlot.cooldownRemaining = 0.f;
                            qSlot.cooldownDuration = 0.f;
                        }
                    }
```

(g) `IreliaGameSim::Tick` — 기존 코드 per-entity `ForEach` 닫힘 `}));`(783행) 아래에 추가 (LeeSinGameSim.cpp:578-589 만료 패턴 동형):

```cpp
        std::vector<EntityID> expiredMarks;
        world.ForEach<IreliaMarkComponent>(
            std::function<void(EntityID, IreliaMarkComponent&)>(
                [&](EntityID entity, IreliaMarkComponent& mark)
                {
                    mark.fRemainingSec = std::max(0.f, mark.fRemainingSec - tc.fDt);
                    if (mark.fRemainingSec <= 0.f || !world.IsAlive(mark.sourceEntity))
                        expiredMarks.push_back(entity);
                }));

        for (EntityID entity : expiredMarks)
            world.RemoveComponent<IreliaMarkComponent>(entity);
```

### 2-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Core/Checkpoint/WorldKeyframe.cpp

기존 코드(353행):

```cpp
		reg.Register<IreliaSimComponent>("IreliaSimComponent");
```

아래에 추가:

```cpp
		reg.Register<IreliaMarkComponent>("IreliaMarkComponent");
```

(include는 기존 31행 `IreliaSimComponent.h`가 담당 — 추가 불필요)

### 2-4. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json

`skill.irelia.e` params 기존 코드(1268-1273행):

```json
      "params": {
        "baseDamage": 70.0,
        "damagePerRank": 30.0,
        "radius": 1.5,
        "stunDurationSec": 0.75
      },
```

아래로 교체 (markDurationSec 5.0, 알파벳순 유지):

```json
      "params": {
        "baseDamage": 70.0,
        "damagePerRank": 30.0,
        "markDurationSec": 5.0,
        "radius": 1.5,
        "stunDurationSec": 0.75
      },
```

`skill.irelia.r` params 기존 코드(1378-1388행) — `"effectDurationSec": 2.5,` 아래에 `"markDurationSec": 5.0,` 1줄 추가 (알파벳순).

### 2-5. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/EconomyGameplayDefs.json

기존 코드(79-82행):

```json
  "timers": {
    "assistCreditWindowSec": 10.0,
    "recallDurationSec": 2.0
  }
```

아래로 교체:

```json
  "timers": {
    "assistCreditWindowSec": 10.0,
    "recallDurationSec": 6.0
  }
```

### 2-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/RecallComponent.h

기존 코드(7행):

```cpp
inline constexpr f32_t kRecallDurationSec = 2.f;
```

아래로 교체 (클라 귀환 액션락 Scene_InGameNetwork.cpp:432가 이 상수 직접 사용 — JSON과 동기 필수):

```cpp
inline constexpr f32_t kRecallDurationSec = 6.f;
```

### 2-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/EconomyGameplayDef.h

기존 코드(63행):

```cpp
    f32_t recallDurationSec = 2.f;
```

아래로 교체:

```cpp
    f32_t recallDurationSec = 6.f;
```

### 재생성 (수기 편집 금지 산출물)

```text
python Tools/LoLData/Build-LoLDefinitionPack.py
```

갱신 산출물: Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp, Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json(미러), ChampionGameplayDefs.json(미러), Data/LoL/SharedContract/DefinitionManifest.json, .md/TODO/06-22/LOL_DEFINITION_PACK_PARITY.json — buildHash 변동. champions.json 무변경이므로 Generator A(build_champion_game_data.py)와 클라 SkillRegistry 감사(17/85/13/16/3)는 비접촉.

## 3. 검증

```text
예측:
- Build-LoLDefinitionPack.py 후 --check 클린, 생성 cpp에 recallDurationSec = 6.f + irelia e/r MarkDurationSec 5.f 등장.
- Winters.sln Debug x64 빌드 PASS (신규 파일 없음 → vcxproj 무변경).
- SimLab 결정성 exit 0. Irelia 관련 프로브/골든 해시는 E 타깃 확대·마크 컴포넌트로 변동 가능 — 변동 시 의도 변경으로 판정하고 명시 기록 (게이트: SimLab 프로브 실패 = 회귀 신호).
- 인게임(서버+클라): E 닫힘이 미니언 웨이브/정글 캠프에 스턴 0.75s+데미지(70+30·rank), 정글 어그로 발동(NotifyJungleAggroFromChampionDamage 경유 자동). E/R 맞은 대상에 Q → HUD Q 쿨다운 즉시 0. 귀환 채널 6초 후 우물 텔레포트, 채널 중 우클릭 이동·피격 시 즉시 끊김(기존 로직).
- Bot AI 경계: Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다 — 본 변경은 명령 실행/시뮬 계층만 수정, 봇 경로 비접촉.
- 이 변경이 깨뜨릴 수 있는 것: E가 미니언을 우선 스턴해 기존 챔피언 낚시 감각 변화(밸런스), 마크 컴포넌트 추가로 키프레임 레지스트리 완전성 검사 — 등록 누락 시 기계검사 FAIL이 잡는다.

검증 명령:
- python Tools/LoLData/Build-LoLDefinitionPack.py && python Tools/LoLData/Build-LoLDefinitionPack.py --check
- msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m:1  (schema-owning aggregate → /m:1 gotcha)
- Tools/Bin/SimLab.exe 600 1234  (exit 0 = 결정적)
- 서버+클라 실행: E→미니언/정글 스턴, 마크 Q 리셋, 귀환 6s/이동·피격 중단 육안 게이트

미검증:
- 표식 시각 표시(target_mark.wfx)는 서버 cue 미신설로 이번 슬라이스에 없음 — 후속.
- 로컬(비네트워크) 스모크 경로의 Q 예측 쿨다운은 서버 리셋을 모름 — 네트워크 권위 모드가 기준.

확인 필요:
- SimLab 골든 해시 변동 폭 (실행 후 판정)
```

## 다음 슬라이스

- 표식 시각화: EmitIreliaREffect 계열 replicated cue로 target_mark.wfx 재생 + 소모 시 제거.
- Q 처치 리셋(LoL 본가 규칙): DamageQueueSystem.cpp:559 kill 분기에 Irelia 훅 — 사용자 요청 시.

---

## 4. 후속 세션 — Q 도착 피해 + 처치 시 0.1초 재사용

```text
Session - 이렐리아 Q 피해를 시전 승인 시점에서 대시 도착 시점으로 옮기고, 챔피언·미니언·정글 처치 시 Q 쿨다운/입력 잠금을 0.1초로 제한한다
좌표: 신규 좌표 후보 · 축: C5 이산화와 오차, C7 권위와 정합성
관련: 이 문서 §1~§3 및 동명 RESULT의 기존 E/R 표식 Q 초기화
천장/바닥 예산: 천장 30% = 실제 연속 Q 처치 조작감/F5 확인, 바닥 70% = 30Hz 도착·피해·처치·재시전 실행 probe와 Debug/Release 빌드
```

### 4-1. 결정 기록

```text
① 문제·제약: 현재 OnQ가 승인 틱에 DamageRequest를 즉시 enqueue하고 대시는 0.25초 동안 뒤늦게 이동한다(IreliaGameSim.cpp:475-536). 목표는 접촉 전 피해 0건, 도착 틱 피해 1건, Q로 챔피언/미니언/정글을 처치하면 같은 틱 cooldownRemaining=0.1이며 30Hz 3틱 뒤 재시전 가능하게 하는 것.
② 순진한 해법의 실패: DamageQueueSystem의 모든 kill 분기에서 skillId만 보고 0.1을 쓰면 구조물·아이템 부가 피해·다른 이렐리아 피해까지 오인하고, 기존 0.36초 ActionState lock이 남아 cooldown만 0.1로 바꿔도 실제 입력은 한 틱 더 막힐 수 있다.
③ 메커니즘: OnQ는 대시 target/rank만 저장한다. IreliaGameSim::Tick이 정상 도착한 틱에 표식 소모와 Q DamageRequest를 enqueue하고, DamageQueue의 데이터 해석·ApplyDamageRequest 직후 IreliaGameSim::OnDamageResolved가 bKilled + Irelia Skill Q + mobile target을 확인해 cooldown과 해당 Q action lock을 0.1초(30Hz 3틱) 상한으로 줄인다.
④ 대조: 기존 E/R 표식은 도착 시 소모 후 0초 상한을 적용해 더 강한 기존 규칙을 보존한다. blocked/cancelled dash와 도착 전에 다른 공격으로 죽은 대상은 Q 피해·처치 리셋이 없다. Structure는 mobile target이 아니므로 제외한다.
⑤ 대가: 피해가 승인 틱에서 최대 0.25초 뒤로 이동해 Q 중 대상이 먼저 죽거나 대시가 막히면 피해가 취소된다. 이는 접촉 기반 규칙의 의도된 대가이며, 향후 moving target 추적 dash가 필요해지면 고정 dashEndPos 정책을 별도 슬라이스로 바꿔야 한다.
```

### 4-2. 반영해야 하는 코드

#### 4-2-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/IreliaSimComponent.h

기존 코드:

```cpp
	u8_t rRank = 0;
	u8_t rHitTargetCount = 0;
	u8_t rWallTargetCount = 0;
	bool_t bHasBlade1 = false;
	bool_t bHasBlade2 = false;
	bool_t bDashActive = false;
	bool_t bRWaveActive = false;
	bool_t bRWallActive = false;
	bool_t bQMarkCooldownReset = false;
```

아래로 교체:

```cpp
	u8_t qRank = 0;
	u8_t rRank = 0;
	u8_t rHitTargetCount = 0;
	u8_t rWallTargetCount = 0;
	bool_t bHasBlade1 = false;
	bool_t bHasBlade2 = false;
	bool_t bDashActive = false;
	bool_t bRWaveActive = false;
	bool_t bRWallActive = false;
```

#### 4-2-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Irelia/IreliaGameSim.h

기존 코드:

```cpp
class CWorld;
struct TickContext;
```

아래로 교체:

```cpp
class CWorld;
struct DamageRequest;
struct DamageResult;
struct TickContext;
```

기존 코드:

```cpp
	void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
```

아래로 교체:

```cpp
	void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
	void OnDamageResolved(
		CWorld& world,
		const TickContext& tc,
		const DamageRequest& request,
		const DamageResult& result);
```

#### 4-2-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Irelia/IreliaGameSim.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/IreliaSimComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/IreliaSimComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
```

익명 네임스페이스의 effect stage 상수 기존 코드:

```cpp
    constexpr u8_t kIreliaREffectStageHit = 2u;
    constexpr u8_t kIreliaREffectStageWall = 3u;
    constexpr u8_t kIreliaREffectStageWallMark = 4u;
```

아래에 추가:

```cpp
    constexpr f32_t kIreliaQKillResetCooldownSec = 0.1f;
```

`GetIreliaState` 기존 코드 아래에 추가:

```cpp
    void LimitIreliaQRecovery(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        f32_t cooldownSec)
    {
        const f32_t clampedCooldown = std::max(0.f, cooldownSec);
        if (world.HasComponent<SkillStateComponent>(caster))
        {
            auto& qSlot = world.GetComponent<SkillStateComponent>(caster)
                .slots[static_cast<u8_t>(eSkillSlot::Q)];
            if (clampedCooldown <= 0.f)
            {
                qSlot.cooldownRemaining = 0.f;
                qSlot.cooldownDuration = 0.f;
            }
            else if (qSlot.cooldownRemaining > clampedCooldown)
            {
                qSlot.cooldownRemaining = clampedCooldown;
                qSlot.cooldownDuration = clampedCooldown;
            }
        }

        if (world.HasComponent<ActionStateComponent>(caster))
        {
            auto& action = world.GetComponent<ActionStateComponent>(caster);
            if (action.sourceChampion == eChampion::IRELIA &&
                action.sourceSlot == static_cast<u8_t>(eSkillSlot::Q))
            {
                const u64_t recoveryTicks = static_cast<u64_t>(std::ceil(
                    static_cast<f64_t>(clampedCooldown) *
                    static_cast<f64_t>(DeterministicTime::kTicksPerSecond)));
                action.lockEndTick = std::min(
                    action.lockEndTick,
                    tc.tickIndex + recoveryTicks);
            }
        }
    }
```

`ApplyIreliaMark` 함수 아래에 추가:

```cpp
    void ResolveIreliaQImpact(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target,
        u8_t rank)
    {
        if (target == NULL_ENTITY || !world.IsAlive(target) ||
            !world.HasComponent<HealthComponent>(target))
        {
            return;
        }

        if (world.HasComponent<IreliaMarkComponent>(target))
        {
            const IreliaMarkComponent& mark =
                world.GetComponent<IreliaMarkComponent>(target);
            if (mark.sourceEntity == caster && mark.fRemainingSec > 0.f)
            {
                world.RemoveComponent<IreliaMarkComponent>(target);
                LimitIreliaQRecovery(world, tc, caster, 0.f);
            }
        }

        const eTeam casterTeam = world.HasComponent<ChampionComponent>(caster)
            ? world.GetComponent<ChampionComponent>(caster).team
            : eTeam::Neutral;
        const f32_t qBaseDamage = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::Q, eSkillEffectParamId::BaseDamage);
        const f32_t qDamagePerRank = ResolveIreliaSkillEffectParam(
            world, tc, caster, eSkillSlot::Q, eSkillEffectParamId::DamagePerRank);

        EnqueuePhysicalDamage(
            world,
            caster,
            target,
            casterTeam,
            qBaseDamage + qDamagePerRank * static_cast<f32_t>(rank),
            static_cast<u16_t>((static_cast<u32_t>(eChampion::IRELIA) << 8) | 1u),
            rank);
    }
```

`OnQ`에서 기존 코드:

```cpp
        state.dashDurationSec = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::Q, eSkillEffectParamId::DashDurationSec);
        state.dashTarget = cmd.targetEntity;
        state.bDashActive = true;

        ClearMove(world, ctx.casterEntity);

        if (world.HasComponent<IreliaMarkComponent>(cmd.targetEntity))
        {
            const IreliaMarkComponent& mark =
                world.GetComponent<IreliaMarkComponent>(cmd.targetEntity);
            if (mark.sourceEntity == ctx.casterEntity && mark.fRemainingSec > 0.f)
            {
                world.RemoveComponent<IreliaMarkComponent>(cmd.targetEntity);
                state.bQMarkCooldownReset = true;
            }
        }

        const f32_t qBaseDamage = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::Q, eSkillEffectParamId::BaseDamage);
        const f32_t qDamagePerRank = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::Q, eSkillEffectParamId::DamagePerRank);

        EnqueuePhysicalDamage(
            world,
            ctx.casterEntity,
            cmd.targetEntity,
            ctx.casterTeam,
            qBaseDamage + qDamagePerRank * static_cast<f32_t>(ctx.skillRank),
            static_cast<u16_t>((static_cast<u32_t>(eChampion::IRELIA) << 8) | 1u),
            ctx.skillRank);
```

아래로 교체:

```cpp
        state.dashDurationSec = ResolveIreliaSkillEffectParam(
            ctx, eSkillSlot::Q, eSkillEffectParamId::DashDurationSec);
        state.dashTarget = cmd.targetEntity;
        state.qRank = ctx.skillRank;
        state.bDashActive = true;

        ClearMove(world, ctx.casterEntity);
```

`IreliaGameSim::Tick` per-entity 람다 진입부에서 기존 코드 삭제:

```cpp
                    if (state.bQMarkCooldownReset)
                    {
                        state.bQMarkCooldownReset = false;
                        if (world.HasComponent<SkillStateComponent>(entity))
                        {
                            auto& qSlot = world.GetComponent<SkillStateComponent>(entity)
                                .slots[static_cast<u8_t>(eSkillSlot::Q)];
                            qSlot.cooldownRemaining = 0.f;
                            qSlot.cooldownDuration = 0.f;
                        }
                    }
```

Q dash 완료 기존 코드:

```cpp
                        if (t >= 1.f || bDashBlocked)
                        {
                            SnapDashArrivalToWalkable(world, tc, entity, state.dashStartPos);
                            state.bDashActive = false;
                            state.dashElapsedSec = 0.f;
                            state.dashDurationSec = 0.f;
                            state.dashTarget = NULL_ENTITY;
                        }
```

아래로 교체:

```cpp
                        if (t >= 1.f || bDashBlocked)
                        {
                            SnapDashArrivalToWalkable(world, tc, entity, state.dashStartPos);
                            const EntityID impactTarget = state.dashTarget;
                            const u8_t impactRank = state.qRank;
                            state.bDashActive = false;
                            state.dashElapsedSec = 0.f;
                            state.dashDurationSec = 0.f;
                            state.dashTarget = NULL_ENTITY;
                            state.qRank = 0u;

                            if (t >= 1.f && !bDashBlocked)
                            {
                                ResolveIreliaQImpact(
                                    world,
                                    tc,
                                    entity,
                                    impactTarget,
                                    impactRank);
                            }
                        }
```

`IreliaGameSim::Tick` 함수 아래에 추가:

```cpp
    void OnDamageResolved(
        CWorld& world,
        const TickContext& tc,
        const DamageRequest& request,
        const DamageResult& result)
    {
        if (!result.bKilled || request.source == NULL_ENTITY ||
            request.target == NULL_ENTITY ||
            request.eSourceKind != eDamageSourceKind::Skill ||
            request.iSourceSlot != static_cast<u8_t>(eSkillSlot::Q) ||
            !world.HasComponent<ChampionComponent>(request.source) ||
            world.GetComponent<ChampionComponent>(request.source).id != eChampion::IRELIA ||
            !GameplayStateQuery::IsMobileCombatUnit(world, request.target))
        {
            return;
        }

        LimitIreliaQRecovery(
            world,
            tc,
            request.source,
            kIreliaQKillResetCooldownSec);
    }
```

#### 4-2-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"
#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"
#include "Shared/GameSim/Champions/Irelia/IreliaGameSim.h"
#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"
```

기존 코드:

```cpp
        FioraGameSim::OnDamageResolved(world, tc, request, result);
        DamageResult yoneStorageResult = result;
```

아래로 교체:

```cpp
        FioraGameSim::OnDamageResolved(world, tc, request, result);
        IreliaGameSim::OnDamageResolved(world, tc, request, result);
        DamageResult yoneStorageResult = result;
```

#### 4-2-5. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`RunIreliaWReleaseRegressionProbe` 함수 바로 아래에 완전한 `RunIreliaQImpactKillResetProbe`를 추가한다. 이 probe는 실제 `CDefaultCommandExecutor`, `IreliaGameSim::Tick`, `CDamageQueueSystem`, `CSkillCooldownSystem`을 순서대로 실행한다.

```cpp
    bool_t RunIreliaQImpactKillResetProbe()
    {
        const auto RunCase = [](
            GameplayStateQuery::eGameplayTargetKind targetKind,
            bool_t bLethal,
            bool_t bMarked,
            bool_t bExpectReset,
            bool_t bVerifyRecast)
        {
            CWorld world;
            DeterministicRng rng(2026071802ull + static_cast<u64_t>(targetKind));
            EntityIdMap entityMap;
            FlatWalkable walkable;
            auto executor = CDefaultCommandExecutor::Create();
            const EntityID irelia = SpawnChampion(
                world, entityMap, eChampion::IRELIA,
                static_cast<u8_t>(eTeam::Blue), 0u);
            const EntityID target = targetKind ==
                    GameplayStateQuery::eGameplayTargetKind::Champion
                ? SpawnChampion(
                    world, entityMap, eChampion::ASHE,
                    static_cast<u8_t>(eTeam::Red), 1u)
                : SpawnStatusProbeTarget(
                    world,
                    targetKind,
                    targetKind == GameplayStateQuery::eGameplayTargetKind::JungleMonster
                        ? eTeam::Neutral
                        : eTeam::Red,
                    Vec3{ 2.f, 0.f, 0.f });
            if (irelia == NULL_ENTITY || target == NULL_ENTITY)
                return false;

            world.GetComponent<TransformComponent>(irelia).SetPosition(Vec3{});
            world.GetComponent<TransformComponent>(target).SetPosition(Vec3{ 2.f, 0.f, 0.f });
            auto& targetHealth = world.GetComponent<HealthComponent>(target);
            targetHealth.fMaximum = bLethal ? 1.f : 10000.f;
            targetHealth.fCurrent = targetHealth.fMaximum;
            targetHealth.bIsDead = false;
            if (bMarked)
                world.AddComponent<IreliaMarkComponent>(
                    target, IreliaMarkComponent{ irelia, 5.f });

            const u8_t qSlot = static_cast<u8_t>(eSkillSlot::Q);
            world.GetComponent<SkillRankComponent>(irelia).ranks[qSlot] = 1u;
            GameCommand q{};
            q.kind = eCommandKind::CastSkill;
            q.issuerEntity = irelia;
            q.targetEntity = target;
            q.slot = qSlot;
            q.sequenceNum = 1u;

            TickContext castTick = MakeProbeTickContext(1u, rng, entityMap, walkable);
            if (executor->ExecuteCommand(world, castTick, q).state !=
                eCommandExecutionState::Accepted)
            {
                return false;
            }

            const f32_t healthBeforeArrival = targetHealth.fCurrent;
            IreliaGameSim::Tick(world, castTick);
            CDamageQueueSystem::Execute(world, castTick);
            if (targetHealth.fCurrent != healthBeforeArrival)
                return false;

            u64_t impactTick = 0u;
            for (u64_t tick = 2u; tick <= 20u; ++tick)
            {
                TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
                CSkillCooldownSystem::Execute(world, tc);
                const bool_t bWasDashing =
                    world.GetComponent<IreliaSimComponent>(irelia).bDashActive;
                IreliaGameSim::Tick(world, tc);
                CDamageQueueSystem::Execute(world, tc);
                if (bWasDashing &&
                    !world.GetComponent<IreliaSimComponent>(irelia).bDashActive)
                {
                    impactTick = tick;
                    break;
                }
                if (targetHealth.fCurrent != healthBeforeArrival)
                    return false;
            }

            const auto& qState = world.GetComponent<SkillStateComponent>(irelia).slots[qSlot];
            const bool_t bKilled = targetHealth.bIsDead && targetHealth.fCurrent <= 0.f;
            if (impactTick == 0u || bKilled != bLethal)
                return false;
            if (bMarked)
            {
                if (world.HasComponent<IreliaMarkComponent>(target) ||
                    qState.cooldownRemaining != 0.f)
                {
                    return false;
                }
            }
            else if (bExpectReset)
            {
                if (std::fabs(qState.cooldownRemaining - 0.1f) > 0.0001f)
                    return false;
            }
            else if (qState.cooldownRemaining <= 0.1f)
            {
                return false;
            }

            if (bVerifyRecast)
            {
                const EntityID followup = SpawnStatusProbeTarget(
                    world,
                    GameplayStateQuery::eGameplayTargetKind::MinionOrSummon,
                    eTeam::Red,
                    Vec3{ 4.f, 0.f, 0.f });
                GameCommand followupQ = q;
                followupQ.targetEntity = followup;
                followupQ.sequenceNum = 2u;
                bool_t bAcceptedAtThreeTicks = false;
                for (u64_t tick = impactTick + 1u; tick <= impactTick + 3u; ++tick)
                {
                    TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
                    CSkillCooldownSystem::Execute(world, tc);
                    if (tick == impactTick + 3u)
                    {
                        bAcceptedAtThreeTicks =
                            executor->ExecuteCommand(world, tc, followupQ).state ==
                            eCommandExecutionState::Accepted;
                    }
                }
                if (!bAcceptedAtThreeTicks)
                    return false;
            }

            return true;
        };

        const bool_t bPass =
            RunCase(GameplayStateQuery::eGameplayTargetKind::Champion, true, false, true, true) &&
            RunCase(GameplayStateQuery::eGameplayTargetKind::MinionOrSummon, true, false, true, false) &&
            RunCase(GameplayStateQuery::eGameplayTargetKind::JungleMonster, true, false, true, false) &&
            RunCase(GameplayStateQuery::eGameplayTargetKind::MinionOrSummon, false, false, false, false) &&
            RunCase(GameplayStateQuery::eGameplayTargetKind::Structure, true, false, false, false) &&
            RunCase(GameplayStateQuery::eGameplayTargetKind::MinionOrSummon, true, true, true, false);
        std::printf(
            "[SimLab][IreliaQReset] %s: arrival damage, mobile-unit kill reset, 3-tick recast, exclusions\n",
            bPass ? "PASS" : "FAIL");
        return bPass;
    }
```

`main`의 `--f4-balance-only` 분기 아래에 추가:

```cpp
    if (argc > 1 && std::strcmp(argv[1], "--irelia-q-only") == 0)
    {
        std::printf("[SimLab] Irelia Q impact/kill-reset probe only\n");
        RegisterAllChampionHooks();
        const bool_t bPass = RunIreliaQImpactKillResetProbe();
        std::printf("[SimLab] %s\n", bPass ? "PASS" : "FAIL");
        return bPass ? 0 : 1;
    }
```

전체 SimLab probe 목록과 최종 `bPass` 식에는 `RunIreliaQImpactKillResetProbe()` 결과를 각각 1회 추가한다.

### 4-3. 검증

```text
예측:
- 현재 실측: Q는 명령 승인 틱에 피해가 들어간다. 변경 후 cast tick~도착 전까지 HP 불변, 정상 도착 틱에만 DamageRequest 1회 적용된다.
- Q가 적 Champion / MinionOrSummon / JungleMonster를 죽이면 DamageResult.bKilled가 확정된 같은 틱에 Q cooldownRemaining/duration=0.1, 해당 Irelia Q action lockEndTick도 impact+3 tick 이하가 되어 정확히 3틱 뒤 다음 Q 명령이 Accepted다.
- 비치명타, Structure 처치, blocked/cancelled dash는 kill reset 없음. E/R 표식 대상은 도착 시 표식이 소모되고 기존대로 0초가 우선한다.
- Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다. Bot이 Q를 쓰더라도 동일 Command→Irelia GameSim→DamageQueue 경로만 탄다.
- 기존 이렐리아 W charge, 전체 damage/item/score/Keyframe 회귀는 full SimLab이 잡는다. 신규 component 파일 추가가 없어 vcxproj 변경은 없다.

검증 명령:
- & 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Tools/SimLab/SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
- & 'Tools/Bin/Debug/SimLab.exe' --irelia-q-only
- & 'Tools/Bin/Debug/SimLab.exe' 1 42
- & 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Shared/GameSim/Include/GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
- & 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
- & 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
- & 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Server/Include/Server.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
- & 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client/Include/Client.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
- git diff --check

미검증:
- F5 실제 화면에서 moving target Q 궤적과 3틱 재입력 손맛은 자동 probe 밖이며 천장 30% 수동 확인 항목이다.

확인 필요:
- 없음. moving target 추적은 현재 고정 dashEndPos 정책을 유지하며 본 슬라이스 범위 밖이다.
```

## 서브 에이전트 비평 — 후속 세션

### 1차 비평 결과와 disposition

```text
- P0 수용: SimLab 신규 probe가 IreliaSimComponent를 직접 읽으므로 명시 include를 계획에 추가한다.
- P1 수용: impact+1/+2 Q는 Cooldown으로 거절, impact+3 Q는 Accepted를 실제 CommandExecutor로 검증한다.
- P1 수용: E/R 표식은 cast 승인 때가 아니라 Q DamageResult.bApplied가 확정된 뒤 소모한다. 무적/피해 거절이면 표식을 보존한다.
- P1 수용: 정상 Q 피해 이벤트는 정확히 1건, blocked/CC-cancelled Q는 피해·표식 소모·초기화가 0건임을 검증한다.
- P2 수용: champion/slot만 보지 않고 skillId == (IRELIA << 8) | Q까지 일치시킨다.
- P1 수용: 대상 파일의 기존 dirty diff를 보존하고, anchor 재확인 + focused diff로 이번 줄만 분리한다.
```

## 5. 사용자 교정 — 고정 0.25초 제거, 실제 접촉 즉시 Q 종료

이 절은 §4의 고정 `dashDurationSec` 이동안과 `impact+3 action lock` 안을 대체한다. 처치 후 0.1초는 Q 쿨다운에만 적용하며, 행동 락은 처치 여부와 무관하게 실제 접촉 틱에 즉시 끝낸다. 따라서 다음 행동 입력은 접촉 다음 서버 틱부터 받을 수 있고, 연속 Q만 30Hz 기준 3틱 쿨다운이 제한한다.

### 5-1. 최종 결정 기록

```text
① 문제·제약: 동일한 0.25초 보간과 0.36초 command lock 때문에 가까운 대상도 대시/Spell1 애니메이션 상태를 끝까지 유지한다. ResolveQDashEndPos는 target distance가 gap(1.35)보다 짧으면 caster 뒤쪽을 종점으로 계산하는 역방향 부작용도 있다.
② 실패하는 해법: 데이터의 0.25만 더 작게 만들면 원거리 Q가 순간이동하고 근거리/원거리 도착이 여전히 같은 시간이 된다. 처치 때만 행동 락을 0.1로 줄이면 비치명 Q 뒤 기본 공격도 늦게 받는다. 클라이언트 타이머만 바꾸면 서버 판정과 시각이 다시 어긋난다.
③ 메커니즘: Q speed=14.0(월드 단위; LoL 1400/100) + 현재 moveSpeed로 매 tick 일정 거리만 이동한다. 살아 있는 target 위치와 gap-clamp 종점을 매 tick 다시 계산한다. 이번 step으로 종점에 닿으면 그 틱에 대시를 종료하고 Q ActionState lock을 현재 tick으로 줄인 뒤 피해를 1회 enqueue한다. DamageQueue가 같은 tick에 피해 적용/표식/처치 초기화를 확정한다. GameRoom의 명령 실행이 Irelia Tick보다 앞이므로 다음 실제 명령은 접촉 다음 tick(최대 33.3ms)부터 Accepted다.
④ 대조: 이미 gap 안쪽인 대상은 역방향 이동 없이 승인 tick의 Irelia Tick에서 즉시 접촉한다. 정상 접촉은 다음 기본 공격/행동을 즉시 받을 수 있다. Q로 챔피언·미니언·정글을 처치하면 Q 쿨다운만 0.1초로 제한해 impact+3 tick에 재Q가 된다. E/R 표식의 0초 초기화가 0.1초보다 우선한다.
⑤ 대가: target 고정 종점 보간보다 tick마다 target을 추적하므로 대상이 빠르게 도망가면 이동 시간이 길어진다. 대신 타깃 지정 돌진의 접촉 판정과 시각이 일치한다. 서버가 최종 권위이며 로컬 smoke의 point dash는 같은 거리/속도 duration으로만 보조한다.
```

### 5-2. 데이터와 생성물

#### C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json

`skill.irelia.q.params`의 기존 코드:

```json
"params": {
  "baseDamage": 5.0,
  "damagePerRank": 20.0,
  "dashDurationSec": 0.25,
  "gap": 1.35
}
```

아래로 교체:

```json
"params": {
  "baseDamage": 5.0,
  "damagePerRank": 20.0,
  "gap": 1.35,
  "speed": 14.0
}
```

`eSkillEffectParamId::Speed`와 F4 ChampionTuner의 Speed 항목은 이미 존재하므로 enum/UI/schema 확장은 하지 않는다. `python Tools/LoLData/Build-LoLDefinitionPack.py`로 manifest와 Server/Client 생성물을 재생성하고 `--check`로 닫는다.

### 5-3. 서버 권위 코드

#### C:/Users/user/Desktop/Winters/Shared/GameSim/Components/IreliaSimComponent.h

기존 Q dash 필드/플래그:

```cpp
	Vec3 dashStartPos{};
	Vec3 dashEndPos{};
	f32_t dashElapsedSec = 0.f;
	f32_t dashDurationSec = 0.f;
	EntityID dashTarget = NULL_ENTITY;
	u8_t rRank = 0;
	bool_t bQMarkCooldownReset = false;
```

아래로 교체(중간 R 필드는 현재 순서를 보존):

```cpp
	Vec3 dashStartPos{};
	Vec3 dashEndPos{};
	f32_t dashSpeed = 0.f;
	EntityID dashTarget = NULL_ENTITY;
	u8_t qRank = 0;
	u8_t rRank = 0;
```

#### C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Irelia/IreliaGameSim.h

namespace 첫머리에 로컬 smoke와 서버 fallback이 공유할 단일 상수를 추가한다. 서버의 F4/runtime overlay 값이 있으면 이 상수보다 데이터가 우선한다.

```cpp
	inline constexpr f32_t kQBaseDashSpeedFallback = 14.f;
	inline constexpr f32_t kQStopGapFallback = 1.35f;
```

`ResolveQDashEndPos`를 아래로 교체해 gap보다 가까운 대상에서 뒤로 이동하지 않게 한다.

```cpp
	inline Vec3 ResolveQDashEndPos(const Vec3& casterPos, const Vec3& targetPos, f32_t dashStopGap)
	{
		const f32_t distance = std::sqrtf(WintersMath::DistanceSqXZ(casterPos, targetPos));
		const f32_t clampedGap = std::min(std::max(0.f, dashStopGap), distance);
		const Vec3 dir = WintersMath::DirectionXZ(casterPos, targetPos, Vec3{});
		return Vec3{
			targetPos.x - dir.x * clampedGap,
			casterPos.y,
			targetPos.z - dir.z * clampedGap
		};
	}
```

헤더에 `<algorithm>`, `<cmath>`와 `DamageRequest`/`DamageResult` 전방 선언, `OnDamageResolved(...)` 선언을 추가한다.

#### C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Irelia/IreliaGameSim.cpp

include 목록에 다음을 직접 추가한다.

```cpp
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
```

익명 네임스페이스에 다음 상수/도우미를 추가한다.

```cpp
    constexpr f32_t kIreliaQKillResetCooldownSec = 0.1f;
    constexpr u16_t kIreliaQSkillId =
        static_cast<u16_t>((static_cast<u32_t>(eChampion::IRELIA) << 8) | 1u);

    void LimitIreliaQCooldown(CWorld& world, EntityID caster, f32_t cooldownSec)
    {
        if (!world.HasComponent<SkillStateComponent>(caster))
            return;

        auto& q = world.GetComponent<SkillStateComponent>(caster)
            .slots[static_cast<u8_t>(eSkillSlot::Q)];
        const f32_t limited = std::max(0.f, cooldownSec);
        q.cooldownRemaining = std::min(q.cooldownRemaining, limited);
        q.cooldownDuration = std::min(q.cooldownDuration, limited);
    }

    void ReleaseIreliaQAction(CWorld& world, const TickContext& tc, EntityID caster)
    {
        if (!world.HasComponent<ActionStateComponent>(caster))
            return;

        auto& action = world.GetComponent<ActionStateComponent>(caster);
        if (action.sourceChampion == eChampion::IRELIA &&
            action.sourceSlot == static_cast<u8_t>(eSkillSlot::Q))
        {
            action.lockEndTick = std::min(action.lockEndTick, tc.tickIndex);
            action.movePolicy = eSkillActionMovePolicy::Allow;
        }
    }
```

`ResolveIreliaQImpact`는 표식을 미리 소모하지 않고 피해만 enqueue한다.

```cpp
    void ResolveIreliaQImpact(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target,
        u8_t rank)
    {
        if (target == NULL_ENTITY || !world.IsAlive(target) ||
            !world.HasComponent<HealthComponent>(target))
        {
            return;
        }

        const auto& health = world.GetComponent<HealthComponent>(target);
        if (health.bIsDead || health.fCurrent <= 0.f)
            return;

        const eTeam casterTeam = world.HasComponent<ChampionComponent>(caster)
            ? world.GetComponent<ChampionComponent>(caster).team
            : eTeam::Neutral;
        EnqueuePhysicalDamage(
            world,
            caster,
            target,
            casterTeam,
            ResolveIreliaSkillEffectParam(
                world, tc, caster, eSkillSlot::Q, eSkillEffectParamId::BaseDamage) +
            ResolveIreliaSkillEffectParam(
                world, tc, caster, eSkillSlot::Q, eSkillEffectParamId::DamagePerRank) *
                static_cast<f32_t>(rank),
            kIreliaQSkillId,
            rank);
    }
```

`OnQ`의 dash state 설정부터 즉시 표식 소모/피해 enqueue까지를 아래로 교체한다.

```cpp
        const f32_t dashStopGap = ResolveIreliaSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::Gap,
            IreliaGameSim::kQStopGapFallback);
        const Vec3 endPos = IreliaGameSim::ResolveQDashEndPos(
            casterPos, targetPos, dashStopGap);

        IreliaSimComponent& state = GetIreliaState(world, ctx.casterEntity);
        state.dashStartPos = casterPos;
        state.dashEndPos = endPos;
        state.dashSpeed = ResolveIreliaSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::Speed,
            IreliaGameSim::kQBaseDashSpeedFallback);
        if (world.HasComponent<StatComponent>(ctx.casterEntity))
            state.dashSpeed += world.GetComponent<StatComponent>(ctx.casterEntity).moveSpeed;
        state.dashSpeed = std::max(0.01f, state.dashSpeed);
        state.dashTarget = cmd.targetEntity;
        state.qRank = ctx.skillRank;
        state.bDashActive = true;

        ClearMove(world, ctx.casterEntity);
```

`Tick`의 Q 고정 duration/Lerp 블록은 아래 일정 속도 접촉 블록으로 교체한다.

```cpp
                    if (state.bDashActive)
                    {
                        const EntityID target = state.dashTarget;
                        const bool_t bCancelled =
                            !GameplayStateQuery::CanMove(world, entity) ||
                            world.HasComponent<ForcedMotionComponent>(entity) ||
                            target == NULL_ENTITY ||
                            !world.IsAlive(target) ||
                            !world.HasComponent<TransformComponent>(target) ||
                            !world.HasComponent<HealthComponent>(target) ||
                            world.GetComponent<HealthComponent>(target).bIsDead ||
                            world.GetComponent<HealthComponent>(target).fCurrent <= 0.f;
                        if (bCancelled)
                        {
                            state.bDashActive = false;
                            state.dashSpeed = 0.f;
                            state.dashTarget = NULL_ENTITY;
                            state.qRank = 0u;
                        }
                        else
                        {
                            ClearMove(world, entity);
                            const Vec3 currentPos = transform.GetLocalPosition();
                            const Vec3 targetPos = world.GetComponent<TransformComponent>(target)
                                .GetLocalPosition();
                            const f32_t gap = ResolveIreliaSkillEffectParam(
                                world,
                                tc,
                                entity,
                                eSkillSlot::Q,
                                eSkillEffectParamId::Gap,
                                IreliaGameSim::kQStopGapFallback);
                            const Vec3 desiredEnd = ResolveQDashEndPos(currentPos, targetPos, gap);
                            state.dashEndPos = desiredEnd;

                            const Vec3 delta{
                                desiredEnd.x - currentPos.x,
                                0.f,
                                desiredEnd.z - currentPos.z };
                            const f32_t remaining = std::sqrtf(
                                delta.x * delta.x + delta.z * delta.z);
                            const f32_t step = state.dashSpeed * tc.fDt;
                            const bool_t bContact = remaining <= step || remaining <= 0.0001f;
                            const Vec3 desiredPos = bContact
                                ? desiredEnd
                                : Vec3{
                                    currentPos.x + delta.x * (step / remaining),
                                    currentPos.y,
                                    currentPos.z + delta.z * (step / remaining) };
                            Vec3 guardedPos = desiredPos;
                            bool_t bBlocked = false;
                            if (tc.pWalkable && !tc.pWalkable->TryClampMoveSegmentXZ(
                                currentPos, desiredPos, 0.5f, guardedPos))
                            {
                                guardedPos = currentPos;
                                bBlocked = true;
                            }
                            else if (tc.pWalkable &&
                                WintersMath::DistanceSqXZ(guardedPos, desiredPos) > 0.0001f)
                            {
                                bBlocked = true;
                            }
                            transform.SetPosition(guardedPos);

                            const Vec3 dir = WintersMath::NormalizeXZ(delta);
                            if (dir.x * dir.x + dir.z * dir.z > 0.0001f)
                            {
                                const Vec3 rot = transform.GetRotation();
                                transform.SetRotation({
                                    rot.x,
                                    ResolveChampionVisualYawNear(
                                        eChampion::IRELIA, dir, rot.y),
                                    rot.z });
                            }

                            if (bBlocked || bContact)
                            {
                                const u8_t impactRank = state.qRank;
                                state.bDashActive = false;
                                state.dashSpeed = 0.f;
                                state.dashTarget = NULL_ENTITY;
                                state.qRank = 0u;
                                if (bContact && !bBlocked)
                                {
                                    SnapDashArrivalToWalkable(
                                        world, tc, entity, state.dashStartPos);
                                    ReleaseIreliaQAction(world, tc, entity);
                                    ResolveIreliaQImpact(
                                        world, tc, entity, target, impactRank);
                                }
                            }
                        }
                    }
```

`OnDamageResolved`의 최종 본문:

```cpp
    void OnDamageResolved(
        CWorld& world,
        const TickContext&,
        const DamageRequest& request,
        const DamageResult& result)
    {
        if (!result.bApplied || request.source == NULL_ENTITY ||
            request.target == NULL_ENTITY ||
            request.eSourceKind != eDamageSourceKind::Skill ||
            request.iSourceSlot != static_cast<u8_t>(eSkillSlot::Q) ||
            request.skillId != kIreliaQSkillId ||
            !world.HasComponent<ChampionComponent>(request.source) ||
            world.GetComponent<ChampionComponent>(request.source).id != eChampion::IRELIA)
        {
            return;
        }

        if (world.HasComponent<IreliaMarkComponent>(request.target))
        {
            const auto& mark = world.GetComponent<IreliaMarkComponent>(request.target);
            if (mark.sourceEntity == request.source && mark.fRemainingSec > 0.f)
            {
                world.RemoveComponent<IreliaMarkComponent>(request.target);
                LimitIreliaQCooldown(world, request.source, 0.f);
            }
        }

        if (result.bKilled &&
            GameplayStateQuery::IsMobileCombatUnit(world, request.target))
        {
            LimitIreliaQCooldown(
                world, request.source, kIreliaQKillResetCooldownSec);
        }
    }
```

`DamageQueueSystem.cpp`는 §4-2-4의 include/hook 호출을 그대로 적용한다.

#### C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

Debug Server 빌드에서 확인된 대상 사망 정리 경로도 새 Q dash 상태 필드와 함께 갱신한다. 기존 `if (irelia.dashTarget == source)` 블록을 아래로 교체한다.

```cpp
            if (irelia.dashTarget == source)
            {
                irelia.dashTarget = NULL_ENTITY;
                irelia.bDashActive = false;
                irelia.dashSpeed = 0.f;
                irelia.qRank = 0u;
            }
```

이는 새 로직을 추가하는 경로가 아니라, 제거된 `dashElapsedSec`/`dashDurationSec` 참조를 새 runtime 상태 정리와 일치시키는 직접 의존 수정이다.

### 5-4. 로컬 시각 보조

#### C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp

include 목록에 다음을 직접 추가한다.

```cpp
#include "Shared/GameSim/Components/StatComponent.h"
```

고정 `getLocalDashDuration()` 사용을 아래 거리/속도 계산으로 교체한다. 이는 로컬 smoke 보조이며 네트워크 판정 권위는 서버 Tick이다.

```cpp
        f32_t dashSpeed = IreliaGameSim::kQBaseDashSpeedFallback;
        if (ctx.pWorld->HasComponent<StatComponent>(ctx.casterEntity))
            dashSpeed += ctx.pWorld->GetComponent<StatComponent>(ctx.casterEntity).moveSpeed;
        const f32_t travelDistance = std::sqrtf(
            WintersMath::DistanceSqXZ(pStart, pEnd));
        const f32_t duration = travelDistance / std::max(0.01f, dashSpeed);
        ctx.startPointDash(pStart, pEnd, duration, target);
```

`startPointDash`를 호출하는 챔피언은 현재 이렐리아뿐이므로, 로컬 smoke의 고정 Lerp도 남기지 않는다.

#### C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

기존 dash runtime에 속도를 추가한다.

```cpp
    f32_t m_fDashDuration = 0.3f;
    f32_t m_fDashSpeed = 0.f;
```

#### C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameLocalSkills.cpp

include 목록에 공유 종점 계산을 추가한다.

```cpp
#include "Shared/GameSim/Champions/Irelia/IreliaGameSim.h"
```

`startPointDash` 람다에서 초기 duration을 속도로 환산해 저장한다.

```cpp
                m_fDashDuration = duration;
                const f32_t distance = std::sqrtf(
                    WintersMath::DistanceSqXZ(start, end));
                m_fDashSpeed = duration > 0.0001f
                    ? distance / duration
                    : 19.f;
```

`UpdateDash`의 elapsed/duration Lerp를 아래 일정 속도/이동 target 추적 블록으로 교체한다.

```cpp
    if (m_bNetworkAuthoritativeGameplay)
    {
        // Network Q position/cancel/contact is consumed only from server snapshots.
        m_bDashActive = false;
        m_fDashElapsed = 0.f;
        m_fDashSpeed = 0.f;
        m_DashTargetEntity = NULL_ENTITY;
        return;
    }

    Vec3 current = m_pPlayerTransform->GetPosition();
    if (m_DashTargetEntity != NULL_ENTITY &&
        m_World.HasComponent<TransformComponent>(m_DashTargetEntity))
    {
        const Vec3 target = m_World.GetComponent<TransformComponent>(
            m_DashTargetEntity).GetPosition();
        m_vDashEnd = IreliaGameSim::ResolveQDashEndPos(
            current, target, IreliaGameSim::kQStopGapFallback);
    }

    const Vec3 delta{
        m_vDashEnd.x - current.x,
        0.f,
        m_vDashEnd.z - current.z };
    const f32_t remaining = std::sqrtf(
        delta.x * delta.x + delta.z * delta.z);
    const f32_t step = std::max(0.01f, m_fDashSpeed) * dt;
    if (remaining <= step || remaining <= 0.0001f)
    {
        SetPlayerPosition(m_vDashEnd);
        m_bDashActive = false;
        m_fDashElapsed = 0.f;
        m_fDashSpeed = 0.f;
        m_DashTargetEntity = NULL_ENTITY;
        m_fLastActionTimer = 0.f;
        ClearActiveSkillRuntime();

        if (m_PlayerEntity != NULL_ENTITY &&
            m_World.HasComponent<SkillStateComponent>(m_PlayerEntity))
        {
            auto& basicAttack = m_World.GetComponent<SkillStateComponent>(
                m_PlayerEntity).slots[static_cast<u8_t>(eSkillSlot::BasicAttack)];
            basicAttack.cooldownRemaining = 0.f;
            basicAttack.cooldownDuration = 0.f;
        }
        return;
    }

    SetPlayerPosition(Vec3{
        current.x + delta.x * (step / remaining),
        current.y,
        current.z + delta.z * (step / remaining) });
```

death/reset 경로에서도 `m_fDashSpeed=0.f`를 함께 초기화한다. 이 로컬 기본 공격 초기화는 기존 `UpdateDash` 완료 분기의 동작을 보존하며 서버 판정 진실을 만들지 않는다.

위 guard는 네트워크 모드에서 point-dash 위치 쓰기 자체를 금지한다. `DispatchSkillInput`도 네트워크 권위 경로에서는 `ApplyLocalPrediction`을 호출하지 않으므로, F4 speed·walkable block·CannotMove·target death는 서버 Tick/snapshot 하나만 소유한다. 로컬 constant-step은 서버가 없는 smoke 모드에서만 실행한다.

### 5-5. 강화된 실행 검증

`Tools/SimLab/main.cpp`에 `IreliaSimComponent.h` include와 `--irelia-q-only` probe를 추가한다. probe는 요약 assert가 아니라 아래 행위를 실제 시스템 순서로 실행한다.

`FlatWalkable`의 기존 필드와 clamp 함수에 정확히 다음을 추가/교체한다.

```cpp
        bool_t bClampMoveSegment = true;

        bool_t TryClampMoveSegmentXZ(
            const Vec3& vFrom,
            const Vec3& vDesired,
            f32_t,
            Vec3& vOutPosition) const override
        {
            if (!bClampMoveSegment)
            {
                vOutPosition = vFrom;
                return false;
            }
            vOutPosition = vDesired;
            return true;
        }
```

§4-2-5의 고정-duration `RunIreliaQImpactKillResetProbe` 초안을 아래 함수로 교체한다. 이 코드가 최종 probe 정본이다.

```cpp
    bool_t RunIreliaQImpactKillResetProbe()
    {
        constexpr u16_t kQSkillId =
            static_cast<u16_t>((static_cast<u32_t>(eChampion::IRELIA) << 8) | 1u);
        const u8_t qSlot = static_cast<u8_t>(eSkillSlot::Q);

        const auto CountQDamageEvents = [](CWorld& world, EntityID source, EntityID target)
        {
            u32_t count = 0u;
            world.ForEach<ReplicatedEventComponent>(
                std::function<void(EntityID, ReplicatedEventComponent&)>(
                    [&](EntityID, ReplicatedEventComponent& event)
                    {
                        if (event.kind == eReplicatedEventKind::Damage &&
                            event.sourceEntity == source &&
                            event.targetEntity == target &&
                            event.skillId == kQSkillId)
                        {
                            ++count;
                        }
                    }));
            return count;
        };

        const auto SpawnTarget = [](CWorld& world,
                                    EntityIdMap& entityMap,
                                    GameplayStateQuery::eGameplayTargetKind kind,
                                    const Vec3& position)
        {
            EntityID target = kind == GameplayStateQuery::eGameplayTargetKind::Champion
                ? SpawnChampion(
                    world,
                    entityMap,
                    eChampion::ASHE,
                    static_cast<u8_t>(eTeam::Red),
                    5u)
                : SpawnStatusProbeTarget(world, kind, eTeam::Red, position);
            if (target != NULL_ENTITY)
                world.GetComponent<TransformComponent>(target).SetPosition(position);
            return target;
        };

        const auto RunKillCase = [&](GameplayStateQuery::eGameplayTargetKind kind,
                                     bool_t bVerifyThreeTickRecast)
        {
            CWorld world;
            DeterministicRng rng(20260718120ull + static_cast<u64_t>(kind));
            EntityIdMap entityMap;
            FlatWalkable walkable;
            auto executor = CDefaultCommandExecutor::Create();
            const EntityID irelia = SpawnChampion(
                world, entityMap, eChampion::IRELIA,
                static_cast<u8_t>(eTeam::Blue), 0u);
            const EntityID target = SpawnTarget(
                world, entityMap, kind, Vec3{ 2.f, 0.f, 0.f });
            if (irelia == NULL_ENTITY || target == NULL_ENTITY)
                return false;

            world.GetComponent<TransformComponent>(irelia).SetPosition(Vec3{});
            auto& health = world.GetComponent<HealthComponent>(target);
            health.fMaximum = 1.f;
            health.fCurrent = 1.f;
            health.bIsDead = false;
            world.GetComponent<SkillRankComponent>(irelia).ranks[qSlot] = 1u;

            GameCommand q{};
            q.kind = eCommandKind::CastSkill;
            q.issuerEntity = irelia;
            q.targetEntity = target;
            q.slot = qSlot;
            q.sequenceNum = 1u;

            u64_t impactTick = 0u;
            for (u64_t tick = 1u; tick <= 20u; ++tick)
            {
                TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
                CSkillCooldownSystem::Execute(world, tc);
                if (tick == 1u &&
                    executor->ExecuteCommand(world, tc, q).state !=
                        eCommandExecutionState::Accepted)
                {
                    return false;
                }

                const bool_t bWasDashing =
                    world.GetComponent<IreliaSimComponent>(irelia).bDashActive;
                IreliaGameSim::Tick(world, tc);
                const bool_t bEnded = bWasDashing &&
                    !world.GetComponent<IreliaSimComponent>(irelia).bDashActive;
                CDamageQueueSystem::Execute(world, tc);
                if (!bEnded &&
                    (health.fCurrent != 1.f ||
                        CountQDamageEvents(world, irelia, target) != 0u))
                {
                    return false;
                }
                if (bEnded)
                {
                    impactTick = tick;
                    break;
                }
            }

            const auto& qState = world.GetComponent<SkillStateComponent>(irelia).slots[qSlot];
            const auto& action = world.GetComponent<ActionStateComponent>(irelia);
            if (impactTick == 0u || !health.bIsDead || health.fCurrent > 0.f ||
                CountQDamageEvents(world, irelia, target) != 1u ||
                std::fabs(qState.cooldownRemaining - 0.1f) > 0.0001f ||
                std::fabs(qState.cooldownDuration - 0.1f) > 0.0001f ||
                action.lockEndTick > impactTick)
            {
                return false;
            }

            if (!bVerifyThreeTickRecast)
                return true;

            const EntityID followup = SpawnStatusProbeTarget(
                world,
                GameplayStateQuery::eGameplayTargetKind::MinionOrSummon,
                eTeam::Red,
                Vec3{ 4.f, 0.f, 0.f });
            if (followup == NULL_ENTITY)
                return false;

            for (u64_t offset = 1u; offset <= 3u; ++offset)
            {
                TickContext tc = MakeProbeTickContext(
                    impactTick + offset, rng, entityMap, walkable);
                CSkillCooldownSystem::Execute(world, tc);
                GameCommand recast = q;
                recast.targetEntity = followup;
                recast.sequenceNum = static_cast<u32_t>(10u + offset);
                const CommandExecutionResult result =
                    executor->ExecuteCommand(world, tc, recast);
                if (offset < 3u)
                {
                    if (result.state != eCommandExecutionState::Rejected ||
                        result.reason != eCommandExecutionReason::Cooldown)
                    {
                        return false;
                    }
                }
                else if (result.state != eCommandExecutionState::Accepted)
                {
                    return false;
                }
            }
            return true;
        };

        const auto RunMovingTargetAndActionCase = [&]()
        {
            CWorld world;
            DeterministicRng rng(20260718130ull);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            auto executor = CDefaultCommandExecutor::Create();
            const EntityID irelia = SpawnChampion(
                world, entityMap, eChampion::IRELIA,
                static_cast<u8_t>(eTeam::Blue), 0u);
            const EntityID target = SpawnChampion(
                world, entityMap, eChampion::ASHE,
                static_cast<u8_t>(eTeam::Red), 5u);
            if (irelia == NULL_ENTITY || target == NULL_ENTITY)
                return false;

            world.GetComponent<TransformComponent>(irelia).SetPosition(Vec3{});
            world.GetComponent<TransformComponent>(target).SetPosition(
                Vec3{ 5.f, 0.f, 0.f });
            auto& health = world.GetComponent<HealthComponent>(target);
            health.fMaximum = 10000.f;
            health.fCurrent = 10000.f;
            world.GetComponent<SkillRankComponent>(irelia).ranks[qSlot] = 1u;

            GameCommand q{};
            q.kind = eCommandKind::CastSkill;
            q.issuerEntity = irelia;
            q.targetEntity = target;
            q.slot = qSlot;
            q.sequenceNum = 40u;

            u64_t impactTick = 0u;
            f32_t previousCasterX = 0.f;
            for (u64_t tick = 1u; tick <= 12u; ++tick)
            {
                TickContext tc = MakeProbeTickContext(tick, rng, entityMap, walkable);
                CSkillCooldownSystem::Execute(world, tc);
                if (tick == 1u &&
                    executor->ExecuteCommand(world, tc, q).state !=
                        eCommandExecutionState::Accepted)
                {
                    return false;
                }
                if (tick > 1u)
                {
                    auto& targetTf = world.GetComponent<TransformComponent>(target);
                    Vec3 moved = targetTf.GetPosition();
                    moved.x -= 0.35f;
                    targetTf.SetPosition(moved);
                }

                const Vec3 before = world.GetComponent<TransformComponent>(irelia)
                    .GetPosition();
                const Vec3 targetPos = world.GetComponent<TransformComponent>(target)
                    .GetPosition();
                const Vec3 expectedEnd = IreliaGameSim::ResolveQDashEndPos(
                    before, targetPos, IreliaGameSim::kQStopGapFallback);
                const bool_t bWasDashing =
                    world.GetComponent<IreliaSimComponent>(irelia).bDashActive;
                IreliaGameSim::Tick(world, tc);
                const auto& qDash = world.GetComponent<IreliaSimComponent>(irelia);
                const Vec3 after = world.GetComponent<TransformComponent>(irelia)
                    .GetPosition();
                if (after.x + 0.0001f < previousCasterX ||
                    (qDash.bDashActive &&
                        WintersMath::DistanceSqXZ(qDash.dashEndPos, expectedEnd) > 0.0001f))
                {
                    return false;
                }
                if (tick == 1u && after.x <= 0.f)
                    return false; // cast tick already owns the first speed/30 step.
                previousCasterX = after.x;

                const bool_t bEnded = bWasDashing && !qDash.bDashActive;
                CDamageQueueSystem::Execute(world, tc);
                if (!bEnded && CountQDamageEvents(world, irelia, target) != 0u)
                    return false;
                if (bEnded)
                {
                    impactTick = tick;
                    break;
                }
            }

            if (impactTick == 0u || impactTick >= 8u ||
                health.fCurrent >= 10000.f ||
                CountQDamageEvents(world, irelia, target) != 1u ||
                world.GetComponent<SkillStateComponent>(irelia)
                    .slots[qSlot].cooldownRemaining <= 0.1f ||
                world.GetComponent<ActionStateComponent>(irelia).lockEndTick > impactTick)
            {
                return false;
            }

            TickContext next = MakeProbeTickContext(
                impactTick + 1u, rng, entityMap, walkable);
            CSkillCooldownSystem::Execute(world, next);
            GameCommand attack{};
            attack.kind = eCommandKind::BasicAttack;
            attack.issuerEntity = irelia;
            attack.targetEntity = target;
            attack.slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
            attack.sequenceNum = 50u;
            return executor->ExecuteCommand(world, next, attack).state ==
                eCommandExecutionState::Accepted;
        };

        const auto RunNearContactCase = [&]()
        {
            CWorld world;
            DeterministicRng rng(20260718140ull);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            auto executor = CDefaultCommandExecutor::Create();
            const EntityID irelia = SpawnChampion(
                world, entityMap, eChampion::IRELIA,
                static_cast<u8_t>(eTeam::Blue), 0u);
            const EntityID target = SpawnStatusProbeTarget(
                world,
                GameplayStateQuery::eGameplayTargetKind::MinionOrSummon,
                eTeam::Red,
                Vec3{ 0.5f, 0.f, 0.f });
            world.GetComponent<TransformComponent>(irelia).SetPosition(Vec3{});
            world.GetComponent<SkillRankComponent>(irelia).ranks[qSlot] = 1u;
            GameCommand q{};
            q.kind = eCommandKind::CastSkill;
            q.issuerEntity = irelia;
            q.targetEntity = target;
            q.slot = qSlot;
            q.sequenceNum = 60u;
            TickContext tc = MakeProbeTickContext(1u, rng, entityMap, walkable);
            CSkillCooldownSystem::Execute(world, tc);
            if (executor->ExecuteCommand(world, tc, q).state !=
                eCommandExecutionState::Accepted)
            {
                return false;
            }
            IreliaGameSim::Tick(world, tc);
            CDamageQueueSystem::Execute(world, tc);
            const Vec3 casterPos = world.GetComponent<TransformComponent>(irelia)
                .GetPosition();
            return !world.GetComponent<IreliaSimComponent>(irelia).bDashActive &&
                std::fabs(casterPos.x) <= 0.0001f &&
                CountQDamageEvents(world, irelia, target) == 1u;
        };

        const auto RunCancelledCase = [&](bool_t bBlocked)
        {
            CWorld world;
            DeterministicRng rng(bBlocked ? 20260718150ull : 20260718151ull);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            auto executor = CDefaultCommandExecutor::Create();
            const EntityID irelia = SpawnChampion(
                world, entityMap, eChampion::IRELIA,
                static_cast<u8_t>(eTeam::Blue), 0u);
            const EntityID target = SpawnStatusProbeTarget(
                world,
                GameplayStateQuery::eGameplayTargetKind::MinionOrSummon,
                eTeam::Red,
                Vec3{ 2.f, 0.f, 0.f });
            world.GetComponent<TransformComponent>(irelia).SetPosition(Vec3{});
            world.GetComponent<SkillRankComponent>(irelia).ranks[qSlot] = 1u;
            world.AddComponent<IreliaMarkComponent>(
                target, IreliaMarkComponent{ irelia, 5.f });
            GameCommand q{};
            q.kind = eCommandKind::CastSkill;
            q.issuerEntity = irelia;
            q.targetEntity = target;
            q.slot = qSlot;
            q.sequenceNum = bBlocked ? 70u : 71u;
            TickContext tc = MakeProbeTickContext(1u, rng, entityMap, walkable);
            CSkillCooldownSystem::Execute(world, tc);
            if (executor->ExecuteCommand(world, tc, q).state !=
                eCommandExecutionState::Accepted)
            {
                return false;
            }
            if (bBlocked)
            {
                walkable.bClampMoveSegment = false;
            }
            else
            {
                auto& state = world.HasComponent<GameplayStateComponent>(irelia)
                    ? world.GetComponent<GameplayStateComponent>(irelia)
                    : world.AddComponent<GameplayStateComponent>(
                        irelia, GameplayStateComponent{});
                state.stateFlags |= kGameplayStateCannotMoveFlag;
            }
            const f32_t hpBefore = world.GetComponent<HealthComponent>(target).fCurrent;
            IreliaGameSim::Tick(world, tc);
            CDamageQueueSystem::Execute(world, tc);
            const auto& qState = world.GetComponent<SkillStateComponent>(irelia).slots[qSlot];
            return !world.GetComponent<IreliaSimComponent>(irelia).bDashActive &&
                world.GetComponent<HealthComponent>(target).fCurrent == hpBefore &&
                CountQDamageEvents(world, irelia, target) == 0u &&
                world.HasComponent<IreliaMarkComponent>(target) &&
                qState.cooldownRemaining > 0.1f;
        };

        const auto RunStructureKillNoResetCase = [&]()
        {
            CWorld world;
            DeterministicRng rng(20260718160ull);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            auto executor = CDefaultCommandExecutor::Create();
            const EntityID irelia = SpawnChampion(
                world, entityMap, eChampion::IRELIA,
                static_cast<u8_t>(eTeam::Blue), 0u);
            const EntityID structure = SpawnStatusProbeTarget(
                world,
                GameplayStateQuery::eGameplayTargetKind::Structure,
                eTeam::Red,
                Vec3{ 0.5f, 0.f, 0.f });
            world.GetComponent<TransformComponent>(irelia).SetPosition(Vec3{});
            world.GetComponent<SkillRankComponent>(irelia).ranks[qSlot] = 1u;
            auto& health = world.GetComponent<HealthComponent>(structure);
            health.fMaximum = 1.f;
            health.fCurrent = 1.f;
            GameCommand q{};
            q.kind = eCommandKind::CastSkill;
            q.issuerEntity = irelia;
            q.targetEntity = structure;
            q.slot = qSlot;
            q.sequenceNum = 80u;
            TickContext tc = MakeProbeTickContext(1u, rng, entityMap, walkable);
            CSkillCooldownSystem::Execute(world, tc);
            if (executor->ExecuteCommand(world, tc, q).state !=
                eCommandExecutionState::Accepted)
            {
                return false;
            }
            IreliaGameSim::Tick(world, tc);
            CDamageQueueSystem::Execute(world, tc);
            return health.bIsDead &&
                world.GetComponent<SkillStateComponent>(irelia)
                    .slots[qSlot].cooldownRemaining > 0.1f;
        };

        const auto RunMarkedAppliedCase = [&](bool_t bInvulnerable)
        {
            CWorld world;
            DeterministicRng rng(bInvulnerable ? 20260718170ull : 20260718171ull);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            auto executor = CDefaultCommandExecutor::Create();
            const EntityID irelia = SpawnChampion(
                world, entityMap, eChampion::IRELIA,
                static_cast<u8_t>(eTeam::Blue), 0u);
            const EntityID target = SpawnStatusProbeTarget(
                world,
                GameplayStateQuery::eGameplayTargetKind::MinionOrSummon,
                eTeam::Red,
                Vec3{ 0.5f, 0.f, 0.f });
            world.GetComponent<TransformComponent>(irelia).SetPosition(Vec3{});
            world.GetComponent<SkillRankComponent>(irelia).ranks[qSlot] = 1u;
            world.AddComponent<IreliaMarkComponent>(
                target, IreliaMarkComponent{ irelia, 5.f });
            if (bInvulnerable)
            {
                auto& state = world.HasComponent<GameplayStateComponent>(target)
                    ? world.GetComponent<GameplayStateComponent>(target)
                    : world.AddComponent<GameplayStateComponent>(
                        target, GameplayStateComponent{});
                state.stateFlags |= kGameplayStateInvulnerableFlag;
            }
            const f32_t hpBefore = world.GetComponent<HealthComponent>(target).fCurrent;
            GameCommand q{};
            q.kind = eCommandKind::CastSkill;
            q.issuerEntity = irelia;
            q.targetEntity = target;
            q.slot = qSlot;
            q.sequenceNum = bInvulnerable ? 90u : 91u;
            TickContext tc = MakeProbeTickContext(1u, rng, entityMap, walkable);
            CSkillCooldownSystem::Execute(world, tc);
            if (executor->ExecuteCommand(world, tc, q).state !=
                eCommandExecutionState::Accepted)
            {
                return false;
            }
            IreliaGameSim::Tick(world, tc);
            CDamageQueueSystem::Execute(world, tc);
            const auto& qState = world.GetComponent<SkillStateComponent>(irelia)
                .slots[qSlot];
            if (bInvulnerable)
            {
                return world.GetComponent<HealthComponent>(target).fCurrent == hpBefore &&
                    world.HasComponent<IreliaMarkComponent>(target) &&
                    qState.cooldownRemaining > 0.1f &&
                    CountQDamageEvents(world, irelia, target) == 0u;
            }
            return world.GetComponent<HealthComponent>(target).fCurrent < hpBefore &&
                !world.HasComponent<IreliaMarkComponent>(target) &&
                qState.cooldownRemaining == 0.f &&
                qState.cooldownDuration == 0.f &&
                CountQDamageEvents(world, irelia, target) == 1u;
        };

        const bool_t bPass =
            RunKillCase(GameplayStateQuery::eGameplayTargetKind::Champion, true) &&
            RunKillCase(GameplayStateQuery::eGameplayTargetKind::MinionOrSummon, false) &&
            RunKillCase(GameplayStateQuery::eGameplayTargetKind::JungleMonster, false) &&
            RunMovingTargetAndActionCase() &&
            RunNearContactCase() &&
            RunCancelledCase(true) &&
            RunCancelledCase(false) &&
            RunStructureKillNoResetCase() &&
            RunMarkedAppliedCase(false) &&
            RunMarkedAppliedCase(true);
        std::printf(
            "[SimLab][IreliaQReset] %s: contact damage, moving target, action unlock, mobile kill reset, 3-tick recast, cancellation\n",
            bPass ? "PASS" : "FAIL");
        return bPass;
    }
```

```text
- 근거리(distance < gap): 뒤로 이동하지 않고 cast tick 접촉, 피해 이벤트 정확히 1건, Q action lockEndTick <= impactTick. 명령 phase가 이미 지난 뒤이므로 후속 행동은 impact+1 tick부터 Accepted.
- 원거리: speed=(14+moveSpeed), step=speed/30으로 계산한 접촉 전 tick까지 HP/피해 이벤트 0건, 접촉 tick 정확히 1건.
- 이동 중 target: 매 tick target 위치를 갱신해 stale 고정 종점/고정 0.25초 잔류가 없음.
- 비치명 Champion: 접촉 다음 tick BasicAttack 명령 Accepted(행동 락 종료), Q cooldown은 정상 유지.
- 치명 Champion/MinionOrSummon/JungleMonster: 접촉 후 Q cooldownRemaining/duration=0.1. impact+1/+2의 고유 sequence Q는 Cooldown 거절, impact+3 Q는 Accepted.
- 비치명/Structure: 처치 초기화 없음.
- E/R mark + 적용된 Q: mark 소모 + Q cooldown 0. 무적/미적용 Q: mark/쿨다운 보존.
- walkable blocked 및 CannotMove 취소: 피해 이벤트·HP 변화·mark 소모·처치 초기화 0건.
```

검증 명령은 §4-3에 다음을 선행/추가한다.

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py
python Tools/LoLData/Build-LoLDefinitionPack.py --check
& 'Tools/Bin/Debug/SimLab.exe' --irelia-q-only
```

### 5-6. 재비평 게이트

재비평 결과와 disposition:

```text
- P0 서버/클라이언트 StatComponent include 누락: 수용, §5-3/§5-4에 직접 include 추가.
- P1 moving target 서버/로컬 불일치: 수용, Scene local point dash도 매 frame target 종점 재계산 + 일정 속도 step으로 교체.
- P1 speed 이중 소유: 수용, 공용 IreliaGameSim.h fallback 상수를 서버 fallback/로컬 smoke가 함께 사용. F4/runtime overlay가 있는 서버 값이 권위이며 네트워크 클라는 snapshot 위치를 소비한다.
- P1 접촉 tick phase 경계: 수용, 실제 다음 명령은 impact+1부터임을 결정/검증에 명시.
- P1 클라이언트 애니메이션 종료 증거: 수용, 로컬 UpdateDash가 접촉 시 dash/action runtime을 해제하고 F5 네트워크에서는 snapshot actionLockEndTick + 후속 ActionStart preemption을 수동 캡처 항목으로 둔다.
- P1 SimLab 구체성/첫 step tick: 수용, cast tick부터 첫 step을 실행하고 고유 sequence/reason/이벤트 수/정확한 impact+1/+2/+3을 probe에 하드 assert한다.
- P1 dirty 충돌: 수용, apply 전 최신 anchor와 대상별 focused diff를 확인하고 생성물은 irelia.q key/buildHash만 별도 확인한다.
- P2 component anchor: 수용, dashElapsedSec/dashDurationSec/bQMarkCooldownReset은 각 정확 anchor에서 삭제하고 dashSpeed/qRank를 인접 필드에 개별 삽입한다.
```

최종 재확인 게이트: 위 disposition 반영본에서 P0/P1 잔존 여부를 같은 독립 서브 에이전트가 확인한 뒤 구현한다.
