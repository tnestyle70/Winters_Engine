# 14. 어려웠던 문제들 — 문제 해결 전략 사례집

> 이 챕터는 "가장 어려웠던 버그가 뭐였나요?" 계열 질문의 실탄 창고다.
> 각 사례는 STAR(상황/과제/행동/결과) + 교훈/재발 방지 골격으로 정리했다.
> 면접에서는 30초 요약 → 면접관이 파고들면 STAR 전개 → 마지막에 "그래서 만든 규칙"으로 닫는다.
> 모든 사례의 공통 결말은 같다: **사고를 개인 기억이 아니라 코드(enum/카운터/훅)와 규칙(gotchas.md)으로 박제했다.**

---

## 사례 1. 챔피언 yaw saga — 서버 권위 vs 로컬 예측 (가장 오래 싸운 버그)

**S (상황)** — 이동을 서버 권위로 전환한 뒤, 우클릭 이동에서 챔피언 몸통이 순간적으로 반대 방향(±180°)으로 튀거나, 챔피언마다 facing이 제각각 어긋나는 증상이 여러 세션(2026-05-20~22)에 걸쳐 반복됐다. 한 번 고치면 다른 챔피언이나 다른 입력 패턴(우클릭 연타)에서 다시 터졌다.

**T (과제)** — 서버 권위(스냅샷이 진실)를 유지하면서, 로컬 클릭의 즉각적인 회전 반응성을 지키고, 챔피언별 facing을 통일하기.

**A (행동)** — 처음에는 증상 지점마다 `+PI` 보정을 덧대는 땜빵을 했고, 그것이 오히려 챔피언별로 facing을 쪼개는 2차 사고를 만들었다. 그래서 문제를 **4개 층으로 분해**해서 각 층에 소유권을 부여했다:

1. **에셋 층**: 챔피언마다 메시 forward 축이 다르다(자체 변환 `.wmesh`는 offset 0, 원본 Riot FBX는 +PI). 산발적 보정을 금지하고 챔피언별 오프셋을 중앙 함수 하나로 라우팅했다. 클라이언트는 `GameplayForwardFromVisualYaw`에서 `ResolveChampionModelYawOffset`을 빼서 gameplay forward를 복원한다 (`Client/Private/Network/Client/SnapshotApplier.cpp:105`).
2. **표현 층**: Transform body yaw는 **wire 값이 아니라 연속 상태**다. 매 틱 정규화하면 빠른 우클릭 시 +PI/-PI 경계를 재교차해 몸이 튄다. 그래서 매 yaw write는 `MakeChampionVisualYawNear`(현재 yaw 근처 값으로 해석)를 쓰고, `Normalize...`는 wire/로그/델타 비교 전용으로 격리했다 (`.claude/gotchas.md` 2026-05-21 항목).
3. **예측 층**: ack가 진전됐다고 예측 yaw 보호를 풀면, 서버가 아직 회전을 반영하기 전 스냅샷이 로컬 연출을 덮어써 캐릭터가 튄다. `SnapshotApplier`에 보호 상태기계를 만들었다: `bServerCaughtProtectedYaw`(0.20rad 이내로 서버가 실제로 따라잡음), `bServerOpposesProtectedYaw`(정확히 반대 방향 = half-turn 특수 감지), `bProtectedAckGraceExpired`(최대 보호 스냅샷 수 초과), 서버 액션락 — 이 조건들이 만족될 때만 보호를 해제한다 (`Client/Private/Network/Client/SnapshotApplier.cpp:660-699`).
4. **시스템 층**: 클라이언트 `CNavigationSystem`이 SnapshotApply와 SyncFromECS 사이에서 스냅샷으로 적용된 yaw를 덮어쓰고 있었다. replicated 챔피언에 대해서는 클라 NavAgent/Velocity 이동 시스템을 아예 끄는 것으로 종결했다 (`.claude/gotchas.md` 2026-05-22). 서버 측에서는 우클릭 연타가 매 틱 눈에 보이는 조향 회전이 되지 않도록, 같은 세션의 pending Move를 최신 것으로 교체(coalescing)한다 (`Server/Private/Game/CommandIngress.cpp:74-79`).

