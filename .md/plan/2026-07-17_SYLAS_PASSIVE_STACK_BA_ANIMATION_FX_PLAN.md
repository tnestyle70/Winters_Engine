Session - 사일러스 스킬 적중이 아닌 승인된 스킬 사용으로 5초 패시브 스택을 쌓고, 다음 기본 공격을 Passive BA 애니메이션과 지정 전기 PNG 이펙트로 재생하는 서버 권위 경로를 완결한다.
좌표: 신규 좌표 후보 · 축: C7 · C8
관련: 2026-07-16_GAMEPLAY_FORMULA_SKILL_ANIMATION_DATA_DRIVEN_REMEDIATION_PLAN.md · 2026-07-16_GAMEPLAY_FORMULA_SKILL_ANIMATION_DATA_DRIVEN_REMEDIATION_RESULT.md

## 1. 결정 기록

① 문제·제약: 현재 스택은 C++의 3회·4초 상수이고 잔여 스택 BA 소모 때 4초로 재설정된다. 목표는 최대 3스택, 마지막 승인 스킬 사용부터 5초(30Hz 기준 150 tick), BA 1회당 1스택 소모, EffectTrigger 1회다.
② 순진한 해법의 실패: 클라이언트 입력에서 스택·FX를 만들면 거절된 스킬에도 보이고 서버 cue와 중복된다. PNG만 교체하면 4초 규칙·지속시간 리셋·하드코딩 애니메이션 분기가 남는다.
③ 메커니즘: ServerPrivate JSON `MaxStacks/StackWindowSec` → `SylasSimComponent` → 승인된 스킬 cast에서 적립 → BA 시작에서 stage 2와 flag로 1회 소모 → impact EffectTrigger → ClientPublic stage 2 애니메이션/WFX atlas 재생 순서로 고정한다.
④ 대조군: UE식 신규 GameplayAbility/GameplayEffect 계층 추가보다 Winters의 기존 `CommandExecutor → CombatAction → ReplicatedEvent → VisualHook`을 재사용한다. 이미 158,024 byte의 `skinned_mesh_sylas_attack_passive.wanim`과 `Sylas.PassiveBA.Hit` cue가 있어 재베이킹은 필요 없다.
⑤ 대가: 스택은 적중이 아니라 서버가 승인한 스킬 사용 시 쌓이고 BA 시작 시 소비된다. Passive BA의 추가 광역 피해·스택 HUD는 이번 명세에 없으므로 만들지 않으며, 필요하면 별도 수치/UX 계획으로 분리한다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json

기존 코드:

```json
    {
      "key": "skill.sylas.basic_attack",
      "params": {},
      "damage": {
```

아래로 교체:

```json
    {
      "key": "skill.sylas.basic_attack",
      "params": {
        "maxStacks": 3.0,
        "stackWindowSec": 5.0
      },
      "damage": {
```

### 2-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Sylas/SylasGameSim.h

기존 코드:

```cpp
	void ArmPassiveOnSkillCast(CWorld& world, EntityID caster);
	bool_t TryConsumePassiveBasicAttack(CWorld& world, EntityID caster);
```

아래로 교체:

```cpp
	void ArmPassiveOnSkillCast(CWorld& world, const TickContext& tc, EntityID caster);
	bool_t TryConsumePassiveBasicAttack(CWorld& world, EntityID caster);
```

### 2-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Sylas/SylasGameSim.cpp

기존 코드:

```cpp
    constexpr u8_t kSylasPassiveMaxStacks = 3u;
    constexpr f32_t kSylasPassiveWindowSec = 4.0f;
```

아래로 교체:

```cpp
    constexpr f32_t kSylasPassiveMaxStacksFallback = 3.f;
    constexpr f32_t kSylasPassiveWindowSecFallback = 5.f;
```

기존 코드:

