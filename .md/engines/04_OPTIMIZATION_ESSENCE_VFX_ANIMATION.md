# 최적화의 본질 — VFX·애니메이션 최적화와 언리얼/유니티/Winters의 도구

작성일: 2026-07-10. 최적화라는 행위의 본질(계측·예산·데이터 형태·유계 설계)을 Winters 실증 사례로 고정하고, VFX(Niagara/VFX Graph/WFX)와 애니메이션 최적화의 비용 구조를 3엔진(UE·Unity·Winters) 도구 관점에서 정리한다. 기술면접의 최적화 질문 대응 프레임과 툴 개발자 포트폴리오 관점의 결론을 포함한다.

표기 규칙: Winters 사실은 레포 경로로 검증했다. 이 세션에서 웹 검증된 외부 사실은 출처 도메인을 괄호로 표기한다(예: (docs.unity3d.com)). 일반 엔지니어링 지식에서 온 사실은 절 안에서 처음 나올 때 (일반 지식)으로 표기하고, 검증하지 못한 항목은 (미검증)으로 표기한다.

---

## §1 최적화의 본질

### 1.1 계측 없이는 최적화가 아니라 추측이다

최적화의 단위 작업은 코드 수정이 아니라 **측정 → 범인 확정 → 최소 수정 → 재측정**의 사이클이다. 측정이 앞에 없으면 뒤의 모든 작업은 추측 위에 서 있고, 재측정이 뒤에 없으면 "고쳤다"는 주장에 증거가 없다.

**Winters 실증 — 17.8ms 사건.** 프레임이 17.8ms로 저하됐을 때, 코드 추론 대신 11곳에 `WINTERS_PROFILE_SCOPE`, 4종의 `WINTERS_PROFILE_COUNT`를 먼저 심었다. 결과: 의심하던 Nav/A\*는 각 0.003ms로 무죄, `Minion::AnimUpdate`가 16ms(프레임의 90%)로 범인 확정. 수정은 정적 엔티티에 `RenderComponent::bAnimated=false`를 도입해 스키닝 갱신을 스킵하는 최소 변경 하나였고, 재측정으로 **17.8ms → 9ms**를 확인했다(`.md/interview/tool-development.md` §9). 이 사건이 남긴 교훈 세 가지:

1. **의심 순서와 실제 범인은 다르다.** "패스파인딩이 느릴 것"이라는 직관은 계측 앞에서 무너졌다. 계측의 첫 번째 가치는 무죄 증명이다 — 범인 후보를 지우는 것이 범인을 찾는 것만큼 빠르다.
2. **시간 프로파일과 카운터는 다른 축이다.** "어디가 느린가"는 scope가, "왜 느린가"는 카운터(호출 횟수, 방문 노드 수)가 답한다. 실물: `Engine/Private/Manager/Navigation/Pathfinder.cpp`에 `WINTERS_PROFILE_SCOPE("AStar::FindPath")`(682행)와 `WINTERS_PROFILE_COUNT("AStar::DirectBypass", 1)`(589행)가 함께 있다 — 시간이 안 나오면 횟수를 보고, 횟수가 안 나오면 시간을 본다.
3. **수정은 범인에게만 한다.** 16ms의 범인이 확정된 뒤에는 렌더러 개편도, ECS 재설계도 아닌 bool 플래그 하나로 끝났다. 계측이 없으면 "혹시 몰라서" 하는 수정이 늘어나고, 그 수정들이 다음 버그의 원인이 된다.

**계측 인프라 자체의 설계 요건 — `Engine/Include/ProfilerAPI.h` 실물.** 계측 코드는 (a) 대상의 성능을 바꾸지 않아야 하고(관찰자 효과), (b) 출시 빌드에서 진짜 0 비용이어야 하며, (c) 멀티스레드에서 안전해야 한다.

```cpp
// Tracy zone + 자체 F3 HUD scope 를 한 매크로로 동시 기록 (ProfilerAPI.h 47~49행)
#define WINTERS_PROFILE_SCOPE(name) \
    ZoneNamedN(WINTERS_PROFILE_CAT(_tracyZone_, __LINE__), name, true); \
    ::CProfileScope WINTERS_PROFILE_CAT(_winProfScope_, __LINE__)(name)
```

