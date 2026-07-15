# 15. 향후 업데이트 구조 · 기술 부채 · 확장 설계

> 면접 질문 유형: "다시 만든다면 무엇을 바꾸겠나", "지금 프로젝트의 한계는", "이 구조로 콘텐츠가 늘어나면 감당되나", "미완성 부분은 어디까지 알고 있나".
> 이 챕터의 핵심 메시지: **나는 부채를 숨기지 않고, 목록화하고, 기계로 강제하고, 슬라이스로 갚는다.** 부채가 없다고 말하는 것이 아니라, 부채 대장(ledger)을 즉석에서 펼칠 수 있다는 것이 무기다.

---

## ① 한 줄 정의

"Winters의 기술 부채 관리는 별도의 TODO 리스트가 아니라 아키텍처의 일부다 — 나는 **규칙의 의도(compass) / 실측 상태(file:line 근거의 dependency map) / 재발 방지(gotchas)** 세 문서로 부채를 분리 소유시키고, 문서로만 남기지 않고 **PreBuild lint·폴백 카운터·빌드 해시 핸드셰이크**라는 기계 장치로 강제하며, 상환은 빅뱅이 아니라 검증 가능한 슬라이스 단위로 한다."

면접에서 "다시 만든다면?"을 받으면 나는 회한 목록이 아니라 **현재 진행 중인 상환 계획표**로 답한다. 아래가 그 계획표다.

---

## ② 부채 관리 시스템의 구조

부채가 "발견 → 박제 → 강제 → 소각 → 검증"의 생명주기를 돌도록 만들었다.

```text
[발견]   grep 전수 감사 (2026-07-09, 65건 발견 / high 18건 확정)
   │       근거: WINTERS_ERROR_HANDLING_POLICY.md:3
   ▼
[박제]   실측 문서에 file:line 근거로 기록 — "의도"와 "실측"을 분리 소유
   │       규칙 의도  = WINTERS_CODEBASE_COMPASS.md
   │       실측 상태  = WINTERS_DEPENDENCY_MAP.md (✅/⚠️ 매트릭스, 스테일 시 재검증 후 인용)
   │       반복 실수  = .claude/gotchas.md (YYYY-MM-DD - [Area] 형식)
   ▼
[강제]   문서가 아니라 기계가 지킨다
   │       - Tools/Harness/Check-SharedBoundary.ps1 → GameSim PreBuild에서
   │         직접 ECS/*, Engine_Defines, DX, imgui include 발견 시 빌드 실패
   │       - 폴백 가시화 16개 지점: "[Data] pack miss -> legacy" bounded 트레이스
   │         (WINTERS_DATA_ARCHITECTURE.md:32)
   │       - Hello.dataBuildHash 핸드셰이크: 클라/서버 데이터 팩 불일치를
   │         "미묘하게 다른 수치"가 아니라 접속 로그로 만듦 (같은 문서 :66, D-6a 완료)
   ▼
[소각]   슬라이스 단위 상환 (Phase 7F 슬라이스 1·2, D-1~D-6 데이터 슬라이스)
   ▼
[검증]   SimLab 결정론 해시 (리팩터 전후 300틱 해시 동일 = 동작 무변경 증명)
         + 정상 로스터 스모크에서 폴백 로그 0줄 확인
```

이 구조의 요점: **부채는 코드 안에서 침묵하는 순간 가장 위험해진다.** 그래서 미해소 부채조차 "알려진 잔여 침묵 지점" 목록(`WINTERS_ERROR_HANDLING_POLICY.md` §4)에 올려두고, 그 목록 첫머리에 *"여기 적힌 것은 고쳐야 할 것이지 따라 해도 되는 패턴이 아니다"* 라는 경고를 붙였다. 해소되면 취소선으로 진행을 추적한다 — 실제로 Pathfinder 빈 경로 원인 무구분과 CHttpClient 가짜 async 두 건은 2026-07-09에 취소선 처리됐다.

---

## ③ 부채 대장 (Debt Ledger) — 알고 있는 부채와 상환 계획

면접에서 그대로 꺼내 쓰는 표. 전 항목이 코드/문서에 file:line 근거가 있는 실측이다.

