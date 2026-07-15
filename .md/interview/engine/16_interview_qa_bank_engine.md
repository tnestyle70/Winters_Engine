# 16. 엔진/아키텍처 면접 질문 은행 (Interview Q&A Bank — Engine/Architecture)

이 챕터는 면접 질문별 **답변 골격만** 모은 은행이다. 상세 서사·코드 설명은 각 도메인 챕터(01~15)가 담당하고, 여기서는 "첫 문장 → 전개 순서 → 근거 파일 → 꼬리질문 대비"만 적는다. C++ 문법 계열 질문은 `.md/interview/cpp/13_interview_qa_bank.md`가 담당한다.

답변 공통 원칙 3가지:
1. **첫 문장에 결론.** 배경 설명부터 시작하지 않는다.
2. **구체 하나로 신뢰 확보.** 수치(30Hz, 28%, 4096) 또는 파일 하나를 반드시 언급한다.
3. **트레이드오프·부채를 내가 먼저 말한다.** 압박 질문의 재료를 선점하면 D단이 쉬워진다.

---

## A. 프로젝트 소개·동기 (11문항)

**A1. 프로젝트를 1분 안에 소개해 달라.**
- 골격: "자체 엔진 WintersEngine.dll 위에 서버 권위(server-authoritative) MOBA 클라이언트와 IOCP 서버를 올린 프로젝트다. Engine/Client/Server/Shared(GameSim) 4계층이 분리되어 있고, 30Hz 결정론 tick → 스냅샷 복제 → 클라 보간/예측 → 리플레이까지 전체 왕복이 실제로 돈다." 기술 나열보다 '왕복이 돈다'는 사실을 먼저.
- 근거: `.md/architecture/WINTERS_CODEBASE_COMPASS.md:38`(제품 방향), `.md/architecture/WINTERS_DEPENDENCY_MAP.md:63`(런타임 흐름)

**A2. 왜 상용 엔진(UE/Unity)을 안 썼나?**
- 골격: 목표가 "게임 하나 완성"이 아니라 "엔진-게임 경계, 서버 권위, 결정론을 층별로 직접 소유하는 것"이었다. 상용 엔진은 이 경계가 이미 그어져 있어 배울 수 없다. 다만 상용 엔진을 무시하지 않는다 — UE의 검증된 런타임 계약(cook, validator)은 이식 대상으로 분석했고, Blueprint 같은 결정론을 깨는 부분은 명시적으로 거부했다.
- 근거: `.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md:96`(채택하지 않는 것과 이유)
- 꼬리: "그럼 UE는 못 쓰나?" → 이식 문서 자체가 UE 구조 분석 결과물이다, 로 반전.

**A3. 왜 LoL 모작인가?**
- 골격: MOBA는 서버 권위 스킬 판정, 미니언/타워 AI, 팀 시야(FOW), 밴픽 로비까지 "온라인 게임 서버 문제의 표준 집합"이라 검증장으로 최적이다. 복제가 목적이 아니라 문제 집합이 목적.
- 근거: `.md/architecture/CLASS_SERVANT_FOUNDATION_DIRECTION.md:7`

**A4. 왜 장르가 다른 두 게임(LoL/Elden)을 한 엔진에 올리나?**
- 골격: 두 게임 다 최종 제품이 아니라 검증장이다 — LoL은 서버 권위/팀전, Elden은 3인칭 액션/스트리밍을 검증한다. 엔진이 정말 제품 중립인지 증명하려면 장르가 다른 두 번째 클라이언트가 필요했다. 렌더러를 복제하지 않고 제품별 `RenderWorldSnapshot` 데이터 계약만 다르게 공급한다.
- 근거: `.md/architecture/CLASS_SERVANT_FOUNDATION_DIRECTION.md:7`, `Engine/Public/Renderer/RenderWorldSnapshot.h`

**A5. 프로젝트 구성(바이너리/의존)을 설명하라.**
- 골격: Engine(DLL) → EngineSDK로 배포, Client(exe)는 SDK 소비자로 링크(ProjectReference 없음 — 의도), Server(exe)는 Engine+GameSim 직접 참조, Shared/GameSim은 static lib으로 클라·서버 양쪽에 들어가는 결정론 시뮬 계약, 그 외 Tools(AssetConverter/SimLab).
- 근거: `.md/architecture/WINTERS_DEPENDENCY_MAP.md:5`(빌드 그래프)
- 꼬리: "Client가 왜 ProjectReference가 없나?" → B5·C13 참조.

**A6. 개발을 어떤 순서로 진행했고, 왜 그 순서였나?**
- 골격: 렌더링 → 씬/툴 → 게임플레이 → 병렬화 → 네트워크(서버 권위 전환) → 아키텍처 정제(경계 감사/lint) 순. 각 단계가 다음 단계의 관측 도구가 된다 — 예: 프로파일러(병렬화 전)가 없었으면 JobSystem race를 못 잡았다. "화면에 보여야 디버깅이 된다"가 순서의 원리.
- 근거: 개발 로그(세션 기록), `.md/architecture/WINTERS_DESIGN_PHILOSOPHY.md:34`(P4 디버깅 우선)

**A7. 본인만의 설계 원칙이 있나?**
- 골격: P1~P4 — 실패는 발생 지점에서 가시적으로(P1), 한 구조의 실패가 다른 구조를 오염시키지 않게(P2), 모든 특수상황은 코드에 명시(P3), 디버깅이 수월한 구조 먼저(P4). 기원은 NYPC 봇 대회 회고 — "1등 봇의 코드가 길었던 이유는 모든 예외를 코드로 명시했기 때문"이라는 관찰을 엔진 전 계층 원칙으로 박제했다.
- 근거: `.md/architecture/WINTERS_DESIGN_PHILOSOPHY.md:4`
- 꼬리: "원칙이 실제 코드가 됐나?" → `ePathFindResult` enum(P3), navgrid 폴백 사다리(P2)를 즉답 카드로.

**A8. 혼자 이 규모를 어떻게 관리했나?**
- 골격: 문서를 역할별로 분리했다 — 행동 규칙(CLAUDE/AGENTS, 짧게), 구조 의도(compass), **실측 상태**(dependency map, 파일:라인 근거+스테일 경고), 반복 실수 로그(gotchas.md, 날짜+영역+예방규칙 형식). 핵심 규칙은 "code wins over docs" — 코드로 답할 수 있는 사실은 문서에 안 적는다.
- 근거: `.md/architecture/WINTERS_DEPENDENCY_MAP.md:3`, `.claude/gotchas.md`

**A9. AI 코딩 도구를 어떻게 활용했나?**
- 골격: 도구가 아니라 협업 프로토콜로 다뤘다 — 반복 실수를 gotchas 로그로 박제해 재발을 세션 간 차단하고, 승인 전 코드 수정을 훅으로 기계적으로 차단(plan-first)하고, 경계 규칙은 문서가 아니라 PreBuild lint로 강제했다. "사람이든 AI든 규칙은 기계가 강제해야 안 무너진다"가 결론.
- 근거: `.claude/gotchas.md`, `Tools/Harness/Check-SharedBoundary.ps1`