```cpp
    void ArmPassiveOnSkillCast(CWorld& world, EntityID caster)
    {
        if (caster == NULL_ENTITY || !world.IsAlive(caster))
            return;

        SylasSimComponent& sylas = world.HasComponent<SylasSimComponent>(caster)
            ? world.GetComponent<SylasSimComponent>(caster)
            : world.AddComponent<SylasSimComponent>(caster, SylasSimComponent{});
        sylas.passiveStacks = static_cast<u8_t>(
            std::min<u32_t>(kSylasPassiveMaxStacks, sylas.passiveStacks + 1u));
        sylas.passiveRemainingSec = kSylasPassiveWindowSec;
    }
```

아래로 교체:

```cpp
    void ArmPassiveOnSkillCast(CWorld& world, const TickContext& tc, EntityID caster)
    {
        if (caster == NULL_ENTITY || !world.IsAlive(caster))
            return;

        const f32_t maxStacksValue = ResolveSylasSkillEffectParam(
            world,
            tc,
            caster,
            eSkillSlot::BasicAttack,
            eSkillEffectParamId::MaxStacks,
            kSylasPassiveMaxStacksFallback);
        const u32_t maxStacks = static_cast<u32_t>(std::clamp(
            std::round(maxStacksValue),
            1.f,
            255.f));
        const f32_t stackWindowSec = std::max(0.f, ResolveSylasSkillEffectParam(
            world,
            tc,
            caster,
            eSkillSlot::BasicAttack,
            eSkillEffectParamId::StackWindowSec,
            kSylasPassiveWindowSecFallback));

        SylasSimComponent& sylas = world.HasComponent<SylasSimComponent>(caster)
            ? world.GetComponent<SylasSimComponent>(caster)
            : world.AddComponent<SylasSimComponent>(caster, SylasSimComponent{});
        sylas.passiveStacks = static_cast<u8_t>(
            std::min<u32_t>(maxStacks, sylas.passiveStacks + 1u));
        sylas.passiveRemainingSec = stackWindowSec;
    }
```

기존 코드:

```cpp
        --sylas.passiveStacks;
        if (sylas.passiveStacks == 0u)
            sylas.passiveRemainingSec = 0.f;
        else
            sylas.passiveRemainingSec = kSylasPassiveWindowSec;
        return true;
```

아래로 교체:

```cpp
        --sylas.passiveStacks;
        if (sylas.passiveStacks == 0u)
            sylas.passiveRemainingSec = 0.f;
        return true;
```

### 2-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

기존 코드:

```cpp
    if (champion == eChampion::SYLAS)
        SylasGameSim::ArmPassiveOnSkillCast(world, effectiveCmd.issuerEntity);
```

아래로 교체:

```cpp
    if (champion == eChampion::SYLAS)
        SylasGameSim::ArmPassiveOnSkillCast(world, tc, effectiveCmd.issuerEntity);
```

### 2-5. C:/Users/user/Desktop/Winters/Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json

기존 코드:

```json
          "stages": [
            {
              "animationPlaybackSpeed": 1.0,
              "castFrame": 4.0,
              "recoveryFrame": 12.0
            }
          ]
```

`skill.sylas.basic_attack`의 위 블록만 아래로 교체:

```json
          "stages": [
            {
              "animationPlaybackSpeed": 1.0,
              "castFrame": 4.0,
              "recoveryFrame": 12.0
            },
            {
              "animationPlaybackSpeed": 1.0,
              "castFrame": 4.0,
              "recoveryFrame": 12.0
            }
          ]
```

### 2-6. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Sylas/SylasRegistration.cpp

기존 코드:

```cpp
		s.castHookId = hookId;
		//???????E ??????沅쀯㎗?
		if (slot == static_cast<u8_t>(eSkillSlot::E))
		{
			s.stageCount = 2;
			s.stage2TargetMode = eTargetMode::Direction;
			s.stage2AnimKey = "skinned_mesh_sylas_spell3_bhit_cast";
			s.stage2LockSec = 0.5f;
			s.stage2Rotate = eRotateMode::TowardsCursor;
			s.stageWindowSec = 3.f;
			s.stage2VisualCastFrame = 4.f;
			s.stage2VisualRecoveryFrame = 12.f;
			s.stage2VisualPlaySpeed = 1.f;
		}
```