| # | 부채 | 현재 상태 (근거) | 상환 계획 |
|---|---|---|---|
| 1 | **Shared→Engine 절단 잔여** | 직접 include 0건까지 절단 완료. 그러나 `Shared/GameSim/Core/World/World.h:11`의 `using World = ::CWorld;`가 여전히 Engine dllexport 타입 → 링크 의존 + `EngineSDK/inc` include 경로 잔존 (`WINTERS_DEPENDENCY_MAP.md` §3) | Shared 소유 결정론 ECS 백엔드를 만들어 어댑터를 repoint. 어댑터 설계 덕분에 백엔드 교체가 "1파일 변경"으로 축소돼 있음 |
| 2 | **서버 sim 병렬화 미완** | 클라 JobSystem은 가동(Chase-Lev deque, FiberShell 실행 모드까지 존재 — `Engine/Private/Core/JobSystem.cpp:173`). 서버는 `CServerEntry`가 Initialize false 반환 스텁, GameRoom은 단일 tick 스레드 (`WINTERS_HANDOFF_GUIDE.md:48`) | IOCP completion→Tick fiber 소비 구조로 통합(워커 fiber화 없이). 유일한 합격 기준: same input → byte-exact snapshot |
| 3 | **UDP 전환 미착수** | IOCPCore는 TCP 단일 (`Server/Private/Network/IOCPCore.cpp:51`, `SOCK_STREAM/IPPROTO_TCP`). PacketEnvelope의 Compressed/Encrypted 플래그는 정의만 있고 사용처 0 (`Shared/Network/PacketEnvelope.h:30-31`) | UDP 데이터면(인게임) + TCP 제어면(밴픽/백엔드) 분리. 마스터플랜 문서화 완료 상태에서 대기 |
| 4 | **스냅샷 델타/AOI 없음** | 매 틱 세션별 전체 상태 재빌드 (`Server/Private/Game/SnapshotBuilder.cpp:106` — 세션별 차이는 yourNetId/lastAckedSeq뿐) | 델타+베이스라인 압축, 관심영역(AOI) 컬링. 단 무상태 full-snapshot의 late-join 단순성은 베이스라인 스냅샷으로 보존 |
| 5 | **랙 보상 미적용** | 200ms/6틱 이력 링버퍼는 라이브 기록 중이나 실제 명령은 `cmd.rewindTicks = 0` 고정 (`Server/Private/Game/GameRoomCommands.cpp:26`) | 인프라는 완성, 정책 결정("엄폐 뒤 피격" 공정성 모델)이 남은 것. 켜는 건 한 줄, 켤 근거를 만드는 게 일 |
| 6 | **데이터 분리 28%** | 18개 도메인 스코어카드 5.0/18 (`WINTERS_DATA_ARCHITECTURE.md:93`). 코드 소유 10개(웨이브/보상/아이템/와드/게임규칙 등) | D-1~D-6 슬라이스. legacy 삭제는 폴백 카운터 0 수렴 시에만 (zero-reader 규칙, 같은 문서 :111) |
| 7 | **Scene_InGame 갓클래스** | 8,844줄 단일 클래스(9개 TU 분할), 챔피언별 상태가 헤더에 하드코딩 → 헤더 수정 시 클라 절반 리컴파일 (`WINTERS_HANDOFF_GUIDE.md:50`) | 신규 챔피언은 갓헤더 수정을 피하는 등록 경로로; 장기적으로 챔피언 상태를 컴포넌트/레지스트리로 탈출 |
| 8 | **빌드 그래프 함정 3종** | flatc 코드젠 동시 실행 레이스, UpdateLib.bat 호출처 6곳 병렬 레이스, Client에 Engine ProjectReference 없음→단독 빌드 시 stale lib 링크 (`WINTERS_DEPENDENCY_MAP.md:30-33`) | flatc 타깃에 Inputs/Outputs 지정, SDK 배포 단일화, sln 경유 강제를 CI로 승격 |
| 9 | **스킨드 메시 RHI 미이관** | `AppendRenderSnapshotMeshes`가 `HasSkeleton()`이면 조기 return — "RHI 스키닝이 명시될 때까지 legacy 렌더러에 남는다"고 주석으로 경계 박제 (`Engine/Private/Renderer/ModelRenderer.cpp:664-667`) | RHI에 skinned vertex path + bone palette 추가 후 경계 해제. 정적 메시는 이미 스냅샷 경로 완료 |
| 10 | **anti-cheat 미집행** | suspicion 카운터는 기록만 되고 `CSession::IsSuspicious` 호출자 0 — 악성 패킷은 드랍만 되고 킥 없음 (`WINTERS_HANDOFF_GUIDE.md:49`) | enforcement 정책(임계→킥/로그) 연결. 카운터 인프라가 이미 있으니 정책 계층만 얹으면 됨 |
| 11 | **에디터 rot 위험** | EldenRingEditor는 CMake 전용이라 sln 빌드가 절대 건드리지 않음 → Engine API 변경 시 조용히 썩음 (`WINTERS_DEPENDENCY_MAP.md:25`) | 툴 투자 전 선결: 에디터를 빌드/CI에 편입. 현재는 검증 하네스가 CMake 단계에서 대신 커버 |
| 12 | **이중 진실(hp) 미러링** | HealthComponent가 authoritative인데 구 컴포넌트 5종이 각자 hp 보유 → 데미지 후 `MirrorHealth`로 전파 (`Shared/GameSim/Systems/Damage/DamagePipeline.cpp:67`) | 호출부가 광범위해 즉시 통합 대신 미러링으로 화해 중. reader를 HealthComponent로 수렴시킨 뒤 필드 삭제 |

