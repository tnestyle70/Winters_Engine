# 12. 구조적 에러 처리 · 복원력 설계 (Error Handling as Structure)

> 면접 대본 겸 지식 베이스. 코드 문법(예외 메커니즘, noexcept 등)은 `.md/interview/cpp/10_error_handling.md`가 담당하고,
> 이 챕터는 "왜 이 구조로 에러를 다루기로 결정했는가"를 다룬다.

---

## ① 한 줄 정의

"저는 에러 처리를 개별 함수의 방어 코드가 아니라 **엔진의 구조 그 자체**로 설계했습니다.
신뢰 경계(네트워크·파일·데이터 팩)에서 검증하고, 모든 실패는 발생 지점에서 bounded trace로 가시화하고,
폴백은 로그가 붙은 명시적 사다리로 중앙화하고, C++ 예외 대신 반환값 모델을 전 계층에 통일했습니다.
그리고 이 규칙들이 시간이 지나도 안 무너지게 문서가 아니라 **빌드 게이트와 컴파일 에러로 강제**했습니다."

이 원칙들의 기원은 추상론이 아니다. NYPC 봇 대회 회고 3줄 —
(1) 디버깅이 수월한 구조가 이긴다, (2) 예외 처리가 다른 구조를 건드리면 안 된다,
(3) 1등 봇의 코드가 길었던 이유는 모든 특수상황을 전부 코드로 명시했기 때문 —
에서 4원칙을 뽑아 엔진 전 계층에 박제했다 (`.md/architecture/WINTERS_DESIGN_PHILOSOPHY.md`):

- **P1** 실패는 발생 지점에서 즉시, 가시적으로 (bare `return;`/`return nullptr;` 금지)
- **P2** 한 구조의 실패가 다른 구조를 오염시키지 않는다 (의존성 방향 + 스레드 경계 = 오염 경계)
- **P3** 모든 특수상황은 코드에 명시한다 (실패 원인이 여럿이면 원인을 구분)
- **P4** 디버깅이 수월한 구조를 먼저 만든다 (증상 튜닝 전에 관측 장치부터)

---

## ② 구조와 데이터 흐름 — 신뢰 경계와 진단 채널

에러 처리를 "구조"로 본다는 건, 코드베이스 전체를 **신뢰 경계(trust boundary)**로 나누고
각 경계마다 (a) 무엇을 검증하는지, (b) 실패가 어느 채널로 보고되는지를 고정한다는 뜻이다.

```
[외부 입력]            [신뢰 경계 검증]                       [실패 채널]
 클라 패킷 ──────▶ PacketDispatcher: FlatBuffers Verify ──▶ FlagSuspicious (silent drop 금지)
 서버 패킷 ──────▶ SnapshotApplier/EventApplier Verify ──▶ bounded(8) OutputDebugStringA
 cooked asset ──▶ CBinaryReader (유일한 throw 경계) ─────▶ 로더가 catch → false 변환
 텍스처 파일 ────▶ RHITextureLoader 단계별 HRESULT ──────▶ LogTextureLoadFailure(stage,path,hr)
 데이터 팩 ──────▶ Hello dataBuildHash 대조 ─────────────▶ [Data] build hash mismatch (bounded 8)
 pack 조회 miss ─▶ GameplayDefinitionQuery 3계층 폴백 ───▶ [Data] pack miss → legacy (bounded 32)
                                                            = legacy DB 삭제 게이트 카운터
```

진단 채널은 계층별로 다르다. 이건 취향이 아니라 함정 때문이다 (아래 ③-4):

| 계층 | 반환 컨벤션 | 실패 로그 채널 |
|---|---|---|
| Engine 팩토리 | `Create()` → nullptr, 초기화 HRESULT/bool | bounded `OutputDebugStringA` (Debug 구성 게이트=1) |
| RHI | 핸들 + `IsValid()`, 실패는 빈 핸들 `{}` | HRESULT+debugName 로그 후 빈 핸들 |
| Server 전송 | bool | `std::cerr` + WSA 에러 코드 (콘솔 앱) |
| Shared sim | bool / NULL_ENTITY | `WintersOutputAIDebugStringA` (SimDebugOutput.h) |
| Client 게임플레이 | bool / nullptr | bounded `OutputDebugStringA` |