- **RAII 스코프**: 생성자 Push / 소멸자 Pop — early return, 예외 경로에도 안전하다.
- **컴파일타임 게이트**: `WINTERS_PROFILING` 미정의 시 매크로 전체가 `((void)0)` — 출시 빌드에서 분기 비용조차 없다(같은 파일 54~58행).
- **`__LINE__` 2단계 CONCAT**: `a##b` 직접 결합은 `__LINE__` 확장 전에 붙어 같은 블록에 스코프 2개를 못 둔다 → 간접 매크로 한 겹으로 지연 확장(41~42행). 직접 밟은 매크로 함정이다.
- **유계 버퍼**: 프레임당 이벤트/카운터/스코프 통계 상한이 컴파일 타임 상수로 박혀 있다 — `PROFILER_MAX_EVENTS_PER_FRAME=4096`, `PROFILER_MAX_COUNTERS_PER_FRAME=128`, `PROFILER_MAX_SCOPE_STATS_PER_FRAME=128`, `PROFILER_MAX_TREE_EVENTS_PER_FRAME=256`(`Engine/Public/Core/Profiler/ProfilerTypes.h` 4~7행). 계측이 스스로 메모리를 무한 증식시키면 계측이 성능 문제가 된다.
- **DLL 경계 처리 2건**: Tracy 구현부는 Engine DLL 한 곳에만 두고 Client/Server는 import로 같은 인스턴스에 기록한다(`ProfilerAPI.h` 5~6행 주석). 카운터 이름 비교는 포인터가 아니라 `strcmp`로 한다 — 같은 문자열 리터럴이 DLL/번역단위마다 다른 주소를 갖기 때문에 포인터 비교는 같은 카운터를 중복 행으로 쪼갠다(`Engine/Private/Core/Profiler/CPUProfiler.cpp` 120~121행 주석).
- **스레드 안전**: 현행 구현은 thread_local 스코프 스택(`CPUProfiler.cpp` 29행) + 뮤텍스 보호 프레임 버퍼다. 이 형태에 도달하기 전, JobSystem 워커의 thread_local 버퍼 수집 race를 실제로 겪고 Decision/Apply 2-pass + 스레드 슬롯 고정으로 수리한 이력이 있다(`.md/interview/tool-development.md` §9) — "프로파일러도 동시성 버그가 나는 코드"라는 실감이 설계 요건 (c)의 근거다.

계측 대상은 코드에 이미 광범위하게 배선되어 있다 — `WINTERS_PROFILE_SCOPE/COUNT` 사용처는 Engine의 NavigationSystem/Pathfinder/ModelRenderer/UI_Manager부터 Client의 Scene/Manager/FX 시스템, Shared의 TurretAISystem까지 60개 파일 이상이다(rg 확인). HUD는 F3 토글로 뜨고 F4가 JSON 캡처 후 열린다(`Engine/Private/Framework/CEngineApp.cpp` 301, 433행).

### 1.2 예산(budget) 사고 — 프레임은 배분하는 것이다

60fps는 16.6ms, 30fps는 33.3ms라는 **고정 지출 한도**다. (일반 지식) 실무 최적화는 "빠르게"가 아니라 "시스템별 예산 안으로"가 목표다 — 렌더 N ms, 시뮬레이션 N ms, UI N ms, 여유분(headroom) N ms처럼 배분하고, 초과한 시스템만 조사한다. 예산이 없으면 "느리다"는 불만은 있어도 "무엇이 초과인가"라는 질문이 성립하지 않는다.

Winters는 이것을 문서 규칙으로 박았다 — `.md/architecture/WINTERS_CODEBASE_COMPASS.md` 112행: **"최적화는 JSON scope/counter와 frame budget으로 증명한다. Profiler 표시를 늘리는 것만으로는 최적화로 보지 않는다."** 성능 계획서가 측정 scope/counter와 프레임 예산을 명시해야 계획으로 인정된다는 뜻이고, 실제 계획 문서들이 이 형식을 따른다(`.md/plan/performance/01_FRAME_PACING_PROFILER_BASELINE_PLAN.md`, `02_UPDATE_SPIKE_BUDGET_PLAN.md`, `2026-06-13_PHASE0_MEASUREMENT_INFRA_PLAN.md` — Phase 0이 측정 인프라인 로드맵).

예산은 프레임 시간만이 아니라 **프레임당 작업 개수**로도 배분된다. 실물 두 가지(`Client/Private/Manager/Minion_Manager.cpp`):

```cpp
static constexpr uint64_t kMinionAnimUpdateBudget = 3u;          // 37행: 프레임당 비우선 애니 갱신 3개
static constexpr uint32_t kNetworkVisualBindBudgetPerFrame = 3u; // 39행: 프레임당 비주얼 바인딩 3개
```

미니언 애니메이션은 프레임당 3개까지만 풀 갱신하고 나머지는 다음 프레임으로 밀며, 초과분은 `WINTERS_PROFILE_COUNT("Anim::BudgetSkipped", ...)`(808행)로 집계돼 "예산 때문에 미룬 양"이 항상 보인다. 네트워크 스폰 시 렌더러 바인딩도 프레임당 3개로 제한해 스파이크를 여러 프레임에 분산한다(706행). 예산 사고의 핵심은 이것이다: **일이 많아지면 프레임이 늘어나는 것이 아니라, 프레임이 고정이고 일이 밀린다.** 서버도 같다 — 30Hz 서버 tick은 33.3ms 예산이고, 시뮬레이션이 이를 초과하면 tick이 밀리므로 결정론 시뮬(GameSim) 코드에는 시각 연출 비용이 애초에 들어가면 안 된다(§3에서 재론).