**A10. 가장 자랑스러운 설계 결정 하나만 꼽으면?**
- 골격: "gameplay truth와 presentation의 분리를 계층 구조 전체의 축으로 삼은 것." Shared/Server가 truth를 소유하고 Client는 presentation으로 강등된다 — 이 한 축에서 서버 권위, 봇의 command 생산자 원칙, 클라 시뮬 게이팅, FOW 책임 분리가 전부 파생된다.
- 근거: `.md/architecture/WINTERS_CODEBASE_COMPASS.md:48`(계층 책임), `:174`(작업 전 체크 1번 질문)

**A11. 이 프로젝트의 최종 목표는?**
- 골격: LoL/Elden 검증장을 거쳐 자체 게임(Class & Servant)을 출시·운영하는 기반. 그래서 두 모작에서 "분리할 것(콘텐츠/플레이 흐름)"과 "공유할 것(Engine, asset 계약, 네트워크 primitive)"의 경계를 미리 긋고 있다.
- 근거: `.md/architecture/CLASS_SERVANT_FOUNDATION_DIRECTION.md:15`

---

## B. 아키텍처 설계 질문 (28문항)

**B1. 왜 서버 권위(server-authoritative)인가?**
- 골격: ① 치트 방어의 유일한 구조적 해법 — 클라는 검증 대상 입력만 보낸다. ② 판정 경로가 하나로 수렴 — 봇도 사람과 같은 GameCommand 스트림으로 합류해 동일 검증을 통과한다. ③ 리플레이/관전이 공짜 — 방송 바이트가 곧 기록 바이트. 대가: 클라 반응성 → 약한 예측+보간으로 보상(B16).
- 근거: `.md/architecture/WINTERS_DEPENDENCY_MAP.md:63`, `Server/Private/Game/ReplayRecorder.cpp`

**B2. 서버 게임 루프(프레임 흐름)를 설명하라.**
- 골격: 30Hz 고정 tick(주기 33,333µs, sleep_until 드리프트 보정). 한 tick 안에서 DrainCommands → ServerBotAI(command 생산만) → ExecuteCommands → SimulationSystems → LagComp 기록 → 이벤트/스냅샷 broadcast 순. 순서 자체가 데이터 인과 계약이다.
- 근거: `Server/Private/Game/GameRoomTick.cpp:68`(TickThread), `:103`(페이즈 순서)

**B3. 왜 ECS인가? 그리고 왜 sparse set인가?**
- 골격: 스냅샷 복제·결정론 시뮬에는 "상태 = POD 데이터 배열"이라는 형태가 필요했고, ECS가 그 형태를 강제한다. 스토리지는 archetype 대신 sparse/dense/data 3배열 sparse set + swap-remove — 구현 단순성과 O(1) 추가/삭제를 얻고, 다중 컴포넌트 join의 캐시 효율을 양보했다.
- 근거: `Engine/Public/ECS/ComponentStore.h:14`
- 꼬리: "join 성능은?" → 순회는 첫 번째 타입 store가 주도한다. 작은 집합을 앞에 두는 관행 + smallest-set driver가 개선안임을 인지하고 있다고 답.

**B4. ECS 시스템의 실행 순서와 병렬 실행은 어떻게 안전하게 했나?**
- 골격: 2층 계약 — 순서는 Phase 정수(생산자-소비자 의존성), 병렬은 시스템이 선언한 read/write 접근권 기반 충돌 검사 배칭. 미선언 시스템 기본값은 "모든 것을 쓴다"로 보수적 — 안전이 기본, 성능은 명시적 선언으로만 완화.
- 근거: `Engine/Private/ECS/SystemScheduler.cpp`, `Engine/Public/ECS/ISystem.h`
- 꼬리: "Phase가 틀리면?" → C3의 미니언 stuck 사고가 실증 사례.

**B5. 엔진을 왜 DLL로 분리했나?**
- 골격: 엔진-제품 경계를 링크 경계로 만들기 위해서다. Engine → EngineSDK(헤더/lib/dll) 배포 허브를 거치고, 내부 매니저 헤더는 purge해서 CGameInstance 게이트웨이 경계를 물리적으로 강제한다. Client는 의도적으로 ProjectReference 없이 SDK lib 소비자다.
- 근거: `UpdateLib.bat`, `.md/architecture/WINTERS_DEPENDENCY_MAP.md:15`
- 꼬리: "stale lib 함정은?" → 알고 있다. 단독 빌드 금지, 반드시 sln 경유 — 실측 지도에 함정으로 기록(`:33`).

**B6. DLL 경계에서 뭘 조심했나?**
- 골격: ① STL 타입 값 반환 금지(out 파라미터/opaque handle) ② dllexport 클래스 + unique_ptr 멤버는 copy ctor/assign 명시 delete ③ 템플릿은 export 불가하므로 헤더 인스턴스화, non-template만 export ④ private ctor + Create 팩토리. 전부 실제 사고에서 역산한 규칙이다.
- 근거: `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md:383`, `.claude/gotchas.md`(2026-04-23)

**B7. 결정론(determinism)을 어떻게 보장했나?**
- 골격: 3축 — ① 시간: 고정 dt 1/30s, 벽시계 배제 ② 난수: xorshift64 + tick/entity/skillId 파생 서브시드(소비 순서 독립) ③ 순회: 정렬 순회로 해시맵 순서 비결정성 제거. 검증은 SimLab same-seed 해시 게이트 — 동작 무변경 슬라이스는 해시 동일이 통과 조건.
- 근거: `Shared/GameSim/Core/Determinism/DeterministicTime.h:9`, `DeterministicRng.h:15`, `.md/architecture/WINTERS_DATA_ARCHITECTURE.md:109`
- 꼬리: "부동소수점은?" → 서버 `/fp:precise`(`Server/Include/Server.vcxproj:56`). 크로스머신 결정성은 미검증 부채로 정직하게 답.

**B8. 왜 예외(exception)를 안 쓰나?**
- 골격: 반환값 기반(bool/HRESULT/핸들 IsValid)이 전 계층 기본이다. 이유: 결정론 tick 안에서 스택 되감기 경로를 배제하고, 계층별 반환 컨벤션을 통일하기 위해. 유일한 throw 경계는 cooked asset 파싱의 CBinaryReader이고 소비하는 로더가 catch해 false로 변환 — throw가 경계를 넘지 않는다.
- 근거: `.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md:7`

**B9. 아키텍처 규칙이 시간이 지나도 안 무너지게 어떻게 보장했나?**
- 골격: 문서는 강제가 아니다 — 컴파일러가 못 잡는 include 방향 규칙을 텍스트 lint(`Check-SharedBoundary.ps1`)로 만들어 GameSim PreBuild에 물렸다. 위반 = 빌드 실패(exit 1, file:line 출력). 어댑터 경로만 화이트리스트.
- 근거: `Tools/Harness/Check-SharedBoundary.ps1:23`, `.md/architecture/WINTERS_DEPENDENCY_MAP.md:48`

