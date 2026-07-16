# 08. GameSim · 챔피언 시뮬레이션 · 결정론

> 면접 대본 겸 지식 베이스. 코드 문법은 `.md/interview/cpp/` 세트가 담당하고, 이 챕터는 **도메인 구조와 설계 결정**만 다룬다. 모든 인용은 실제 파일을 열어 확인한 것이며 경로는 repo-relative다.

---

## ① 한 줄 정의 (면접 첫 문장)

"Shared GameSim은 클라이언트와 서버가 **같은 정적 라이브러리로 링크하는 서버 권위(server-authoritative) 시뮬레이션 계층**입니다. 30Hz 고정 tick 위에서 15개 챔피언의 스킬·데미지·AI가 결정론적으로 돌고, 챔피언 로직은 (champion × variant) 2차원 훅 테이블로 디스패치해서 150챔피언 스케일까지 중앙 코드 수정 없이 확장되도록 설계했습니다."

---

## ② 구조와 데이터 흐름

### 2-1. 왜 시뮬 코드를 공유하는가

Shared/GameSim은 `WintersGameSim.lib`(정적 라이브러리)로 빌드되고 Client / Server / SimLab(테스트 하네스)이 ProjectReference로 소비한다. 소스 파일을 양쪽 프로젝트에 중복 컴파일하는 방식이 아니다.

이유는 세 가지다.

1. **판정 코드의 단일 소스** — 데미지 공식, 쿨다운, 스킬 판정이 서버와 클라에서 두 벌로 존재하면 반드시 발산한다. 한 벌만 존재하면 "클라가 다르게 계산했다"는 버그 클래스 자체가 사라진다.
2. **예측(prediction)의 전제** — 클라가 서버와 같은 코드로 로컬 예측을 돌릴 수 있어야 스냅샷 도착 전 반응성을 만들 수 있다.
3. **테스트 격리** — SimLab이 렌더러/네트워크 없이 GameSim만 링크해 same-seed 해시 비교 스모크를 돌린다. 결정론 회귀를 CI성 게이트로 검출하는 기반이다.

### 2-2. 런타임 데이터 흐름 (30Hz 서버 tick)

```text
[클라 입력] → CommandSerializer → TCP(FlatBuffers CommandBatch)
     │
     ▼
[서버] CommandIngress (seq 게이트 · Move 병합 · ingress mutex)
     │  ← 네트워크 스레드는 여기까지만. tick 락을 잡지 않는다.
     ▼
[30Hz tick] DrainCommands
     ├─ ServerBotAI ──────── GameCommand "생산만" (truth 직접 조작 금지)
     ├─ ICommandExecutor::ExecuteCommand
     │    └─ HandleCastSkill: 검증 게이트 → 훅 디스패치 → 복제 이벤트 방출
     ├─ Shared 시스템들 + 15챔피언 GameSim::Tick
     ├─ CombatActionSystem: uImpactTick 도달 시 BA 임팩트
     └─ DamageQueueSystem: tick 끝에 데미지 요청 정렬 드레인
     │
     ▼
Snapshot / Event (FlatBuffers) 전 세션 broadcast
     │
     ▼
[클라] SnapshotApplier / EventApplier → 보간·예측·FX 큐 재생
```

이 파이프라인의 모든 시스템은 `TickContext` 하나로 컨텍스트를 주입받는다 (`Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h:49`):

```cpp
struct TickContext
{
    uint64_t tickIndex = 0;
    f32_t fDt = DeterministicTime::kFixedDt;
    DeterministicRng* pRng = nullptr;          // 시스템이 자기 RNG를 못 만든다
    EntityIdMap* pEntityMap = nullptr;          // 로컬 ↔ 네트워크 id 경계
    const IWalkableQuery* pWalkable = nullptr;
    const ILagCompensationQuery* pLagCompensation = nullptr;
    const GameplayDefinitionPack* pDefinitions = nullptr;  // 서버가 주입하는 데이터 팩
};
```

시간·난수·데이터·id 매핑이 전부 **주입**이다. 시스템이 전역에서 뭔가를 집어오는 순간 결정론과 테스트 격리가 깨지기 때문에, 컨텍스트를 단일 구조체로 좁혀 놓았다.

### 2-3. 데이터 소유권 3분할

authoring 데이터(champions.json 등)는 Python cook 도구가 3개 팩으로 굽는다 (`.md/architecture/WINTERS_DATA_ARCHITECTURE.md` §1):

| 팩 | 위치 | 내용 |
|---|---|---|
| SharedContract | Shared/GameSim/Definitions | 타입 + 결정론 조회. **값 없음** |
| ServerPrivate | Server 측 .generated.cpp | 스탯/스킬 수치/성장 — 서버 tick이 판정 |
| ClientPublic | Client 측 .generated.cpp | 모델/텍스처/애님 키/비주얼 프레임 |