이 표에서 내가 강조하는 건 **모든 부채에 "왜 아직 안 갚았나"에 대한 답이 있다**는 점이다. 2번은 결정론 검증이 선행 조건이고, 5번은 기술이 아니라 게임 디자인 결정이 남았고, 6번은 카운터가 0이 되기 전에 지우면 사고가 나기 때문이다.

---

## ④ 핵심 설계 결정과 트레이드오프

### 결정 1. 빅뱅 재작성 대신 어댑터 슬라이스 (Phase 7F)

- **왜**: Shared/GameSim이 Engine ECS 헤더 80개를 직접 include하고, `Engine_Defines.h` 체인이 `dinput.h`·`using namespace`·`#define new`·OutputDebugStringA 매크로 재정의를 결정론 sim의 TU에 전이 오염시키고 있었다 (`WINTERS_DEPENDENCY_MAP.md` §3).
- **대안**: (a) Shared 소유 ECS를 처음부터 새로 만들어 한 번에 교체, (b) 위반을 문서에만 적고 방치.
- **선택**: 3단 슬라이스 — ① 헤더 오염 체인 절단(명시 include로 치환) ② `Core/Ecs/` 어댑터 9종 신설 + 78+1개 파일 재라우팅 ③ 백엔드 교체는 뒤로 미룸. 어댑터 파일 주석이 의도를 그대로 말한다: *"replacing the backing world becomes a one-file change later"* (`Shared/GameSim/Core/World/World.h:8-11`).
- **감수한 비용**: 이중 구조 공존 기간. 어댑터는 "한 줄짜리 재노출 파일"이라 그 자체가 부채처럼 보이지만, 각 슬라이스가 **빌드+SimLab 해시로 독립 검증 가능**하다는 이득이 크다. 빅뱅이었다면 검증 불가능한 거대 diff 하나였을 것이다.

### 결정 2. 규칙을 리뷰 습관이 아니라 빌드 실패로 승격

- **왜**: "하지 마라"는 문서는 3개월이면 무너진다. 경계 위반은 코드가 늘수록 재발한다.
- **대안**: 코드 리뷰 체크리스트, 주기적 수동 감사.
- **선택**: `Check-SharedBoundary.ps1`을 GameSim PreBuild에 물려 직접 include 발견 시 **빌드 자체가 실패**하게 했다 (`WINTERS_DEPENDENCY_MAP.md:48`). 데이터 쪽도 같은 철학 — 클라/서버가 서로 다른 생성 데이터로 빌드된 조합은 Hello의 dataBuildHash 비교로 접속 시점에 로그로 드러난다.
- **감수한 비용**: PreBuild 시간 소폭 증가, lint 스크립트 자체의 유지보수. 그리고 정직하게 — include-path 레벨의 완전한 강제(EngineSDK/inc 경로 제거)는 부채 1번이 끝나야 가능하다. lint는 include 문장을 잡지, 경로 자체를 없애진 못한다.

### 결정 3. legacy 삭제는 zero-reader 게이트로만

- **왜**: 이중 소스(JSON pack + 코드 하드코딩)를 없앨 때 legacy를 먼저 지우면, pack에 구멍이 있는 경우 런타임이 조용히 깨진다.
- **대안**: "테스트 다 돌려보고 지우기" — 하지만 커버리지가 완전하다는 보장이 없다.
- **선택**: "pack miss → legacy 값" 폴백 지점 16곳에 bounded 트레이스를 심고, **정상 로스터 스모크에서 해당 카운터가 0으로 수렴한 조회 경로부터** legacy reader를 삭제한다 (`WINTERS_DATA_ARCHITECTURE.md:32,111`). 실제 집행 사례: SkinRegistry/SkinDef는 reader 0 확인 후 삭제, ChampionDef는 Shared/Server reader 0 확인 후 Client로 이동 (같은 문서 :38-39).
- **감수한 비용**: 이중 소스 공존 기간이 길어진다. 28%라는 낮은 분리율 숫자를 당분간 안고 가야 한다. 대신 삭제 사고가 구조적으로 불가능하다.

### 결정 4. TCP full-snapshot MVP 먼저, UDP/델타/랙보상은 뒤로