출처: `.md/architecture/WINTERS_ERROR_HANDLING_POLICY.md` §0, §1.7.

경계별 필수 처리는 초크포인트 표로 고정돼 있다 (같은 문서 §2): RHI 리소스 생성 실패 → HRESULT+debugName,
씬 전환 OnEnter 실패 → 로그+E_FAIL, 네트워크 verify 실패 → bounded 로그+drop, 서버 send 실패 → WSA 코드+세션 단절,
FX cue 미스 → bounded 로그(침묵 no-op 금지) 등. 실제 구현 확인:

- `Engine/Private/Scene/Scene_Manager.cpp:30-36` — OnEnter 실패 시 sceneID 로그 + E_FAIL (과거엔 반환값 무시하고 무조건 S_OK였다)
- `Engine/Private/RHI/RHITextureLoader.cpp:66-130` — WIC 디코딩 7단계(CoCreateInstance/CreateDecoder/GetFrame/GetSize/FormatConverter/Initialize/CopyPixels) 각각에 `LogTextureLoadFailure(stage, path, hr)`
- `Client/Private/GameObject/FX/FxCuePlayer.cpp:102,113` — `LogMissingCue`/`LogSkippedCueEmitter`
- `Server/Private/Network/PacketDispatcher.cpp:74-77,104-107` — `VerifyCommandBatchBuffer`/`VerifyLobbyCommandBuffer` 실패 → `FlagSuspicious()`

---

## ③ 핵심 설계 결정과 트레이드오프

### 1. 예외 없는 에러 모델 — 반환값 기반 + 단일 throw 경계

- **왜**: 서버는 30Hz 결정론 tick 위에서 스냅샷을 복제한다. 예외 언와인딩이 tick 중간에 상태를
  반쯤 수정한 채 튀어나가면 "한 구조의 실패가 다른 구조를 오염시키지 않는다"(P2)가 깨진다.
  또 Engine이 DLL 경계라 예외를 경계 너머로 던지는 것 자체가 ABI 지뢰다.
- **대안**: (a) 전면 예외 사용, (b) `std::expected` 스타일 결과 타입 전면 도입.
- **선택**: 반환값 기반(bool/HRESULT/nullptr/핸들 IsValid)을 기본으로 통일하고,
  **유일한 throw 경계**를 cooked asset 파싱의 `CBinaryReader`(std::runtime_error)에만 허용했다.
  바이너리 파싱은 "어느 오프셋에서든 bounds 초과가 날 수 있는" 코드라 매 Read마다 bool 체크를 강제하면
  로더가 체크 코드로 덮이기 때문이다. 이 예외는 소비하는 로더가 catch해서 false로 변환하므로
  **경계 밖에서는 예외가 존재하지 않는 것과 같다**. 새 throw 경계 추가는 compass 결정 사항으로 잠갔다.
- **감수한 비용**: 반환값 체크를 까먹으면 컴파일러가 못 잡는다. 그래서 "실패 분기에 P1 진단이 없으면
  리뷰 반려"라는 사람 규약과, 경계별 초크포인트 표로 보완했다.

### 2. Silent fail 금지 — 실패 원인의 enum 승격 (P3)

- **왜**: 미니언 stuck 사고(2026-04-28)에서 배웠다. `Find_Path`가 빈 vector 하나로 실패를 뭉개니
  Chase 상태 미니언이 제자리 애니로 멈췄는데, 그게 "길이 없는" 건지 "시작 셀이 막힌" 건지 구분할 방법이 없었다.