원칙은 "**값은 한쪽만 소유, 클라는 재계산 금지**"다. 성장 수치는 서버가 레벨을 적용해 StatComponent로 복제하고, 클라는 복제된 스탯과 비주얼 팩만 소비한다. 애니메이션 수치도 이원화했다: 게임플레이 타이밍(lock/windup)은 서버 팩, 비주얼 재생(animKey/castFrame)은 클라 팩 — **클라의 비주얼 프레임이 서버 판정을 만들면 안 된다**는 규칙이다. 부수 효과로 서버 수치가 클라 바이너리에 들어가지 않아 치트 분석 표면도 줄어든다.

---

## ③ 핵심 설계 결정과 트레이드오프

### 결정 1. 결정론(determinism)의 3대 축 — 시간 · 난수 · 순회

**왜**: 서버 권위 시뮬에서 리플레이 재현, same-seed 스모크 검증, 나중의 롤백/예측까지 가려면 "같은 입력 → 같은 상태"가 절대 조건이다. 발산의 3대 고전 원인을 각각 장치 하나로 봉쇄했다.

| 발산 원인 | 봉쇄 장치 | 근거 파일 |
|---|---|---|
| 벽시계 시간이 시뮬에 유입 | `DeterministicTime`: 30Hz 고정 dt(1/30), 시간은 tick 정수로만 (`TickToSec`/`SecToTick`) | `Shared/GameSim/Core/Determinism/DeterministicTime.h` |
| 전역 RNG 소비 순서에 판정이 의존 | `DeterministicRng`: xorshift64 상태 머신, `GetState`/`SetState`로 스냅샷 가능, `MakeSubSeed(tick, entityId, skillId)`로 호출 지점별 독립 스트림 파생 | `Shared/GameSim/Core/Determinism/DeterministicRng.h` |
| `unordered_map` 순회 순서 | `DeterministicEntityIterator::CollectSorted`: ECS ForEach 결과를 EntityID로 정렬 후 처리 | `Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h` |

- **대안**: 시간은 가변 dt + 보정, 난수는 전역 `std::mt19937` 하나, 순회는 ECS가 주는 순서 그대로 — 전부 처음엔 동작하는 것처럼 보인다.
- **선택 이유**: 발산은 증상이 "몇 분 뒤 미묘하게 다른 체력"으로 나타나 디버깅이 최악이다. 원인 계층별로 장치를 두면 회귀 지점을 구조적으로 좁힐 수 있다.
- **감수한 비용**: 정렬 순회는 매번 vector 수집 + sort 비용을 낸다. 데미지/폼오버라이드처럼 순서 민감 시스템에만 쓰고, 순서 무관 시스템은 일반 ForEach를 유지해 비용을 제한했다.

여기에 컴포넌트 계층의 전제 조건이 하나 더 있다: **sim 컴포넌트는 POD여야 한다**. 규칙을 문서가 아니라 컴파일 에러로 박제했다 (`Shared/GameSim/Components/CombatActionComponent.h:48`):

```cpp
static_assert(std::is_trivially_copyable_v<CombatActionComponent>,
    "CombatActionComponent must be trivially_copyable for sim determinism.");
```

포인터/가상함수/힙 멤버가 들어가면 memcpy 기반 스냅샷·직렬화·롤백이 깨진다. 팀원이 무심코 `std::string`을 넣으면 리뷰어가 아니라 **빌드가** 거부한다.

### 결정 2. GameplayHookRegistry — (champion × variant) 2D 함수포인터 테이블

**왜**: 챔피언 스킬 로직을 중앙 switch로 쌓으면 챔피언 수에 비례해 한 파일이 비대해지고, 협업 시 같은 파일에서 병합 충돌이 난다. 150챔피언이 목표라면 "새 챔피언 추가 = 자기 파일만 작성"이 되어야 한다.

**구조** (`Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h`):

```cpp
using HookFn = void(*)(GameplayHookContext&);
HookFn m_table[256][256];   // [champion][variant]

constexpr u32_t MakeGameplayHookId(eChampion champ, u16_t variant)
{
    return (static_cast<u32_t>(champ) << 16) | variant;
}
```

variant는 **스킬 슬롯(BA/Q/W/E/R) × 라이프사이클 단계**의 조합이다: KeySwap `0x001x` / OnCastAccepted `0x002x` / CastFrame `0x003x` / Recovery `0x004x`. 한 스킬이 캐스트 수락 시점과 판정 프레임에 서로 다른 훅을 가질 수 있다. 챔피언은 자기 `.cpp`에서 필요한 조합만 `Register`하고, 등록이 없으면 `Dispatch`가 false를 반환해 범용 폴백 데미지로 흐른다.