**B10. 레이어 위반(Shared→Engine)을 어떻게 갚았나?**
- 골격: 빅뱅 대신 3단 슬라이스 — ① 헤더 오염 체인(Engine_Defines→dinput/using namespace) 절단 ② 직접 include 78+개 파일을 Core/Ecs 어댑터 9종 경유로 재라우팅 ③ 백엔드 교체(Shared 소유 ECS)는 마지막 단계로 유보. 각 단계를 lint로 고정해 후퇴를 차단했다.
- 근거: `.md/architecture/WINTERS_DEPENDENCY_MAP.md:53`(§3)
- 꼬리: "왜 아직 Engine CWorld를 쓰나?" → 링크 의존은 남았다고 선인정 — 어댑터 repoint 한 번으로 교체 가능한 상태까지 만든 것이 이 슬라이스의 목적.

**B11. 게임 데이터는 어떻게 관리하나?**
- 골격: 소유권 3분할 — SharedContract(타입+결정론 조회, 값 없음) / ServerPrivate(스탯·스킬 수치, 서버에만 컴파일) / ClientPublic(모델·애님 키, 클라에만 컴파일). 불변식: "런타임은 JSON을 읽지 않는다" — JSON은 authoring 입력이고 런타임은 cook된 immutable pack만 읽는다. 서버 값이 클라 바이너리에 없으니 데이터 경계가 곧 치트 방어선.
- 근거: `.md/architecture/WINTERS_DATA_ARCHITECTURE.md:5`, `.md/architecture/WINTERS_CODEBASE_COMPASS.md:3`

**B12. 클라와 서버의 데이터 버전이 어긋나면?**
- 골격: Hello 핸드셰이크에 dataBuildHash를 실어 접속 시점에 대조한다 — "미묘하게 다른 수치"라는 최악의 침묵 drift가 접속 로그 한 줄로 바뀐다. 프로토콜 하위호환은 새 필드를 끝에 추가(0=구버전 검사 생략)로 유지.
- 근거: `.md/architecture/WINTERS_DATA_ARCHITECTURE.md:66`(D-6a)

**B13. 챔피언 150개 스케일을 어떻게 확장 가능하게 설계했나?**
- 골격: 중앙 switch 대신 (champion × variant) 2D 함수포인터 테이블(GameplayHookRegistry) + self-registration. 신규 챔피언 = 자기 파일 하나 추가, 중앙 파일 수정 없음 → O(1) 디스패치 + 병합 충돌 최소화. 수치는 코드가 아니라 skill effect param(데이터)에서 읽는다.
- 근거: `Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h:36`
- 꼬리: "그런데 CommandExecutor에 특수케이스가 남았던데?" → D-1 이관 로드맵으로 선인정(`WINTERS_DATA_ARCHITECTURE.md:43`).

**B14. 멀티스레드 서버에서 상태 경합을 어떻게 막았나?**
- 골격: "스레드 경계 = 오염 경계" 원칙. IOCP 워커는 ingress mutex까지만 접근하고, truth는 30Hz tick 스레드가 단독 소유(m_stateMutex). 네트워크 실패는 세션 단위로 격리해 룸 상태를 오염시키지 않는다. 알려진 대가: join/leave가 stateMutex를 잡아 느린 tick이 accept를 지연시킬 수 있음 — 실측 문서에 기록.
- 근거: `.md/architecture/WINTERS_DESIGN_PHILOSOPHY.md:25`(P2), `.md/architecture/WINTERS_DEPENDENCY_MAP.md:77`, `Server/Private/Game/GameRoomTick.cpp:84`

**B15. 스냅샷에 델타 압축이 없다. TCP 단일인 이유는?**
- 골격: 의도된 MVP 트레이드오프다. full-snapshot은 무상태라 재접속/관전/리플레이가 단순해지고, TCP 단일은 순서 보장을 공짜로 얻는다. 델타/AOI/UDP 데이터면은 로드맵이고, 구현과 설계 의도를 구분해서 관리한다.
- 근거: `Server/Private/Game/SnapshotBuilder.cpp`, `Server/Private/Network/IOCPCore.cpp`
- 꼬리: "대역폭 계산은?" → 수치를 모르면 지어내지 말고 "10인 룸 기준 측정 후 델타 도입 판단이 다음 단계"로 답.

**B16. 클라이언트 예측은 어디까지 하나?**
- 골격: 약한 예측(weak prediction)만 — 이동 yaw와 대시 연출 정도를 로컬 선반영하고, 위치 truth는 스냅샷 보간이 소유한다. 핵심 장치는 로컬 이동 yaw 보호 상태기계: ack만 믿지 않고 서버 yaw가 실제로 예측을 따라잡을 때까지(최대 12 스냅샷) 보호한다.
- 근거: `Client/Private/Network/Client/SnapshotApplier.cpp:438`(ProtectLocalMoveYaw), `:80`(보호 상한), `.claude/gotchas.md`(2026-05-20 local yaw)

**B17. 렌더 백엔드 추상화(RHI)는 어떻게 설계했나?**
- 골격: IRHIDevice 인터페이스 + generation/index를 u64에 패킹한 핸들 모델(use-after-free를 세대 불일치 nullptr로 방어). DX11은 immediate 모드라 배리어/렌더패스를 no-op으로 흡수하고, DX12는 같은 인터페이스 뒤에서 fence/배리어를 실제 구현 — 상위 계층은 백엔드를 모른다. public 헤더에는 native 타입 대신 void*만 통과.
- 근거: `Engine/Public/RHI/IRHIDevice.h`, `Engine/Public/RHI/RHIHandles.h:31`, `.md/architecture/WINTERS_DEPENDENCY_MAP.md:43`(ID3D11 노출 0건 실측)

**B18. 한 엔진으로 두 제품을 어떻게 지원하나?**
- 골격: 렌더러 클래스 계층을 복제하지 않는다 — 제품별로 다른 것은 `RenderWorldSnapshot`이라는 데이터 계약뿐이다. LoL과 Elden은 각자의 씬/카메라/입력 모듈에서 스냅샷을 다르게 채우고, 같은 RHI 렌더러가 소비한다.
- 근거: `.md/architecture/WINTERS_CODEBASE_COMPASS.md:118`(RHI 방향), `Engine/Public/Renderer/RenderWorldSnapshot.h`

**B19. 자체 에셋 포맷(.wmesh 등)을 왜 만들었나?**
- 골격: 런타임에서 FBX/JSON 파싱을 제거하기 위해서다. 오프라인 CLI(AssetConverter)가 좌표계/탄젠트/본을 정규화해 검증된 바이너리를 굽고, 런타임은 pack(1) + static_assert로 잠근 고정 레이아웃 POD를 zero-copy로 GPU에 올린다. 파일은 신뢰하지 않는 경계 — 헤더 단계에서 상한/정합 검증.
- 근거: `Engine/Public/AssetFormat/Mesh/WMeshFormat.h`, `Engine/Private/Tools/AssetConverter/main.cpp`