- **대안**: 호출부마다 개별 로그를 심기 (산탄식).
- **선택**: 실패 원인을 인터페이스에 박았다. `Engine/Public/Manager/Navigation/Pathfinder.h:13`의
  `ePathFindResult { Success, NullGrid, StartBlocked, GoalBlocked, NoRoute, BrokenPath }` out-param.
  헤더 주석에 "과거 minion-stuck 사고의 silent empty path 대책"이라고 계보까지 명시했다.
  같은 원리로 스키마 드리프트도 잡는다: FlatBuffers verify 실패를 bare return으로 삼키면
  **스키마 드리프트가 네트워크 stall과 같은 증상(조용한 월드 프리즈)이 된다**. 그래서 복제 경계의
  verify 실패는 bounded trace가 필수다 — "패킷이 안 옴"과 "패킷이 거부됨"을 로그로 갈라야 한다(P4).
- **감수한 비용**: 시그니처에 out-param이 늘고, 호출자가 원인을 무시할 수는 있다(기본값 nullptr).
  대신 Karpathy 가드레일과 경계를 그었다 — **외부 입력(네트워크/파일/유저/에셋)은 특수상황 분기,
  내부 불변식은 assert**. 모든 함수에 result enum을 붙이는 과잉 방어는 하지 않는다.

### 3. Dead diagnostics 금지 + bounded trace 기본

- **왜**: 2026-07-09 전수 감사에서 스냅샷 적용 핵심 경로 SnapshotApplier(1,652줄)에
  OutputDebugString 호출이 0개, sprintf_s로 포맷한 진단 8곳이 전부 출력 없이 버려지고 있었다.
  포맷만 하고 방출하지 않는 진단은 로그가 없는 것보다 나쁘다 — 디버깅하는 사람이
  "트레이스가 있다"고 착각하게 만들기 때문이다 (`WINTERS_DESIGN_PHILOSOPHY.md` P1 근거 사례).
- **대안**: (a) 로깅 프레임워크(spdlog 등) 도입 + 레벨 필터, (b) 무제한 로깅.
- **선택**: 함수-로컬 static 카운터로 상한을 둔 bounded trace를 기본으로 규약화했다.
  상한은 실패류 8 / 컨텐츠 miss류 64 / 계측류 512 (`WINTERS_ERROR_HANDLING_POLICY.md` §1.2).
  추가 규칙 두 개가 사고에서 나왔다:
  - **성공/실패가 같은 카운터를 공유하면 안 된다** — CommandExecutor cast 로그가 64캡 하나를
    성공 로그가 소진해 실패가 안 보였던 사고.
  - **성공 로그는 반환값 확인 후에만** — `Load_X(); Log("X loaded")`는 거짓말이다.
  그리고 루틴 트레이스와 실패 진단을 구분했다: 루틴(틱/스냅샷/yaw 정상 흐름)은 리팩터 중 추가 금지·사용 후 제거,
  실패 진단은 영구 보존. 이 구분이 "로그 좀 지워라"와 "실패는 보여야 한다"를 양립시키는 운영 규칙이다.
- **감수한 비용**: 카운터가 상한에 닿으면 그 이후 실패는 다시 침묵한다. 장기 실행 서버에는 부족하고,
  카운터 리셋/집계 텔레메트리가 로드맵이다.

### 4. 디버그 출력 게이트 — 계층마다 채널이 다른 이유

- **왜**: `Engine_Defines.h`가 `OutputDebugStringA`를 게이트 래퍼(`WINTERS_ENABLE_NON_AI_DEBUG_STRING`,
  기본 0=무음)로 **매크로 재매핑**한다. Engine/Client Debug 구성만 게이트=1을 정의하고
  Server/GameSim은 정의하지 않는다. 결과: 무심코 raw `OutputDebugStringA`를 Server 코드에 쓰면
  로그가 **조용히 사라진다**. 진단이 도착하지 않는 것은 dead diagnostics만큼 위험하다.