- **대안 1**: 중앙 switch/if 체인 — 초기 구현은 이랬고, 지금도 잔재가 남아 있다(④ 참조).
- **대안 2**: 챔피언별 가상 인터페이스(`IChampionSim` 상속) — vtable 포인터가 컴포넌트에 들어가는 순간 trivially_copyable 전제와 충돌하고, 훅 단위(스킬×단계)의 세밀한 등록이 어렵다.
- **선택 이유**: 함수포인터 테이블은 O(1) 디스패치 + 개방-폐쇄(새 챔피언이 중앙 코드를 안 건드림) + POD 전제 유지의 3박자가 맞는다.
- **감수한 비용**: 256×256 포인터 테이블(8바이트 × 65536 = 512KB)의 고정 메모리. 챔피언 로스터 규모에서 무시 가능하다고 판단했다. 또 함수포인터라 챔피언별 상태는 별도 컴포넌트(`AsheSimComponent` 등)로 분리해야 하는데, 이건 오히려 ECS 원칙과 일치한다.

### 결정 3. 스킬 캐스트 파이프라인 — 검증 → 상태 변경 → 로직 훅 → 복제 이벤트

`HandleCastSkill` (`Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp:1947`)이 한 번의 캐스트를 4단으로 처리한다:

1. **다중 거부 게이트** — 스킬 상태 없음 / 잘못된 슬롯 / 상태 블록(CanCast) / 죽은 타겟 / 언타겟터블 / 미습득 / 쿨다운. 각 거부는 `LogCastSkill("reject", 사유)`로 사유가 남는다. "왜 스킬이 안 나갔나"가 로그 한 줄로 답이 된다.
2. **정체성 해석 + 상태 변경** — `CSpellbookFormOverrideSystem::ResolveSkill`로 base/hook/cooldown 챔피언 3분할 해석(결정 5), 스테이지 판정(2스테이지 윈도), 쿨다운 기록, ActionState 시작(`uStartTick`/`uImpactTick`/`uEndTick`).
3. **로직 훅 디스패치** — `BuildPrimarySkillHookId` → `DispatchGameplayHookIfAvailable`. 훅이 투사체 스폰·상태이상·펜딩 히트를 만든다. 훅이 없으면 `EnqueueFallbackSkillDamage` 범용 폴백.
4. **복제 이벤트 방출** — `SkillCast` + `EffectTrigger` 이벤트를 큐잉. `EffectTrigger.effectId`에 훅 id를 그대로 실어 클라 FX 큐 선택의 키로 쓰고, stage/rank/slot을 u16 flags에 비트패킹한다 (`:2233`).

핵심은 **판정(서버 상태 변경)과 연출(복제 이벤트)이 같은 함수에서 분리 방출**된다는 것이다. 클라는 EffectTrigger를 받아 FX를 재생할 뿐, 판정에 관여하지 않는다. BA의 실제 임팩트는 즉시가 아니라 `CombatActionSystem`이 `uImpactTick` 도달 시 발행한다 (`Shared/GameSim/Systems/Combat/CombatActionSystem.cpp:304`) — 캐스트 프레임과 판정 프레임의 분리다.

### 결정 4. 데미지를 즉시 적용하지 않고 "명령 엔티티"로 큐잉

**왜**: 여러 소스가 같은 tick에 같은 타겟을 때릴 때, 적용 순서가 호출 순서(=순회 순서)에 의존하면 결정론이 깨진다.

`EnqueueDamageRequest`는 새 엔티티를 만들어 `DamageRequestComponent`만 붙인다 — 데미지가 곧 명령 엔티티다 (`Shared/GameSim/Systems/Damage/DamagePipeline.cpp:386`). `CDamageQueueSystem::Execute`가 tick 끝에 `CollectSorted`로 정렬 수집해 순서대로 적용하고, 킬이면 스코어/킬피드/Viego 소울/XP 후속 처리를 한 곳에서 수행한 뒤 명령 엔티티를 파괴한다 (`Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp:308`).

적용 자체는 고정 파이프라인이다 (`DamagePipeline.cpp:335`): `BuildRawDamage`가 `DamageRequest`에 미리 해석된 flat/total AD/bonus AD/AP/대상 체력 계수를 합산 → 치명타 → 타입별 저항(물리=armor+관통, 마법=MR, True=무시) → Yasuo/Annie 실드 → clamp → 체력바닥(Kindred R). 계수와 정책은 캐스트/챔피언 훅이 `GameplayDefinitionQuery`와 `SkillEffectSpec.damage/params`에서 읽어 `DamageRequest`에 한 번만 기록한다. 사용되지 않던 `SkillScalingRegistry/Table` 이중 공식 경로는 제거했다.

- **감수한 비용**: 데미지당 엔티티 생성/파괴 1회와 1tick 이내의 적용 지연. 30Hz에서 체감 불가능하고, 대가로 "동시 데미지의 결정론적 순서 + 킬 후속처리 집중화"를 얻었다.

### 결정 5. SpellbookFormOverride — 한 캐스트에 챔피언 정체성이 셋

Sylas 궁 강탈, Viego 빙의를 지원하며 얻은 통찰: **하나의 캐스트에 "누구의 스킬인가"가 세 개다**. `ResolveSkill`이 `{baseChampion, hookChampion, cooldownChampion}`을 분리 반환한다 (`Shared/GameSim/Systems/SpellbookFormOverride/SpellbookFormOverrideSystem.cpp:63`).

