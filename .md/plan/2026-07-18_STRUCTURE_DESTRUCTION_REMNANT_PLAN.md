# 2026-07-18 구조물 파괴 잔해(포탑/억제기) 계획서

```text
Session - 포탑/억제기 파괴 시 전체 소멸 대신 파괴 상태 모델 잔존 — 잔해는 비피격(TargetableTag 사망 계약)·비공격(기존 AI 게이트), Codex S035 인프라 완성 배선
좌표: 신규 좌표 후보 · 축: C4 수명은 선언된다, C7 권위와 정합성
관련: Plan/S035_GAME_END_LIFECYCLE_AND_TURRET_ZFIGHT_SESSION_20260715.md(visibilityStates 등록·전체숨김의 출처), Codex 2026-07-18 조작감 보고(갭 5종 좌표)
```

## 1. 결정 기록

```text
① 문제·제약: 파괴 시 SnapshotApplier가 모델 전체를 숨겨(1666행 hp>0 게이트) 맵에 구멍. S035가 등록한 포탑 destroyed 상태는 7개 변형 전부 visibleWhenDestroyed=true라 숨김을 풀면 8겹 Z-fight 재발. 억제기 visibilityStates는 빈 배열. 클라가 사망 무시하고 TargetableTag 무조건 재부착(419행). 구조물 사망 시 서버 TargetableTag 제거 계약 부재. 구조물 파괴 SimLab 프로브 0개.
② 순진한 해법의 실패: 숨김만 제거 → 8서브메시 중첩 Z-fight(S035가 잡은 결함 재발). 현행 스키마로 잔해 1개만 표시 → 불가능: 상태당 표현이 항상표시/생존전용/파괴전용 3종뿐이라 "양 상태 모두 숨김"(잔여 변형 6종에 필요)을 못 쓴다.
③ 메커니즘: 상태 스키마에 visibleWhenAlive(기본값 = !visibleWhenDestroyed → 기존 항목 의미 불변) 추가 → 마스크 판정식을 bDestroyed ? bVisibleWhenDestroyed : bVisibleWhenAlive로. 포탑 = [0]Base 생존전용 + [3]Stage3Stump 파괴전용 + 나머지 6종 양측 숨김(8상태 = kVisualSubmeshStateCount 상한 정확 일치). 억제기 = 2서브메시(wmesh 실측, 넥서스와 동형) → [0] 파괴전용/[1] 생존전용. 전체숨김 블록 삭제(마스크 경로가 소유). 서버는 DeathSystem 첫 사망 블록에서 구조물 한정 TargetableTag 제거 + 소생 시 복구. 클라 태그는 hp 기반 bTargetable 파라미터로 일치화. 신규 프로브가 사망 계약(엔티티 잔존·비피격·BA DeadTarget 거절·소생 복구)을 고정.
④ 대조: 넥서스는 이미 동일 계보(visibleWhenDestroyed 스왑)로 파괴 연출 사용 중 — 포탑/억제기를 같은 경로에 합류시키는 것. LoL 원본도 잔해(스텀프) 잔존+비타겟. 서버 권위: 비피격 진실은 서버 태그 제거가 정본, 클라 태그는 표시/호버 정합용.
⑤ 대가: 잔해 서브메시 선택([3] Stump, 억제기 [0]/[1] 방향)은 익명 서브메시라 시각 게이트 전 확정 불가 — 틀리면 JSON 스왑+재생성으로 교정(코드 불변). 억제기 부활(LoL 원본 규칙)은 미구현 상태 유지 — 도입 시 DeathSystem 소생 분기가 태그를 복구하므로 계약은 선제 대응됨. 파괴 FX/사운드는 기존 경로 유지(비접촉).
```

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Client/Private/Data/LoLVisualDefinitionPack.h

기존 코드(91-95행):

```cpp
    struct StructureVisualSubmeshStateDef
    {
        u32_t submeshIndex = 0u;
        bool_t bVisibleWhenDestroyed = false;
    };
```

아래로 교체:

```cpp
    struct StructureVisualSubmeshStateDef
    {
        u32_t submeshIndex = 0u;
        bool_t bVisibleWhenDestroyed = false;
        bool_t bVisibleWhenAlive = false;
    };
```

