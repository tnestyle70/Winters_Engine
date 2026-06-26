# 03. ECS / 데이터주도 정의 파이프라인 — 면접 대비

> 근거 문서: `.md/이력서/WINTERS_DOMAIN_HONEST_MAP_2026-06-26.md` 의 "### 3. ECS / 데이터주도 정의 — working"
> 이 문서의 모든 주장은 코드 파일:라인으로 검증 가능하며, 정직성 지도의 레드플래그를 위반하지 않는다.

---

## 0. 한 줄 본질 + 현재 상태

**한 줄 본질**: "게임 월드의 상태를 *연속 메모리 컴포넌트*로 분해해 시스템이 캐시 친화적으로 순회하고(ECS), 그 시스템이 무엇을 Read/Write하는지 선언하면 충돌 없는 시스템을 자동으로 병렬 묶는 스케줄러를 붙이고(데이터 접근 스케줄러), 챔피언/스킬 수치를 코드에서 떼어 오프라인으로 cook한 불변 Def pack을 dense index로 조회하는(데이터주도) 파이프라인."

**현재 성숙도 (혼재, 정직하게)**:

| 부분 | 상태 | 근거 |
|---|---|---|
| sparse-set 컴포넌트 스토어 | **production** | 매 tick 실제 순회, `CComponentStore<T>` |
| generation EntityHandle | **working** | use-after-free 차단, 단 ForEach는 EntityID 경로 |
| 데이터 접근 충돌 스케줄러 | **working (배치 미발화)** | 코드 경로 완성, 실측 `MaxBatchSize=1` |
| 멀티컴포넌트 ForEach | **working (naive)** | 첫 store 순회 + `Has()` probe, archetype 아님 |
| 불변 Def pack (champion/skill) | **working (부분 cutover)** | `GameplayDefinitionPack` dense index 동작, **fallback 잔존** |
| 오프라인 codegen | **working** | Python → `ChampionGameData.generated.cpp` |
| 결정론 골든 게이트 (SimLab) | **production** | FNV per-tick hash, exit code gate |
| Foundation 추출 / 풀 cutover / hot reload | **planned-only** | 계획서(12, 00 INDEX)만, 코드 0 |

핵심 정직선: **"sparse-set ECS이며 archetype이 아니다. 데이터주도는 진행 중(부분 cutover)이고 fallback 상수가 남아 있다. hot reload는 미구현."** 이걸 먼저 인정하는 게 이 도메인의 메타인지 강점이다.

---

## 1. 핵심 개념 (본질)

### 1.1 왜 ECS인가 — Data-Oriented Design의 1차 원리

전통적 OOP 게임 객체(`class Champion : public GameObject`)는 가상함수 디스패치 + 객체 단위 힙 산재(scattered allocation)로 두 가지 비용을 낳는다:

1. **캐시 미스**: `for (obj : objects) obj->UpdatePosition()` 는 객체마다 64B+ 전체를 캐시라인에 올린다. 하지만 위치 업데이트가 실제로 건드리는 건 `Vec3 position` 12B뿐. 나머지 캐시라인은 낭비된다. CPU는 메모리 레이턴시(수백 사이클)에 묶이고 ALU는 논다.
2. **분기 예측 실패**: 가상함수는 vtable 간접 호출이라 분기 예측기가 못 맞춘다.

ECS의 1차 원리는 **"같이 처리되는 데이터를 같이 둔다(Structure of Arrays)"** 이다. 위치만 모은 배열을 선형 순회하면 캐시라인이 100% 유효 데이터로 채워지고, 하드웨어 prefetcher가 다음 라인을 미리 끌어온다. 이게 "data-oriented"의 본질 — *데이터의 메모리 레이아웃을 CPU 캐시 계층에 맞춰 설계*하는 것이다.

ECS의 3요소:
- **Entity**: 정체성만 가진 ID(여기선 `EntityID = uint32_t`). 데이터 없음.
- **Component**: 순수 데이터 POD(`TransformComponent`, `HealthComponent`). 로직 없음.
- **System**: 컴포넌트 집합을 읽어 변환하는 로직. 상태 없음.

### 1.2 sparse-set vs archetype — 두 진영의 1차 원리

ECS 구현은 크게 두 갈래다. 둘 다 "컴포넌트를 연속 배열로" 라는 목표는 같지만 **엔티티→컴포넌트 매핑**을 다르게 푼다.

**sparse-set (내가 택한 것)**:
- 컴포넌트 타입마다 독립된 store. store는 `sparse[entityId] -> denseIndex`, `dense[]`(entity 목록), `data[]`(컴포넌트 본체) 세 배열.
- `Add`는 dense 끝에 push (O(1)). `Remove`는 swap-and-pop (O(1)) — 마지막 원소를 빈 자리로 옮기고 sparse를 갱신.
- 단일 컴포넌트 순회는 `data[]`를 선형으로 — 완벽한 캐시 지역성.
- **약점**: 멀티 컴포넌트 교집합. `(Transform, Health)`를 동시에 가진 엔티티를 순회하려면 한 store를 돌면서 다른 store에 `Has(entity)` probe를 해야 한다 — 두 번째 store 접근은 sparse 랜덤 액세스라 캐시 지역성이 깨진다.