- Sylas가 훔친 Lux R을 쓰면: base=Sylas(애니/비주얼 정체성), hook=Lux(게임플레이 로직), cooldown=Sylas(CD 추적).
- Viego 빙의(FormOverrideComponent, skillSlotMask로 킷 전체 교체)면 hook/cooldown이 희생자 챔피언으로 넘어간다.

이 분해 없이 "현재 챔피언" 단일 변수로 처리하면 훔친 스킬의 CD가 원래 주인에게 걸리는 식의 버그가 계속 나온다. 정체성을 축별로 쪼갠 것이 이 시스템의 전부다.

### 결정 6. Engine 어댑터 경계 (Phase 7F) + 컴파일러 밖 규칙은 lint로

**왜**: Shared/GameSim이 Engine의 ECS/DX/Windows 헤더에 강결합돼 있었다. 한 번에 절단하면 78+ 파일이 동시에 흔들린다.

**전략 — 점진적 마이그레이션**: `Shared/GameSim/Core/Ecs/*` 9종과 `Core/World/World.h` 어댑터 파일을 만들었다. 각 어댑터는 한 줄로 Engine 헤더를 재노출하고, GameSim 전 파일은 어댑터만 include한다:

```cpp
// Shared/GameSim/Core/World/World.h
namespace SharedSim
{
    // Temporary adapter boundary for Phase 7F.
    using World = ::CWorld;   // 백엔드는 아직 Engine — 교체는 이 파일 1개 변경
}
```

- **트레이드오프를 정직하게**: 백엔드는 여전히 Engine의 `::CWorld`라서 dllexport 링크 의존과 EngineSDK include 경로가 남아 있다. 지금 얻은 것은 "Shared 소유 결정론 ECS로의 교체가 **1파일 변경**으로 축소된 상태"까지다. include 경계 절단(완료)과 백엔드 교체(미완)를 의도적으로 분리한 것이다.

**규칙의 기계 강제**: "Shared는 Engine/DX11/ImGui/제품 코드를 include하지 않는다"는 규칙은 컴파일러가 강제할 수 없다 — include하면 그냥 컴파일된다. 그래서 GameSim PreBuild에서 도는 텍스트 lint `Tools/Harness/Check-SharedBoundary.ps1`가 `#include "ECS/`, `Engine_Defines.h`, `d3d11`, `imgui`, `Client/`, `Server/` 패턴을 스캔해 위반 시 exit 1로 **빌드를 실패**시킨다. 어댑터 디렉터리만 화이트리스트다. 아키텍처 규칙의 회귀를 리뷰어의 눈이 아니라 빌드 게이트로 막는다.

### 결정 7. 봇도 플레이어와 같은 문으로 — GameCommand 생산자 원칙

봇 AI는 Shared에 있고 서버 tick 안에서 돌지만, **truth 컴포넌트를 직접 mutate하지 않는다**. `CChampionAISystem::Execute`는 GameCommand만 생산하고, 그 명령이 인간 입력과 동일한 CommandExecutor 검증 경로를 통과한다 (`Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.h` 협업 규약 주석). 봇이 구조적으로 치트할 수 없다.

의사결정은 이질적 가치(킬/파밍/시즈/생존)를 **골드 단일 통화**로 환산해 비교한다 (`Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.h`): 챔피언 가치=킬골드×체력비율, 미니언=막타골드, 포탑=시즈골드, 레벨 1≈120골드 등가. 입력은 평탄화된 수치뿐이고 난수·시간이 없어 brain 자체가 결정론적이다. brain은 무상태(stateless)이고 모든 상태는 `ChampionAIComponent`에 있어 엔티티 간 공유·재진입이 안전하다.

---

## ④ 어려웠던 점과 해결

### 4-1. MirrorHealth — 이중 진실(dual truth)의 대가

체력의 authoritative 소스는 `HealthComponent`지만, 초기 설계에서 Champion/Minion/Structure/Turret/Jungle 컴포넌트가 각자 hp/maxHp 필드를 들고 있었다. 지금은 데미지 적용 후 `MirrorHealth`가 HealthComponent 값을 5개 컴포넌트 패밀리에 전부 복사한다 (`Shared/GameSim/Systems/Damage/DamagePipeline.cpp:67`). 팀 조회 `TryGetTeam`도 6종 컴포넌트를 순차 검사한다.

단일 진실 원칙을 어긴 설계 실수를 즉시 통합하지 않고 "쓸 때 미러링"으로 화해한 이유는 호출부가 광범위해 일괄 교체 리스크가 컸기 때문이다. 올바른 방향(HealthComponent 단일화 + 리더 이관)은 알고 있고, 미러링은 그 이관이 끝날 때까지의 명시적 부채다. 면접에서 "설계 실수 경험"을 물으면 이 사례를 그대로 말한다.

### 4-2. LeeSin 콤보 R 스킵 + 콤보 교착 (commit 3847f3f)

봇 콤보에서 두 버그를 같이 잡았다:

1. **stepCount off-by-one**: LeeSin 콤보는 9스텝(Q/Q2/BA/E/BA/E2/Ward/W-hop/R)인데 stepCount가 8로 지정돼 있었다. 인덱스가 `comboStep % stepCount`로 계산되므로 (`Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp:1665`) index 8 — 마지막 R — 이 **영원히 선택되지 않는다**. 궁이 조용히 스킵되는, 모듈러 연산과 배열 길이 상수의 고전적 불일치.
2. **콤보 교착**: 사거리 안에 들어온 스텝이 자기 게이트(hp 비율 등)를 통과하지 못하면 다음 스텝으로 넘어가지 않고 멈춰서, Sylas R 강탈 스텝이 프리즈됐다. 실패한 in-range 스텝이 stall 대신 advance하도록 상태머신 진행 규칙을 고쳤다.

교훈: 콤보는 데이터 상수(stepCount)와 진행 로직 두 층에서 각각 깨질 수 있다. 이런 AI 버그를 코드 추론으로 잡지 않으려고 `PushChampionAIDecisionTrace`가 매 결정의 점수(championScore/farmScore/structureScore)·체력비·블록 사유를 링버퍼로 남긴다 (`ChampionAISystem.cpp:484`) — "봇이 왜 이랬나"를 계측된 이력으로 역추적한다.

### 4-3. 침묵 폴백이 데이터 회귀를 밸런스 버그로 위장하는 문제

수치 조회는 3계층 폴백이다: 엔티티별 오버라이드 컴포넌트 → GameplayDefinitionPack → 레거시 하드코딩 DB. 함정은 팩 miss가 **조용히** 레거시로 흘러가면 "깨진 데이터 팩"이 "밸런스가 미묘하게 이상함"으로만 나타난다는 것.

그래서 모든 레거시 폴백 진입점에 bounded 카운터 로그를 심었다 (`Shared/GameSim/Definitions/GameplayDefinitionQuery.cpp:48`, `[Data] pack miss -> legacy ...`, 상한 32). 운영 규칙: **정상 로스터 스모크에서 이 로그가 0줄이 된 조회 경로부터 레거시 DB 리더를 삭제한다.** "두 번째 truth"인 폴백을 관측 가능하게 만들어, 레거시 제거의 안전한 순서를 계측으로 정의한 것이다.

같은 계열의 방어가 데이터 버전 drift다: cook 산출물이 체크인된 .generated.cpp라 원본과 어긋나도 빌드는 침묵한다. `DataPackManifest.uBuildHash`를 두고 Hello 패킷 끝에 `dataBuildHash` 필드를 추가했다 (`Shared/Schemas/Hello.fbs:12` — 끝 필드 추가라 하위호환, 0이면 구버전으로 보고 검사 생략). 클라가 자기 해시와 비교해 불일치 시 `[Data] build hash mismatch` bounded 로그를 남긴다 (`Client/Private/Network/Client/SnapshotApplier.cpp:505`). "미묘하게 다른 수치"가 아니라 **접속 시점의 명시적 진단**으로 드러난다.

### 4-4. Windows 매크로 오염 두 건

- **디버그 출력 무음화**: Engine_Defines.h가 OutputDebugStringA를 게이트 래퍼로 매크로 재정의하는데, 게이트가 Server/GameSim 빌드에서는 꺼져 있다. Shared에서 raw OutputDebugStringA를 쓰면 무음, Engine_Defines를 include하면 `<dinput.h>`/using namespace 오염이 Shared로 전이. 해결: Shared 소유 `Shared/GameSim/Core/Debug/SimDebugOutput.h`가 Engine_Defines와 **같은 가드**(`WINTERS_DEBUG_STRING_GATE_DEFINED`)를 공유하는 최소 정의만 제공 — 두 헤더가 한 TU에서 만나도 충돌하지 않고, sim 진단은 `WintersOutputAIDebugStringA`로 살아남는다.
- **min/max vs FlatBuffers**: 복제 이벤트 직렬화 경계에서 Windows min/max 매크로가 FlatBuffers 헤더를 깨는 문제를 `#pragma push_macro/undef/pop_macro`로 감쌌다 (`Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.cpp:3`).

### 4-5. 네트워크 id 경계

서버 내부 EntityID를 와이어에 노출하지 않는다. `EntityIdMap`이 로컬 EntityID ↔ NetEntityId(u32, 1부터 증가)를 두 unordered_map으로 양방향 매핑한다 (`Shared/GameSim/Replication/EntityIdMap.h`). `IssueNew`는 idempotent(이미 있으면 기존 반환)이고, 복제 이벤트 직렬화는 net id 미바인딩 엔티티의 이벤트를 drop한다. 투사체는 스폰 시 IssueNew, 파괴 이벤트 전송 후 언바인드 예약으로 생성-파괴 생명주기를 net id와 동기화한다.

---

## ⑤ 향후 개선 방향

챔피언 추가 절차의 현재와 목표를 함께 말한다.

**현재의 챔피언 추가 절차** (Ashe가 템플릿 — `Shared/GameSim/Champions/Ashe/AsheGameSim.cpp:311`):