### 1.3 데이터 형태가 곧 성능이다

(일반 지식) CPU는 메모리를 바이트가 아니라 캐시 라인(통상 64B) 단위로 읽는다. 같은 연산이라도 데이터가 어떻게 놓였는가에 따라 캐시 미스 횟수가 달라지고, 그것이 곧 실행 시간이다. 그래서 최적화의 상당수는 알고리즘 교체가 아니라 **데이터 배치 교체**다.

- **SoA vs AoS**: 파티클처럼 "모든 원소의 같은 필드"를 훑는 워크로드는 Structure of Arrays가 캐시와 SIMD에 유리하다. Winters 실물 — `Engine/Public/FX/Exec/FxParticlePool.h`의 `CFxParticlePool`은 `position/velocity/color/size/age/lifetime`을 각각 독립 `std::vector`로 든 순수 SoA이고, 설계 근거는 `.md/plan/EffectTool/03_STAGE2_PARTICLE_POOL_SOA.md`에 있다.
- **zero-copy 로드**: 쿠킹된 `.wmesh`는 파일 블롭을 통째로 읽고 정점/인덱스 블롭은 원본 버퍼 내부를 가리키는 포인터로 GPU에 직행한다(`Engine/Public/AssetFormat/Mesh/WMeshLoader.h`, `.md/interview/tool-development.md` §3). "파싱하지 않는 포맷"은 로드 최적화의 종착점이다 — 오프라인 cook이 데이터 형태를 런타임이 원하는 모양으로 미리 구부려 놓기 때문에 가능하다.
- **trivially-copyable 컴포넌트**: Winters ECS 컴포넌트는 trivially-copyable을 유지해 스냅샷 복제를 memcpy 계열로 만들 수 있게 한다 — UObject/GC/리플렉션을 전면 도입하지 않는 이유이기도 하다(`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §5).

면접 화법으로 정리하면: "빠른 코드"는 대개 "CPU가 예측 가능한 순서로, 연속된 메모리를, 필요한 필드만 읽게 만든 코드"다.

### 1.4 상한 없는 작업 금지 — 유계(bounded) 설계

프레임 루프 안의 모든 작업·버퍼·로그는 상한이 있어야 한다. 상한이 없는 것은 "평소에는 빠른데 최악의 날 프레임을 죽이는" 시한폭탄이다. Winters의 유계 설계 실물 목록:

| 대상 | 상한 | 근거 파일 |
|---|---|---|
| 프로파일러 이벤트/카운터/통계 | 4096 / 128 / 128 per frame | `Engine/Public/Core/Profiler/ProfilerTypes.h` 4~7행 |
| 실패 경로 로그 | 실패류 8, 컨텐츠 miss 64, 계측 512 (함수-로컬 static 카운터) | `.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md` 18행 |
| 미니언 애니 풀 갱신 | 프레임당 3 | `Client/Private/Manager/Minion_Manager.cpp` 37행 |
| 네트워크 비주얼 바인딩 | 프레임당 3 | 같은 파일 39행 |
| FX 파티클 | 풀 capacity에서 `Allocate`가 잘라냄(성장 없음) | `Engine/Private/FX/Exec/FxParticlePool.cpp` 24~38행 |
| 스키닝 본 수 | 로더가 `bone_count > MAX_BONES(1024)` 즉시 거부 | `Engine/Private/AssetFormat/Mesh/WMeshLoader.cpp` 11, 17행 |

로그 상한 규칙에는 실제 사고 각주가 붙어 있다: 성공/실패가 같은 카운터를 공유하면 성공이 예산을 소진해 실패가 안 보인다(위 정책 문서, CommandExecutor cast 로그 64캡 사례). 유계 설계는 성능 규칙이면서 동시에 관측가능성 규칙이다.

---

## §2 프로파일링 도구 — 3엔진 지도

도구는 "어떤 질문에 답하는가"로 분류해야 실전에서 꺼내 쓸 수 있다. 이 절의 UE/Unity 도구 사실은 이 세션의 웹 검증 범위 밖이므로 전부 (일반 지식)이다. Winters 항목은 레포로 검증했다.

| 질문 | Unreal Engine (일반 지식) | Unity (일반 지식) | Winters (검증) | 공통 (일반 지식) |
|---|---|---|---|---|
| **어디가 느린가** (프레임 내 시간 분포) | `stat unit`(Game/Draw/GPU 분리), `stat game`, Unreal Insights(Trace 기반 타임라인) | Profiler 창(CPU/GPU/Rendering 모듈, 타임라인 뷰) | F3 HUD scope 통계 + Tracy 존 타임라인(`WINTERS_PROFILE_SCOPE`가 둘을 동시 기록, `Engine/Include/ProfilerAPI.h`) | — |
| **왜 느린가** (원인 분해) | `ProfileGPU`(GPU 패스별 비용), Niagara Debugger(이미터/파티클 수·비용), `stat scenerendering`(드로우콜) | Frame Debugger(드로우콜 단위 재생), Profile Analyzer(캡처 간 비교·회귀 검출) | `WINTERS_PROFILE_COUNT` 카운터 축(A\* 방문 수, `Anim::BudgetSkipped` 등) + F4 JSON 캡처(`CEngineApp.cpp` 433행) | RenderDoc(드로우콜/셰이더/리소스 단위 GPU 캡처), PIX(DX 계열 GPU/CPU 타임라인) |
| **메모리는** | `stat memory`, LLM(Low-Level Memory Tracker), Memory Insights | Memory Profiler 패키지(스냅샷 비교, 관리 힙/네이티브 분리) | 전용 메모리 프로파일러 없음 — CRT 디버그 힙(DBG_NEW) 수준. 식별된 갭이다 | — |
| **스파이크는 언제** | Unreal Insights 장기 트레이스 | Profiler 롤링 캡처 | 유계 프레임 버퍼 + Tracy on-demand 연결(`TRACY_ON_DEMAND`, `ProfilerAPI.h` 9행) | — |

사용 원칙 세 가지:

1. **CPU/GPU 경계를 먼저 가른다.** CPU 프레임이 짧은데 화면이 느리면 GPU 바운드다 — 이때 CPU 프로파일러를 아무리 파도 답이 없다. Winters는 이 분기 실패를 실제로 겪었다: FX 텍스처가 안 보이는 문제를 CPU 코드 추론으로 며칠 쫓다가 RenderDoc으로 UV-alpha 미스매치를 즉시 확정한 사례(2026-04-26) 이후, "셰이더 Read + 데이터 계측 우선, CPU/GPU 경계 분기 강제"가 디버깅 규칙이 됐다.
2. **자체 계측과 외부 도구는 대체재가 아니라 보완재다.** 자체 scope/counter는 도메인 어휘("Anim::BudgetSkipped")로 말하고 항상 켜져 있으며, Tracy/RenderDoc은 범용 어휘로 깊게 판다. Winters가 한 매크로로 자체 HUD와 Tracy를 동시 기록하는 것은 이 보완 관계의 구현이다.
3. **도구가 답하는 질문이 겹치면 싼 쪽을 먼저 쓴다.** F3 HUD 한 번이 RenderDoc 캡처보다 싸고, RenderDoc이 GPU 드라이버 추측보다 싸다.

---

## §3 VFX 최적화의 본질 — Niagara / VFX Graph / WFX

### 3.1 파티클 비용의 구조

(일반 지식) 파티클 시스템의 비용은 네 덩어리로 분해된다:

1. **시뮬레이션** — 파티클별 위치/속도/수명 갱신. CPU sim이면 게임 스레드/워커 비용, GPU sim이면 compute 비용.
2. **오버드로우(fill rate)** — 반투명 파티클이 같은 픽셀을 겹쳐 그리는 비용. **파티클 수가 아니라 화면 픽셀 점유 × 겹침 수**가 지배한다. 화면을 덮는 큰 파티클 10장이 작은 파티클 1,000개보다 비싼 경우가 흔하다.
3. **드로우콜/상태 변경** — 이미터·머티리얼·블렌드 모드별 배치 분리 비용. 인스턴싱과 아틀라스로 줄인다.
4. **스폰/킬 관리** — 매 프레임 생성·소멸하는 수명 관리와 메모리 이동 비용. 풀링과 컴팩션 전략의 영역이다.

최적화 순서도 이 구조를 따른다: 오버드로우(픽셀) → 시뮬레이션(파티클 수·연산) → 드로우콜 → 스폰 정책.

### 3.2 Niagara — UE의 답

Niagara는 System > Emitter > Module 계층의 노드 그래프 에셋이고, 시뮬레이션은 CPU/GPU sim 코드로 컴파일되어 실행된다 (dev.epicgames.com). 그 위의 최적화 수단은 이 세션의 검증 범위 밖이므로 (일반 지식)으로 표기한다:

- **CPU sim vs GPU sim 선택**: 수백 개 이하·게임플레이 상호작용(이벤트 콜백)·정밀 제어가 필요하면 CPU sim, 수천~수만 개 순수 시각 파티클이면 GPU sim. GPU sim은 파티클당 비용이 극히 싸지만 CPU 쪽에서 파티클 상태를 읽는 경로가 막히거나 비싸진다.
- **Fixed Bounds**: 파티클 전체를 훑어 동적 바운드를 계산하는 대신 고정 바운드를 지정한다. GPU sim은 CPU가 파티클 위치를 모르므로 사실상 필수이고, CPU sim에서도 바운드 계산 비용 제거 + 컬링 안정화 효과가 있다. 바운드가 실제보다 작으면 화면 안에서 이펙트가 잘리는 버그가 되므로 검증 대상이다.
- **Scalability / Effect Type**: 이펙트를 Effect Type으로 묶고 플랫폼·품질 레벨별로 거리 컬링, 최대 인스턴스 수, 컬링 시 동작(pause/kill)을 데이터로 지정한다. "저사양에서 이 이펙트는 아예 스폰하지 않는다"를 코드가 아니라 에셋 설정으로 내린다.
- **풀링**: 파티클 컴포넌트 스폰/파괴를 반복하지 않고 재사용한다(스폰 API의 풀링 메서드 경유). 스폰 히칭의 표준 처방.
- **LOD/거리 기반 스폰율**: 거리에 따라 spawn rate를 줄이는 스케일 커브를 둔다 — 멀리 있는 이펙트는 파티클 절반으로도 같은 인상을 준다.
- **Niagara Debugger**로 활성 시스템 수·파티클 수·시뮬레이션 비용을 라이브로 본다 — §1의 "계측 먼저"가 VFX에서도 동일하게 적용된다.

### 3.3 Unity — Shuriken vs VFX Graph

선택 기준이 공식 문서에 명시되어 있다 (docs.unity3d.com): Built-in Particle System(Shuriken)은 **CPU 시뮬레이션**으로 수천 개 규모를 다루고, Unity 물리와 직접 상호작용하며, 파티클 개별에 대한 스크립트 읽기/쓰기가 완전히 열려 있고, 모든 렌더 파이프라인에서 동작한다. VFX Graph는 노드 기반으로 **GPU compute에서 수백만 개** 규모를 시뮬레이션할 수 있으나 URP/HDRP 전용이고, 월드와의 상호작용은 depth buffer 충돌 같은 사용자 정의 요소로 제한되며, C#에는 그래프 프로퍼티 단위로만 노출된다 (docs.unity3d.com).

따라서 선택 공식은 Niagara의 CPU/GPU sim 선택과 동형이다: **게임플레이가 파티클을 만져야 하면 CPU(Shuriken), 순수 시각 물량이면 GPU(VFX Graph)**. 파이프라인 제약(Built-in RP에서는 VFX Graph 불가)이 Unity 특유의 추가 변수다.

### 3.4 Winters — WFX의 답과 "분리가 곧 최적화"

**유계 SoA 풀 + swap-back kill.** `CFxParticlePool`(§1.3)은 SoA 배치에 더해 죽은 파티클을 마지막 원소와 스왑하고 alive 카운트를 줄이는 swap-back kill을 쓴다(`Engine/Private/FX/Exec/FxParticlePool.cpp` `KillExpired`, 40~58행; 설계는 `.md/plan/EffectTool/03_STAGE2_PARTICLE_POOL_SOA.md`). alive 구간이 항상 배열 앞쪽에 연속으로 유지되므로 시뮬레이션 순회가 캐시 친화적이고, `Allocate`는 capacity를 넘는 요청을 잘라낸다 — §1.4의 유계 원칙이 파티클에 적용된 형태다. 렌더 계층은 빌보드 인스턴싱 + 블렌드 모드별 배치 + 소팅으로 설계되어 있고(`.md/plan/EffectTool/06_STAGE5_DX11_RENDERING.md`), 장기 로드맵은 그래프→HLSL 코드젠 + GPU compute다(`08_STAGE7_GPU_COMPUTE.md`, `22_COMPILE_GRAPH_TO_HLSL_VM_BAKE.md`).

**결정적 판정 FX와 시각 FX의 분리 — 이것이 최적화이기도 한 이유.** `.md/plan/EffectTool/09_INTEGRATION.md`(292행~)는 FX를 두 계급으로 가른다: 판정에 영향을 주는 것(히트 위치 등)은 서버 권위 시뮬레이션이 소유하고, 순수 시각 연출은 클라이언트 FX 시스템이 소유하며 서버는 스폰 이벤트(에셋 핸들 + seed)만 내려보낸다. 이 분리는 보통 결정론/치트 방지 논리로 설명되지만, 동시에 성능 설계다:

1. **서버는 시각 파티클을 한 개도 계산하지 않는다.** 30Hz tick 33.3ms 예산(§1.2)에서 파티클 시뮬레이션·렌더 비용이 구조적으로 0이다. 룸 수를 늘릴 때 서버 비용이 "게임 규칙 비용"만으로 스케일한다.
2. **클라이언트 FX는 마음껏 유계 최적화할 수 있다.** 시각 FX는 판정과 무관하므로 풀 고갈 시 스폰 드롭, 거리 컬링, 품질 스케일링을 게임 정합성 걱정 없이 적용할 수 있다. 판정과 시각이 한 시스템에 얽혀 있으면 "파티클을 줄이면 게임이 달라지는" 최악의 결합이 생긴다 — Niagara가 게임플레이 판정과 얽힐 때 생기는 비결정성 문제를 구조로 회피한 것이다(`.md/interview/tool-development.md` §8).

**Fab/Niagara 자산 대응.** Niagara 에셋은 직접 이식이 불가능하므로 참조용으로 사서 WFX 노드(burst, velocity over life, size curve, ribbon 등가물)로 재작성하는 것이 "이식"의 실체다(`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §3-4). 스프라이트 시트/플립북 텍스처는 `.wtex`로 그대로 수용한다.