**archetype/chunk (택하지 않은 것)**:
- 동일한 컴포넌트 조합("archetype")을 가진 엔티티를 하나의 chunk(16KB 등)에 SoA로 묶어 저장.
- 멀티 컴포넌트 쿼리가 chunk 단위 선형 순회라 빠르다.
- **약점**: 컴포넌트 추가/제거가 archetype 이동(메모리 복사)을 유발. 구현 복잡도가 sparse-set의 수 배. (Unity DOTS, EnTT group 등이 이 진영.)

### 1.3 데이터 접근 기반 스케줄러 — 왜 "선언"이 필요한가

멀티스레드에서 두 시스템이 같은 컴포넌트를 동시에 Write하면 data race다. 락으로 막으면 직렬화되고 캐시 라인 ping-pong이 생긴다. 1차 원리적 해법은 **"충돌하는 시스템을 애초에 같은 시각에 안 돌린다"** — 컴파일/등록 시점에 각 시스템이 어떤 컴포넌트를 Read/Write하는지 *선언*하게 하고, 그 선언으로 충돌 그래프를 만들어 비충돌 시스템만 병렬 배치한다. Read-Read는 안전, Write가 끼면 충돌. 이건 Bevy/Unity DOTS의 system scheduler와 같은 발상이다.

### 1.4 데이터주도 + 불변 Def pack — 왜 string lookup을 frame에서 제거하나

밸런스 수치(쿨다운, 사거리, 데미지 계수)를 C++ `constexpr`에 박으면 기획자가 못 만진다(빌드 필요). JSON으로 빼면 기획자 자립이 가능하지만, **매 프레임 `map["irelia"]["q"]["cooldown"]` string lookup은 해시 계산 + 캐시 미스 + 분기**라 30Hz × 수십 엔티티 × 수 resolver에서 비용이 누적된다.

1차 원리적 해법: **"문자열은 load 시점에만, frame은 정수 index만"**. 저작 JSON을 오프라인에서 검증·정렬해 dense 배열로 cook하고(`stable DefinitionKey -> dense DefId`), 엔티티는 spawn 때 한 번 DefId를 해석해 컴포넌트에 저장한다. frame 시스템은 `pack.skills[defId.value - 1]` 직접 인덱싱만 한다. 이게 immutable definition pack 패턴 — match 동안 불변이라 락도 필요 없다.

---

## 2. 왜 이 선택인가 — 기술 스택 선택 이유 + Trade-off

### 2.1 sparse-set vs archetype

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **sparse-set (택함)** | O(1) add/remove, 단일 컴포넌트 순회 완벽한 지역성, 구현 단순(스토어 60줄) | 멀티 컴포넌트 교집합이 `Has()` probe라 두 번째 store 캐시 지역성 손실 | LoL 규모(엔티티 수백 단위)에선 멀티컴포넌트 probe 비용이 무시 가능. 신입 1인 범위에서 archetype 복잡도(chunk 관리/마이그레이션)는 과투자 |
| archetype/chunk | 멀티 컴포넌트 쿼리 chunk 선형 순회로 빠름 | 컴포넌트 추가/제거 시 archetype 이동 복사, 구현 수 배 복잡 | 엔티티 1만+ / 대규모 쿼리에서만 이득. 현재 씬은 드로우콜 ~94, CPU 바운드 진단됨 → 병목이 ECS 순회가 아님 |

**근본 trade-off**: archetype은 "쿼리 속도 ↔ 구조 변경 비용 + 구현 복잡도"를 쿼리 쪽에 베팅한다. 내 워크로드는 엔티티가 적고 컴포넌트 조합이 자주 바뀌지 않아(spawn 때 확정) sparse-set의 단순함이 합리적. **측정으로 ECS 순회가 병목이 아님을 확인했기 때문에**(프로파일러 진단) archetype으로 갈 이유가 아직 없다 — 이게 핵심 방어선이다.

### 2.2 generation handle vs raw pointer / unique_ptr

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **EntityHandle (index+generation, 택함)** | use-after-free를 generation mismatch로 검출, 64bit 값 복사라 DLL 경계 안전, 네트워크/저장 불가능한 lifetime id | resolve 한 단계(slot 검사) 비용 | 서버권위 + 멀티스레드 + DLL 경계에서 dangling 안전이 필수. handle은 "죽은 엔티티를 가리키면 조용히 실패" |
| raw `EntityID` (index만) | resolve 없음, 가장 빠름 | id 재사용 시 use-after-free, 검출 불가 | frame 내부 dense 순회에는 이걸 유지 (이미 살아있음이 보장된 경로) |
| `unique_ptr<Entity>` | 소유권 명확 | 힙 산재 → 캐시 미스, DLL 경계 CRT 충돌 위험, ECS 철학 위반 | OOP 회귀라 채택 안 함 |

**근본 trade-off**: handle은 "안전(generation 검사) ↔ resolve 비용"이다. 내 설계는 **경계는 handle, 내부 dense 순회는 EntityID** 라는 이원화로 양쪽을 취한다 (`Entity.h:8-9` 주석이 이 의도를 명시). Factory/public API는 `EntityHandle`, 시스템 내부 tight loop는 `EntityID`.

