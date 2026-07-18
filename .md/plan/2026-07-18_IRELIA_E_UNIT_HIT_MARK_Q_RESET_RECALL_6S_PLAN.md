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