1. `Shared/GameSim/Champions/<Name>/` 에 state 컴포넌트(POD, static_assert) + 훅 함수 + `Tick` + `RegisterHooks`(static 가드로 1회 등록) 작성.
2. 수치는 코드에 쓰지 않고 `GameplayDefinitionQuery`, `SkillEffectSpec.damage`, `ResolveSkillEffectParam`으로 데이터에서 읽어 `DamageRequest`의 해석된 scalar로 넘긴다. 기획자 밸런싱과 개발자 로직의 분리 지점이다.
3. champions.json에 스탯/스킬 수치 authoring → cook → 3팩 재생성.
4. 클라 쪽은 비주얼 팩(모델/애님 키/FX 큐) 등록.

**남은 부채와 로드맵**:

- **D-1. CommandExecutor 특수케이스 이관**: 훅 레지스트리로 대부분 옮겼지만 2792줄짜리 CommandExecutor에 챔피언 분기가 잔존한다 — `BuildPrimarySkillHookId`의 챔피언별 switch(`:1065`, Ezreal/Kalista E와 Riven Q는 OnCastAccepted, 나머지는 CastFrame), Kalista 패시브 대시(`:879`), Yasuo/Yone/Annie/Sylas 스테이지·타겟 특수처리(`:2007~`). 방향은 pre-cast 훅 변형(StageResolve/TargetResolve/CooldownPolicy)을 레지스트리에 추가해 챔피언당 1슬라이스로 각자의 GameSim.cpp로 옮기는 것. 한 번에 못 옮기는 이유는 챔피언별 동작 검증이 각각 필요하기 때문이다.
- **데이터 분리 스코어카드 28%**: 18개 도메인을 "기획자가 코드 없이 값을 바꿀 수 있나"로 채점해 5.0/18≈28% (`.md/architecture/WINTERS_DATA_ARCHITECTURE.md` §4). 과장하지 않는 대신, 밸런싱 빈도가 가장 높은 스탯/스킬 수치 도메인은 이미 JSON→재cook으로 분리 완료라는 가중 관점을 함께 제시한다. 남은 병목은 D-2~D-6 슬라이스로 명시돼 있다.
- **어댑터 백엔드 교체**: Phase 7F의 마지막 단계 — `using World = ::CWorld`를 Shared 소유 결정론 ECS로 바꾸는 1파일 변경. 이때 dllexport 링크 의존이 함께 끊긴다.
- **id 체계 결정(D-6)**: wire identity가 eChampion(u8, 255 cap)인데 150챔프+스킨 스케일에서 DefinitionKey(u32)로 승격할지 결정 대기.
- **빌드 그래프**: flatc 코드젠 타깃이 Inputs/Outputs 명시 없이 3개 프로젝트에서 매 빌드 실행돼 병렬 빌드(`msbuild /m`) 시 같은 *_generated.h를 동시 재작성할 수 있는 레이스가 문서화돼 있다 (`.md/architecture/WINTERS_DEPENDENCY_MAP.md`). 증분 조건 명시가 수정 방향.

---

## ⑥ 면접 Q&A

### Q1. "결정론 시뮬레이션을 어떻게 보장했나요?"

**답변 골격**: 발산 원인 세 가지를 각각 장치로 봉쇄했다. (1) 시간 — 벽시계가 시뮬에 못 들어오게 30Hz 고정 tick 정수만 사용(`DeterministicTime`). (2) 난수 — 전역 RNG 소비 순서 의존을 없애려 xorshift64를 TickContext로 주입하고 `MakeSubSeed(tick, entity, skill)`로 호출 지점별 독립 스트림 파생. (3) 순회 — unordered_map 순회 순서가 리플레이 발산의 고전 원인이라, 순서 민감 시스템은 EntityID 정렬 순회(`CollectSorted`)로 통일. 추가로 컴포넌트를 static_assert로 trivially_copyable 강제해 memcpy 스냅샷 전제를 지켰다.

**꼬리질문 대비**: "부동소수점 결정론은?" → 같은 바이너리(MSVC, 같은 플래그)를 클라/서버가 링크하므로 현 범위(단일 플랫폼)에서는 문제가 안 된다. 크로스 플랫폼 록스텝이라면 고정소수점 또는 SW float가 필요해지는데, 서버 권위 + 스냅샷 복제 구조라 클라 발산이 즉시 교정된다는 점도 완충이다.

### Q2. "150개 챔피언을 어떻게 확장 가능하게 설계했나요?"

**답변 골격**: 중앙 switch 대신 `HookFn m_table[256][256]` — (champion × variant) 2D 함수포인터 테이블. variant는 슬롯×라이프사이클(KeySwap/OnCastAccepted/CastFrame/Recovery)이라 한 스킬이 단계별로 다른 훅을 갖는다. 챔피언은 자기 .cpp에서 self-registration하고 없으면 범용 폴백. 새 챔피언 추가가 중앙 코드 수정 없이 끝나서 O(1) 디스패치와 병합 충돌 최소화를 같이 얻는다.

