# 2026-07-18 지속 평타(어택 체이스 영속화) 계획서

```text
Session - 우클릭/A클릭 공격 명령을 영속 타깃 주문으로 승격: 다른 명령 전까지 BA 반복, 사거리 밖이면 추격, 대상 사망 시 idle
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성, C4 수명은 선언된다
관련: 2026-07-18_IRELIA_VIEGO_EZREAL_STAGE_INPUT_REGRESSION_RECOVERY_PLAN(병행·CommandExecutor 편집 중), 2026-07-18_CHAMPIONAI_MID_DEFENSE_LIFECYCLE_PLAN(병행·봇 AI 계약 동결)
```

## 1. 결정 기록

```text
① 문제·제약: 공격 명령이 1회성 — AttackChaseSystem이 BA 1발 발사 직후 컴포넌트를 삭제(AttackChaseSystem.cpp:364)하고, 사거리 내 명령은 애초에 영속 상태를 만들지 않음(CommandExecutor.cpp:3232 ClearAttackChase). 클라도 전송 직후 intent 소거(Scene_InGameInput.cpp:865). 제약: CommandExecutor.cpp(+272줄)·CombatActionSystem.cpp(+19줄)가 병행 세션 미커밋 편집 중, 봇 AI 계약(MidDefense 해시 1735AF9283C9F02E)은 다른 병행 세션이 동결.
② 순진한 해법의 실패: (a) 클라 재전송 복구(865행 Clear 제거) → 초당 ~10회 명령 스팸+쿨다운 리젝 소음, 지속성이 클라 프레임에 종속, 서버 권위 원칙 역행. (b) 무조건 서버 영속화 → 봇 BA도 영속화되어 AI가 명령을 멈춰도 공격이 계속됨 = ChampionAI 프로브·병행 미드래치 세션과 충돌.
③ 메커니즘: AttackChaseComponent에 bSustain 추가. 플레이어(=ChampionAIComponent 없음, ConfigureChampionControlRole이 봇에만 부여: GameRoomSpawn.cpp:606-660) BA 명령만 bSustain=true로 영속화 — HandleBasicAttack이 쿨다운 게이트 전에 주문을 upsert, AttackChaseSystem은 발사 후 bSustain이면 유지. 취소는 기존 매트릭스 재사용(Move/Cast/Recall/Flash/Ward의 ClearAttackChase 7개소 + CC 인터럽트 + 대상 사망 abort). 큐된 이동 소비 지점(CombatActionSystem.cpp:417)에만 해제 1개 추가(Queue 정책 챔피언의 이동 명령이 ClearAttackChase에 도달 못 하는 구멍).
④ 대조: LoL 원본 = 서버가 공격 주문을 영속 보유(클라는 1회 전송). 봇은 ChampionAISystem이 매 틱 재발행하는 기존 방식 유지 — 이원화가 아니라 "주문 소유자" 구분(플레이어=서버 영속, 봇=AI 재결정). 신규 커맨드 종류 불필요(BasicAttack=3 재사용, Command.fbs 비접촉).
⑤ 대가: bSustain 분기로 AttackChase 수명 규칙이 두 갈래(1회성/영속) — 봇·스킬추격을 영속으로 통일하는 날 이 분기는 제거 대상. 스턴/에어본 CC는 기존 규칙대로 주문 자체를 소거(LoL은 CC 후 재개) — 재개 원하면 후속. A+땅클릭은 클릭 시점 타깃 획득만 지원(걸어가며 자동 획득하는 진짜 어택무브는 범위 외). 신규 지속 루프를 커버하는 SimLab 프로브 없음 = 기계 게이트 공백.
```

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/AttackChaseComponent.h

기존 코드:

```cpp
    f32_t repathTimer = 0.f;
    bool_t bActive = false;
};
```

아래로 교체:

```cpp
    f32_t repathTimer = 0.f;
    bool_t bActive = false;
    bool_t bSustain = false;
};
```

### 2-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

(a) `StartAttackChase`(277행) 바로 위에 추가 (익명 네임스페이스):