**B20. 봇 AI 구조를 설명하라.**
- 골격: 두 원칙 — ① 봇은 truth를 직접 조작하지 않고 사람과 동일한 GameCommand를 생산해 같은 검증 경로를 통과한다(봇 치트 원천 차단). ② 판단은 이질적 목표를 '골드 단위' 단일 통화로 환산해 비교하고, 난수·벽시계 없이 결정론을 유지한다.
- 근거: `.md/architecture/WINTERS_DEPENDENCY_MAP.md:76`, `Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.h`
- 꼬리: "예외는 없나?" → 미니언/터렛/사망·리스폰은 서버 권위 코드가 의도적으로 직접 mutate — 예외를 문서에 명시해 관리(`:76`).

**B21. 수십 마리 미니언의 길찾기를 어떻게 감당했나?**
- 골격: 계층 폴백 — 전역 흐름은 사전계산 flow-field, 막힘 해소만 틱당 예산 캡이 있는 A*, 지역 회피는 스티어링, 최후는 depenetration. 전 유닛 A*는 비용상 불가라는 판단이 출발점.
- 근거: `Server/Private/Game/ServerMinionFlowField.cpp`, `Server/Private/Game/GameRoomUnitAI.cpp`

**B22. 싱글턴 게이트웨이(GameInstance)가 핫패스를 죽이지 않게 어떻게 했나?**
- 골격: 호출 빈도 기준 2티어 — 저빈도(씬 전환, 사운드)는 포워딩 허용, 핫패스(ECS 쿼리, Job 제출)는 포워딩 금지하고 Getter 반환값을 캐시해 직접 호출. "편의 API가 프레임 비용이 되는 것"을 규칙으로 차단.
- 근거: `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md:291`(Tier 기준), `:379`(금지사항)

**B23. 리플레이를 어떻게 구현했나?**
- 골격: 새 직렬화를 만들지 않았다 — 서버가 broadcast한 스냅샷/이벤트 바이트 스트림을 그대로 .wrpl로 기록하고, 재생은 라이브와 동일한 SnapshotApplier/EventApplier 경로를 재사용한다. 라이브-리플레이 패리티가 구조적으로 공짜.
- 근거: `Server/Private/Game/ReplayRecorder.cpp`, `Client/Private/Replay/ReplayPlayer.cpp`

**B24. 실패를 관측 가능하게 만드는 설계란?**
- 골격: ① 실패 원인을 enum으로 분해 — 빈 경로 하나로 뭉개지던 4원인을 `ePathFindResult`(NullGrid/StartBlocked/GoalBlocked/NoRoute/BrokenPath)로 분리 ② 폴백은 침묵 금지 — navgrid 4단 폴백 사다리 각 단계에 로그 ③ 진단은 bounded 카운터(실패 8/miss 64/계측 512)로 폭주 방지.
- 근거: `Engine/Public/Manager/Navigation/Pathfinder.h:13`, `Server/Private/Game/GameRoomNav.cpp:161`, `.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md:18`

**B25. 메모리 관리는 어떻게 했나? 커스텀 할당자는 없나?**
- 골격: 커스텀 할당자는 안 만들었다 — 도입 조건("할당이 프로파일러에 병목으로 잡히면")이 아직 충족된 적 없다. 대신 두 층이 있다: ① 누수 탐지 — Debug 빌드는 공용 헤더에서 `new`를 파일/라인 기록 매크로(DBG_NEW)로 전역 재매핑하고, 진입점에서 CRT 힙 검사(`_CRTDBG_LEAK_CHECK_DF`)를 켜 종료 시 누수를 file:line으로 받는다. ② 소유권 규칙 — private ctor + `Create()` 팩토리가 unique_ptr을 반환하는 컨벤션 전 계층 통일로 수명 문제를 타입에 위임. 핫패스는 동적 할당 대신 고정 용량 선할당(JobSystem deque 4096 등)이 원칙.
- 근거: `Engine/Public/Engine_Defines.h:38`(DBG_NEW), `Client/Private/main.cpp:95`(_CrtSetDbgFlag), `Engine/Public/Core/JobSystem/WorkStealingDeque.h:11`
- 꼬리: "RAM/VRAM 풋프린트와 에셋 메모리 예산은?" → 실측 수치가 없음을 인정 — Tracy 계측/DXGI 예산 쿼리로 챔피언 1종당 에셋 비용표를 만드는 것이 다음 단계라고 답. 수치를 지어내지 않는다.

**B26. 사운드는 어떻게 처리했나?**
- 골격: FMOD를 엔진 내부 매니저(CSound_Manager)로 감쌌다. 구조 포인트 3개 — ① 경계: 공개 헤더는 FMOD 전방 선언만 두고 fmod.hpp를 노출하지 않으며, SDK 배포 금지 내부 매니저라 Client는 CGameInstance 포워딩으로만 접근(저빈도 호출이라 B22 기준 포워딩 허용 티어). ② 채널 모델: 고정 채널(eSoundChannel — BGM 등 정지 후 교체)과 자동 채널(PlayEffect — 겹침 허용)을 분리. ③ 수명: 시작 시 Resource/Sound/ 재귀 프리로드(상대 경로 → Sound 맵), 매 프레임 Tick으로 FMOD 시스템 갱신. 믹싱 스레드는 FMOD가 소유하고 우리 쪽 진입은 메인 스레드 단일 경로라 매니저에 락이 없다.
- 근거: `Engine/Public/Sound/Sound_Manager.h:5`(전방 선언), `:33`(채널 분리)
- 꼬리: "대형 프로젝트라면?" → 전량 프리로드가 한계 — 사운드 자산이 커지면 로드 시간·메모리를 계측한 뒤 뱅크/스트리밍 도입이 순서라고 답.

**B27. 애니메이션 런타임은 어디까지 구현했나?**
- 골격: 단일 트랙 재생기까지다 — CAnimator가 .wanim 키프레임을 샘플링해 로컬→글로벌→파이널(스킨) 본 행렬 3단을 만들고, 애니 이벤트는 프레임 통과 판정(`HasFramePassed`)으로 처리한다(캐스트/타격 프레임이 이 API 위에 있다). 재생 속도는 ImGui 슬라이더로 런타임 튜닝 가능. 한계를 내가 먼저 말한다: 블렌딩/크로스페이드 없음(PlayAnimation은 즉시 교체), 상태 전이는 엔진이 아니라 게임플레이 코드 소유, 루트모션 없음.
- 근거: `Engine/Public/Resource/Animator.h:19`(PlayAnimation), `:36`(HasFramePassed)
- 꼬리: "Elden 액션 확장은 이걸로 되나?" → 안 된다 — 2-포즈 크로스페이드 → 상태기계의 엔진 승격 → 루트모션 순으로 도입하는 것이 계획이고, 블렌드 트리보다 크로스페이드가 먼저라는 우선순위까지 답.