계측도 함께 심었다: `ModelRenderer::SetYawTraceContext`가 expected yaw/forward를 주입받아 렌더 시점의 실제 world 행렬에서 추출한 yaw와 비교하고, half-turn(tolerance 0.35)을 감지하면 bounded(512회) 트레이스를 낸다 (`Engine/Private/Renderer/ModelRenderer.cpp:26,263,326`).

**R (결과)** — 우클릭 연타·챔피언 교체·스킬 중 이동 모두에서 몸통 튐이 사라졌다. 이 과정에서 나온 규칙 6개 이상이 `.claude/gotchas.md` 2026-05-20~22 클러스터로 성문화됐다.

**교훈 / 재발 방지**
- 각도 버그는 수학 문제가 아니라 **소유권과 표현의 문제**였다. 같은 float도 "wire 값(정규화)"과 "연속 상태(근접 해석)"로 역할이 갈리고, 이 구분을 함수 이름(`...Near` vs `Normalize...`)에 박아 실수를 타입처럼 막았다.
- 산발적 상수 보정(+PI 땜빵)은 금지. 에셋 패밀리별 사실은 중앙 카탈로그 함수 한 곳에.
- 예측 보호 해제 조건은 "ack가 왔다"가 아니라 "서버가 실제로 따라잡았다"여야 한다.

---

## 사례 2. 미니언 stuck — Phase 순서 race + silent empty path

**S** — 2026-04-28, Chase 상태 미니언이 적을 감지한 뒤 제자리에서 걷기 애니메이션만 반복하며 멈추는 stuck. 게다가 한 팀에서만 발생하는 비대칭 증상이라 더 혼란스러웠다.

**T** — 비결정적으로 보이는 stuck의 원인을 특정하고, 같은 클래스의 침묵 실패가 재발하지 않게 만들기.

**A** — 코드 추론으로 세 번 가설을 세웠지만 전부 빗나갔다. 방향을 바꿔 profiler counter를 붙였더니 5분 만에 원인이 두 개로 갈라졌다:

1. **Phase 순서 race**: 시스템 실행 순서가 Transform(0)→Nav(1)→AI(2)였다. AI가 Phase 2에서 Chase를 결정하고 `nav.vTarget`을 갱신해도 Nav는 이미 그 프레임 실행을 끝낸 뒤라, 다음 프레임에야 경로를 재계산했다. 첫 Chase 프레임은 LaneMove 시절의 stale velocity로 움직였다. 해결은 Phase swap(AI를 1로, Nav를 2로)으로 같은 프레임 안에서 set→consume을 보장하는 것. 여기서 "**ECS Phase 순서 = 데이터 의존성 순서**"라는 규칙이 나왔다.
2. **silent empty path**: 적이 unwalkable 셀(포탑/넥서스 반경)에 들어가면 `Find_Path`가 빈 vector를 반환하고, NavigationSystem은 speed=0만 처리했다. Chase 상태인데 이동이 조용히 skip돼 제자리 애니가 됐다. 해결은 Chase 한정 직선 fallback — 경로가 비어도 목표까지 `SegmentWalkable`이면 직선 이동하고 `Nav::DirectFallback` 카운터를 남긴다 (`Engine/Private/ECS/Systems/NavigationSystem.cpp:192-225`, `Nav::PathEmpty` 카운터 포함).

**R** — stuck 해소. 그리고 이 사고의 근본 교훈을 나중에 인터페이스에 박았다: 빈 vector 하나로 뭉개지던 실패 원인을 `ePathFindResult`(NullGrid/StartBlocked/GoalBlocked/NoRoute/BrokenPath)로 구분하는 out-param을 추가했고, 헤더 주석에 "과거 minion-stuck 사고의 silent empty path 대책"이라고 계보를 명시했다 (`Engine/Public/Manager/Navigation/Pathfinder.h:11-21`).

**교훈 / 재발 방지**
- 빈 반환값은 정보 손실이다. 실패가 여러 원인을 가지면 원인을 enum으로 구분한다 (설계 원칙 P3, `.md/architecture/WINTERS_DESIGN_PHILOSOPHY.md:27-32`).
- 추측이 2회 빗나가면 즉시 계측으로 전환한다. counter 5분이 추측 1시간을 이겼다.
- 침묵 실패는 나중에 RL/MCTS를 붙일 때 학습 transition 노이즈(sim2real 격차)가 된다 — 모든 이동 액션은 deterministic effect(이동하거나, 명시된 이유로 막히거나)를 가져야 한다.