---

## §4 애니메이션 최적화의 본질

### 4.1 비용의 구조

(일반 지식) 스켈레탈 애니메이션의 프레임 비용은 세 단계로 분해된다:

1. **그래프 평가/블렌딩** — 상태머신·블렌드 트리를 평가해 본별 로컬 트랜스폼을 산출. 본 수 × 활성 블렌드 노드 수에 비례.
2. **본 행렬 갱신** — 로컬 → 컴포넌트/월드 공간 계층 곱, 스키닝 팔레트(역바인드 포즈 곱) 생성.
3. **스키닝** — 정점마다 본 행렬 가중합. 정점 수 × 영향 본 수(보통 4)에 비례하며, GPU 스키닝이면 정점 셰이더 비용, CPU 스키닝이면 게임 스레드/워커 비용.

지배 원칙은 하나다: **안 보이면, 멀면, 덜 계산한다.** 1~2단계는 화면에 없으면 아예 건너뛰거나 갱신 주기를 낮출 수 있고, 3단계는 LOD로 정점·본 수 자체를 줄인다.

### 4.2 UE의 수단 (일반 지식)

- **URO(Update Rate Optimization)**: 스켈레탈 메시의 애니 갱신 주기를 거리/화면 크기에 따라 낮추고(매 프레임 → N프레임에 1회), 건너뛴 프레임은 보간한다.
- **Visibility Based Anim Tick Option**: "렌더될 때만 포즈를 tick한다" 계열 옵션으로, 화면 밖 캐릭터의 그래프 평가·본 갱신을 스킵한다. 화면 밖에서도 판정용 본 위치가 필요한 캐릭터(예: 서버 히트박스)는 예외로 다뤄야 한다는 함정까지가 한 세트다.
- **Animation Budget Allocator** (일반 지식, Fortnite에서 유래한 플러그인으로 알려져 있다): 프레임당 애니메이션 총 예산(ms)을 정해 두고, 초과 시 중요도가 낮은 캐릭터의 갱신 품질/주기를 자동으로 낮춘다 — §1.2의 "프레임이 고정이고 일이 밀린다"를 애니메이션에 시스템화한 것.
- **LOD별 본 감소**: 스켈레탈 메시 LOD에서 하위 LOD일수록 본을 제거해 2·3단계 비용을 함께 줄인다.