- **대안**: 게이트를 전 계층에서 켜기 (루틴 트레이스 스팸 부활), 매크로 재매핑 제거 (레거시 호출부 전수 수정).
- **선택**: 채널을 계층별로 분리 명문화했다 — Server 실패는 `std::cerr`+WSA 코드,
  Shared sim 진단은 `Shared/GameSim/Core/Debug/SimDebugOutput.h`의 `WintersOutputAIDebugStringA`(AI 게이트 기본 1).
  SimDebugOutput.h는 Engine_Defines.h와 **동일한 가드(`WINTERS_DEBUG_STRING_GATE_DEFINED`)를 공유**해서
  두 헤더가 한 TU에서 만나도 먼저 온 쪽 정의가 쓰이게 했고, Engine_Defines의 `<dinput.h>`/`using namespace`
  오염을 Shared TU로 끌어오지 않는 최소 정의만 담았다. 실패 진단을 no-op 스텁 채널
  (`Winters::DevSmoke::Log` — 컴파일되는 빈 함수)에 넣는 것도 금지다.
- **감수한 비용**: "어느 계층에서 어떤 함수를 쓰는가"를 외워야 하는 인지 비용. gotcha로 박제해 상쇄.

### 5. 폴백은 침묵하지 않는 명시적 사다리 + 카탈로그 중앙화 (P2)

- **왜**: "실패했지만 계속 진행"이 침묵하면 데이터 오구성이 정상처럼 보인다.
  폴백이 제2의 truth가 되는 순간 원본 데이터가 썩어도 아무도 모른다.
- **대안**: 폴백 없이 fail-fast (에셋 하나 깨지면 게임이 안 뜸 — 개발 속도 저하),
  호출부마다 흩어진 ad-hoc 폴백 (리뷰 불가).
- **선택**: 두 패턴으로 고정했다.
  - **명시적 폴백 사다리**: 서버 내비 초기화가 모범이다. authored navgrid 로드 →
    (실패/커버리지 거부 시) terrain bake → (wmesh 실패 시) structures-only →
    (bake 실패 시) all-walkable 4단계로 내려가며 **각 단계가 `[ServerNav]` 태그로 로그**된다
    (`Server/Private/Game/GameRoomNav.cpp:161,274,320` — 최종 그리드 hash까지 출력).
  - **폴백 가시화 카운터 = 삭제 게이트**: 데이터 조회는 팩 miss 시 legacy 하드코딩 DB로 폴백하는데,
    모든 폴백 진입점에 `[Data] pack miss -> legacy ...` bounded(32) 카운터를 심었다
    (`Shared/GameSim/Definitions/GameplayDefinitionQuery.cpp:45-61`). 주석 그대로 —
    "**이 카운터를 0으로 만드는 것이 legacy DB 삭제의 게이트다**". 정상 로스터 스모크에서
    이 로그가 0으로 수렴한 조회 경로부터 legacy 리더를 제거한다 (zero-reader 규칙).
  - FX도 같다: cue 재생 실패는 `LogMissingCue` bounded 로그 + caller가 generic 빌보드 폴백을 태운다.
    폴백/FX 라우팅을 일회성 호출부에 흩어놓지 않고 이름 있는 카탈로그/헬퍼로 모은다 (gotcha 2026-05-26).
- **감수한 비용**: all-walkable까지 내려간 서버는 "동작하지만 이상한" 상태로 게임을 진행한다.
  대신 로그가 남으므로 QA/스모크에서 반드시 걸린다는 것에 베팅했다.

### 6. 규칙의 기계 강제 — 문서는 무너지고 빌드 게이트는 안 무너진다

- **왜**: "하지 마라"를 문서에 적어도 3개월 뒤의 나(또는 팀원)는 어긴다.
- **선택**: 컴파일러가 표현할 수 있는 규칙은 컴파일 에러로, 못 하는 규칙은 빌드 lint로 강제했다.
  - sim 컴포넌트 POD 강제: `static_assert(std::is_trivially_copyable_v<...>)`
    (`Shared/GameSim/Components/CombatActionComponent.h`) — memcpy 스냅샷/결정론 전제를 컴파일 에러로.
  - 상수버퍼 정렬: `static_assert(sizeof(T)%16==0)` (`Engine/Private/RHI/DX11/DX11ConstantBuffer.h`) —
    조용한 셰이더 데이터 깨짐을 컴파일 타임으로 승격.
  - include 방향 규칙(Shared는 Engine/DX/ImGui를 모른다)은 컴파일러가 못 잡는다(include하면 그냥 컴파일됨).
    그래서 `Tools/Harness/Check-SharedBoundary.ps1`이 GameSim PreBuild에서 Shared 전 파일을 정규식 스캔해
    금지 include 발견 시 **빌드를 exit 1로 실패**시킨다. Phase 7F 어댑터 경로만 화이트리스트.