---

## 사례 3. Profiler thread_local race — 계측 도구가 병렬 안전하지 않으면

**S** — NavigationSystem을 JobSystem으로 병렬화한 직후 crash. 병렬화한 게임 로직이 아니라 **프로파일러 자체**가 범인이었다.

**T** — crash 원인 특정 + "병렬화를 계측할 도구가 병렬 안전해야 한다"는 역설 해소.

**A** — CPUProfiler의 scope stack이 단일 `vector` 하나였고, 여러 worker가 동시에 `push_back`하다가 vector 재할당이 겹치며 터졌다. 구조를 바꿨다:
- scope stack을 `thread_local`로 분리 — 각 스레드가 자기 스택만 만진다 (`Engine/Private/Core/Profiler/CPUProfiler.cpp:29`의 `t_vProfilerStack`).
- 결과 병합(이벤트/통계/카운터)만 mutex로 보호하고, `EndFrame`에서 current↔last 벡터를 swap하는 더블버퍼로 오버레이 읽기와 수집을 분리했다 (`CPUProfiler.cpp:45-53`).
- 부가 함정도 하나 잡았다: scope 이름을 포인터로 비교하면 DLL/번역단위 경계에서 같은 문자열 리터럴이 다른 주소가 되어 통계 행이 중복된다 → `strcmp` 기반 `SameProfilerName`으로 교체 (`CPUProfiler.cpp:20-27`).

**R** — crash 해소. 이 사고가 계기가 되어 병렬 미니언 AI의 Worker-Safety 정책 5종(thread_local=작업버퍼, atomic=카운터, lock+buffer+main merge=결과수집, self-entity only=ECS write, per-worker buffer+main flush=cross-entity write)과 Decision/Apply 2-pass(읽기와 쓰기를 프레임 안에서 시간 분리)를 성문화했다.

**교훈 / 재발 방지**
- 관측 도구는 관측 대상보다 먼저 병렬 안전해야 한다. 도구가 흔들리면 모든 계측 데이터가 의심 대상이 된다.
- "공유 자료구조 + 락"보다 "스레드 소유 자료구조 + 최소 병합 락"이 기본형.
- 문자열 리터럴 주소 동일성은 DLL 경계를 넘는 순간 깨진다 — 이름 비교는 내용 비교로.

---

## 사례 4. JobSystem Chase-Lev race — 보류(fallback)로 살리고, 정식으로 고치기

**S** — Phase 5-A에서 JobSystem을 켜자 특정 조건(엔티티 36+ 루트가 병렬 임계치 16을 초과해 병렬 경로 진입)에서 Main 스레드가 hang. 데드락도 crash도 아닌, 그냥 영원히 기다리는 상태.

**T** — lock-free work-stealing deque가 낀 hang의 근본 원인 특정. 단, 데모/개발 진행을 멈출 수는 없었다.

**A** — 원인: Chase-Lev deque는 **owner(해당 worker)만 bottom에 push/pop 할 수 있다**는 불변식 위에 서 있는데, 당시 `PushToSomeDeque`가 Main 스레드에서 남의 deque에 직접 push하고 있었다. memory ordering 관점에서 owner 규약이 깨지자 worker의 pop이 빈 deque로 오인 → 작업 counter가 0으로 내려가지 않음 → `WaitForCounter`의 help-stealing이 무한 루프. 대응을 두 단계로 나눴다:
1. **보류**: `Set_JobSystem`을 비활성화(nullptr fallback = 싱글스레드)해서 기능 회귀 없이 개발을 계속했다. 근본 수정 옵션 3개(Main 전용 submission queue / Main도 자기 deque를 owner로 소유 / MPMC 큐 교체)를 문서로 박제.
2. **정식 수정**: 현재 코드는 `thread_local t_iWorkerIdx`(worker면 자기 인덱스, 외부 스레드면 -1)로 호출자를 판별해서, worker 본인일 때만 자기 deque에 push하고, Main/외부 스레드이거나 deque overflow면 mutex 보호 `m_GlobalQueue`로 우회한다 (`Engine/Private/Core/JobSystem.cpp:18,126-160`). owner 불변식이 코드 구조상 깨질 수 없게 됐다. 유휴 worker도 yield 스핀 대신 `m_WakeCV.wait_for(1ms)`로 바꿔, 깨우기 누락이 있어도 1ms 안에 자동 회복하면서 CPU를 태우지 않는다 (`JobSystem.cpp:184-187`).