### 2.3 offline codegen vs runtime JSON

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **offline codegen (택함)** | frame에 string/parse 0, 컴파일 타임에 데이터 존재 보장, 결정론(정렬된 dense id) | 데이터 바꾸면 재빌드(현재), hot reload 없음 | 서버권위 결정론이 최우선 — frame lookup이 정수 인덱싱이어야 same-seed 재현이 안정. cook 단계에서 검증(중복키/음수 쿨다운)도 강제 |
| runtime JSON 직접 파싱 | hot reload 자연스러움, 재빌드 불필요 | frame parse 비용, 런타임 데이터 오류, 결정론 깨지기 쉬움 | dev 편의는 좋지만 결정론·성능과 충돌. hot reload는 **dev 빌드 overlay로 별도 단계**(S5)로 미룸 |

**근본 trade-off**: "기획자 반복 속도 ↔ frame 성능 + 결정론". 나는 결정론 쪽에 베팅하고, 반복 속도는 추후 dev-only runtime overlay로 보완하는 단계적 설계를 택했다.

### 2.4 함수포인터 hook vs 1급 GAS 객체 (이 도메인 경계)

스킬 effect를 UE5 GAS처럼 `GameplayTag`/`AttributeSet`/`GameplayEffect` 1급 객체로 만들지 않고, **champ×variant 함수포인터 디스패치 테이블**(`CGameplayHookRegistry`, 현재 256×256)로 구현했다. 이유: 1급 Tag/Effect 객체는 힙 할당 + 가상 디스패치 + 직렬화 복잡도를 낳는데, 서버권위 결정론에서는 "어떤 함수가 이 effect를 실행한다"는 정적 매핑으로 충분하다. (이 부분은 도메인 7 챔피언/스킬과 겹치며, 여기선 "데이터→effect policy 연결"의 데이터주도 측면만 다룬다.)

---

## 3. 실제 구현 (코드 근거)

### 3.1 sparse-set 컴포넌트 스토어 — `Engine/Public/ECS/ComponentStore.h`

3-배열 sparse-set의 정수:
- `Add` (`:11-22`): `sparse[entity] = denseIdx; dense.push_back(entity); data.push_back(component);` — O(1).
- `Remove` (`:24-38`): **swap-and-pop**. 마지막 원소를 지울 자리로 옮기고(`data[index] = move(data[last])`) `sparse[lastEntity] = index`로 매핑 갱신, 그 후 pop. dense 배열에 구멍을 안 남겨 순회가 항상 조밀.
- `data[]` 주석(`:65`) "연속된 메모리 사용!" 이 SoA 의도를 명시.
- 순회 인터페이스: `Entities()`(`:57`), `Data()`(`:58`) 가 dense 포인터를 그대로 노출 → 시스템이 선형 접근.

면접에서 "코드 보여달라" → 이 60줄을 짚으면 된다. swap-and-pop의 generation 안전성(remove 후 sparse[entity]=INVALID, `:37`)을 설명할 수 있어야 한다.

### 3.2 generation EntityHandle — `Engine/Public/ECS/Entity.h`

- `EntityHandle` (`:17-69`): 상위 32bit generation + 하위 32bit index를 uint64로 패킹(`Make`, `:41-51`). `value==0`이 NULL.
- `CEntityManager` (`:73-`): slot마다 `generation`을 들고, **짝/홀로 생사를 인코딩**한다 — `IsAliveGeneration = (gen & 1) != 0` (`:189-192`). Destroy 때 `NextDeadGeneration`(`:207`)이 generation을 증가시켜 짝수(죽음)로, Create 재사용 때 `NextAliveGeneration`(`:194`)이 홀수(생존)로 토글.
- `TryResolve` (`:152-169`): handle의 generation과 slot의 현재 generation이 **불일치하면 false** → 죽은 엔티티를 가리키는 handle은 use-after-free 대신 조용히 실패. free-list(`m_iFreeHead`, `:228`)로 id 재사용.

핵심 호출 경로: `CWorld::TryResolveEntity`(`World.h:68`) → `CEntityManager::TryResolve`. Factory가 `CreateEntityHandle`로 handle을 받고, 시스템 진입 시 `ResolveEntity`로 EntityID를 얻어 dense 순회.

### 3.3 멀티컴포넌트 ForEach — `Engine/Public/ECS/World.h:146-177`

2/3 컴포넌트 ForEach가 sparse-set의 약점을 그대로 보여준다:
```
for (i in s1->Count())
    e = s1->Entities()[i];
    if (s2->Has(e) [&& s3->Has(e)])   // <- probe
        fn(e, s1->Data()[i], s2->Get(e), [s3->Get(e)]);
```
첫 store(`s1`)는 선형이지만 `s2->Has(e)`/`s2->Get(e)`는 sparse 랜덤 액세스. **이게 archetype이 아닌 증거이자 trade-off의 실물**. 면접에서 "왜 첫 store 기준이냐 / 가장 작은 store를 driver로 고르면 probe가 줄지 않냐"는 최적화 질문에 답할 수 있어야 한다(→ 5절).

### 3.4 데이터 접근 선언 + 충돌 판정 — `Engine/Public/ECS/SystemAccess.h`

- `CSystemAccessBuilder` (`:28-`): 시스템이 `.Read<HealthComponent>().Write<TransformComponent>()` 식으로 접근을 선언. `bUnknown`(미선언=모든 것 Write로 보수적 처리), `bWritesWorldStructure`(엔티티 생성/삭제=구조 변경) 플래그.
- `SystemAccessConflicts` (`:77-96`): 충돌 규칙의 정수 —
  - 한쪽이 `bUnknown`이면 충돌(보수적).
  - 한쪽이 구조 변경이면 충돌.
  - 같은 컴포넌트 타입에 대해 **한쪽이라도 Write면 충돌**(Read-Read만 안전).