**B28. 우리 팀은 UE를 쓴다. 어떻게 적응할 건가?**
- 골격: 개념 대응표로 답한다 — Winters의 각 구조는 UE 대응물이 있다: GameplayHookRegistry(챔피언×variant 훅 테이블+데이터 파라미터) ↔ GAS의 어빌리티 등록/디스패치, 스냅샷 복제+클라 보간/약한 예측 ↔ 액터 리플리케이션+무브먼트 스무딩, RenderWorldSnapshot(게임→렌더 데이터 계약) ↔ 게임-렌더 스레드 프록시 미러(FScene/SceneProxy), IRHIDevice 핸들 모델 ↔ UE RHI 추상화, AssetConverter cook ↔ UE 쿠킹 파이프라인, EngineSDK 경계 ↔ 모듈 API 경계. "같은 문제를 이미 풀어봤으니 배울 것은 API 표면과 팀 컨벤션"으로 마무리 — UE 툴 이식 분석 문서가 이 매핑을 실제로 수행한 증거.
- 근거: `Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h:36`, `Engine/Public/Renderer/RenderWorldSnapshot.h`, `.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md:96`
- 꼬리: "GAS 실사용 경험은 없잖나" → 인정 — 다만 어빌리티 디스패치·수치의 데이터 분리·예측 충돌이라는 문제 형태를 이미 겪었으므로 학습 비용은 개념이 아니라 표면이라고 답.

---

## C. 트러블슈팅·경험 질문 (17문항)

각 항목의 상세 서사(증상→계측→원인→수정→재발 방지)는 14장(트러블슈팅 사례집)이 담당한다. 여기서는 어떤 질문에 어떤 사례를 매핑할지와 답변 골격만 적는다.

**C1. 가장 오래 잡은 버그는?**
- 사례: 챔피언 yaw 대서사. 골격: "각도를 연속 상태로 볼지 wire 값으로 볼지"라는 관점 문제였다 — 매 tick 정규화가 +PI/-PI 경계를 재교차시키고, 챔피언별 메시 forward axis가 달랐고, 클라 nav 시스템이 스냅샷 yaw를 덮어썼다. 해법은 소유권 규칙화: Transform yaw는 near-resolve, 정규화는 wire/로그 전용, 복제 챔피언의 클라 nav 비활성.
- 근거: `.claude/gotchas.md`(2026-05-20~22 클러스터 7건)
- 꼬리: "왜 오래 걸렸나?" → 한 버그가 아니라 서버 저장·클라 예측·메시 에셋·시스템 게이팅 4계층에 걸친 사고 클러스터였다고 구조로 답.

**C2. lock-free 자료구조에서 겪은 가장 어려운 버그는?**
- 사례: JobSystem Chase-Lev deque race. 골격: Chase-Lev의 불변식은 "bottom push는 owner 스레드만"인데 Main 스레드가 임의 워커 deque에 push해 규약을 깼다. 코드 추론으로는 못 잡았고 프로파일러 카운터로 잡았다. 임시로 fallback 경로로 기능을 살린 뒤, 전역 큐 하이브리드(owner-only push)로 정식 수정.
- 근거: `Engine/Public/Core/JobSystem/WorkStealingDeque.h:11`(4096 고정/alignas(64)/seq_cst fence), `Engine/Private/Core/JobSystem.cpp`, 개발 로그
- 꼬리: "seq_cst fence는 왜?" → cpp 세트 09장(동시성)으로 연결.

**C3. ECS 시스템 실행 순서 때문에 사고 난 적 있나?**
- 사례: 미니언 제자리 stuck. 골격: NavSystem/AISystem의 Phase가 뒤집혀 1프레임 stale velocity가 남았다. "Phase 번호 = 데이터 의존성 선언"임을 사고로 체득했고, 이후 Phase 계약을 문서화된 규칙으로 승격.
- 근거: 개발 로그(2026-04-28), `Engine/Private/ECS/SystemScheduler.cpp`

**C4. 침묵 실패(silent failure) 때문에 고생한 경험은?**
- 사례: Pathfinder 빈 경로. 골격: 경로 실패가 빈 vector 하나로 반환돼 미니언 stuck의 원인을 구분할 수 없었다. bool/empty 대신 5값 result enum으로 원인을 분해하고 NavAgent 트레이스에 노출 — "실패의 정보 손실을 타입으로 복원"한 사례.
- 근거: `Engine/Public/Manager/Navigation/Pathfinder.h:11`(주석에 사고 이력 명시)

**C5. 성능 저하를 잡은 경험을 말해 보라.**
- 사례: 프레임 17.8ms→9ms(세션 기록 기준). 골격: 추측하지 않고 자체 프로파일러로 병목을 스키닝 갱신으로 특정 → 정적 엔티티 bAnimated 스킵 플래그(같은 세션의 .wmesh 쿠킹 통합은 로드 경로 기여분). 교훈: "최적화는 counter/scope로 증명한다"를 팀 원칙으로 박제.
- 근거: 개발 로그(2026-04-24), `.md/architecture/WINTERS_CODEBASE_COMPASS.md:112`(성능 증명 규칙)

**C6. 관측 도구 자체가 버그였던 적은?**
- 사례: CPUProfiler 병렬화 직후 crash. 골격: 프로파일러 scope 스택이 공유 상태라 워커 스레드와 race — thread_local 분리 + merge만 최소 락 + 프레임 더블버퍼로 수정. "계측기가 먼저 스레드 안전해야 병렬화를 디버깅할 수 있다"는 역설이 포인트.
- 근거: `Engine/Private/Core/Profiler/CPUProfiler.cpp`, 개발 로그(2026-04-28)