**R** — hang 재현 없음. worker 소비 순서는 자기 deque(LIFO) → GlobalQueue → 랜덤 victim steal(FIFO)로 정착.

**교훈 / 재발 방지**
- lock-free 자료구조는 알고리즘만이 아니라 **불변식을 지키는 호출 규약까지가 자료구조**다. 규약을 호출자의 선의에 맡기지 말고 코드 경로(t_iWorkerIdx 분기)로 강제한다.
- "지금 당장 근본 수정"과 "기능을 살리는 보류"는 대립이 아니다. fallback으로 회귀를 막고, 옵션을 문서화한 뒤, 별도 슬라이스에서 정식 수정하는 순서가 옳았다.

---

## 사례 5. CHttpClient 가짜 async — 폐기된 future가 만든 동기 블로킹

**S** — `AsyncGet`/`AsyncPost`라는 이름의 함수가 실제로는 호출 지점을 블로킹하고 있었다. 2026-07-09 에러 경계 전수 감사에서 확정.

**T** — 진짜 비동기로 전환하되, 숨어 있는 2차 결함까지 같은 변경에서 해소하기.

**A** — 원인은 C++ 표준의 유명한 함정: `std::async(launch::async, ...)`가 반환한 `std::future`를 버리면, 그 임시 future의 소멸자가 작업 완료를 기다린다 — 즉 호출이 사실상 동기가 된다. 더 위험한 발견은 **그 우연한 블로킹이 raw `this` 캡처를 안전하게 만들던 유일한 이유**였다는 것. 블로킹만 없애면 곧바로 use-after-free가 열리는 구조라, 두 결함을 한 변경으로 끊었다:
- future를 `m_PendingRequests`(vector)가 소유한다 (`Client/Public/Network/Backend/CHttpClient.h:71`).
- worker는 멤버 대신 호출 시점 `RequestSnapshot` 값 복사본만 읽는다 — `SetAuthToken`과의 race도 함께 차단 (`CHttpClient.h:43-50`).
- 소멸자가 진행 중 요청을 전량 드레인한다. 블로킹을 "호출 시점"에서 "파괴 시점"으로 한정하는 계약을 헤더 주석에 명시했다 (`CHttpClient.h:23-24`).

**R** — 진짜 async 전환 완료. `.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md:53`에 해소 항목으로 기록. 남은 계약도 정직하게 문서화: 드레인을 깨면(future detach 등) UAF가 열리고, 소유자 리셋은 최악의 경우 WinHTTP 타임아웃만큼 대기할 수 있다.

**교훈 / 재발 방지**
- async 래퍼는 future의 수명을 소유해야 한다 — gotcha로 박제 (`.claude/gotchas.md` 2026-07-09 async lifetime).
- **두 결함이 서로를 가리는 결합**(가짜 async ↔ raw this 안전)은 하나만 고치면 더 나빠진다. 반드시 세트로 고친다.

---

## 사례 6. FX 텍스처 UV-alpha 미스매치 — 픽셀 파이프라인 안에서 죽는 버그

**S** — 이렐리아 E 검 이펙트가 화면에 안 나옴. `DrawMesh`/`CModel::Render`는 정상 호출됨을 브레이크포인트로 확인. 1.5시간 동안 CPU 측 가설(정점, 행렬, 드로우 순서)만 누적하며 헛돌았다.

**T** — "호출은 됐는데 화면에 0픽셀" 클래스의 버그를 잡는 접근법 확립.

**A** — 전환점은 두 가지였다. 첫째, **셰이더를 CPU 코드와 동급으로 읽기 시작**했다: 픽셀 셰이더에 `clip(texColor.a - 0.05f)`가 있었다 (`Shaders/Mesh3D.hlsl:158`). 알파가 낮으면 픽셀 단위로 폐기되므로, CPU 디버거에는 아무 이상이 안 잡힌다. 둘째, **데이터 계측**: LoL 추출 에셋에 동봉된 `render/*.png`는 mesh diffuse가 아니라 클라이언트 스프라이트 캡처였고, FBX UV가 그 이미지의 알파 0 영역만 가리키고 있었다 — 전 픽셀이 clip으로 버려진 것. 정상이던 beam과 깨진 sword의 두 PNG 알파 분포를 비교(대조군)했으면 더 빨리 잡았을 문제였다. 본체 머티리얼(`*_texture.png`/`*_mult.png`)로 교체해 해결. `render/*.png`는 UV 0~1 전체를 매핑하는 FxBillboard 전용으로 격리했다.