### 4.3 Unity의 수단 (일반 지식)

- **Animator Culling Mode**: `AlwaysAnimate` / `CullUpdateTransforms`(화면 밖이면 리타겟·IK·트랜스폼 기록 생략) / `CullCompletely`(화면 밖이면 애니메이션 전체 정지) — "안 보이면 덜 계산"의 컴포넌트 스위치.
- **GPU 스키닝**: 스키닝을 정점 셰이더/compute로 옮겨 CPU를 비운다(프로젝트 설정).
- **Optimize GameObjects**: 리그 임포트 시 본 트랜스폼 GameObject 계층을 평탄화해, 본마다 Transform 오브젝트를 갱신하는 오버헤드를 제거한다(노출이 필요한 본만 예외 지정).
- **Playables API**: Animator Controller 전체 그래프 대신 필요한 클립/블렌드만 코드로 구성해 평가한다 — "평가할 그래프 자체를 줄이는" 접근.

### 4.4 Winters의 수단 (전부 레포 검증)

**(1) `bAnimated=false` — 정적 엔티티 스키닝 스킵, 17.8→9ms의 실제 수단.** `RenderComponent`는 `bAnimated` 플래그를 갖고(`Engine/Public/ECS/Components/RenderComponent.h` 14행), 맵 같은 정적 엔티티는 스폰 시 `false`로 박는다(`Client/Private/Scene/Scene_InGameLifecycle.cpp` 787행). 애니 갱신 루프는 `!rc.bAnimated || !HasSkeleton()`이면 즉시 스킵하고 스킵 수를 센다(`Client/Private/Manager/Minion_Manager.cpp` 759~763행). 렌더 쪽도 같은 플래그로 정적/스킨드 경로를 가른다(`Client/Private/Scene/Scene_InGameRender.cpp` 293행). "애니메이션이 없는 것에 애니메이션 비용을 내지 않는다"는 당연한 문장이 계측 없이는 3개월간 안 보였다는 것이 §1.1 사건의 요지다.