- 타입 식별은 `std::type_index(typeid(T))` — RTTI 기반.

### 3.5 충돌 기반 배치 스케줄러 — `Engine/Private/ECS/SystemScheduler.cpp`

- `RebuildExecutionPlan` (`:45-83`): phase별로 시스템을 순회하며, 현재 batch와 충돌하지 않으면 같은 batch에 추가, 충돌하면 `flushBatch()`로 끊고 새 batch 시작. greedy 1패스 배치.
- `Execute` (`:85-132`): batch 크기 < `kMinParallelBatchSize`(2, `:11`)면 직렬, 아니면 JobSystem에 `Submit` + `WaitForCounter`로 fork-join 병렬.
- **정직성의 핵심**: `:90-93`, `:128-131` 에서 `MaxBatchSize`/`ParallelBatches`/`SequentialBatches`/`SubmittedJobs`를 `WINTERS_PROFILE_COUNT`로 매 프레임 계측. 이 계측이 **"실게임에서 MaxBatchSize=1, 즉 병렬 배치가 한 번도 안 발화"** 를 드러냈다. 코드 경로는 완성됐지만 현재 등록된 시스템들이 서로 충돌하거나 phase가 갈려서 2개 이상 묶이지 못한다. 이건 숨기지 않고 측정으로 노출한 한계다.

### 3.6 불변 Def pack + dense 조회 — `Shared/GameSim/Definitions/`

- `DefinitionIds.h`: `DefinitionKey = u32_t`(stable 키), `ChampionDefId`/`SkillDefId`(dense u16 index, `IsValid = value != 0`). **계획서(12)는 더 많은 typed id를 그렸지만 실제 구현은 champion/skill/summonerSpell 3종만** — 정직하게 부분 구현이다.
- `GameplayDefinitionPack.h/.cpp`: `champions`/`skills`/`summonerSpellDefs` 포인터 + count.
  - `FindDense` (`.cpp:5-20`): `index = id.value - 1; if (definitions[index].id.value != id.value) return nullptr;` — **dense 직접 인덱싱 + id 자기검증**. frame 경로의 핵심. O(1).
  - `FindByKey` (`.cpp:22-39`): DefinitionKey로 **선형 탐색**. 계획대로 spawn/load 경계 전용. O(n).
  - `FindChampion(eChampion legacy)` (`.cpp:52-63`): **legacy enum 선형 탐색 fallback** — 아직 cutover가 안 끝나 남아 있는 다리.
- `SkillGameplayDef.h`: `DefinitionKey key` + `SkillDefId id` + `ownerChampionId` + Target/Cost/Cooldown/Range/Stage/Facing/Effect/SummonPolicy spec. 계획서가 "champion/slot을 소유하지 않는다"고 했지만 **실제 코드엔 아직 `slot`/`legacySkillId`/`ownerChampionId`가 남아 있다** — 전환 중 상태.

### 3.7 데이터주도 resolver — `GameplayDefinitionQuery.cpp`

frame 시스템이 pack을 읽는 진입점. **부분 cutover의 실물 증거**:
- `FindChampion` (`:85-108`): 먼저 `ChampionDefinitionComponent.championDefId`로 dense 조회 시도 → 실패하면 `ResolveChampionFallback`로 fallthrough.
- `ResolveChampionFallback` (`:23-41`): `StatComponent.championId` → `ChampionComponent.id` 순으로 legacy enum을 긁는다. **이게 honesty map의 "StatComponent 여전히 championId 사용"·"fallback 잔존"의 코드 근거**.
- 파일 상단(`:10`)이 `ChampionGameDataDB.h`를 여전히 include — 25회 폴백이 살아있다는 증거.
- `ResolveSkillCooldown`/`ResolveSkillRange`/`ResolveSkillEffectParam`/`IsSkillTwoStage` 등은 pack의 dense def를 읽되, pack이 없거나 def가 없으면 fallback. **데이터주도 경로는 동작하지만 legacy와 병존**.

### 3.8 spawn 경계의 pack 주입 — `StatSystem.cpp`

`CStatSystem::Execute(CWorld&, const GameplayDefinitionPack& definitions)`(`:247`) — 시스템이 pack을 **const ref로 주입**받는 구조가 실제로 동작한다. `BuildBaseStats(..., const GameplayDefinitionPack&)`(`:108`). 즉 "GameSimContext가 immutable pack을 소유하고 시스템이 const ref로 받는다"는 북극성의 일부는 구현됨.

### 3.9 오프라인 codegen — `Tools/ChampionData/build_champion_game_data.py`

`champions.json`(17 champion) → `Shared/GameSim/Generated/ChampionGameData.generated.h/.cpp`. generated 헤더(`:7-13`)는 `GetBuildHash()`/`GetChampionTable()`/`FindChampion(eChampion)`를 노출. **현재 라이브 데이터 경로는 이 generated table** 이고, 새 `GameplayDefinitionPack`이 그 위로 점진 cutover 중. (`Tools/LoLData/Build-LoLDefinitionPack.py`는 12 계획의 새 pack generator로 별도 존재.)

---