**R** — 해결. 그리고 이 사고의 자기비판에서 디버깅 방법론이 나왔다: "Render 호출됐는데 화면 0" → 즉시 셰이더의 clip/discard/Sample을 읽는다. "옆 케이스는 되는데 이것만 안 됨" → 코드 추론이 아니라 데이터를 직접 계측(alpha bbox vs UV bbox). 이 클래스는 RenderDoc/PIX 같은 GPU 캡처가 정답인 영역이라는 도구 경계 인식도 함께.

**교훈 / 재발 방지**
- 버그 가설을 세울 때 **CPU/GPU 경계에서 후보를 강제로 반씩 나눈다**. 당시 모든 가설이 VS 정점 영역에 몰려 있었고 PS 픽셀 단계 후보가 0개였던 것이 실패 원인.
- 외부 에셋의 텍스처는 "이름이 그럴듯한 파일"이 아니라 데이터 실측으로 검증한다.

---

## 사례 7. Z-fighting — 렌더 파라미터가 아니라 에셋 데이터 문제

**S** — Phase B-4에서 소환사의 협곡 glb를 로드하자 맵 전면에 Z-fighting. `Layer1~8` 이름의 노드 646개 메시가 기본 지형과 코플라나(동일 평면)로 겹쳐 있었다.

**T** — Near/Far plane, depth bias 같은 렌더 파라미터 조정으로는 코플라나 지오메트리를 절대 못 고친다는 것을 인지하고, 원인 데이터를 제거하기.

**A** — 증상(depth precision) 튜닝이 아니라 데이터 원인을 추적했다: `Layer` prefix 노드들은 원본 게임에서 별도 처리되는 오버레이 메시였고, 그대로 그리면 지형과 완전히 겹친다. 처음에는 런타임 로드 시 노드 이름으로 스킵했고(메시 37% 감소 부수 효과), 이후 이 필터를 **쿠킹 단계로 승격**했다: `WMeshWriter`의 `IsLayerOverlayNode`가 prefix "Layer" 노드를 재귀 스킵하고, kept/skippedNodes/skippedMeshes 통계를 로그로 남기며, `--include-layers` 옵트인과 "필터 결과가 비면 전체 메시로 폴백"까지 갖췄다 (`Engine/Private/AssetFormat/Mesh/WMeshWriter.cpp:36-124`).

**R** — Z-fighting 완전 제거 + 메시 수 감소로 성능 이득. 런타임 임시 필터가 에셋 파이프라인의 정식 단계가 됐다.

**교훈 / 재발 방지**
- 코플라나 Z-fighting은 렌더러 설정 문제가 아니라 **입력 데이터 문제**다. 증상이 렌더에 보여도 원인 계층(에셋)에서 고친다.
- 임시 런타임 우회가 검증되면 파이프라인 앞 단계(쿠킹)로 옮겨 모든 소비자가 공짜로 혜택을 받게 한다. 필터에는 통계 로그와 옵트아웃, 빈 결과 폴백을 붙여 침묵 실패를 막는다.

---

## 사례 8. 넥서스/바론 중첩 메시 — DIAG 로그 한 방이 그럴듯한 가설을 반증

**S** — 넥서스/억제기가 카메라 이동 시 표면이 울리고(shimmering), 바론의 눈이 텍스처 없이 흰색으로 나옴. 처음 가설은 "블루팀 구조물이 -X scale 미러라서 노멀/와인딩이 뒤집혔다"는 것 — 꽤 그럴듯했다.

**T** — 가설을 코드 수정으로 검증하기 전에, 실제 에셋 내부 구조를 실측하기.