- **왜**: 네트워크 스택에서 가장 먼저 증명해야 하는 것은 전송 최적화가 아니라 **결정론과 권위 모델**이다. same input → byte-exact snapshot이 검증 기준.
- **대안**: 처음부터 UDP + 델타 압축 + 랙 보상 풀스택.
- **선택**: TCP 단일 + 무상태 full-snapshot으로 순서 보장·재접속·late-join 문제를 공짜로 얻고, 그 위에서 30Hz 권위 tick·ingress 게이트·Move 코얼레싱·결정론 정렬을 완성했다. 랙 보상은 링버퍼 기록까지만 켜고 적용(rewind)은 0으로 뒀다.
- **감수한 비용**: 대역폭이 엔티티수×세션수로 선형 증가하고, 느린 클라이언트에 대한 백프레셔가 없다. 이건 "몰라서 안 한 것"이 아니라 **부채 대장 3·4·5번으로 등재된, 순서가 정해진 미래 작업**이다.

### 결정 5. 미완성 경계는 주석+조기 return으로 못박기

- **왜**: 이관이 진행 중인 시스템은 "어디까지 됐고 어디부터 안 됐는지"가 코드에서 안 보이면 후속 작업자(미래의 나 포함)가 경계를 넘다 사고를 낸다.
- **선택**: 스킨드 메시의 RHI 스냅샷 진입을 `HasSkeleton()` 조기 return으로 차단하고, 바로 위에 이유를 주석으로 박았다 — *"Skinned models must stay on the legacy animated renderer until RHI skinning is explicit"* (`ModelRenderer.cpp:664-667`). 같은 패턴으로, 죽은 스텁(`Client/Public/Network/Backend/`의 동명 클래스 ODR 함정, includer 0에 TPS=20인 `Shared/Network/PacketDef.h`)은 지뢰밭 목록에 등재했다 (`WINTERS_HANDOFF_GUIDE.md:46-47`).
- **감수한 비용**: 코드에 "미완성"이라는 자백이 남는다. 나는 이걸 비용이 아니라 자산으로 본다 — 경계 없는 미완성이 진짜 비용이다.

---

## ⑤ 어려웠던 점과 해결

**가장 어려웠던 것은 부채가 부채로 보이지 않는 상태였다.**

1. **침묵하는 부채**: 2026-07-09 감사 전까지 SnapshotApplier(1,652줄)에는 sprintf_s로 포맷만 하고 출력하지 않는 진단이 8곳 있었다(dead diagnostics). 이건 로그가 없는 것보다 나쁘다 — 읽는 사람이 "트레이스가 있다"고 착각하게 만든다. 해결: "포맷만 하고 출력하지 않는 코드 금지"를 정책으로 승격하고(`WINTERS_ERROR_HANDLING_POLICY.md:19`), 잔여 2곳은 §4 목록에 "보류" 상태로 명시했다.
2. **성공을 가장하는 부채**: `CServerEntry`는 Initialize가 false를 반환하는 스텁이고, suspicion 시스템은 기록만 하고 집행하지 않는다. 코드가 존재한다는 사실이 "동작한다"로 오독되는 게 위험이라, 핸드오프 가이드의 지뢰밭 섹션에 호출자 수(0)까지 적어 박제했다.
3. **문서 자체의 스테일**: 실측 문서는 코드가 바뀌면 거짓말을 하기 시작한다. 해결: "code wins over docs" + 실측 문서 첫머리에 "재검증 후 인용" 규칙 + 이미 해소된 위반 주장의 재인용 금지 선례를 명문화했다 (`WINTERS_DEPENDENCY_MAP.md:3,82`).
4. **이중 빌드 시스템의 사각지대**: CMake 전용 타깃(EldenRingEditor)은 sln 워크플로에서 조용히 썩는다. 완전한 해결(빌드 편입) 전까지는 검증 하네스가 CMake 단계를 강제로 돌려 회귀를 대신 잡는다.

이 경험이 P1("실패는 발생 지점에서 즉시·가시적으로")을 부채 관리에도 적용하게 만들었다: **부채도 실패처럼, 발생 지점에서 가시적이어야 한다.**

---

## ⑥ 향후 확장 설계 — 로드맵 본체

### 6-1. 150챔프 스케일: 콘텐츠 추가 = 코드 수정이 아니라 등록

현재 `Shared/GameSim/Champions/`에 15개 챔피언이 각자 폴더로 산다(Annie~Zed, 실측). 확장 구조의 세 기둥:

1. **Self-register 훅 레지스트리**: 각 챔피언 GameSim이 자기 훅을 `CGameplayHookRegistry::Instance().Register(...)`로 스스로 등록한다 (`Shared/GameSim/Champions/Ashe/AsheGameSim.cpp:311-326`). 중앙 switch 테이블을 늘리는 게 아니라 파일을 추가하는 구조 — 챔피언 N+1의 추가 비용이 기존 코드 수정량과 무관해진다.
2. **와이어 안정성 있는 명시 enum**: `eChampion : uint8_t`가 전 값 명시(IRELIA=1 … LEESIN=17, END=255)로 고정돼 있어(`Shared/GameSim/Definitions/LoLMatchContext.h:5-26`) 네트워크/DB 직렬화가 재컴파일 순서에 흔들리지 않는다. 추가는 빈 ID, 삭제/재사용 금지.
3. **값은 데이터, 분기는 훅**: 수치는 champions.json → cook → ServerPrivate pack으로, 챔피언 고유 로직만 훅 함수로. 목표 상태는 "기획자가 JSON을 고치고 재cook하면 끝" — 가장 밸런싱 빈도가 높은 스탯/스킬 수치 도메인은 이미 이 상태다.