## 4. 검증 — 동작을 어떻게 증명했나

### 4.1 결정론 골든 게이트 — SimLab (`Tools/SimLab/main.cpp`)

이 도메인의 "됐다" 판정의 척추. headless 5v5 30Hz scripted match를 돌려 per-tick 상태 해시로 결정론을 검증한다:
- **per-tick FNV-1a 해시** (`:420-438`): offset basis `1469598103934665603`(0xcbf29ce484222325)에서 tick 번호 + 각 엔티티의 `pos.x/y/z`, `hp.fCurrent`, `bIsDead`, `mana`, `level`, `gold.amount`, 마지막으로 `rng.GetState()`를 섞는다. **RNG 상태까지 해시에 넣어 결정론 RNG 소비 순서 변화를 잡는다.**
- **final 해시**: tick 해시들을 다시 FNV로 누적(`:441-444`).
- **3-way 비교** (`:616-638`):
  - `runA = RunMatch(seed)`, `runB = RunMatch(seed)` → **A==B여야 same-seed 결정론**.
  - `runC = RunMatch(seed+1)` → C가 A와 **달라야 seed 민감도**(상수 출력 버그 검출).
  - A≠B면 **first divergent tick을 출력**(`:626-634`)하고 **exit code 1**. 0=결정론.
- 추가 어서션: `[SimLab][YoneE]` 같은 스킬 상태기계 복귀 검사(`:544`).

이게 데이터주도 cutover의 안전망이다 — Def pack을 바꿔도 SimLab 해시가 baseline과 같으면 동작이 보존됐다는 강한 증거. 다르면 어느 tick에서 갈라졌는지 즉시 안다.

### 4.2 스케줄러 자기계측

`SystemScheduler.cpp:128-131`의 `WINTERS_PROFILE_COUNT`가 매 프레임 배치 통계를 JSON 캡처로 노출. **"병렬 배치가 발화하는가?"를 추측이 아니라 측정으로 판정** → MaxBatchSize=1을 발견. 이게 "검증이 오히려 한계를 드러낸" 사례.

### 4.3 dense id 자기검증

`FindDense`(`GameplayDefinitionPack.cpp:13-15`)는 `definitions[index].id.value != id.value`면 nullptr — pack 배열의 index와 저장된 id가 어긋나면(잘못된 cook) 조용히 틀린 def를 주는 대신 실패. cook 단계 정렬이 깨지면 여기서 걸린다.

### 4.4 빌드/경계 게이트

`12` 계획의 검증 명령(GameSim/Server/Client/SimLab 4-프로젝트 빌드 + SimLab.exe 실행 + `rg`로 legacy symbol/forbidden include 0 확인)이 cutover 회귀 가드. 단 풀 cutover가 아직 안 끝나 legacy symbol은 의도적으로 0이 아니다(전환 중).

---

## 5. 최적화

### 5.1 실제로 한 것

- **swap-and-pop O(1) remove** (`ComponentStore.h:24-38`): 정렬 유지를 포기하고 dense 조밀성을 택해 remove를 O(1)로. 순회 시 구멍 검사 불필요.
- **경계 handle / 내부 EntityID 이원화**: tight loop에서 resolve 비용 제거. 안전이 필요한 경계에서만 generation 검사.
- **frame string lookup 제거 (부분)**: dense DefId 직접 인덱싱(`FindDense`)으로 cutover된 경로는 frame에서 string/map 조회 0. (단 fallback 경로는 아직 enum 선형 탐색.)
- **충돌 스케줄러 배치 경로 구현**: 비충돌 시스템 자동 병렬 묶기 코드 완성 — 단 아래 한계.

### 5.2 측정으로 드러난 한계 + 계획 중인 최적화

정량 수치는 정직성 지도에 확정된 것만 쓴다. **FPS/ms 수치는 이 도메인에 확정 측정이 없어 "측정 예정"으로 둔다.**

- **MaxBatchSize=1 해소 (계획)**: 현재 충돌 판정이 `bUnknown` 시스템을 모든 것과 충돌로 처리(보수적)하고, phase 분리로 묶일 후보가 적다. → 시스템들의 Read/Write 선언을 정밀화하고 phase를 재조정해 실제 병렬 배치를 발화시키는 게 다음 단계. **측정 scope는 이미 있음**(`Scheduler::MaxBatchSize`), 목표는 "MaxBatchSize ≥ 2가 실게임에서 관측".
- **멀티컴포넌트 ForEach driver 최적화 (계획)**: 현재 무조건 첫 타입 store를 driver로 순회. → **가장 작은 store를 driver로 선택**하면 `Has()` probe 횟수가 줄어든다(probe는 작은 쪽 count만큼). archetype 없이도 얻는 저비용 최적화.
- **dense iteration 캐시 정렬 (계획)**: 자주 같이 읽히는 컴포넌트의 store 정렬 순서를 맞춰 probe의 캐시 적중률 향상.

---

## 6. 구현 예정 (Planned) — 동일한 깊이로

이 부분은 **실제로 구현할 것**이라 구현된 부분과 같은 수준으로 설계를 잡는다. 근거: `12_WINTERS_DATA_ORIENTED_DEF_ENTITY_REWRITE_PLAN.md`, `00_INDEX_DATA_DRIVEN_ENTITY_PIPELINE.md`.

### 6.1 데이터주도 풀 cutover — legacy fallback 제거