**A** — Model 로더에 DIAG 로그(머티리얼별 diffuse 경로를 OutputDebugString으로 출력)를 임시로 심고 실행했다. 실측 결과가 가설을 반증했다: glb 내부에 'Destroyed' 상태 메시(13,106 verts)와 정상 메시(9,100 verts)가 **같은 위치에 중첩**돼 있었고(파괴 전/후 두 벌을 동시에 그리고 있었던 것), 바론 Eye는 diffuse가 `<NONE>`이라 기본 흰색이었다. 미러 가설이었다면 하지 않았을 수정 방향이 데이터 한 번의 실측으로 확정됐다. 그 세션에서는 수정안에 사이드 이펙트가 있어 원복하고, 원인 확정 상태로 다음 세션에서 재개했다 — 원인 확정과 수정 적용을 분리한 것.

**R** — "중첩 메시 두 벌 + 무텍스처 서브메시"로 원인 확정. 같은 계열 문제(오버레이 중복 메시)는 이후 사례 7의 쿠킹 단계 필터로 구조화됐다.

**교훈 / 재발 방지**
- 외부 에셋 문제는 추론 대신 **구조 실측(메시/머티리얼 덤프)이 먼저**다. 그럴듯한 가설(미러)일수록 반증 비용이 싼 로그부터.
- "원인 확정"과 "수정 적용"은 별개의 완료 조건이다. 수정이 흔들리면 원복하되, 확정된 원인은 기록으로 보존한다.

---

## 사례들을 관통하는 문제 해결 방법론

여덟 개 사례에서 반복해서 작동한 원칙을 다섯 개로 압축한다. 이 원칙들은 NYPC 봇 대회 회고("디버깅이 수월한 구조가 이긴다")에서 출발해 설계 원칙 P1~P4로 박제됐다 (`.md/architecture/WINTERS_DESIGN_PHILOSOPHY.md`).

### 1. 관측 장치 먼저, 증상 튜닝은 나중 (P4)

증상을 코드로 만지기 전에 보이게 만든다. 미니언 stuck은 profiler counter가, yaw saga는 렌더 스레드의 expected-vs-actual 트레이스 훅이, 넥서스는 DIAG 로그가 각각 원인을 확정했다. 이동/경로 버그는 현재 셀, 다음 웨이포인트, 보정 방향, stuck/resolve 이유를 오버레이로 노출하는 것이 프로젝트 규칙이다 (CLAUDE.md 디버깅 파이프라인 항목).

### 2. 추측 2회 빗나가면 데이터 계측으로 전환

미니언 stuck에서 코드 추론 3회가 실패하고 counter 5분이 성공한 뒤 규칙으로 만들었다. 가설은 누적하는 것이 아니라 falsify하는 것이고, 가장 싼 falsifier는 대부분 로그/카운터/데이터 덤프다. 비교 대조군(되는 케이스 vs 안 되는 케이스의 데이터 diff)을 의식적으로 세운다 — UV-alpha 사고에서 beam/sword 두 PNG를 비교하지 않은 것이 시간을 태운 주범이었다.

### 3. CPU/GPU 경계에서 가설을 강제로 분기

"호출은 됐는데 화면에 없음"은 픽셀 파이프라인 안에서 죽는 클래스다. CPU 디버거가 원리적으로 무력하므로, 셰이더의 clip/discard/Sample을 CPU 코드와 동급으로 읽고, RenderDoc/PIX 캡처로 넘어간다. 가설 목록을 만들 때 CPU 측과 GPU 측 후보를 반반 강제 배분하면 이 함정을 구조적으로 피할 수 있다.

### 4. 실패를 침묵에서 신호로 승격 (P1/P3)

빈 반환값은 `ePathFindResult` 같은 원인 enum으로, 폴백은 단계별 로그가 붙은 명시적 사다리로, 실패 경로에는 bounded 진단(실패류 8, miss류 64, 계측류 512 상한)을 남긴다 (`.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md:18`). 특히 **dead diagnostics** — `sprintf_s`로 포맷만 하고 출력하지 않는 코드 — 는 로그가 없는 것보다 나쁘다. 트레이스가 있다고 믿게 만들기 때문이다. 2026-07-09 감사에서 1,652줄짜리 SnapshotApplier에 출력 0개/포맷 8곳이 적발돼 정책화됐다 (`WINTERS_DESIGN_PHILOSOPHY.md:18`). 아직 남은 침묵 지점도 "고칠 것" 목록으로 정직하게 관리한다 (`WINTERS_ERROR_HANDLING_POLICY.md:48` §4).

### 5. 최소 재현과 보류의 기술, 그리고 실수의 규칙화