```cpp
    bool_t ShouldSustainAttackChase(CWorld& world, const GameCommand& cmd)
    {
        return cmd.kind == eCommandKind::BasicAttack &&
            !world.HasComponent<ChampionAIComponent>(cmd.issuerEntity);
    }

    AttackChaseComponent& UpsertAttackChaseOrder(
        CWorld& world, const GameCommand& cmd, f32_t effectiveRange)
    {
        auto& chase = world.HasComponent<AttackChaseComponent>(cmd.issuerEntity)
            ? world.GetComponent<AttackChaseComponent>(cmd.issuerEntity)
            : world.AddComponent<AttackChaseComponent>(cmd.issuerEntity, AttackChaseComponent{});

        chase.target = cmd.targetEntity;
        chase.sequenceNum = cmd.sequenceNum;
        chase.commandKind = static_cast<u8_t>(cmd.kind);
        chase.slot = cmd.slot;
        chase.itemId = cmd.itemId;
        chase.effectiveRange = effectiveRange;
        chase.groundPos = cmd.groundPos;
        chase.direction = cmd.direction;
        chase.repathTimer = 0.f;
        chase.bActive = true;
        chase.bSustain = ShouldSustainAttackChase(world, cmd);
        return chase;
    }
```

(b) `StartAttackChase` 본문 필드 세팅 블록 — 기존 코드(280-293행):

```cpp
        auto& chase = world.HasComponent<AttackChaseComponent>(cmd.issuerEntity)
            ? world.GetComponent<AttackChaseComponent>(cmd.issuerEntity)
            : world.AddComponent<AttackChaseComponent>(cmd.issuerEntity, AttackChaseComponent{});

        chase.target = cmd.targetEntity;
        chase.sequenceNum = cmd.sequenceNum;
        chase.commandKind = static_cast<u8_t>(cmd.kind);
        chase.slot = cmd.slot;
        chase.itemId = cmd.itemId;
        chase.effectiveRange = effectiveRange;
        chase.groundPos = cmd.groundPos;
        chase.direction = cmd.direction;
        chase.repathTimer = 0.f;
        chase.bActive = true;
```

아래로 교체:

```cpp
        UpsertAttackChaseOrder(world, cmd, effectiveRange);
```

(c) `HandleBasicAttack` — 기존 코드(same-team 게이트, 3150-3156행) 아래에 추가 (쿨다운 게이트 3158행 이전 — 스윙 중 클릭한 주문도 보존):

```cpp
    const bool_t bSustainOrder = ShouldSustainAttackChase(world, cmd);
    if (bSustainOrder)
        UpsertAttackChaseOrder(world, cmd, 0.f);
```

(효과사거리 0 = AttackChaseSystem이 :309-313 폴백으로 매 틱 재계산; 이동 시드는 다음 틱 repath 블록이 담당)

(d) `HandleBasicAttack` 사거리 내 경로 — 기존 코드(3232행):

```cpp
    ClearAttackChase(world, cmd.issuerEntity);
```

아래로 교체:

```cpp
    if (!bSustainOrder)
        ClearAttackChase(world, cmd.issuerEntity);
```

### 2-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/AttackChase/AttackChaseSystem.cpp

BA 발사 분기 — 기존 코드(362-364행):

```cpp
                outCommands.push_back(MakeBasicAttackCommand(
                    tc, entity, chase.target, chase.sequenceNum, selfPos, targetPos));
                world.RemoveComponent<AttackChaseComponent>(entity);
```

아래로 교체 (영속 주문은 유지 → 쿨다운 경과 후 같은 분기가 재발사):

```cpp
                outCommands.push_back(MakeBasicAttackCommand(
                    tc, entity, chase.target, chase.sequenceNum, selfPos, targetPos));
                if (!chase.bSustain)
                    world.RemoveComponent<AttackChaseComponent>(entity);
```

### 2-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Combat/CombatActionSystem.cpp

include 블록 — 기존 코드(11행):

```cpp
#include "Shared/GameSim/Components/ActionStateComponent.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Components/AttackChaseComponent.h"
```