- **무엇**: `GameplayDefinitionQuery`의 `ResolveChampionFallback`(StatComponent.championId/ChampionComponent.id 긁기) 제거, `ChampionGameDataDB` include·25회 폴백 제거, `SkillGameplayDef`의 `slot`/`legacySkillId`/`ownerChampionId` 제거.
- **왜**: fallback이 살아있는 한 "데이터주도"는 절반이다. 두 진실(legacy enum table + dense pack)이 공존하면 어느 쪽이 source인지 모호해지고 결정론·유지보수가 위험.
- **어떻게**: 12 계획의 S3~S8 순서 — (S3) additive runtime path로 pack 경로를 legacy와 parity 맞춰 병존 → (S6) `ChampionComponent`의 중복 truth(hp/mana/level/cooldown)를 Health/Mana/Experience/SkillState 단일 owner로 이동, reader 58개를 *한 번에 안 깨고* reader별 parity 전환 → (S8) reader가 0이 된 뒤에야 legacy struct/table/registry 삭제.
- **자료구조**: `ChampionDefinitionComponent{ DefinitionKey, ChampionDefId }`(이미 존재), `SkillLoadoutComponent`(불변 def 참조) / `SkillStateComponent`(가변 쿨다운) / `SkillRankComponent`(성장) **3역할 분리** — 다시 합치지 않는다.
- **trade-off 예상**: 전환 중에는 두 경로 parity 유지 비용(같은 값을 두 곳에서). reader 58개 전환은 대량 수정이라 회귀 위험 → SimLab 해시가 안전망.
- **검증**: 각 reader 전환마다 SimLab same-seed 해시 == baseline. 최종 `rg`로 `ChampionGameDataDB|SkillTable|ChampionTable|FindSkillDef` 0건 + 4-프로젝트 빌드.

### 6.2 GameplayPolicyId dense 함수 테이블

- **무엇**: 현재 `CGameplayHookRegistry`의 256×256 `MakeGameplayHookId(champ, variant)` 2D 테이블을, JSON `effectPolicyKey` → generator가 부여한 dense `GameplayPolicyId` 1D 테이블로 교체.
- **왜**: champ×variant 조합 키는 (a) 65536 슬롯 대부분 빈 sparse 낭비, (b) champion enum에 effect가 결합돼 데이터주도가 아님. dense policy id는 "JSON이 effect policy를 가리키고 코드는 policy id에 함수 한 번 등록".
- **어떻게**: codegen이 `effectPolicyKey` 문자열을 deterministic sort 후 dense id로 매핑 → champion GameSim이 그 id에 함수포인터 등록 → cast 경로가 `Dispatch(policyId)`.
- **trade-off**: 등록 코드가 enum 자동조합에서 명시 등록으로 바뀌어 누락 위험 → cook 단계 "missing GameplayPolicy binding" 검증으로 막음(12 계획 검증 실패 조건에 명시).
- **검증**: policy id 충돌 0, 모든 replicated action이 대응 policy 보유, SimLab 해시 보존.

### 6.3 ECS primitive의 Foundation 추출 (의존성 역전 해소)

- **무엇**: `EntityHandle`/`CWorld`/`ComponentStore`가 현재 **Engine에 있어 Shared/GameSim이 EngineSDK를 include**하는 역전을, `Foundation` 계층(Core types + ECS)으로 끌어내려 `Foundation <- Engine`, `Foundation <- Shared/GameSim`, `<- Client`, `<- Server` 단방향으로 정리.
- **왜**: GameSim(서버 결정론 코어)이 Engine(렌더러/UI/DX)을 include하면 헤드리스 빌드·결정론·재사용이 오염된다. honesty map·gotchas의 "Engine public header에 LoL/Shared 타입 누출 금지"와 직결.
- **어떻게(단계)**: (1) 파일 본문을 **변경 없이 먼저 이동**(export macro/project ref 확인) → (2) `GameSim`이 Engine renderer/resource/UI를 include 안 하는 상태를 **빌드로 증명** → (3) compatibility forwarding header 삭제. 12 계획 S9, CONFIRM_NEEDED로 별도 세션 격리.
- **trade-off**: 헤더 이동은 거대한 include 변경 → 빌드 폭발 위험. 그래서 "본문 불변 이동 먼저, 삭제는 빌드 증명 후"로 단계 분리.
- **검증**: `rg "#include .*(Engine|Renderer|UI|ImGui|DX)" Shared/GameSim` 0건 + 4-프로젝트 빌드.

### 6.4 데이터 hot reload (dev-only) + reflection 인스펙터

- **무엇**: dev 빌드 한정 파일 watcher → JSON 재로드 → resolver overlay → live entity 재적용. 이후 reflection(`REFLECT`/`FIELD` 메타) 기반 자동 인스펙터.
- **왜**: 기획자 반복 속도(2.3의 trade-off에서 미룬 쪽). codegen baked table 위에 **dev-only runtime overlay**를 얹어 결정론(release)을 안 깨면서 dev 편의 확보.
- **어떻게**: baked table은 그대로 두고 overlay map을 우선 조회(dev), release는 overlay 컴파일 아웃. immutable pack에 generation/observer/shared_ptr을 **지금은 추가 안 함**(12 계획 명시) — hot reload는 풀 cutover 완료 후에만 착수.
- **trade-off**: overlay 우선 조회는 frame에 분기 추가 → dev 빌드 한정으로 격리. live 재적용 시 in-flight 상태 일관성 위험.
- **검증**: dev에서 champions.json 수정 → 재시작 없이 반영, release 빌드에서 overlay 심볼 0.