### 2-2. C:/Users/user/Desktop/Winters/Tools/LoLData/Build-LoLDefinitionPack.py

(a) 파서 — 기존 코드(1197-1209행) `visible_when_destroyed` 파싱 아래로 교체(visibleWhenAlive 추가, 기본값 = not visible_when_destroyed):

```python
            visible_when_destroyed = state.get("visibleWhenDestroyed", False)
            if not isinstance(visible_when_destroyed, bool):
                fail(
                    f"structures[{index}].visibilityStates[{state_index}].visibleWhenDestroyed "
                    "must be a bool"
                )
            visible_when_alive = state.get("visibleWhenAlive", not visible_when_destroyed)
            if not isinstance(visible_when_alive, bool):
                fail(
                    f"structures[{index}].visibilityStates[{state_index}].visibleWhenAlive "
                    "must be a bool"
                )
            states.append(
                {
                    "name": str(state.get("name", "")),
                    "submeshIndex": submesh_index,
                    "visibleWhenDestroyed": visible_when_destroyed,
                    "visibleWhenAlive": visible_when_alive,
                }
            )
```

(b) 이미터 — 기존 코드(2274-2281행) 상태 대입 루프에 `bVisibleWhenAlive` 대입 1줄 추가.

### 2-3. C:/Users/user/Desktop/Winters/Data/LoL/ClientPublic/Visual/ObjectVisualDefs.json

- 억제기 blue/red: `"visibilityStates": []` → `[{Destroyed, 0, visibleWhenDestroyed true}, {Alive, 1, visibleWhenDestroyed false}]` (넥서스 동형; 방향 확인 필요 → 시각 게이트)
- 포탑 blue/red: 7상태 → 8상태 재구성: `[0]Base false`(생존전용) 신설, `[3]Stage3Stump true`(잔해) 유지, `[1][2][4][5][6][7]` → `visibleWhenDestroyed false + visibleWhenAlive false`(양측 숨김)

### 2-4. C:/Users/user/Desktop/Winters/Client/Private/Manager/Structure_Manager.cpp

기존 코드(24-29행):

```cpp
        const bool_t bDestroyed = structure.hp <= 0.f;
        for (u8_t i = 0u; i < pVisual->submeshStateCount; ++i)
        {
            const ClientData::StructureVisualSubmeshStateDef& state = pVisual->submeshStates[i];
            SetSubmeshVisible(mask, state.submeshIndex, state.bVisibleWhenDestroyed == bDestroyed);
        }
```

아래로 교체:

```cpp
        const bool_t bDestroyed = structure.hp <= 0.f;
        for (u8_t i = 0u; i < pVisual->submeshStateCount; ++i)
        {
            const ClientData::StructureVisualSubmeshStateDef& state = pVisual->submeshStates[i];
            SetSubmeshVisible(mask, state.submeshIndex,
                bDestroyed ? state.bVisibleWhenDestroyed : state.bVisibleWhenAlive);
        }
```

### 2-5. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

(a) `EnsureSnapshotStructureRuntimeTags` — 시그니처에 `bool_t bTargetable` 추가, 기존 코드(418-419행):

```cpp
        if (!world.HasComponent<TargetableTag>(entity))
            world.AddComponent<TargetableTag>(entity);
```

아래로 교체:

```cpp
        if (bTargetable)
        {
            if (!world.HasComponent<TargetableTag>(entity))
                world.AddComponent<TargetableTag>(entity);
        }
        else if (world.HasComponent<TargetableTag>(entity))
        {
            world.RemoveComponent<TargetableTag>(entity);
        }
```

호출 2곳: 스냅샷 갱신 경로(1653행) `es->hp() > 0.f` 전달, 스폰 경로(TryBindStageStructureVisual 내) `true` 전달.

(b) 전체숨김 블록 — 기존 코드(1661-1667행):

```cpp
            // 포탑/억제기는 파괴 서브메시 상태가 없어 사망 시 본체 메시를 숨긴다.
            // (넥서스는 Structure_Manager 의 visibleWhenDestroyed 서브메시 스왑 경로 유지)
            if (kind != Shared::Schema::EntityKind::Nexus &&
                world.HasComponent<RenderComponent>(e))
            {
                world.GetComponent<RenderComponent>(e).bVisible = es->hp() > 0.f;
            }
```