- 재현 조건을 수치로 좁힌다(JobSystem hang = 엔티티 36+ 루트가 병렬 임계치 16 초과). 조건이 좁혀지면 원인 후보가 급감한다.
- 근본 수정이 크면 **fallback으로 기능을 살리고 옵션을 문서화한 뒤 별도 슬라이스에서 정식 수정**한다(JobSystem 싱글스레드 fallback → GlobalQueue 하이브리드).
- 해결된 사고는 반드시 재사용 가능한 규칙으로 추출해 `.claude/gotchas.md`에 날짜별로 박제한다. yaw saga 하나에서만 6개 이상의 규칙이 나왔고, 이 규칙들이 다음 사고를 실제로 막았다.

---

## 면접 Q&A

**Q1. 가장 오래 잡은 버그는 무엇이었나요?**
- 골격: 챔피언 yaw saga. "각도 float 하나"가 사실은 에셋(forward 축)/표현(연속 상태 vs wire 값)/예측(보호 상태기계)/시스템(클라 nav 덮어쓰기) 4개 층의 소유권 문제였다는 분해 서사로 답한다. 마무리는 `...YawNear` vs `Normalize...` 함수 분리로 실수를 구조적으로 막았다는 것.
- 꼬리질문 대비: "±PI 경계 재교차가 정확히 왜 생기나?" → 매 write 정규화가 -PI 근처 값을 +PI 근처로 스냅시켜 보간이 반대 방향 반바퀴를 돌기 때문. 현재 yaw 근처 표현으로 쓰면 경계가 없다.

**Q2. lock-free 자료구조에서 겪은 가장 어려운 버그는?**
- 골격: Chase-Lev deque의 owner-only push 불변식을 Main 스레드가 깬 race. 증상은 crash가 아니라 counter가 안 내려가는 hang이었고, 재현 조건(병렬 임계치 초과)을 좁혀 특정했다. 정식 수정은 `thread_local` worker 인덱스 분기 + 외부 스레드는 mutex 전역 큐 우회.
- 꼬리질문 대비: "왜 MPMC 큐로 전부 바꾸지 않았나?" → hot path(worker 자기 deque)는 무락 LIFO를 유지하고, 드문 외부 submit만 락을 내는 하이브리드가 성능/복잡도 균형점.

**Q3. 멀티스레드 환경에서 프로파일러는 어떻게 안전하게 만들었나요?**
- 골격: scope stack을 thread_local로 분리해 수집은 무락, 병합만 mutex, 프레임 더블버퍼 swap으로 UI 읽기와 분리. 계기는 프로파일러 자체가 병렬화 직후 crash한 사고.
- 꼬리질문 대비: "DLL 경계에서 또 뭐가 깨졌나?" → 같은 문자열 리터럴이 모듈마다 다른 주소가 되어 포인터 비교 기반 이름 매칭이 중복 행을 만듦 → strcmp로 교체.

**Q4. std::async의 함정을 설명해 보세요.**
- 골격: 반환 future를 버리면 임시 future의 소멸자가 join해서 호출이 동기가 된다. 실제로 CHttpClient가 이 패턴이었고, 더 무서운 건 그 블로킹이 raw this 캡처의 안전성을 우연히 지탱하고 있었다는 점. future 소유 + 스냅샷 값 복사 + 소멸자 드레인을 한 변경으로 적용했다.
- 꼬리질문 대비: "지금 설계의 남은 비용은?" → 파괴 시점 블로킹이 최악의 경우 WinHTTP 타임아웃만큼 걸릴 수 있다는 계약을 헤더에 명시했다.

**Q5. 렌더링이 호출은 되는데 화면에 안 나오면 어떻게 접근하나요?**
- 골격: CPU/GPU 경계 분기부터. 픽셀 셰이더의 clip/discard를 즉시 읽고(`clip(texColor.a - 0.05f)` 같은), 데이터(UV bbox vs alpha bbox)를 계측한다. 이렐리아 E 검 사고에서 sprite 캡처 PNG를 diffuse로 쓴 UV-alpha 미스매치를 이 방법으로 잡았다.
- 꼬리질문 대비: "도구는?" → 이 클래스는 CPU 디버거가 원리적으로 무력하고 RenderDoc/PIX 캡처가 정답 영역.