> 면접에서 "그거 아직 안 했죠?"가 나오면: **"네, fallback 제거·dense policy·Foundation 추출·hot reload는 12 계획서에 단계(S3~S10)와 검증 게이트까지 설계돼 있고 코드는 부분 반영입니다. 풀 cutover를 SimLab 해시 보존 없이 한 번에 밀면 reader 58개가 깨지므로 reader별 parity 전환으로 쪼갰습니다"** 로 답한다.

---

## 7. 면접 예상 질문 & 모범 답변

**Q1. (기본) ECS가 뭐고 왜 OOP 게임 객체보다 나은가?**
A. Entity(ID)/Component(데이터)/System(로직)으로 분리하는 패턴입니다. 핵심은 메모리 레이아웃 — 같이 처리되는 컴포넌트를 연속 배열(SoA)로 두면 캐시라인이 유효 데이터로 차고 prefetcher가 동작해 CPU가 메모리 레이턴시에 덜 묶입니다. OOP 객체 배열은 위치만 업데이트해도 객체 전체를 캐시에 올려 라인을 낭비하고, 가상 디스패치로 분기 예측도 실패합니다.

**Q2. (기본) sparse-set이 뭔가? remove를 O(1)로 어떻게?**
A. `sparse[entityId]->denseIndex`, `dense[]`(entity), `data[]`(컴포넌트) 3배열입니다. remove는 swap-and-pop — 지울 자리에 마지막 원소를 옮기고 그 entity의 sparse를 갱신한 뒤 pop. 정렬은 포기하지만 dense에 구멍이 안 생겨 순회가 항상 조밀합니다. `ComponentStore.h:24-38`에 있습니다.

**Q3. (설계) 왜 archetype이 아니라 sparse-set인가?**
A. archetype은 멀티컴포넌트 쿼리가 chunk 선형 순회라 빠르지만, 컴포넌트 추가/제거가 archetype 이동 복사를 유발하고 구현이 수 배 복잡합니다. 제 워크로드는 엔티티가 수백 단위고 컴포넌트 조합이 spawn 때 확정돼 자주 안 바뀝니다. 그리고 **프로파일러로 현재 씬이 드로우콜 ~94의 CPU 바운드임을, 병목이 ECS 순회가 아님을 확인**했습니다. archetype 복잡도를 투자할 측정 근거가 아직 없어서 단순한 sparse-set이 합리적입니다. 엔티티가 1만+로 가면 재검토합니다.

**Q4. (설계) generation handle은 왜 필요한가? raw 포인터로 하면?**
A. 엔티티가 죽고 id가 재사용되면 raw id/포인터는 use-after-free를 못 잡습니다. handle은 index+generation 64bit인데, slot마다 generation을 들고 죽을 때 증가시켜 짝/홀로 생사를 인코딩합니다(`Entity.h:189`). resolve 때 handle generation과 현재 generation이 다르면 조용히 실패. 게다가 64bit 값이라 DLL 경계 복사가 안전합니다. 대신 resolve 비용이 있어서 **경계는 handle, frame tight loop 내부는 EntityID** 로 이원화했습니다.

**Q5. (설계) 데이터 접근 스케줄러는 충돌을 어떻게 판정하나?**
A. 각 시스템이 `CSystemAccessBuilder`로 Read/Write 컴포넌트를 선언합니다. `SystemAccessConflicts`(`SystemAccess.h:77`)가 같은 타입에 한쪽이라도 Write면 충돌, Read-Read만 안전으로 봅니다. 미선언 시스템(`bUnknown`)이나 엔티티 생성/삭제(구조 변경)는 보수적으로 모두와 충돌. greedy 1패스로 비충돌 시스템을 batch에 묶고 충돌 시 끊습니다(`SystemScheduler.cpp:45`).

**Q6. (adversarial) "엔진을 병렬화했다"는데, 실제로 병렬 실행되는 시스템이 몇 개인가?**
A. 정직하게 말하면 **현재 충돌 스케줄러의 병렬 배치는 실게임에서 한 번도 발화하지 않습니다 — MaxBatchSize=1**. 코드 경로는 완성됐지만 시스템들이 phase로 갈리거나 `bUnknown`/구조변경으로 보수적 충돌 처리돼 2개 이상 안 묶입니다. 중요한 건 이걸 추측이 아니라 `SystemScheduler.cpp:131`의 `Scheduler::MaxBatchSize` 프로파일러 카운터로 **측정해서 발견**했다는 점입니다. 실제 병렬은 Transform/Nav fan-out 두 곳이고, 스케줄러 배치 발화는 다음 최적화로 정의돼 있습니다. 측정 인프라를 먼저 만들었더니 제 코드의 한계가 드러난 사례입니다.