**정직한 잔여**: CommandExecutor에 아직 챔피언 특수케이스가 산재한다(Kalista 패시브 대시, Yasuo/Yone 스테이지 특수처리, Viego 소울 하드코딩 등 — 2,792줄짜리 파일이 훅 아키텍처의 미완성분 지표다). 상환 방향은 pre-cast 훅 변형(StageResolve/TargetResolve/CooldownPolicy)을 레지스트리에 추가해 챔피언당 1슬라이스로 각자의 GameSim.cpp로 옮기는 것(D-1). 이게 끝나야 "챔피언 추가 = 폴더 1개 + 등록"이 예외 없이 성립한다.

### 6-2. Fiber 서버

Fiber는 내 주력 기술로 선언하고 로드맵을 세운 영역이다. 현재 상태와 계획:

- **이미 있는 것**: JobSystem에 `eJobExecutionMode::FiberShell` 실행 모드가 구현돼 있다 — 워커가 `ConvertThreadToFiber`로 스레드를 파이버화하고 잡을 `CreateFiber`/`SwitchToFiber`로 실행한다 (`Engine/Private/Core/JobSystem.cpp:173,275-296`). public API(Submit/WaitForCounter)는 내부가 스레드→파이버로 바뀌어도 불변이 되도록 먼저 고정해 뒀다.
- **계획**: 서버 통합은 IOCP 워커를 파이버화하지 않고, completion을 큐로 넘겨 Tick fiber가 소비하는 구조를 설계안으로 잡아 뒀다(스레드 경계 = 오염 경계 원칙 유지). 100 GameRoom을 워커 풀에서 파이버로 스케줄링하는 것이 목표.
- **합격 기준은 하나**: 병렬화 전후 same input → byte-exact snapshot. 결정론이 깨지는 병렬화는 성능이 아니라 회귀다.

### 6-3. MCTS / RL

- **이미 있는 것**: UCB1 4단계 정석 MCTS 플래너(추상 액션 모델 — 매크로 의사결정 전용), 그리고 `CRLBridge` — 상태 인코딩(24차원: 위치/hp/쿨다운/마나)과 액션 디코딩 인터페이스는 완비돼 있으나 `LoadModel`이 항상 false를 반환하는 명시적 스캐폴드다 (`Engine/Private/AI/RLBridge.cpp:27-31`). MCTS의 액션 공간을 그대로 재사용해, 학습된 정책이 생기면 교체만 하면 되는 자리다.
- **RL 관점에서의 선행 투자**: silent fail 제거가 곧 학습 준비다. 빈 경로 반환을 `ePathFindResult` enum(NullGrid/StartBlocked/GoalBlocked/NoRoute/BrokenPath)으로 분해한 것(`Engine/Public/Manager/Navigation/Pathfinder.h:11-21`)은 디버깅 대책인 동시에, 모든 이동 액션이 deterministic effect를 갖게 만들어 sim2real 격차(환경 모델의 노이즈)를 줄이는 작업이다. 봇이 플레이어와 동일한 GameCommand 경로로만 상태를 바꾸는 원칙도 학습 데이터의 재현성을 위한 것.
- **다음 단계**: ONNX 런타임 연결 → 셀프플레이 데이터는 CReplayRecorder가 이미 쌓고 있는 스냅샷/명령 스트림에서.

### 6-4. 에디터 · UE/Fab 툴 방향

방향 문서의 한 줄 결론을 그대로 쓴다: *"UE에서 가져올 것은 개별 툴이 아니라 '자산이 검증된 런타임 계약으로만 게임에 들어간다'는 파이프라인 규율이다"* (`WINTERS_UE_FAB_TOOL_ADOPTION.md:7`).

- **수용**: Fab 자산은 전부 외부 원료 취급 — Import 스테이징 → cook(.w*) → Validator 게이트 → 런타임 계약 단일 경로. 애님팩은 cook 시점 오프라인 리타겟(.wskel 리타겟 프로필)이 선결 과제 — 런타임 IK보다 결정론/성능 원칙에 맞다 (같은 문서 §3-2).
- **명시적 거부**: Blueprint 런타임 VM은 채택하지 않는다 (같은 문서 §1 표). 서버 권위 30Hz 결정론 tick에 인터프리터 로직을 넣으면 SimLab 해시 검증이 무너진다. 대체: 수치=generated pack, 분기 로직=GameplayHookRegistry, UI 연출만 Lua. UObject/GC/리플렉션도 trivially-copyable 컴포넌트 + 스냅샷 복제 모델과 충돌해 거부.
- **선결 과제**: 툴에 투자하기 전에 툴이 썩지 않는 빌드 경계부터 — 에디터 빌드 편입(부채 11번). 에디터 계층 구조(Content Browser / Importer·Validator / Material Resolver / WFX / Sequencer / World Partition)는 compass가 소유한다 (`WINTERS_CODEBASE_COMPASS.md:141-155`).