**꼬리질문 대비**: "그럼 지금 완벽히 분리돼 있나?" → 아니다. CommandExecutor에 pre-cast 특수케이스(스테이지/타겟 해석)가 잔존하고, 이를 pre-cast 훅 변형으로 이관하는 D-1 로드맵이 있다. 이상과 현실의 갭을 아는 것까지가 답이다.

### Q3. "서버 권위 스킬 처리 흐름을 설명해 보세요."

**답변 골격**: `HandleCastSkill` 4단 — ① 다중 거부 게이트(각 거부에 사유 로그) ② 정체성 해석(base/hook/cooldown 3분할)과 쿨다운/ActionState 상태 변경 ③ 훅 디스패치(투사체/상태이상 생성, 데미지는 큐잉) ④ SkillCast/EffectTrigger 복제 이벤트 방출. 판정은 서버 상태에만, 연출은 이벤트로만 — 클라는 EffectTrigger의 effectId로 FX 큐를 고를 뿐 판정에 관여하지 않는다. BA 임팩트는 uImpactTick 도달 시 CombatActionSystem이 발행해 캐스트와 판정 프레임을 분리한다.

**꼬리질문 대비**: "클라가 쿨다운을 속이면?" → 쿨다운 게이트가 서버 HandleCastSkill 안에 있으므로 클라 조작은 reject 로그 한 줄로 끝난다.

### Q4. "왜 데미지를 바로 적용하지 않고 큐로 뺐나요?"

**답변 골격**: 같은 tick에 여러 소스가 같은 타겟을 때릴 때 적용 순서가 호출 순서에 의존하면 결정론이 깨진다. 데미지를 DamageRequestComponent만 붙인 "명령 엔티티"로 만들고 tick 끝에 EntityID 정렬로 드레인하면 순서가 항상 같다. 부가 이득으로 킬 후속처리(스코어/킬피드/XP/Viego 소울)가 드레인 지점 한 곳에 모인다.

**꼬리질문 대비**: "엔티티 생성 비용은?" → 데미지 빈도(30Hz, 수십 엔티티)에서 무시 가능. 프로파일에서 문제가 되면 풀링이 다음 단계지만, 측정 없이 미리 최적화하지 않았다.

### Q5. "클라와 서버가 공유하는 데이터를 어떻게 나눴나요?"

**답변 골격**: authoring JSON을 cook해서 3팩으로 — SharedContract(타입만, 값 없음) / ServerPrivate(스탯·스킬 수치, tick 판정용) / ClientPublic(비주얼). 원칙은 "값은 한쪽만 소유, 클라는 재계산 금지". 성장 공식은 서버가 적용해 StatComponent로 복제한다. 애니메이션도 게임플레이 타이밍(서버 팩)과 비주얼 재생(클라 팩)으로 이원화 — 클라 프레임이 서버 판정을 만들면 안 된다. 치트 방지(서버 수치 비공개)와 협업(기획/아트/개발 역할 분리)이 같은 구조에서 나온다.

**꼬리질문 대비**: "클라/서버 데이터 버전이 어긋나면?" → Hello 핸드셰이크의 dataBuildHash 대조로 접속 시점에 폭발시킨다. 프로토콜 끝 필드 추가라 하위호환(0=검사 생략).

### Q6. "아키텍처 규칙이 시간이 지나며 무너지는 걸 어떻게 막았나요?"

**답변 골격**: 규칙을 계층별로 다른 강제 장치에 심었다. 컴파일러가 잡을 수 있는 것은 static_assert(trivially_copyable). 컴파일러가 못 잡는 include 경계는 PreBuild 텍스트 lint(Check-SharedBoundary.ps1)가 위반 시 빌드 실패. 런타임 계약(팩 miss, 해시 drift)은 bounded 카운터 로그. 문서로만 남긴 규칙은 반드시 풍화된다는 전제로, "리뷰어의 눈"을 "기계 게이트"로 바꾸는 것이 원칙이다.

**꼬리질문 대비**: "텍스트 lint는 우회 가능하지 않나?" → 맞다, 정규식이라 매크로 간접 include는 못 잡는다. 최종형은 vcxproj include 경로 자체에서 EngineSDK를 제거하는 것이고, 그게 Phase 7F 어댑터의 완성 조건이다.

### Q7. "봇이 치트하지 않는다는 걸 어떻게 보장하나요?"

**답변 골격**: 구조로 보장한다. 봇 시스템은 truth 컴포넌트를 직접 수정할 수 없고 GameCommand만 생산한다 — 인간 입력과 동일한 CommandExecutor 검증 게이트를 통과한다. 사거리 밖 스킬, 쿨다운 중 캐스트는 봇이어도 reject된다. 의사결정은 골드 단일 통화 환산(ChampionAIValuation)으로 결정론적이고, brain은 무상태라 재진입이 안전하다.