**Q7. (adversarial) "데이터주도"라는데, 밸런스를 정말 코드 수정 없이 바꿀 수 있나? fallback이 있던데?**
A. 부분 cutover입니다. 새 `GameplayDefinitionPack`의 dense 조회 경로(`FindDense`)는 동작하고 cutover된 스킬은 frame에서 string lookup이 0입니다. 하지만 `GameplayDefinitionQuery.cpp:23`의 `ResolveChampionFallback`이 아직 `StatComponent.championId`/`ChampionComponent.id` legacy enum을 긁고, `ChampionGameDataDB`가 25회 폴백으로 살아있습니다. 그래서 "100% 데이터주도"라고 말하지 않습니다. 12 계획서 S3~S8에 reader 58개를 parity 전환 후 legacy를 삭제하는 순서가 있고, 안전망은 SimLab 해시입니다.

**Q8. (adversarial) hot reload 된다고 했나? archetype ECS인가?**
A. 둘 다 아닙니다. **hot reload는 미구현** — offline codegen이라 데이터 바꾸면 현재는 재빌드입니다. dev-only runtime overlay 설계(12 계획 S5)는 있지만 풀 cutover 후에 착수합니다. ECS도 **archetype이 아니라 sparse-set** 입니다. 멀티컴포넌트 ForEach가 첫 store 순회 + `Has()` probe라(`World.h:146`) archetype의 chunk 선형 순회가 아닙니다. 이 둘은 제가 "했다"고 절대 말하지 않는 경계입니다.

**Q9. (심화) 데이터주도인데 결정론을 어떻게 보장·검증하나?**
A. cook 단계에서 def를 deterministic sort해 dense id를 부여하므로 같은 입력이면 항상 같은 id입니다. frame은 정수 인덱싱만 해 lookup 순서 비결정성이 없습니다. 검증은 SimLab(`Tools/SimLab/main.cpp`)으로 same-seed 두 런(A,B)의 per-tick FNV 해시가 일치해야 하고(A==B), seed+1 런(C)은 달라야 합니다. 해시에 pos/hp/mana/level/gold + **RNG 상태**까지 넣어(`:437`) RNG 소비 순서 변화도 잡습니다. 갈라지면 first divergent tick을 찍고 exit 1. Def pack을 바꿔도 이 해시가 baseline과 같으면 동작 보존이 증명됩니다.

**Q10. (심화) 멀티컴포넌트 ForEach가 sparse-set의 약점이라며. 어떻게 최적화할 건가?**
A. 현재는 무조건 첫 타입 store를 driver로 순회하며 나머지를 `Has()` probe합니다. probe는 sparse 랜덤 액세스라 캐시가 깨집니다. 최적화는 **가장 작은 store를 driver로 선택**하는 겁니다 — probe 횟수가 driver count만큼이므로 작은 쪽을 돌면 probe 총량이 줄죠. archetype을 안 만들고 얻는 저비용 개선입니다. 그 이상이 필요하면 자주 같이 읽히는 조합만 archetype-lite group으로 빼는 단계적 접근을 보겠습니다.

**Q11. (심화) Def pack을 매치 동안 불변으로 둔 이유는? hot reload랑 충돌하지 않나?**
A. 불변이면 락 없이 멀티스레드 read가 안전하고 결정론이 흔들리지 않습니다. 그래서 12 계획에서 **지금은 pack에 generation/observer/shared_ptr을 일부러 안 넣습니다**. hot reload는 release 결정론을 안 깨려고 dev 빌드 한정 overlay로 분리하고, baked immutable pack은 그대로 둔 채 dev에서만 overlay를 우선 조회하는 설계입니다. "불변성(결정론/성능) vs 반복 속도(dev)" trade-off를 빌드 모드로 가른 거죠.

**Q12. (심화) EntityHandle을 네트워크로 보내거나 저장하면 안 되는 이유는?**
A. handle은 process-local lifetime id입니다. generation은 이 프로세스의 slot 재사용 이력에 종속되므로, 다른 프로세스(서버↔클라)나 다음 실행에선 의미가 다릅니다. 그래서 네트워크엔 `NetEntityId`(네트워크 인스턴스), 정의 식별엔 `DefinitionKey`(stable)를 따로 보냅니다. JSON에 EntityHandle 저장, 네트워크에 EntityHandle 전송, DefinitionKey를 EntityID로 쓰기는 12 계획이 명시적으로 금지한 결합입니다.

---

## 8. 30초 엘리베이터 피치

"제 엔진의 ECS는 sparse-set 컴포넌트 스토어로 컴포넌트를 연속 메모리에 깔고, 시스템이 Read/Write를 선언하면 충돌 안 나는 시스템을 자동으로 병렬 배치하는 스케줄러를 붙였습니다. 그리고 챔피언/스킬 수치를 코드에서 떼서 오프라인으로 검증·정렬해 cook한 불변 Def pack을 dense index로 조회하게 했고요. 자랑보다 정직하게 말하면 — archetype이 아니라 sparse-set이고, 데이터주도는 fallback이 남은 부분 cutover, hot reload는 미구현입니다. 대신 그 한계를 제가 만든 프로파일러 카운터로 *측정해서* 압니다. 스케줄러 병렬 배치가 실게임에서 한 번도 안 발화한다는 걸 MaxBatchSize=1로 발견했고, 그게 다음 작업을 정의했습니다. SimLab이라는 결정론 골든 게이트가 같은 seed면 같은 per-tick 해시를 강제해서, Def를 바꿔도 동작이 보존됐는지 exit code로 판정합니다. 화려한 기능 목록보다 측정과 검증으로 경계를 긋는 루프를 보여드리고 싶습니다."