### 6-5. 멀티 제품: 하나의 Engine.dll, 두 검증장

`WintersEngine.dll` 위에 WintersLOL.exe와 WintersElden.exe를 별도 제품 클라로 올린다. 렌더러는 복제하지 않는다 — LoL과 Elden이 같은 RHI renderer에 서로 다른 `RenderWorldSnapshot`을 공급한다 (`WINTERS_CODEBASE_COMPASS.md:118-139`). LoL 모작과 Elden 모작은 최종 제품이 아니라 각각 "서버 권위 팀 전투"와 "3인칭 액션/스트리밍"을 검증하는 두 시험장이고, 최종 목표 제품이 이 검증된 공통 기반(Engine/asset contract/network primitive) 위에 선다. Elden 첫 슬라이스는 전체 복각이 아니라 작은 필드에서 skinned load / idle-run-attack-dodge / lock-on / streaming boundary 증명으로 범위를 잘랐다.

---

## ⑦ 면접 Q&A

### Q1. "다시 만든다면 무엇을 바꾸겠나?"

**답변 골격**: 세 가지를 처음부터 다르게 한다. (1) **Shared 소유 결정론 ECS를 1일차에** — Engine ECS를 빌려 쓰기 시작한 것이 최대 위반 클러스터가 됐고, 절단에 어댑터 9종 + 79파일 재라우팅 + lint까지 들었다. (2) **경계 lint를 규칙과 동시에** — 규칙을 문서로만 두면 반드시 무너진다는 걸 배웠다. (3) **hp 같은 핵심 상태의 단일 진실을 처음부터** — MirrorHealth 같은 화해 코드는 초기 설계 5분이면 안 생겼을 코드다. 단, 순서는 다시 해도 같게 간다: 렌더 → 씬 → 게임플레이 → 병렬화 → 네트워크 → 아키텍처 정제. 각 단계가 다음 단계의 검증 수단을 만들었기 때문이다.

**꼬리질문 대비**: "그럼 왜 처음에 그렇게 안 했나?" → 몰라서가 아니라 우선순위였다. 초기에 화면에 뭔가 나오는 것이 모든 후속 검증의 전제였고, 경계 부채는 '갚을 수 있는 형태'로 지는 것이 중요했다. 실제로 갚고 있는 중이라는 게 증거다.

### Q2. "지금 가장 큰 기술 부채는 무엇인가?"

**답변 골격**: Shared→Engine 잔여 링크 의존이다. 직접 include는 0건까지 절단했고 lint가 재발을 막지만, `World.h:11`의 `using World = ::CWorld` 한 줄이 남아 결정론 sim이 Engine DLL에 링크로 묶여 있다. 이게 풀려야 EngineSDK/inc include 경로를 제거해 경계를 **컴파일 불가능 수준으로** 강제할 수 있다. 상환 계획은 Shared 소유 ECS 백엔드 신설 후 어댑터 repoint — 어댑터 설계 덕분에 1파일 변경으로 축소돼 있다.

**꼬리질문 대비**: "왜 백엔드까지 한 번에 안 갔나?" → 백엔드 교체는 설계가 필요한 큰 작업이고, include 절단·lint까지는 각각 독립 검증 가능한 슬라이스였다. 검증 불가능한 거대 diff 하나보다 검증 가능한 세 슬라이스가 낫다.

### Q3. "네트워크가 TCP 단일에 full-snapshot이면 스케일이 되나?"

**답변 골격**: 안 된다, 그리고 그걸 정확히 알고 있다. 대역폭은 엔티티수×세션수 선형이고, 송신 큐에 백프레셔가 없고, 랙 보상은 기록만 하고 적용은 rewindTicks=0이다. 이건 MVP의 의도된 절충이다 — 먼저 증명해야 했던 것은 결정론(same input → byte-exact snapshot)과 권위 모델이었고, TCP full-snapshot은 순서 보장·late-join·재접속을 공짜로 줘서 그 검증에 집중하게 해줬다. 다음 단계는 순서까지 정해져 있다: UDP 데이터면 분리 → 델타/AOI → 랙 보상 정책 결정 후 적용.

**꼬리질문 대비**: "랙 보상은 인프라가 있는데 왜 안 켰나?" → 켜는 건 한 줄이지만, 켜면 "쏜 사람 화면 기준" 판정이 되면서 맞은 사람의 엄폐-뒤-피격이 생긴다. 이건 기술 문제가 아니라 게임 공정성 정책이라, 근거 없이 켜는 게 더 나쁜 결정이라고 판단했다.