**꼬리질문 대비**: "봇 행동 버그는 어떻게 디버깅?" → 매 결정의 점수·블록 사유를 링버퍼로 계측(PushChampionAIDecisionTrace)해 이력으로 역추적. 실제로 LeeSin R 영구 스킵(stepCount 8인데 스텝 9개 — 모듈러가 index 8을 못 만듦)과 게이트 실패 시 진행 정지 교착을 이렇게 잡았다.

### Q8. "설계 실수를 겪은 경험이 있나요?"

**답변 골격**: 체력의 이중 진실. 초기에 컴포넌트 지역성을 이유로 Champion/Minion/Turret 등 5개 컴포넌트에 hp를 중복 보유시켰고, HealthComponent를 도입한 뒤에도 구 리더들이 남아 데미지 적용마다 MirrorHealth로 5곳에 복사하는 코드가 생겼다. 즉시 통합하지 않은 건 호출부가 광범위해 리스크가 컸기 때문 — "쓸 때 미러링"으로 버티며 리더를 점진 이관하는 중이다. 단일 진실 원칙은 어기는 순간이 아니라 되돌릴 때 비용을 청구한다는 걸 배웠다.

**꼬리질문 대비**: "지금 다시 설계한다면?" → 처음부터 HealthComponent 단일 + 파생 조회 헬퍼. hp처럼 모든 엔티티 타입이 갖는 상태는 타입별 컴포넌트가 아니라 공통 컴포넌트가 기본값이어야 한다.

### Q9. "레거시 코드를 어떻게 안전하게 제거하나요?"

**답변 골격**: 폴백 경로를 계측해서 삭제 게이트를 만든다. 팩 miss → 레거시 DB 폴백 진입점 전부에 bounded 카운터 로그를 심고, 정상 로스터 스모크에서 로그 0줄이 된 경로부터 레거시 리더를 삭제한다. 침묵 폴백은 데이터 회귀를 밸런스 버그로 위장시키므로, "두 번째 truth"는 반드시 관측 가능해야 한다.

**꼬리질문 대비**: "로그 상한은 왜?" → 에러 정책상 실패류는 bounded(이 경우 32)가 기본. 무한 로그는 런타임을 오염시키고, 성공/실패가 카운터를 공유하면 성공이 예산을 소진해 실패를 은폐한 실사고가 있었다(`.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md`).

### Q10. "가장 까다로웠던 스킬 메커니즘은?"

**답변 골격**: Sylas 궁 강탈과 Viego 빙의. 해법의 핵심은 "한 캐스트의 챔피언 정체성이 셋"이라는 분해 — 비주얼(base), 로직(hook), 쿨다운(cooldown)을 각각 다른 챔피언으로 라우팅하는 `SkillOverrideResolveResult`. Sylas가 훔친 궁을 쏘면 로직은 원래 주인, CD는 Sylas. FormOverride(킷 전체, 슬롯 마스크)와 SpellbookOverride(슬롯 하나)를 별도 컴포넌트로 두고 만료는 fRemainingSec로 처리했다.

**꼬리질문 대비**: "이게 훅 레지스트리와 어떻게 맞물리나?" → ResolveSkill이 반환한 hookChampion/hookSlot으로 훅 id를 만들기 때문에, 챔피언 훅 코드는 자기 스킬이 도난당했는지 알 필요가 없다. 정체성 해석과 로직 실행이 직교한다.

---

## ⑦ 다른 챕터와의 연결

- **네트워크/복제**: 이 챕터의 복제 이벤트(SkillCast/EffectTrigger)와 EntityIdMap이 와이어로 나가는 지점 — FlatBuffers 직렬화, verify 실패 bounded drop, dataBuildHash 핸드셰이크의 프로토콜 세부는 `.md/interview/cpp/12_network_serialization.md`가 다룬다.
- **ECS 아키텍처**: CWorld/컴포넌트 스토리지의 구조와 Phase 7F 어댑터가 감싸는 대상은 `.md/interview/cpp/11_architecture_ecs.md` 참조. 이 챕터는 "왜 어댑터로 절단했나"의 의사결정 쪽을 담당한다.
- **에러 처리 철학**: bounded 카운터, dead diagnostics 금지, 성공/실패 카운터 분리 규칙의 전체 정책은 `.md/interview/cpp/10_error_handling.md`와 `.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md`.
- **데이터 파이프라인 원본 문서**: 3팩 cook, 스코어카드, D-1~D-6 슬라이스의 1차 소스는 `.md/architecture/WINTERS_DATA_ARCHITECTURE.md`.
- **의존성/빌드 그래프**: 정적 라이브러리 소비 구조, flatc 코드젠 레이스, EngineSDK include 경로 현황은 `.md/architecture/WINTERS_DEPENDENCY_MAP.md`.
- 엔진 세트의 서버 런타임(IOCP/스레딩) 챕터와는 "ingress mutex까지만 네트워크 스레드, tick 락 분리" 지점에서 만나고, FX/렌더 챕터와는 EffectTrigger.effectId → 클라 FX 큐 선택 지점에서 만난다.