아래로 교체:

```cpp
		s.castHookId = hookId;
		if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
		{
			s.stageCount = 2;
			s.stage2TargetMode = eTargetMode::UnitTarget;
			s.stage2AnimKey = "skinned_mesh_sylas_attack_passive";
			s.stage2LockSec = 0.55f;
			s.stage2Rotate = eRotateMode::TowardsCursor;
			s.stage2VisualCastFrame = 4.f;
			s.stage2VisualRecoveryFrame = 12.f;
			s.stage2VisualPlaySpeed = 1.f;
		}
		else if (slot == static_cast<u8_t>(eSkillSlot::E))
		{
			s.stageCount = 2;
			s.stage2TargetMode = eTargetMode::Direction;
			s.stage2AnimKey = "skinned_mesh_sylas_spell3_bhit_cast";
			s.stage2LockSec = 0.5f;
			s.stage2Rotate = eRotateMode::TowardsCursor;
			s.stageWindowSec = 3.f;
			s.stage2VisualCastFrame = 4.f;
			s.stage2VisualRecoveryFrame = 12.f;
			s.stage2VisualPlaySpeed = 1.f;
		}
```

### 2-7. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

기존 코드:

```cpp
    case eReplicatedActionId::BasicAttack:
        animName = PrefixAnim(*cd,
            animationChampion == eChampion::SYLAS && actionStage >= 2u
                ? "skinned_mesh_sylas_attack_passive"
                : cd->basicAttackKey);
        break;
```

아래로 교체:

```cpp
    case eReplicatedActionId::BasicAttack:
    {
        const SkillDef* def = CSkillRegistry::Instance().Find(
            animationChampion,
            static_cast<u8_t>(eSkillSlot::BasicAttack));
        if (!def)
        {
            def = FindSkillDef(
                animationChampion,
                static_cast<u8_t>(eSkillSlot::BasicAttack));
        }
        const char* pAnimKey = actionStage >= 2u && def && def->stage2AnimKey
            ? def->stage2AnimKey
            : cd->basicAttackKey;
        animName = PrefixAnim(*cd, pAnimKey);
        break;
    }
```

### 2-8. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Sylas/passive_ba.wfx

기존 코드:

```json
    {
      "name": "passive_core_flash",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/particles/sylas_base_p_swipe_highlight.png",
      "max_particles": 1,
      "spawn_rate": 0.0,
      "lifetime": 0.24,
      "fade_in": 0.01,
      "fade_out": 0.16,
      "width": 2.2,
      "height": 2.2,
      "color": [2.0, 1.62, 0.82, 0.70],
      "attach_offset": [0.0, 1.05, 0.35],
      "billboard": true,
      "blockable_by_wind_wall": false
    }
```

아래로 교체:

```json
    {
      "name": "passive_recall_electricity",
      "render_type": "Billboard",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Sylas/particles/sylas_base_recall_electricity.png",
      "max_particles": 1,
      "spawn_rate": 0.0,
      "lifetime": 0.36,
      "fade_in": 0.01,
      "fade_out": 0.18,
      "width": 2.8,
      "height": 2.8,
      "color": [1.75, 1.38, 0.92, 0.78],
      "attach_offset": [0.0, 1.05, 0.0],
      "billboard": true,
      "atlas_cols": 2,
      "atlas_rows": 2,
      "atlas_frame_count": 4,
      "atlas_fps": 12.0,
      "atlas_loop": false,
      "blockable_by_wind_wall": false
    }
```