### Q4. "챔피언이 150개가 되면 이 코드가 감당되나?"

**답변 골격**: 감당되게 만드는 구조가 이미 절반 이상 들어가 있다. 챔피언 로직은 self-register 훅(중앙 테이블 수정 없음), id는 명시값 enum(와이어 안정), 수치는 JSON→cook pack(코드 수정 없음). 현재 15개 챔피언이 이 구조로 이관돼 있다. 남은 병목 두 개도 알고 있다: CommandExecutor의 특수케이스 잔재(pre-cast 훅 변형으로 D-1 이관)와 Scene_InGame 갓헤더의 챔피언별 상태(등록 콜백 레지스트리로 탈출). 목표는 챔피언 추가가 "폴더 하나 + 파일 몇 개 + 등록"으로 끝나 기존 파일 diff가 0에 수렴하는 것이다.

**꼬리질문 대비**: "데이터 분리율 28%면 낮지 않나?" → 정직한 숫자다. 단 가중치를 보면 밸런싱 빈도가 가장 높은 스탯/스킬 수치는 이미 1.0(JSON 편집→재cook)이고, 0점 도메인은 웨이브 구성·아이템처럼 변경 빈도가 낮은 것들이다. 그리고 28%라는 숫자가 있다는 것 자체가 관리의 증거다 — 측정하지 않는 부채는 갚을 수도 없다.

### Q5. "부채가 다시 쌓이지 않는다는 보장은?"

**답변 골격**: 사람의 규율에 의존하지 않는 장치가 세 겹이다. (1) PreBuild lint — Shared의 경계 위반 include는 빌드가 실패한다. (2) 폴백 카운터 — pack miss가 legacy 값으로 조용히 덮이는 대신 `[Data] pack miss` 로그로 드러나고, 이 카운터가 legacy 삭제의 게이트다. (3) 빌드 해시 핸드셰이크 — 클라/서버 데이터 불일치가 접속 로그로 보인다. 문서 쪽은 "code wins over docs"와 스테일 위반 재인용 금지 규칙으로 문서 자체의 부패를 관리한다.

**꼬리질문 대비**: "그래도 lint가 못 잡는 위반은?" → 있다. Engine 어휘의 제품 종속(StatusPanelState의 "Baron" 등) 같은 그레이존은 기계로 못 잡아서 실측 문서의 ⚠️ 항목으로 추적한다. 기계 강제가 안 되는 것과 안 하는 것을 구분해서 관리한다.

### Q6. "JobSystem이 있는데 서버는 왜 아직 단일 스레드인가?"

**답변 골격**: 서버 병렬화의 합격 기준이 클라보다 훨씬 엄격하기 때문이다 — 성능이 올라도 same input → byte-exact snapshot이 깨지면 리플레이·검증·(미래의) 학습 데이터가 전부 무너진다. 클라 쪽에서 Chase-Lev deque의 single-owner 규약 위반 race 같은 사고를 이미 겪어봤고, 그 경험 위에서 서버는 IOCP completion을 Tick fiber가 소비하는 구조(워커는 파이버화하지 않음)로 설계만 마친 상태다. FiberShell 실행 모드가 이미 JobSystem에 들어가 있어서, public API 불변인 채 내부만 교체하는 경로가 준비돼 있다.

**꼬리질문 대비**: "결정론과 병렬성은 어떻게 양립하나?" → 스테이지 간 의존 그래프를 고정하고(Stat∥Cool→Buff→Move→Damage→Death), 스테이지 내부만 병렬화하며, cross-entity write는 per-worker buffer + 메인 flush로 몰아 write 순서를 결정화한다. 클라 미니언 AI의 Decision/Apply 2-pass에서 이미 검증한 패턴이다.

### Q7. "상용 엔진(UE) 기능 중 뭘 가져오고 뭘 안 가져올지 어떻게 정했나?"

**답변 골격**: 기준은 "우리 불변식과 충돌하는가"다. 가져오는 것은 툴이 아니라 규율 — 자산은 검증된 런타임 계약(.w* + Validator)으로만 게임에 들어간다는 파이프라인. 거부한 것은 Blueprint 런타임 VM(결정론 tick에 인터프리터를 넣으면 SimLab 해시 검증이 무너짐 → pack+훅 레지스트리+UI 한정 Lua로 대체)과 UObject/GC/리플렉션(trivially-copyable 컴포넌트 + 스냅샷 복제와 충돌). Fab 애님팩은 런타임 IK 대신 cook 시점 리타겟을 선택했다 — 역시 결정론/성능 원칙이 기준.