- **감수한 비용**: 텍스트 lint는 정규식이라 우회 가능하고(매크로 간접 include 등), PreBuild 시간이 늘어난다.
  완전성보다 "회귀를 리뷰어 눈이 아니라 빌드가 잡는다"는 방향성을 샀다.

---

## ④ 어려웠던 점과 해결

### (1) CHttpClient 가짜 async — future 폐기가 만든 동기 블로킹

`std::async(launch::async, ...)`의 반환 future를 버리면 그 임시 future의 소멸자가 완료를 기다려
호출이 **사실상 동기 블로킹**이 된다. CHttpClient의 AsyncGet/AsyncPost가 이 패턴이었다.
더 미묘한 건 — 역설적으로 그 우연한 블로킹이 raw `this` 캡처를 안전하게 만들던 **유일한 이유**였다는 것.
진짜 비동기로 "고치기만" 하면 use-after-free가 열린다. 그래서 한 변경에서 세트로 재설계했다:
future를 `m_PendingRequests`(vector)가 소유하고, worker는 호출 시점 `RequestSnapshot` 값 복사본만 읽어
SetAuthToken과의 race를 차단하고, 소멸자가 전량 드레인해 블로킹을 파괴 시점으로 한정했다
(`Client/Public/Network/Backend/CHttpClient.h:23-24` 주석이 이 계약을 명시). 교훈은 gotcha로 박제:
**async 래퍼는 future의 수명을 소유해야 하고, raw this 캡처는 같은 변경에서 고쳐야 한다.**

### (2) Dead diagnostics 전수 감사

2026-07-09 감사(발견 65건, high 18건 확정)에서 SnapshotApplier 1,652줄에 로그 0개·sprintf_s 8곳
전량 미출력을 적발했고, 이걸 개별 수리가 아니라 **정책**으로 만들었다: 포맷-후-폐기는 리뷰 반려,
실패 경로 트레이스는 bounded emit 아니면 삭제. 같은 감사 계열에서 Scene_Manager::Change_Scene의
"OnEnter 반환값 무시하고 무조건 S_OK" 경로와 RHITextureLoader의 bare `return {}` 침묵 실패를
fail-fast + 단계별 진단으로 전환했다.

### (3) 로그가 계층에 따라 조용히 증발하는 함정

Server에서 실패 로그를 찍었는데 안 나왔다. 원인은 코드가 아니라 매크로 — Engine_Defines.h의
게이트 재매핑이 Server 구성에서 무음이었다. "로그를 추가했다"와 "로그가 도착한다"는 다른 문제라는 걸
배웠고, 계층별 채널 규약(③-4)과 SimDebugOutput.h 분리 헤더로 해소했다.

### (4) 성공 로그가 실패 로그를 은폐한 카운터 공유 사고

CommandExecutor의 cast 로그 64캡을 성공 케이스가 소진해서, 정작 실패가 났을 때는 카운터가 이미 바닥나
아무것도 안 보였다. "bounded는 좋은데 예산을 누구와 나누는가"가 규칙이 됐다 — 성공/실패 카운터 분리.

---

## ⑤ 향후 개선 방향

정책 문서 §4가 "알려진 잔여 침묵 지점"을 **해소/보류 상태로 정직하게 목록화**하고 있다.
"여기 적힌 것은 고쳐야 할 것이지 따라 해도 되는 패턴이 아니다"라는 경고와 함께 —
후속 작업자가 기존 코드를 모범으로 착각하고 모방하는 것을 막기 위해서다.