**C7. "호출은 되는데 화면에 안 보이는" 렌더링 버그는 어떻게 접근하나?**
- 사례: LoL FX 텍스처 — render/*.png는 mesh diffuse가 아니라 sprite 캡처였고, UV가 알파 0 영역을 가리켜 clip으로 전 픽셀이 버려졌다. 골격: CPU 디버거가 무력한 GPU 경계 버그였다 — 코드 추론 대신 데이터 계측(UV bbox vs alpha bbox) + RenderDoc이 정답이었다. 이후 "CPU/GPU 경계 분기 강제"를 디버깅 파이프라인 규칙으로.
- 근거: 개발 로그(2026-04-26), `CLAUDE.md` Progressive Sections(디버깅 파이프라인 규칙)

**C8. 비동기 코드에서 겪은 미묘한 버그는?**
- 사례: CHttpClient 가짜 async. 골격: std::async 반환 future를 폐기하면 소멸자가 대기해 사실상 동기 블로킹이 된다 — 그리고 그 블로킹이 raw this 캡처를 우연히 안전하게 만들고 있었다. 두 결함이 한 세트임을 파악하고 future 소유+요청 스냅샷 복사+소멸자 드레인으로 함께 재설계.
- 근거: `.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md:53`, `.md/architecture/WINTERS_HANDOFF_GUIDE.md:45`, `Client/Public/Network/Backend/CHttpClient.h`

**C9. 로그가 안 찍혀서 헤맨 적 있나?**
- 사례: OutputDebugStringA 게이트. 골격: Engine_Defines.h가 OutputDebugStringA를 게이트 매크로로 재매핑하는데 게이트 정의가 구성/계층마다 달라 Server·GameSim에서는 무음이었다. 채널 규약으로 해소: Server=std::cerr, Shared sim=전용 진단 헤더, 실패 진단은 no-op 채널 금지.
- 근거: `.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md:25`, `Shared/GameSim/Core/Debug/SimDebugOutput.h`

**C10. 로깅 규율을 어떻게 세웠나?**
- 사례: dead diagnostics 감사. 골격: 1,652줄짜리 SnapshotApplier에 로그 호출 0개, sprintf_s 8곳 전부 포맷만 하고 미출력 — "트레이스가 있다고 믿게 만드는" 최악 패턴을 전수 감사로 적발하고 "출력하거나 삭제한다"를 정책화. 루틴 트레이스(게이트/제거)와 실패 진단(상시 bounded)을 구분하는 게 운영 핵심.
- 근거: `.md/architecture/WINTERS_DESIGN_PHILOSOPHY.md:18`, `WINTERS_ERROR_HANDLING_POLICY.md:19`

**C11. 헤더 include 순서 의존 버그를 겪었나?**
- 사례: 공개 헤더 unqualified vector. 골격: `using namespace std;` 선행을 가정한 공개 헤더가 다른 include 경로에서 컴파일 실패 — 공개 헤더 std:: 전면 명시 + using 금지로 일괄 수정하고 컨벤션으로 박제.
- 근거: `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md:110`(SkillTable.cpp 실패 경로 명시)

**C12. MSVC/툴체인 특이성으로 당한 적은?**
- 사례: dllexport + unique_ptr 멤버. 골격: dllexport 클래스는 copy ctor가 강제 인스턴스화돼 이동 전용 멤버와 충돌한다 — 특수 멤버 명시 delete를 규칙화. 문법 상세는 cpp 세트 02장으로.
- 근거: `.claude/gotchas.md`(2026-04-23), `Engine/Public/ECS/SystemScheduler.h`

**C13. 멀티 프로젝트 빌드에서 겪은 어려움은?**
- 사례: 빌드 그래프 함정 3종. 골격: ① flatc 코드젠이 3프로젝트에서 Inputs/Outputs 없이 병렬 실행돼 생성 헤더 레이스 ② UpdateLib.bat 호출처 6곳의 SDK 파일 레이스 ③ Client의 Engine ProjectReference 부재로 단독 빌드 시 stale lib 링크. 전부 감사로 확정해 실측 지도에 함정 목록으로 기록 — "알고 있는 함정"과 "모르는 함정"은 다르다.
- 근거: `.md/architecture/WINTERS_DEPENDENCY_MAP.md:30`

**C14. 클라 예측과 서버 권위가 충돌한 사고는?**
- 사례: yaw 보호 + 스냅샷 이동락. 골격: ack 시퀀스만 믿고 예측 보호를 풀면 서버 yaw가 따라잡기 전 스냅샷이 연출을 되돌린다 — "ack가 아니라 서버 상태가 예측을 따라잡았는가"를 조건으로 하는 프레임 카운트 상태기계로 수정. 서버 쪽도 스킬 중 이동 플래그를 스냅샷 빌드 시점에 억제해 튐을 제거.
- 근거: `Client/Private/Network/Client/SnapshotApplier.cpp:438`, `.claude/gotchas.md`(2026-05-20 local yaw prediction)

**C15. 외부 에셋을 그대로 못 쓰는 문제는 어떻게 진단했나?**
- 사례: 구조물 glb 이중 메시 Z-fighting + 무텍스처 메시. 골격: 추측 대신 DIAG 로그로 메시/머티리얼 구조를 실측해 원인(Destroyed 중첩 메시, 텍스처 없는 Eye)을 확정. 이후 쿠킹 단계에서 오버레이 노드를 필터링해 런타임이 아니라 에셋 단계에서 제거.
- 근거: 개발 로그(2026-04-20), `Engine/Private/AssetFormat/Mesh/WMeshWriter.cpp`

**C16. 로그 예산 설계 실수 경험은?**
- 사례: 성공/실패 공유 카운터. 골격: cast 로그 64캡을 성공 로그가 소진해 정작 실패가 안 보였다 — "성공과 실패는 카운터를 공유하지 않는다"를 정책 조항으로 승격.
- 근거: `.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md:18`

**C17. 본인의 디버깅 스타일을 요약하면?**
- 골격: "코드 추론 누적 금지, 데이터 계측 우선." 증상 튜닝 전에 관측 장치(오버레이/bounded 트레이스/캡처)부터 붙인다. 이동 버그면 현재 셀·다음 웨이포인트·경로·stuck 사유를 먼저 노출한다. 이 원칙 자체가 P4로 문서화되어 있고, AI 결정 트레이스를 스냅샷에 실어 클라 패널로 보는 관찰-조종 폐루프까지 도구화했다.
- 근거: `.md/architecture/WINTERS_DESIGN_PHILOSOPHY.md:34`, `Client/Private/UI/AIDebugPanel.cpp`

---

## D. 압박·약점 질문 (16문항)

답변 전략 3원칙: ① **사실은 즉시 인정** — 방어부터 시작하면 진다. ② **"몰랐다"가 아니라 "알고, 기록하고, 보류했다"** — 부채가 실측 문서에 file:line으로 있으면 약점이 관리 능력 증거로 반전된다. ③ **보류의 기준을 말한다** — 왜 그것이 지금이 아닌지(리스크/의존성/검증 비용).

**D1. "혼자 만든 거라 협업 경험이 없지 않나?"**
- 골격: 인원은 혼자였지만 협업 구조는 실제로 운영했다 — 파일 소유권 매트릭스와 lock 파일 목록으로 2-머신 충돌을 관리했고, 규칙은 사람 기억이 아니라 PreBuild lint·훅으로 강제했고, 문서는 의도(compass)와 실측(dependency map)을 분리해 "코드가 이긴다" 원칙으로 운영했다. "협업의 본질은 인원수가 아니라 규칙의 기계적 강제"라고 프레임 전환.
- 근거: `.md/collab/OWNERSHIP_MATRIX.md`, `Tools/Harness/Check-SharedBoundary.ps1`, `.md/architecture/WINTERS_DEPENDENCY_MAP.md:3`
- 꼬리: "사람과의 갈등 경험은 없잖나" → 인정 후, 리뷰 사이클(측정 우선/최소 수정/매트릭스 차분)을 체화한 상태라 팀 리뷰에 바로 이식 가능하다고 답.

**D2. "이 규모에 이 구조는 과설계 아닌가?"**
- 골격: 기준을 제시하며 반박 — 과설계는 "쓰지 않는 유연성"인데, 이 구조의 각 경계는 실제 사고에서 역산됐다: 클라 nav 게이팅은 yaw 덮어쓰기 사고에서, lint는 include 위반 78개 파일 실측에서, 폴백 가시화는 데이터 회귀 은폐에서. 반대로 안 만든 것도 명확하다 — 델타 압축, UDP, 동적 deque 확장은 필요 증명 전이라 안 만들었다.
- 근거: `.claude/gotchas.md`(2026-05-22), `.md/architecture/WINTERS_DEPENDENCY_MAP.md:53`
- 꼬리: "그래도 문서가 너무 많다" → 문서 정책 자체가 "코드로 답할 수 있는 건 안 적는다"로 통제됨(`CLAUDE.md` Document Policy).

**D3. "미완성 기능은 뭐가 있나?"**
- 골격: 목록을 즉답한다(머뭇거림이 최악) — ① 스냅샷 델타 압축/AOI 없음 ② UDP 데이터면 미구현(TCP 단일) ③ 랙 보상은 이력 기록까지만, 되감기 적용은 유보(`rewindTicks=0`) ④ 스키닝 메시의 RHI 스냅샷 경로 미완 ⑤ Shared→Engine 링크 의존의 마지막 절단 잔존. 각각 "왜 보류인가"의 기준(공정성 모델 미결정, 검증 비용, 챔피언별 회귀 리스크)까지 붙여서.
- 근거: `Server/Private/Game/GameRoomCommands.cpp:26`, `.md/architecture/WINTERS_DEPENDENCY_MAP.md:59`, `.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md:48`(잔여 침묵 지점 목록)

**D4. "데이터 분리율 28%면 데이터 주도 설계 실패 아닌가?"**
- 골격: 그 28%가 바로 내가 만든 스코어카드 수치다 — 실패의 증거가 아니라 측정의 증거. 가중 관점을 덧붙인다: 밸런싱 빈도가 가장 높은 도메인(스탯/스킬 수치)은 이미 JSON 편집→재cook으로 기획자 수정이 가능하고, 남은 항목은 D-슬라이스 우선순위로 관리된다. 삭제 게이트도 규율화 — 폴백 카운터 0 수렴이 legacy 삭제 조건.
- 근거: `.md/architecture/WINTERS_DATA_ARCHITECTURE.md:93`(스코어카드), `:32`(폴백 가시화 16곳)

**D5. "8,844줄 갓클래스가 있던데?"**
- 골격: 인정 + 대응 이력 — Scene_InGame은 책임별 9개 TU로 동작 불변 기계적 분할을 했고 단일 클래스라는 부채는 남아 있다. 핵심은 이것이 숨겨진 게 아니라 온보딩 가이드의 '지뢰밭 목록'에 명시돼 있고, 새 챔피언 추가가 갓헤더를 수정하지 않는 방향(레지스트리)으로 신규 코드를 유도한다는 것.
- 근거: `.md/architecture/WINTERS_HANDOFF_GUIDE.md:50`, `Client/Private/Scene/Scene_InGameLifecycle.cpp`

**D6. "AI가 코드를 다 짜준 것 아닌가?"**
- 골격: 도구는 썼고, 설계 결정·사고 수습·규칙 제정은 전부 내 것이다 — 그 증거가 이 레포에 있다: 반복 실수 로그 27건+은 내가 사고를 분석해 예방 규칙으로 승격한 기록이고, AI가 지시 전에 코드를 건드리지 못하게 훅으로 차단한 것도 나다. 면접에서 어떤 파일이든 지목하면 그 자리에서 설계 의도를 설명하겠다고 제안(가장 강한 반증).
- 근거: `.claude/gotchas.md`, `.claude/hooks/implementation-handoff-pretool-guard.ps1`

**D7. "바퀴의 재발명 아닌가? 실무에선 UE 쓸 텐데."**
- 골격: 재발명의 목적은 바퀴가 아니라 바퀴의 원리다 — 상용 엔진의 결정(디스크립터 힙, cook 파이프라인, 스냅샷 복제)을 직접 겪어야 UE에서도 "왜 이렇게 돼 있는지"를 아는 개발자가 된다. 실제로 UE 툴 이식 분석 문서에서 "가져올 것(런타임 계약 규율)"과 "안 가져올 것(Blueprint VM)"을 판단했다 — 이 판단력이 재발명의 산출물이다.
- 근거: `.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md:96`

**D8. "테스트 코드가 부족하지 않나?"**
- 골격: 단위 테스트 커버리지는 낮다고 인정한다. 대신 이 도메인에 맞는 검증층을 만들었다 — SimLab same-seed 해시(결정론 회귀), 서버+클라 왕복 스모크, 경계 lint(빌드 게이트), 빌드+스모크+audit을 묶은 검증 하네스. "게임 시뮬의 회귀는 assert보다 결정론 해시가 잘 잡는다"는 도메인 논리로 전개.
- 근거: `.md/architecture/WINTERS_DATA_ARCHITECTURE.md:106`(게이트 4종), `Tools/Harness/Run-S17RhiValidation.ps1`
- 꼬리: "그래도 단위 테스트는?" → 순수 로직(데미지 파이프라인, RNG)부터 도입 여지 인정 — 반박하지 않는다.

**D9. "안티치트가 없던데?"**
- 골격: 구조적 방어선은 이미 있다 — 서버 권위라 클라는 입력만 보내고, 서버 값은 클라 바이너리에 컴파일되지 않으며, 시퀀스/쿨다운/사거리 검증 게이트가 있다. 미집행 부분도 정확히 안다: suspicion 카운터는 기록만 되고 킥 정책이 없다(IsSuspicious 호출자 0). enforcement 정책(오탐 비용)이 설계 문제라 보류했다고 답.
- 근거: `.md/architecture/WINTERS_HANDOFF_GUIDE.md:49`, `.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md:55`

**D10. "렌더 경로가 legacy DX11과 RHI 두 벌이다. 지저분하지 않나?"**
- 골격: 이관 중 이중 경로는 선택한 전략이다 — 라이브 경로(DX11)를 유지한 채 RHI 경로를 나란히 붙여 커맨드라인으로 격리 검증하고, 정적 메시부터 단계적으로 넘긴다. "빅뱅 전환의 리스크 vs 이중 경로의 유지비"에서 전자를 더 큰 비용으로 판단했고, 진행 상태(정적=완료, 스키닝=미완)를 경계 주석으로 박제했다.
- 근거: `Client/Private/Scene/Scene_InGameRender.cpp`, `Engine/Private/Renderer/ModelRenderer.cpp`

**D11. "LoL 모작이면 포트폴리오/IP 문제는 없나?"**
- 골격: 경계를 미리 그었다 — 원본 추출 에셋은 로컬 검증용이고, 공개 빌드/포트폴리오는 대체 가능 에셋과 자체 파이프라인 코드 중심으로 유지한다는 규칙이 compass에 명문화되어 있다. 포트폴리오의 주장 대상은 리소스가 아니라 아키텍처·파이프라인·서버 코드다.
- 근거: `.md/architecture/WINTERS_CODEBASE_COMPASS.md:134`(Elden Client 방향, 포트폴리오 경계)

**D12. "이 엔진으로 실제 게임을 출시할 수 있다고 보나?"**
- 골격: 지금 그대로는 아니다 — 그래서 출시 대상(Class & Servant)과 검증장(LoL/Elden)을 구분해 뒀다. 출시 전 필수 목록도 갖고 있다: UDP/델타 대역폭 작업, 랙 보상 정책 확정, 에셋 스트리밍 비동기화, 크래시 리포팅. "가능한가"가 아니라 "무엇이 남았는지 아는가"가 이 질문의 진짜 채점 기준이라는 자세로.
- 근거: `.md/architecture/CLASS_SERVANT_FOUNDATION_DIRECTION.md:7`, `.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md:48`

**D13. "서버는 몇 명, 몇 룸까지 버티나?"**
- 골격: 실측이 없다는 사실부터 인정한다 — 현재 근거는 10인 1룸 스모크가 30Hz tick으로 도는 수준이고, 동시 룸 스케일·틱 1회 소요 ms·스냅샷 평균 바이트는 미계측이다. 대신 "어떻게 잴지"를 즉답한다: ① 룸 tick 스레드(`CGameRoom::TickThread`)의 페이즈 경계가 이미 명확해 페이즈별 타이머를 꽂을 지점이 정해져 있고 ② SnapshotBuilder 직렬화 직후 바이트 카운터 ③ 봇 전용 룸 N개 스폰 스케일 테스트. 구조적 병목 후보도 안다 — 룸당 전용 tick 스레드 모델이라 룸 수가 스레드 수로 직결되고, join/leave의 stateMutex 경합(B14)이 첫 한계일 것.
- 근거: `Server/Private/Game/GameRoomTick.cpp:68`(TickThread), `Server/Private/Game/SnapshotBuilder.cpp`
- 꼬리: "그럼 IOCP는 왜 썼나?" → 수용량 주장이 아니라 워커-tick 스레드 경계 설계(B14)가 포트폴리오의 주장 대상이라고 축을 옮긴다. B15와 같은 원칙 — 수치를 지어내지 않는다.

**D14. "라이브에서 크래시 리포트가 오면 어떻게 하나?"**
- 골격: 지금은 못 받는다 — 미니덤프 수집/심볼 보관 파이프라인이 미구현이고, 크래시 경계는 12장에서도 남은 부채로 명시된 항목이다. 현재 있는 것: Debug 빌드 CRT 누수 검사+file:line new 추적, bounded 실패 진단 로그, 접속 시 dataBuildHash로 "어느 빌드·어느 데이터인지"는 이미 식별 가능(B12). 도입 계획 골격: SetUnhandledExceptionFilter → MiniDumpWriteDump(스택+모듈 최소 덤프), 빌드별 PDB 아카이브, 덤프에 dataBuildHash·룸/틱 컨텍스트 첨부. 재현 불가 크래시는 서버 권위+리플레이(.wrpl) 구조 덕에 스냅샷 스트림 재생으로 상태를 재구성할 수 있다는 것이 이 아키텍처의 triage 강점.
- 근거: `.md/interview/engine/12_error_resilience.md:214`, `Client/Private/main.cpp:95`, `Server/Private/Game/ReplayRecorder.cpp`

**D15. "빌드 시간은 얼마나 걸리고 어떻게 관리하나?"**
- 골격: 풀빌드/증분 수치를 계측해 둔 게 없다 — 약점으로 인정하고 시작한다. 있는 장치: Engine은 PCH(WintersPCH.h)를 쓰고, 공개 헤더의 서드파티 노출을 차단하며(FMOD 전방 선언이 사례), EngineSDK 경계 덕에 Engine 내부(.cpp) 수정은 Client 리컴파일로 번지지 않는다. 알려진 문제도 명확하다 — Scene_InGame 갓헤더(D5)를 수정하면 클라 다수 TU가 리컴파일된다. 계획: msbuild 시간 로그로 풀/증분 수치부터 확보 → 갓헤더 include 다이어트 → 유니티 빌드는 측정 후 판단(측정 없는 대책 도입은 이 프로젝트 원칙 위반).
- 근거: `Engine/Include/Engine.vcxproj:63`(PrecompiledHeader/WintersPCH.h), `Engine/Public/Sound/Sound_Manager.h:5`(전방 선언 사례)

**D16. "지금 프레임 어디에 몇 ms 쓰는지 말해 보라."**
- 골격: 상시 예산표는 미작성이라고 인정하되, 즉시 만들 수 있는 상태임을 보인다 — 계측 인프라는 이미 있다: Tracy 존/카운터가 Engine DLL 단일 인스턴스로 전 모듈에 연결돼 있고(`WINTERS_PROFILE_SCOPE`가 Tracy zone과 F3 HUD scope를 동시 기록), 과거 이 인프라로 병목을 특정한 실측 사례(17.8ms→9ms, 스키닝)도 있다. 다만 60fps 16.6ms 예산 대비 Update/Render/시뮬/UI 분해표는 현재 없다 — "캡처 한 번이면 나오는 상태"와 "수치 카드가 준비된 상태"는 다르다는 것을 안다고 답.
- 근거: `Engine/Include/ProfilerAPI.h:47`(WINTERS_PROFILE_SCOPE), C5 사례
- 준비: 면접 전 Tracy 캡처 1회로 분해 수치 카드를 만들어 이 골격의 "미작성"을 실측치로 교체할 것.

---

## 다른 챕터와의 연결

- **A단(소개/동기)** 의 상세 서사 — 01장(프로젝트 개요/구조 총론)이 소유. 여기서는 1분 골격만.
- **B1·B2·B14~B16(서버 권위/tick/네트워크)** — 서버·네트워크 챕터가 IOCP/세션 수명/프레이밍 상세를 소유.
- **B3·B4(ECS/스케줄러)** 와 **C2·C6(JobSystem/Profiler)** — ECS·병렬화 챕터 + `.md/interview/cpp/09_concurrency.md`(memory ordering 문법).
- **B7·B8(결정론/에러 모델)** — `.md/interview/cpp/10_error_handling.md`가 noexcept/반환값 문법을, 이 세트의 결정론 챕터가 도메인 설계를 소유.
- **C단 전체** — 14장(트러블슈팅 사례집)이 각 사례의 증상→계측→수정 풀 스토리를 소유. 면접에서 꼬리가 깊어지면 14장 서사로 전환.
- **D3·D4·D12(부채/로드맵)** — 향후 개선 방향 챕터(15장)와 `.md/architecture/WINTERS_DATA_ARCHITECTURE.md` D-슬라이스가 원본.
- **DLL/템플릿/특수 멤버 문법 꼬리질문** — `.md/interview/cpp/02_compile_link_dll.md`, `05_class_design_value_semantics.md`로 넘긴다.
- **B25~B28·D13~D16(메모리/사운드/애니/UE 매핑/수용량/크래시/빌드/프레임 예산)** — 실측·구현이 얇은 영역은 "한계 인정 + 계측/도입 계획" 골격으로 답한다. 수치를 확보하면 해당 골격의 "미계측/미작성" 문구를 실측치로 교체한다.