**꼬리질문 대비**: "Blueprint 없이 기획자가 로직을 만지려면?" → 수치는 JSON, 분기는 개발자가 훅으로. 완전한 비주얼 스크립팅 대신 "값의 소유권"을 기획자에게 넘기는 것이 서버 권위 게임에서 현실적인 절충이라고 판단했다.

### Q8. "미완성/스텁 코드가 남아 있는 건 문제 아닌가?"

**답변 골격**: 미완성 자체가 아니라 **경계 없는 미완성**이 문제라고 생각한다. Winters의 스텁은 세 가지 규율을 따른다. (1) 경계를 코드로 못박는다 — 스킨드 메시는 RHI 스냅샷 경로에서 조기 return + 이유 주석. (2) 지뢰밭 목록에 등재한다 — CServerEntry 스텁, suspicion 미집행, ODR 함정 스텁 전부 호출자 수까지 문서화. (3) "따라 하지 마라"를 명시한다 — 잔여 침묵 지점 목록의 첫 줄이 그 경고다. 해소되면 취소선으로 추적한다.

**꼬리질문 대비**: "그 목록에서 최근에 갚은 건?" → Pathfinder 빈 경로 원인 무구분(5값 result enum으로 해소)과 CHttpClient 가짜 async(future 소유 + 스냅샷 복사 + 소멸자 드레인으로 해소). 둘 다 2026-07-09 슬라이스에서 취소선 처리됐다.

### Q9. "부채를 이렇게까지 문서화하는 게 과한 투자 아닌가?"

**답변 골격**: 혼자 개발이라면 과할 수 있다. 하지만 이 프로젝트의 목표 제약이 "수십 명이 동시 작업해도 견디는 구조"이고, 그 관점에서 부채 문서화는 커뮤니케이션 비용의 선불이다. 실제 효과를 봤다 — 감사에서 발견한 65건을 high 18건으로 우선순위화해 11파일 슬라이스로 갚았고, 리팩터 전후 SimLab 300틱 해시가 동일함을 증명해 "동작 무변경 리팩터"를 리뷰 없이도 신뢰 가능하게 만들었다. 부채를 아는 것과 모르는 것의 차이는, 갚을 계획을 세울 수 있느냐의 차이다. 시니어의 코드에도 부채는 있다 — 차이는 부채의 존재가 아니라 부채의 **가시성과 상환 계획**이라고 생각한다.

**꼬리질문 대비**: "그 문서들이 코드와 어긋나면?" → 그래서 실측 문서에는 작성일·커밋 기준·"재검증 후 인용" 규칙이 붙어 있고, 최종 심판은 항상 코드다(code wins over docs).

---

## 다른 챕터와의 연결

- **레이어 경계·의존성 규칙** (엔진 세트의 아키텍처 경계 챕터): 이 챕터의 부채 1번(Shared→Engine 절단)의 "왜 그 경계가 필요한가"는 5계층 소유권 모델(gameplay truth vs presentation)이 근거다. 여기서는 상환 계획만 다뤘다.
- **서버 권위 네트워킹** (엔진 세트의 네트워크 챕터): 부채 3·4·5번(UDP/델타/랙보상)의 현재 동작 상세 — ingress 게이트, Move 코얼레싱, 결정론 정렬, 스냅샷 브로드캐스트 — 는 그쪽이 소유한다. 와이어 포맷/enum 설계는 `.md/interview/cpp/12_network_serialization.md`.
- **ECS·JobSystem** (엔진 세트의 병렬화 챕터 + `.md/interview/cpp/11_architecture_ecs.md`): 부채 2번(서버 병렬화)의 전제인 Chase-Lev deque, Worker-Safety 정책, Decision/Apply 2-pass의 상세.
- **에러 핸들링 철학** (`.md/interview/cpp/10_error_handling.md`): 이 챕터의 "침묵하는 부채" 절은 P1~P4 원칙(NYPC 회고 기원)과 dead diagnostics 금지 정책의 응용이다.
- **데이터 아키텍처** (엔진 세트의 데이터 파이프라인 챕터): 스코어카드 28%의 도메인별 상세, 3소유자 분할(SharedContract/ServerPrivate/ClientPublic), cook 파이프라인은 그쪽이 소유한다. 여기서는 zero-reader 삭제 게이트만 다뤘다.
- **렌더러/RHI 이관** (엔진 세트의 렌더링 챕터): 부채 9번(스킨드 RHI)의 배경인 DX11 legacy / RenderWorldSnapshot 병행 구조와 백엔드 가드 전략.

**이 챕터의 마무리 문장으로 외워둘 것**: "완성된 엔진을 보여드리는 게 아니라, 어디가 미완성인지 정확히 알고 갚는 순서까지 정해 둔 엔진을 보여드리는 겁니다. 부채 목록이 없는 프로젝트는 부채가 없는 게 아니라 측정을 안 한 것입니다."