### 2-9. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/AnnieSimComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/SylasSimComponent.h"
#include "Shared/GameSim/Components/AnnieSimComponent.h"
```

기존 `RunCombatActionGenerationProbe` 함수 바로 아래에 추가:

```cpp
    bool_t RunSylasPassiveBasicAttackProbe()
    {
        CWorld world;
        DeterministicRng rng(2026071701ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        auto executor = CDefaultCommandExecutor::Create();

        const EntityID sylas = SpawnChampion(
            world, entityMap, eChampion::SYLAS,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID target = SpawnChampion(
            world, entityMap, eChampion::JAX,
            static_cast<u8_t>(eTeam::Red), 5u);
        world.GetComponent<TransformComponent>(sylas).SetPosition(Vec3{});
        world.GetComponent<TransformComponent>(target).SetPosition(
            Vec3{ 1.f, 0.f, 0.f });

        GameCommand castQ{};
        castQ.kind = eCommandKind::CastSkill;
        castQ.issuerEntity = sylas;
        castQ.slot = static_cast<u8_t>(eSkillSlot::Q);
        castQ.sequenceNum = 1u;
        castQ.issuedAtTick = 1ull;
        TickContext armTick = MakeProbeTickContext(
            castQ.issuedAtTick, rng, entityMap, walkable);
        const CommandExecutionResult rejectedCast =
            executor->ExecuteCommand(world, armTick, castQ);
        if (rejectedCast.state != eCommandExecutionState::Rejected ||
            world.HasComponent<SylasSimComponent>(sylas))
        {
            std::printf("[SimLab][SylasPassive] FAIL: rejected skill created a stack\n");
            return false;
        }

        world.GetComponent<SkillRankComponent>(sylas).ranks[
            static_cast<u8_t>(eSkillSlot::Q)] = 1u;
        castQ.sequenceNum = 2u;
        castQ.issuedAtTick = 2ull;
        armTick = MakeProbeTickContext(
            castQ.issuedAtTick, rng, entityMap, walkable);
        const CommandExecutionResult acceptedCast =
            executor->ExecuteCommand(world, armTick, castQ);
        if (acceptedCast.state != eCommandExecutionState::Accepted ||
            !world.HasComponent<SylasSimComponent>(sylas) ||
            world.GetComponent<SylasSimComponent>(sylas).passiveStacks != 1u)
        {
            std::printf("[SimLab][SylasPassive] FAIL: accepted skill did not create a stack\n");
            return false;
        }
        for (u32_t i = 0u; i < 3u; ++i)
            SylasGameSim::ArmPassiveOnSkillCast(world, armTick, sylas);

        if (!world.HasComponent<SylasSimComponent>(sylas))
        {
            std::printf("[SimLab][SylasPassive] FAIL: passive state missing\n");
            return false;
        }
        const auto& capped = world.GetComponent<SylasSimComponent>(sylas);
        if (capped.passiveStacks != 3u ||
            std::fabs(capped.passiveRemainingSec - 5.f) > 0.001f)
        {
            std::printf(
                "[SimLab][SylasPassive] FAIL: JSON cap/window mismatch stacks=%u time=%.3f\n",
                static_cast<unsigned>(capped.passiveStacks),
                capped.passiveRemainingSec);
            return false;
        }

        TickContext beforeExpiry = armTick;
        beforeExpiry.fDt = 4.99f;
        SylasGameSim::Tick(world, beforeExpiry);
        if (world.GetComponent<SylasSimComponent>(sylas).passiveStacks != 3u)
        {
            std::printf("[SimLab][SylasPassive] FAIL: stack expired before five seconds\n");
            return false;
        }
        TickContext afterExpiry = armTick;
        afterExpiry.fDt = 0.02f;
        SylasGameSim::Tick(world, afterExpiry);
        if (world.GetComponent<SylasSimComponent>(sylas).passiveStacks != 0u)
        {
            std::printf("[SimLab][SylasPassive] FAIL: stack survived five-second window\n");
            return false;
        }

        for (u32_t i = 0u; i < 3u; ++i)
            SylasGameSim::ArmPassiveOnSkillCast(world, armTick, sylas);
        TickContext ageTick = armTick;
        ageTick.fDt = 1.f;
        SylasGameSim::Tick(world, ageTick);
        const f32_t remainingBeforeAttack =
            world.GetComponent<SylasSimComponent>(sylas).passiveRemainingSec;

        GameCommand passiveAttack{};
        passiveAttack.kind = eCommandKind::BasicAttack;
        passiveAttack.issuerEntity = sylas;
        passiveAttack.targetEntity = target;
        passiveAttack.direction = Vec3{ 1.f, 0.f, 0.f };
        passiveAttack.sequenceNum = 3u;
        passiveAttack.issuedAtTick = 30ull;
        TickContext attackTick = MakeProbeTickContext(
            passiveAttack.issuedAtTick, rng, entityMap, walkable);
        const CommandExecutionResult passiveResult =
            executor->ExecuteCommand(world, attackTick, passiveAttack);
        const auto& passiveState = world.GetComponent<SylasSimComponent>(sylas);
        if (passiveResult.state != eCommandExecutionState::Accepted ||
            passiveState.passiveStacks != 2u ||
            std::fabs(passiveState.passiveRemainingSec - remainingBeforeAttack) > 0.001f ||
            !world.HasComponent<CombatActionComponent>(sylas))
        {
            std::printf("[SimLab][SylasPassive] FAIL: passive BA consume/remaining mismatch\n");
            return false;
        }

        const CombatActionComponent passiveAction =
            world.GetComponent<CombatActionComponent>(sylas);
        if (passiveAction.uStage != 2u ||
            (passiveAction.uFlags & CombatActionFlags::SylasPassive) == 0u)
        {
            std::printf("[SimLab][SylasPassive] FAIL: passive BA stage/flag mismatch\n");
            return false;
        }

        TickContext impactTick = MakeProbeTickContext(
            passiveAction.uImpactTick, rng, entityMap, walkable);
        CCombatActionSystem::Execute(world, impactTick);
        u32_t passiveEffectCount = 0u;
        world.ForEach<ReplicatedEventComponent>(
            [&](EntityID, ReplicatedEventComponent& event)
            {
                const u8_t eventStage = static_cast<u8_t>(
                    (event.flags >> 12) & 0x0fu);
                if (event.kind == eReplicatedEventKind::EffectTrigger &&
                    event.sourceEntity == sylas &&
                    event.slot == static_cast<u8_t>(eSkillSlot::BasicAttack) &&
                    eventStage == 2u)
                {
                    ++passiveEffectCount;
                }
            });
        if (passiveEffectCount != 1u)
        {
            std::printf(
                "[SimLab][SylasPassive] FAIL: passive BA cue count=%u\n",
                passiveEffectCount);
            return false;
        }

        if (!SylasGameSim::TryConsumePassiveBasicAttack(world, sylas) ||
            !SylasGameSim::TryConsumePassiveBasicAttack(world, sylas) ||
            SylasGameSim::TryConsumePassiveBasicAttack(world, sylas))
        {
            std::printf("[SimLab][SylasPassive] FAIL: one-stack-per-BA consume mismatch\n");
            return false;
        }

        TickContext endTick = MakeProbeTickContext(
            passiveAction.uEndTick, rng, entityMap, walkable);
        CCombatActionSystem::Execute(world, endTick);
        auto& baSlot = world.GetComponent<SkillStateComponent>(sylas).slots[
            static_cast<u8_t>(eSkillSlot::BasicAttack)];
        baSlot.cooldownRemaining = 0.f;
        baSlot.cooldownDuration = 0.f;
        GameCommand baseAttack = passiveAttack;
        baseAttack.sequenceNum = 4u;
        baseAttack.issuedAtTick = passiveAction.uEndTick + 1ull;
        TickContext baseTick = MakeProbeTickContext(
            baseAttack.issuedAtTick, rng, entityMap, walkable);
        const CommandExecutionResult baseResult =
            executor->ExecuteCommand(world, baseTick, baseAttack);
        if (baseResult.state != eCommandExecutionState::Accepted ||
            !world.HasComponent<CombatActionComponent>(sylas))
        {
            std::printf("[SimLab][SylasPassive] FAIL: base BA was not accepted\n");
            return false;
        }
        const auto& baseAction = world.GetComponent<CombatActionComponent>(sylas);
        if (baseAction.uStage != 1u ||
            (baseAction.uFlags & CombatActionFlags::SylasPassive) != 0u)
        {
            std::printf("[SimLab][SylasPassive] FAIL: base BA stage contract mismatch\n");
            return false;
        }

        std::printf(
            "[SimLab][SylasPassive] PASS: JSON 3-stack/5s, consume, BA stage 1/2, one cue\n");
        return true;
    }
```

`--kalista-projectile-only` 분기 바로 위에 추가:

```cpp
    if (argc > 1 && std::strcmp(argv[1], "--sylas-passive-only") == 0)
    {
        std::printf("[SimLab] Sylas passive basic-attack probe only\n");
        RegisterAllChampionHooks();
        const bool_t bPass = RunSylasPassiveBasicAttackProbe();
        std::printf("[SimLab] %s\n", bPass ? "PASS" : "FAIL");
        return bPass ? 0 : 1;
    }
```

기존 코드:

```cpp
    const bool_t bCombatActionGenerationProbePass =
        RunCombatActionGenerationProbe();
    const bool_t bKalistaProjectileAuthorityProbePass =
```

아래로 교체:

```cpp
    const bool_t bCombatActionGenerationProbePass =
        RunCombatActionGenerationProbe();
    const bool_t bSylasPassiveBasicAttackProbePass =
        RunSylasPassiveBasicAttackProbe();
    const bool_t bKalistaProjectileAuthorityProbePass =
```

기존 코드:

```cpp
        bChampionAIStateGateCommitmentProbePass &&
        bCombatActionGenerationProbePass &&
        bKalistaProjectileAuthorityProbePass &&
```

아래로 교체:

```cpp
        bChampionAIStateGateCommitmentProbePass &&
        bCombatActionGenerationProbePass &&
        bSylasPassiveBasicAttackProbePass &&
        bKalistaProjectileAuthorityProbePass &&
```

생성 산출물은 직접 편집하지 않는다. canonical JSON 변경 후 `Build-LoLDefinitionPack.py`로 생성하고 `--check`로 재검증한다. Bot AI는 계속 GameCommand 생산자이며 스택·stage·피해 진실을 직접 변경하지 않는다.

## 3. 검증

예측:

- definition pack hash와 visual definition hash는 canonical JSON 변경 때문에 새 값으로 한 번 바뀌고, 즉시 `--check`에서 동일 값으로 고정된다.
- `--sylas-passive-only`는 `JSON 3-stack/5s, consume, BA stage 1/2, one cue`를 출력한다. 4번째 적립이 3을 넘거나, 4.99초 전에 만료되거나, BA 소모가 시간을 5초로 재설정하거나, stage 2 cue가 0/2회면 실패한다.
- 요청 PNG는 2x2 atlas 4프레임으로 사일러스 중심에 additive 재생되고, 기존 torus/swing mesh는 유지된다. 기존 베이킹 애니메이션 파일이 없을 때만 재베이킹이 필요하다.
- 서버가 거절한 스킬은 `ArmPassiveOnSkillCast` 이후 지점에 도달하지 않으므로 스택이 쌓이지 않는다.

검증 명령:

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Tools/SimLab/SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
Tools/Bin/Debug/SimLab.exe --sylas-passive-only
Tools/Bin/Debug/SimLab.exe
git diff --check
```

미검증:

- Client/Server 전체 빌드는 실행하지 않는다. SimLab 프로젝트 종속성으로 Engine/GameSim이 함께 빌드되는 것은 허용한다.
- 실제 인게임에서 Passive BA 애니메이션의 체감 타이밍, 전기 atlas의 크기·높이·색은 F5 육안 검증이 남는다.

확인 필요:

- 육안 검증 후 `width/height 2.8`, 중심 높이 `1.05`, 12fps가 캐릭터 실루엣에 맞는지 결정한다.
- Passive BA의 별도 광역 피해/마법 피해와 스택 HUD가 추가 명세인지 확인한다. 이번 변경은 요청된 스택·애니메이션·이펙트 계약만 적용한다.
- 검증 예산은 바닥 70%(codegen·집중/전체 SimLab), 천장 30%(F5 비교 캡처)로 고정하고 공개 가능한 인게임 캡처 마감은 2026-07-18로 둔다.