큐된 이동 소비 분기 — 기존 코드(417-427행):

```cpp
        if (action.bImpactIssued && action.bQueuedMove)
        {
            TryAssignQueuedMoveTarget(
                world,
                tc,
                entity,
                action.vQueuedMoveTarget,
                action.vQueuedMoveDirection);
            world.RemoveComponent<CombatActionComponent>(entity);
            continue;
        }
```

아래로 교체 (큐된 이동 = 플레이어의 이동 의사 → 지속 공격 주문 해제):

```cpp
        if (action.bImpactIssued && action.bQueuedMove)
        {
            TryAssignQueuedMoveTarget(
                world,
                tc,
                entity,
                action.vQueuedMoveTarget,
                action.vQueuedMoveDirection);
            if (world.HasComponent<AttackChaseComponent>(entity))
                world.RemoveComponent<AttackChaseComponent>(entity);
            world.RemoveComponent<CombatActionComponent>(entity);
            continue;
        }
```

### 비접촉 (설계상 무변경 확인)

- 클라이언트(Scene_InGameInput.cpp) — 1회 전송 유지, 서버가 영속. 애니는 ActionStateComponent.sequence 증가로 자동 재생(스냅샷 기존 필드).
- Command.fbs / 스키마 코드젠 — 신규 커맨드 종류 없음.
- 취소 매트릭스 — HandleMove/HandleCastSkill/HandleRecall/HandleFlash/HandleUseItem의 기존 ClearAttackChase 7개소, StatusEffectSystem CC 인터럽트, AttackChaseSystem abort(대상 사망→ClearMoveTarget+제거→idle), GameRoomCommands 사망 청소 — 전부 그대로 재사용.

## 3. 검증

```text
예측:
- 빌드 PASS(신규 파일 없음). SimLab: RunCombatActionGenerationProbe PASS 예측 — 프로브의 후속 CastSkill/Move가 쿨다운(≈1s) 경과 전에 chase를 소거하므로 재발사 미발생. ChampionAI 전 프로브 불변(봇=bSustain false) — MidDefense 해시 1735AF9283C9F02E 불변이 병행 세션 비충돌의 판정 기준.
- 인게임(서버+클라): ①적 우클릭 1회 → 쿨다운 주기로 BA 반복, 대상이 사거리를 벗어나면 추격 후 재개 ②이동/스킬/귀환/플래시 → 즉시 중단 ③대상 사망 → 추격 해제·제자리 idle(다음 타깃 자동 획득 없음) ④스윙 중(쿨다운 중) 클릭도 주문 보존 → 준비되면 발사.
- 이 변경이 깨뜨릴 수 있는 것: BA 이동정책 Queue 챔피언의 이동-후-복귀(2-4가 게이트), 봇 행동 변화(bSustain 게이트가 차단, ChampionAI 프로브가 게이트). 지속 루프 자체는 커버 프로브 없음 = "게이트 없음" — 인게임 게이트가 유일.
- Bot AI 경계: Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다 — 본 변경은 봇 경로의 수명 규칙을 바꾸지 않는다(bSustain=false 고정).

검증 명령:
- msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m:1
- Tools/Bin/Debug/SimLab.exe 600 1234  (exit 0 + MidDefense hash=1735AF9283C9F02E 불변 + CombatActionGeneration PASS)
- 서버+클라 실행: 위 ①~④ 육안 게이트

미검증:
- 지속 BA 루프 전용 SimLab 프로브 부재(후속 슬라이스 후보).
- CC(스턴) 후 공격 주문 재개는 미구현(기존 규칙 유지 — LoL과 다름, 요청 시 후속).

확인 필요:
- 없음
```

## 다음 슬라이스

- 사거리 밖 BA 지속 루프 SimLab 프로브(추격→발사→재발사→Move 해제 계약).
- 진짜 어택무브(A+땅: 이동 중 자동 타깃 획득), CC 후 주문 재개, DebugDrawSystem에 AttackChase 타깃 라인 시각화.