- 해소됨(취소선 처리): Pathfinder 빈 경로 원인 무구분 → `ePathFindResult`, CHttpClient 가짜 async → future 소유.
- 보류 중: CommandExecutor의 무효 issuer/unknown command kind 침묵 drop,
  PacketDispatcher의 malformed frame 무로그 단절, **suspicion 카운터는 기록만 되고 enforcement 0**
  (IsSuspicious 호출자 0 — 악성 패킷이 드랍만 되고 킥이 없다),
  SnapshotApplier의 yaw 계측 블록 2곳(여전히 포맷-후-폐기 상태로 보존 — 재무장 or 삭제 결정 필요),
  FMOD 실패 무로그, Loader Preload_* 결과 폐기(로딩 화면이 성공을 가장).
- 구조 차원 로드맵: bounded 카운터를 넘어서는 집계형 텔레메트리(카운터 값 자체를 스냅샷/리포트로 노출),
  크래시 경계(minidump)와 CI에서의 하네스 상시 실행 — "규칙은 있는데 기계 강제가 없는" 남은 지점들을
  줄이는 방향이다.

---

## ⑥ 면접 Q&A

**Q1. 에러 처리를 어떻게 설계했나요? (완성 답안)**
- 골격: "네 겹으로 설계했습니다. (1) **모델** — 예외 대신 반환값 기반, 유일한 throw 경계는
  바이너리 파서 하나뿐이고 로더가 흡수합니다. (2) **가시성** — 모든 실패 exit에 bounded trace,
  dead diagnostics(포맷-후-폐기) 금지, 성공 로그는 반환값 확인 후에만. (3) **격리** — 스레드 경계와
  의존성 방향이 오염 경계입니다. 세션 실패는 세션만 끊고 룸 truth는 안 건드립니다.
  폴백은 각 단계가 로그되는 명시적 사다리로만 허용합니다. (4) **강제** — static_assert와
  PreBuild lint로 규칙을 빌드 실패로 승격했습니다. 이 원칙은 봇 대회에서 '디버깅 수월한 구조가 이긴다'는
  실전 경험에서 나왔고, 전수 감사로 65건을 적발해 정책 문서로 박제했습니다."
- 꼬리질문 대비: "감사에서 뭘 발견했나" → SnapshotApplier 1,652줄 로그 0개/sprintf_s 8곳 미출력 사례.

**Q2. 왜 예외를 안 썼나요?**
- 골격: "30Hz 결정론 tick과 스냅샷 복제 모델에서, 예외 언와인딩이 tick 중간에 상태를 반쯤 바꾼 채
  탈출하면 실패 격리(P2)가 깨집니다. DLL 경계 너머로 예외를 던지는 ABI 문제도 있고요.
  다만 전면 금지의 비용도 압니다 — 바이너리 파싱처럼 모든 Read가 실패할 수 있는 코드는
  bool 체크 지옥이 되므로, CBinaryReader 한 곳만 throw를 허용하고 로더가 catch해 false로 변환합니다.
  경계 밖에서는 예외가 존재하지 않는 것과 같습니다."
- 꼬리질문: "std::expected는?" → 반환값+진단 규약으로 같은 효과를 얻고 있고, 도입한다면 신뢰 경계의
  result 구조체(FxAssetLoadResult 같은 기존 패턴)부터 수렴시키는 게 순서라고 답한다.

**Q3. silent fail이 왜 위험한지 실제 사고로 설명해줄 수 있나요?**
- 골격: "미니언이 적을 쫓다 제자리에서 걷는 stuck 사고가 있었습니다. 원인 중 하나가 Pathfinder의
  빈 경로 반환 — '길 없음/시작 막힘/목표 막힘/그리드 없음' 네 원인이 빈 vector 하나로 뭉개져서
  진단이 불가능했습니다. 해결로 `ePathFindResult` enum을 out-param으로 추가해 실패 원인을
  인터페이스에 명시했고(P3), 헤더 주석에 사고 계보까지 남겼습니다. 실패를 값으로 승격하면
  디버깅뿐 아니라 나중에 AI/학습을 붙일 때도 transition이 결정적이 됩니다."