**(2) 미니언 애니 갱신 관문 — 가시성 스킵 + 주기 스로틀 + 프레임 예산.** `Minion::AnimUpdate`(`Minion_Manager.cpp` 742~808행)는 UE URO/Visibility Tick/Budget Allocator에 해당하는 세 수단을 한 루프에 겹쳐 놓았다:

```cpp
// 1) FOW/팀 시야 밖 → 스킵 (visibilitySkipped)
if (!UI::IsRenderableForLocal(...)) { ...; return; }
// 2) 화면 근방 밖 → 스킵 (screenSkipped)
if (pViewProj && !IsWorldPointNearScreen(...)) { ...; return; }
// 3) 상태 기반 갱신 주기: Attack/Dead 는 고주기, 나머지는 저주기 (URO 등가)
const f32_t updateInterval = bHighPriorityAnim ? kMinionHighPriorityAnimUpdateInterval : ...;
if (ms.animUpdateAccumulator < updateInterval) { ...; return; }
// 4) 프레임 예산: 비우선 갱신은 프레임당 3개 (Budget Allocator 등가)
if (!bHighPriorityAnim && animCount >= kMinionAnimUpdateBudget) { ...; return; }
rc.pRenderer->Update(ms.animUpdateAccumulator);   // 누적 시간으로 갱신 → 스킵분 보상
```