**Q6. ECS에서 시스템 실행 순서가 왜 중요한가요?**
- 골격: Phase 순서 = 데이터 의존성 순서. AI가 목표를 쓰고 Nav가 소비한다면 AI가 같은 프레임에서 먼저 돌아야 한다. 순서가 어긋나면 1프레임 stale 상태가 생기고, 미니언 stuck처럼 "가끔, 한 팀만" 같은 비결정적 증상으로 나타난다.
- 꼬리질문 대비: "그걸 어떻게 발견했나?" → 코드 추론 3회 실패 후 profiler counter 5분. 이후 "추측 2회 빗나가면 계측 전환"을 규칙화.

**Q7. 실패 처리를 어떻게 규율하나요?**
- 골격: 원칙은 4개 — 실패는 발생 지점에서 가시적으로(P1), 실패는 격리(P2), 특수상황은 코드에 명시(P3), 디버깅 수월한 구조 먼저(P4). 구현체는 `ePathFindResult` 원인 enum, 단계별 로그가 붙은 서버 nav 폴백 사다리, bounded 진단 상한 규약, dead diagnostics 리뷰 반려.
- 꼬리질문 대비: "과잉 방어는 아닌가?" → 외부 입력(네트워크/파일/유저/에셋)은 분기, 내부 불변식은 assert — 이 경계를 문서에 명시해 Karpathy式 단순성 원칙과 균형을 잡았다.

**Q8. 본인의 디버깅 스타일을 한마디로 하면?**
- 골격: "가설을 누적하지 않고 falsify한다." 관측 장치 먼저, 추측 2회 실패 시 계측 전환, CPU/GPU 경계 강제 분기, 대조군 비교, 그리고 해결된 사고는 gotchas 규칙으로 승격해 같은 실수를 두 번 하지 않는다.
- 꼬리질문 대비: "규칙화의 실제 효과 사례?" → yaw saga의 "body yaw는 연속 상태" 규칙이 이후 미니언 yaw 작업에서 같은 클래스의 사고를 사전에 차단했다.

**Q9. 수정하지 못하고 보류한 문제도 있나요? (정직성 검증 질문)**
- 골격: 있다, 그리고 목록으로 관리한다. `WINTERS_ERROR_HANDLING_POLICY.md` §4가 "알려진 잔여 침묵 지점"을 '고쳐야 할 것이지 따라 해도 되는 패턴이 아니다'라는 경고와 함께 나열하고, 해소된 항목(ePathFindResult, CHttpClient)은 취소선으로 진행을 추적한다. JobSystem race도 처음엔 싱글스레드 fallback으로 보류했다가 정식 수정했다.
- 꼬리질문 대비: "예시 하나만?" → SnapshotApplier의 yaw 계측 블록 일부는 아직 포맷-후-폐기 상태로 보존 중이고, "재무장 or 삭제"를 결정할 세션이 예약돼 있다 — 부채를 모범으로 착각하지 않게 문서에 명시했다.

---

## 다른 챕터와의 연결

- **사례 1(yaw saga)** 의 서버 권위/스냅샷/예측 구조 전반은 네트워크 아키텍처 챕터에서, wire 포맷과 FlatBuffers verify 실패 처리는 `.md/interview/cpp/12_network_serialization.md`에서 다룬다.
- **사례 3, 4(Profiler/JobSystem)** 의 메모리 모델, thread_local, condition_variable, future 수명은 `.md/interview/cpp/09_concurrency.md`가 문법 측면을 담당한다.
- **사례 5(CHttpClient)** 와 방법론 4절(실패의 신호 승격)의 반환값 기반 에러 모델은 `.md/interview/cpp/10_error_handling.md`와 `.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md`로 이어진다.
- **사례 3의 문자열 리터럴 주소 함정**과 dllexport 특수멤버 이슈는 `.md/interview/cpp/02_compile_link_dll.md`의 DLL 경계 장에서 문법적으로 설명한다.
- **사례 2(Phase 순서)** 의 ECS 구조·시스템 스케줄링은 `.md/interview/cpp/11_architecture_ecs.md`와 엔진 ECS 챕터가 담당한다.
- 이 챕터의 사고들이 낳은 설계 원칙(P1~P4)과 그 기원(NYPC 회고)은 설계 철학을 다루는 챕터의 본문이다 — 여기서는 "사고가 원칙을 만들었다"는 인과의 방향만 기억하면 된다.