- 꼬리질문: "모든 함수에 그렇게 하나?" → 아니다. 외부 입력은 특수상황 분기, 내부 불변식은 assert.

**Q4. 네트워크 패킷 검증 실패는 어떻게 처리하나요?**
- 골격: "FlatBuffers verify 실패를 bare return으로 삼키면 스키마 드리프트가 네트워크 stall과
  같은 증상 — 조용한 월드 프리즈 — 이 됩니다. 그래서 '패킷이 안 옴'과 '패킷이 거부됨'을 반드시
  구분합니다. 서버는 verify 실패 시 세션 의심도를 올리고(FlagSuspicious), 클라는 복제 경계
  (Hello/Snapshot/Event) 전부에 bounded(8) 실패 트레이스를 둡니다. 추가로 Hello에 데이터 빌드 해시를
  실어 서버/클라가 서로 다른 생성 데이터로 빌드된 drift를 접속 시점에 로그로 드러냅니다."
- 꼬리질문: "의심도 이후 조치는?" → 정직하게: 현재 기록만 되고 enforcement가 없다(§4 보류 목록).
  킥/차단 정책은 로드맵.

**Q5. 로그 스팸은 어떻게 막았나요?**
- 골격: "두 축입니다. 첫째, 루틴 트레이스와 실패 진단을 구분합니다 — 루틴은 게이트 뒤나 사용 후 제거,
  실패 진단은 영구 보존. 둘째, bounded가 기본입니다 — static 카운터로 실패류 8/miss류 64/계측류 512 상한.
  단 성공과 실패가 카운터를 공유하면 성공이 예산을 소진해 실패를 은폐합니다 — 실제로 겪었고,
  카운터 분리를 규칙으로 만들었습니다."
- 꼬리질문: "상한 이후엔?" → 다시 침묵하는 한계를 인정하고, 집계 텔레메트리가 다음 단계라고 답한다.

**Q6. 초기화가 실패하면 어떻게 되나요? (폴백 설계)**
- 골격: "'실패했지만 계속 진행'은 로그가 붙은 의도된 폴백일 때만 허용합니다. 서버 내비 초기화가
  모범 사례인데, authored navgrid → terrain bake → structures-only → all-walkable 4단계 사다리를
  내려가며 각 단계가 [ServerNav] 태그와 최종 hash까지 로그합니다. 데이터 조회 폴백은 한 발 더 나가서,
  폴백 진입 카운터를 legacy 코드 **삭제 게이트**로 씁니다 — 스모크에서 pack miss 로그가 0으로 수렴한
  경로부터 legacy DB 리더를 제거합니다. 폴백이 제2의 truth로 굳는 걸 막는 장치입니다."
- 꼬리질문: "부분 초기화 실패는?" → 만든 것을 정리하고 실패를 반환하거나 명시적 degraded 모드+로그
  (RHISceneRenderer::EnsureDrawSlots가 부분 생성물 파괴 후 false 반환).

**Q7. 로그를 추가했는데 안 찍히는 경험이 있나요?**
- 골격: "있습니다. Engine_Defines.h가 OutputDebugStringA를 게이트 래퍼로 매크로 재매핑하는데,
  게이트가 Engine/Client Debug 구성에서만 켜지고 Server/GameSim에선 꺼져 있어 Server 로그가
  조용히 증발했습니다. 해결은 채널 분리 규약 — Server 실패는 std::cerr+WSA 코드,
  Shared sim은 전용 헤더(SimDebugOutput.h)의 AI 게이트 함수. 이 헤더는 Engine_Defines와 같은
  include 가드를 공유해 충돌 없이 공존하면서 Windows 헤더 오염은 안 끌어옵니다.
  '로그를 추가했다'와 '로그가 도착한다'가 다른 문제라는 게 교훈입니다."