삭제 (포탑/억제기도 Structure_Manager 상태 마스크 경로가 소유 — 스테일 주석 포함 제거).

### 2-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Death/DeathSystem.cpp

include 블록에 `#include "Shared/GameSim/Components/GameplayComponents.h"` 추가. 첫 사망 블록 — 기존 코드(40-41행) `SkillChargeStateComponent` 제거 아래에 추가:

```cpp
                if (world.HasComponent<StructureComponent>(entity) &&
                    world.HasComponent<TargetableTag>(entity))
                {
                    world.RemoveComponent<TargetableTag>(entity);
                }
```

소생 분기 — 기존 코드(44-47행):

```cpp
        else if (health.bIsDead)
        {
            health.bIsDead = false;
        }
```

아래로 교체:

```cpp
        else if (health.bIsDead)
        {
            health.bIsDead = false;

            if (world.HasComponent<StructureComponent>(entity) &&
                !world.HasComponent<TargetableTag>(entity))
            {
                world.AddComponent<TargetableTag>(entity);
            }
        }
```

### 2-7. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp — RunStructureDestructionRemnantProbe 신설

RunCombatActionGenerationProbe 스캐폴드(2219-2256행 동형)로: 공격자 챔피언 + 포탑 엔티티(Transform/Structure/Turret/Health 300/TargetableTag) 구성 → ①생존: CanBeTargetedBy true ②hp=0 + CDeathSystem::Execute → 엔티티 잔존(IsAlive true)·bIsDead·TargetableTag 부재·CanBeTargetedBy false ③BA 명령 → Rejected(DeadTarget) ④hp=100 소생 → bIsDead false·TargetableTag 복구. 실행 목록에 등록. (전문은 반영 시 작성 — 스캐폴드 함수/헬퍼 시그니처는 실코드 확인 완료)

### 재생성

```text
python Tools/LoLData/Build-LoLDefinitionPack.py && --check   # LoLVisualDefinitions.generated.cpp + 미러/manifest/parity 갱신
```

## 3. 검증

```text
예측:
- 생성 cpp에 bVisibleWhenAlive 대입 등장(포탑 16상태 = 8×2팀, 억제기 4상태). 빌드 PASS. 신규 프로브 PASS + 기존 전체 PASS(계약 해시 불변 — 스킬 데이터 비접촉).
- 인게임: 포탑 파괴 → 스텀프 잔해 1종만 표시(Z-fight 없음), 억제기 파괴 → 파괴 서브메시 표시, 잔해 호버/우클릭 무반응(비피격), 파괴 포탑 공격 없음(기존 AI 게이트). 잔해 서브메시 방향이 어색하면 JSON 스왑 게이트.
- 깨질 수 있는 것: 넥서스 경로(bNexus 분기 기존 유지 — 비접촉이나 마스크 판정식 공유) → 넥서스 상태 2종은 visibleWhenAlive 기본값(!destroyed)이라 의미 불변; 게이트 = 기존 게임종료 연출 육안.
- Bot AI 경계: Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다 — 본 변경은 구조물 수명/태그/시각 상태만 다룬다.

검증 명령:
- python Tools/LoLData/Build-LoLDefinitionPack.py && python Tools/LoLData/Build-LoLDefinitionPack.py --check
- msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m:1
- Tools/Bin/Debug/SimLab.exe 600 1234  (신규 프로브 포함 exit 0)
- 인게임: 포탑/억제기 파괴 후 잔해·비피격·비공격 육안 게이트

미검증:
- 잔해 서브메시 선택([3]/억제기 [0]) — 시각 게이트 전 확정 불가, 오선택 시 JSON 스왑.

확인 필요:
- 없음 (서버 엔티티 잔존·포탑 AI 게이트는 Codex 반영분 — 프로브가 잔존을 고정)
```

## 다음 슬라이스

- 잔해 시각 게이트 후 서브메시 확정, 억제기 부활 규칙(LoL 원본) 도입 시 리스폰 타이머 + 태그 복구 활용.