다섯 개 카운터(`Anim::UpdateCalls/Skipped/VisibilitySkipped/ScreenSkipped/BudgetSkipped`, 804~808행)가 각 관문의 기각량을 매 프레임 노출한다 — 최적화 수단마다 계측이 붙어 있어 "어느 관문이 일하고 있는가"가 항상 보인다. 공격/사망 상태를 고우선으로 분리한 것은 "덜 계산하되, 전투 판정이 걸린 순간의 시각 품질은 지킨다"는 예외 규칙이다.

**(3) 본 팔레트 상한 — 포맷이 강제하는 유계.** 초기 스키닝 경로는 cbuffer `g_BoneMatrices[256]`이 한계였고 Writer가 `bone_count >= 256`을 즉시 거부했다(`.md/guide/CHAMPION_WMESH_PIPELINE_GUIDE.md` 142행; 챔피언 평균 70~150본이라 실발생 0). 현행 셰이더는 `StructuredBuffer<SkinBoneMatrix> g_BoneMatrices : register(t8)`로 이관됐고(`Shaders/Skinned3D.hlsl` 37행) 상한은 1024로 올라갔다 — Writer가 `bone_count > 1024`를 FATAL로 거부하고(`Engine/Private/AssetFormat/Mesh/WMeshWriter.cpp` 144행) Loader도 `MAX_BONES=1024`로 검증한다(`WMeshLoader.cpp` 11행). 핵심은 숫자가 아니라 방식이다: **상한이 셰이더·포맷·로더 3중 계약으로 박혀 있고, 위반은 런타임이 아니라 cook 시점에 죽는다.** 이 검증을 Validator 게이트로 승격하는 것이 확정 방향이다(`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §3-1).

**(4) cook 시점 리타겟 — 런타임 IK 대신 오프라인 베이크.** Fab 애님팩 수용 설계에서 Control Rig 수준의 런타임 IK는 후순위로 밀고, `.wskel` 리타겟 프로필(본 매핑 + 리스트 포즈 보정)을 AssetConverter의 cook 단계(`--retarget=<profile>`)로 넣는 방향을 고정했다(`WINTERS_UE_FAB_TOOL_ADOPTION.md` §3-2). 근거가 그대로 최적화 논리다: 리타겟은 (원본, 프로필)의 순수 함수이므로 매 프레임 계산할 이유가 없고, 오프라인 베이크가 30Hz 결정론·성능 원칙에 맞는다. "런타임에 하던 일을 빌드 타임으로 옮길 수 있는가"는 애니메이션에 한정되지 않는 범용 최적화 질문이다.

---

## §5 종합 — 최적화 도구를 만든다는 것

### 5.1 프로파일러와 디버그 오버레이도 툴 제품이다

Winters의 관측가능성 규칙(`CLAUDE.md` Progressive Sections, `.claude/gotchas.md` 2026-05-29)은 디버그 오버레이를 임시 코드가 아니라 제품의 일부로 취급한다: 이동/패스파인딩 버그면 현재 셀·다음 웨이포인트·해석된 경로·stuck 사유를 오버레이로 먼저 노출하고 나서 튜닝한다. 실물이 그 증거다 — F3 프로파일러 HUD(`Engine/Private/Manager/Profiler/ProfilerOverlay.cpp`), NavGrid 오버레이(`Client/Private/Scene/Scene_Editor.cpp` `RenderNavGridOverlay`, 872행), 라이브 튜닝 패널(`Client/Public/UI/EffectTuner.h`, `Client/Private/UI/SkillTimingPanel.cpp`). 이들은 (a) 사용자(자신 포함 개발자)가 있고, (b) 데이터 계약(scope 이름, 카운터 축, JSON 캡처 포맷)이 있고, (c) 성능 요건(유계 버퍼, 0비용 게이트)이 있다 — 툴 제품의 3요소를 전부 갖췄다.

툴 개발자 포트폴리오 관점에서 이것이 갖는 의미: 상용 생태계에서도 계측·생산성 도구는 독립 제품군이다. Unity Profile Analyzer는 공식 패키지로 존재하고 (docs.unity3d.com), Tracy는 Winters가 실제로 통합해 쓰는 외부 제품이며, 에디터 생산성 플러그인은 Fab/Asset Store에서 실제로 팔리는 카테고리다 — Fab의 Tools & Plugins 카테고리와 Blueprint Assist 같은 사례 (fab.com), Unity 에디터 툴의 $15~140 가격대 클러스터 (assetstore.unity.com). "게임을 만들다 필요해서 만든 계측 도구"는 그 자체로 포트폴리오 항목이자, 어빌리티 타임라인 에디터(§ 도구 로드맵, `.md/interview/tool-development.md` §12) 같은 flagship 툴에 붙는 신뢰 근거가 된다 — 자기 도구의 성능 문제를 자기 프로파일러로 잡아 본 사람이라는 증거이기 때문이다.

UE5.7 소스 감사(2026-07-10)의 메타패턴 — "Winters는 규칙은 현업급인데 강제가 사람·문서 의존, UE는 기계 강제"(`.md/interview/tool-development.md` §11) — 은 최적화 도구에도 적용된다. `WINTERS_PROFILE_SCOPE`는 이미 컴파일타임 게이트로 기계 강제되지만, "성능 계획은 scope/counter/예산 명시"(compass 112행) 규칙은 아직 문서 강제다. 다음 단계는 성능 회귀를 CI가 잡게 만드는 것(JSON 캡처 비교의 자동화)이고, 이는 P0 "배선만" 로드맵과 접합된다.

### 5.2 면접에서 최적화 질문에 답하는 프레임

최적화 질문("프레임이 떨어지면 어떻게 하나", "파티클이 느리면", "캐릭터 100명이 버벅이면")의 모범 답변 골격은 세 층이다:

1. **계측 서사** — 추측하지 않고 측정으로 범인을 확정한다는 태도를 실화로 증명한다. Winters 화자는 17.8→9ms 사건을 그대로 쓰면 된다: 11 scope + 4 counter → Nav 무죄 → `Minion::AnimUpdate` 16ms 확정 → `bAnimated=false` 최소 수정 → 재측정. "의심하던 곳이 무죄였다"는 디테일이 이 서사의 신뢰도를 만든다.
2. **예산 언어** — "빠르게"가 아니라 "16.6ms/33.3ms 예산에서 어느 시스템이 초과인가"로 문제를 다시 쓴다. 프레임당 작업 개수 예산(애니 3개, 바인딩 3개)과 상한 설계(유계 버퍼·로그·풀)까지 이어지면 시니어 답변이 된다.
3. **데이터 형태** — 마지막에 "같은 알고리즘이라도 SoA/zero-copy/연속 메모리로 데이터를 구부리는 것이 최적화의 절반"을 실물(FxParticlePool, WMeshLoader)로 닫는다.

도메인별 각론은 이 프레임의 인스턴스다. VFX면 "파티클 수보다 픽셀 비용(오버드로우)부터 본다 → CPU/GPU sim 선택 기준 → 판정 FX와 시각 FX의 분리(서버는 파티클을 계산하지 않는다)". 애니메이션이면 "비용 3단 분해(그래프 평가/본 갱신/스키닝) → 안 보이면·멀면 덜 계산(가시성 스킵, 주기 스로틀, 프레임 예산) → 상한은 포맷 계약으로(본 1024 cook 거부) → 런타임 일을 cook으로 옮긴다(리타겟 베이크)". 세 엔진 중 무엇을 묻든 같은 프레임으로 답하고, 엔진별 고유명사(URO/Culling Mode/bAnimated)는 그 프레임의 구현 사례로 배치한다 — 도구 이름을 아는 사람이 아니라 비용 구조를 아는 사람으로 보이는 것이 목표다.