**Q8. 비동기 코드에서 겪은 가장 미묘한 버그는요?**
- 골격: "std::async의 반환 future를 버리면 임시 future의 소멸자가 join해서 사실상 동기 블로킹이
  됩니다. HTTP 클라이언트가 그 패턴이었는데, 진짜 함정은 그 우연한 블로킹이 raw this 캡처를
  안전하게 만들던 유일한 이유였다는 겁니다. 블로킹만 고치면 UAF가 열리는 결합이라, 한 변경에서
  future 소유(m_PendingRequests) + 호출 시점 상태 스냅샷 복사(race 차단) + 소멸자 전량 드레인
  (블로킹을 파괴 시점으로 한정)으로 세트 재설계했습니다."
- 꼬리질문: "그 계약을 깨면?" → future를 detach하면 UAF가 열리고, 소유자 리셋은 최악의 경우
  WinHTTP 타임아웃만큼 대기할 수 있다 — 계약을 헤더 주석으로 명시했다.

**Q9. 아키텍처/에러 규칙이 시간이 지나며 무너지는 건 어떻게 막나요?**
- 골격: "문서는 무너진다고 전제하고 세 단계로 강제합니다. 컴파일러가 잡을 수 있으면 static_assert
  (sim 컴포넌트 trivially_copyable, 상수버퍼 16바이트 정렬), 컴파일러가 못 잡으면 PreBuild lint
  (Check-SharedBoundary.ps1 — 금지 include 발견 시 빌드 실패), 그것도 안 되면 리뷰 반려 규칙
  ('실패를 관측할 수 없는 변경은 완성이 아니다'). 그리고 남은 부채는 숨기지 않고 정책 문서 §4에
  '이건 모범이 아니라 고칠 것' 경고와 함께 해소/보류 상태로 목록화합니다."

**Q10. 기술부채를 어떻게 관리하나요?**
- 골격: "감사로 발견한 침묵 실패 지점을 전부 고치지 않고, 해소한 것(취소선)과 보류한 것을
  한 목록에서 추적합니다. 보류 항목에는 '따라 해도 되는 패턴이 아니다'라는 경고를 붙여
  후속 작업자의 잘못된 모방을 막습니다. suspicion enforcement 부재, Preload 결과 폐기처럼
  '알지만 이번 슬라이스가 아닌 것'을 정직하게 남기는 게, 모르는 부채보다 훨씬 쌉니다."

---

## 다른 챕터와의 연결

- **문법 레이어**: 예외 메커니즘·noexcept·RAII와 에러 전파의 언어적 기반은
  `.md/interview/cpp/10_error_handling.md`, future/async 수명과 race는 `.md/interview/cpp/09_concurrency.md`.
  이 챕터는 그 위의 "무엇을 선택했고 왜"를 담당한다.
- **네트워크/서버 권위 챕터**: 신뢰 경계 verify(PacketDispatcher/SnapshotApplier)와 세션 단위 실패 격리,
  suspicion 시스템은 서버 권위 파이프라인 챕터의 프레이밍·시퀀스 게이트와 한 세트다 —
  거기서는 "데이터가 어떻게 흐르나", 여기서는 "흐름이 깨졌을 때 어떻게 보이나".
- **데이터 아키텍처 챕터**: pack miss 폴백 카운터와 빌드 해시 핸드셰이크는 데이터 소유권 3분할
  (SharedContract/ServerPrivate/ClientPublic)과 zero-reader 삭제 전략의 실행 장치다.
- **계층 경계/의존성 챕터**: P2(오염 격리)의 최전선은 의존성 방향이고, Check-SharedBoundary.ps1과
  Phase 7F 어댑터 슬라이스가 그 강제 장치다 — 에러 격리와 레이어 격리는 같은 원칙의 두 얼굴.
- **내비게이션/AI 챕터**: ePathFindResult, chase fallback, stuck 트레이스는 이동 디버깅 파이프라인
  (셀/웨이포인트/보정 방향/stuck 이유 노출)의 일부로 그쪽에서 더 깊게 다룬다.
