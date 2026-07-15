# Winters Error Handling Policy

작성일: 2026-07-09. 상위 원칙은 `WINTERS_DESIGN_PHILOSOPHY.md`(P1~P4), 이 문서는 실행 규약과 계층별 컨벤션을 고정한다. 2026-07-09 전수 감사(발견 65건, high 18건 확정)와 1차 적용(`.md/plan/2026-07-09_ERROR_BOUNDARY_REFACTORING.md`)이 근거다.

## 0. 에러 모델 (전 계층 공통)

- **예외는 쓰지 않는다.** 반환값 기반(bool/HRESULT/nullptr/핸들 IsValid)이 기본. 유일한 throw 경계는 cooked asset 파싱의 `CBinaryReader`(std::runtime_error)이고, 이를 소비하는 로더가 catch해서 false로 변환한다. 새 throw 경계를 추가하려면 compass 결정 필요.
- 계층별 반환 컨벤션 (기존 코드 실측 — 새 코드는 소속 계층을 따른다):
  - Engine 매니저/팩토리: `Create()`가 nullptr 반환, 초기화는 HRESULT 또는 bool.
  - RHI: 핸들 타입 + `IsValid()`. 생성 실패는 빈 핸들 `{}`.
  - Server 전송 계층: bool 반환 + `std::cerr`(콘솔 앱) 로그, WSA 에러 코드 포함.
  - Client/Shared 게임플레이: bool/NULL_ENTITY/nullptr + bounded `OutputDebugStringA`(또는 `WintersOutputAIDebugStringA`).
  - Go Services: 구조화 slog + 시작 실패 `os.Exit(1)` + Recovery 미들웨어 (이미 모범).

## 1. 실패 진단 규약 (필수)

1. **모든 실패 exit에는 진단이 있어야 한다**: 무엇이(스테이지/함수), 무엇에 대해(경로/이름/netId/HRESULT/WSA 코드), 어디서([태그]) 실패했는지.
2. **bounded가 기본**: 함수-로컬 static 카운터로 상한 (실패류 8, 컨텐츠 miss류 64, 계측류 512 — 기존 패턴과 일치). 성공/실패가 같은 카운터를 공유하면 안 된다 (성공이 예산을 소진해 실패가 안 보이는 사고 — CommandExecutor cast 로그 64캡 사례).
3. **포맷만 하고 출력하지 않는 코드 금지** (dead diagnostics). 로그를 끄고 싶으면 게이트(constexpr 플래그/#ifdef) 뒤로 옮기거나 삭제한다. sprintf_s 후 버리는 패턴은 리뷰 반려 대상.
4. **성공 로그는 결과 확인 후에만**. `Load_X(); Log("X loaded")` 금지 — 반환값을 잡아 분기한다.
5. **루틴 트레이스와 실패 진단을 구분한다** (gotcha 2026-05-28과의 양립 규칙):
   - 루틴 트레이스(틱/스냅샷/yaw 정상 흐름): 리팩터 중 추가 금지, 디버깅 세션 후 제거 또는 게이트.
   - 실패 진단(verify 실패, 로드 실패, 스폰 실패, stuck): 항상 살아 있어야 하며 제거 대상이 아니다.
6. **로그 채널**: 실패 진단은 no-op 스텁 채널(현재 `Winters::DevSmoke::Log`는 no-op)에 넣지 않는다 — 실패는 `OutputDebugStringA` 직행. DevSmoke는 스모크 러닝용 루틴 채널.
7. **OutputDebugStringA 게이트 함정**: `Engine_Defines.h`가 `OutputDebugStringA`를 `WintersOutputDebugStringA`(게이트 `WINTERS_ENABLE_NON_AI_DEBUG_STRING`, 기본 0=무음)로 매크로 재매핑한다. Engine/Client Debug 구성만 게이트=1을 정의하고 Server/GameSim은 정의하지 않으므로 — Server 실패 로그는 `std::cerr`, Shared sim 진단은 `Shared/GameSim/Core/Debug/SimDebugOutput.h`의 `WintersOutputAIDebugStringA`(게이트 기본 1)를 사용한다.

## 2. 경계별 필수 처리 (초크포인트 목록)

| 경계 | 규약 | 기준 구현 (2026-07-09 적용) |
|---|---|---|
| RHI 리소스 생성 | FAILED 시 HRESULT+debugName 로그 후 빈 핸들 | CDX11Device::CreateBuffer/CreateTexture (`[CDX11Device] FAIL:`) |
| 텍스처 파일 로드 | 스테이지+경로+HRESULT 로그 | RHITextureLoader `LogTextureLoadFailure` |
| 씬 전환 | OnEnter 실패 → 로그 + E_FAIL (침묵 진행 금지) | Scene_Manager::Change_Scene |
| 네트워크 verify | FlatBuffers verify 실패 → bounded 로그 + drop | SnapshotApplier OnHello/OnSnapshot, EventApplier OnEvent |
| 서버 send 실패 | WSA 코드 로그 + 세션 단절 (보낼 수 없는 세션은 죽은 세션) | Session::OnSendComplete |
| 엔티티/에셋 스폰 실패 | 원인(no-def/renderer-init) + id 로그 | ChampionSpawnService::Spawn, SnapshotApplier::EnsureEntity |
| FX cue 라우팅 | unknown cue/emitter는 bounded 로그 (침묵 no-op 금지) | FxCuePlayer LogMissingCue/LogSkippedCueEmitter |
| 스테이지 데이터 로드 | HRESULT 분기 + 실패 경로 로그 | Scene_InGameLifecycle Stage1 블록 |
| 이동 stuck | dead-end 시 tick/entity/pos/reason 트레이스 | MoveSystem `[MoveSystem][Stuck]` |

## 3. 실패 격리 규약 (P2 실행)

- 부분 초기화 실패: 만든 것을 정리하고 실패를 반환하거나, 명시적 degraded 모드로 전환 + 로그. 모범: RHISceneRenderer::EnsureDrawSlots(부분 생성물 파괴 후 false), GameRoomNav 폴백 사다리.
- 세션/연결 실패는 세션 단위로 격리: recv/send 실패 → OnDisconnect → 매니저 정리 캐스케이드. 룸 truth는 건드리지 않는다.
- 폴백 값은 진단과 함께: ChampionGameDataDB의 하드코딩 폴백 스탯, GameplayDefinitionQuery의 legacy 폴백 체인은 sim을 살리지만 데이터 팩 오구성을 가릴 수 있다 — 폴백 진입 시 bounded 로그 추가가 로드맵 (데이터 폴백이 제2의 truth가 되면 안 된다는 FOLDER_ESSENCE 규칙과 연결).
- 실패 시 재시도 루프는 가시화: EnsureEntity처럼 매 틱 재시도되는 지점은 반드시 실패 로그가 있어야 무한 침묵 재시도를 방지.

## 4. 알려진 잔여 침묵 지점 (미해소 — 새 작업 시 주의)

감사에서 확인되었으나 이번 슬라이스에서 의도적으로 보류(설계 필요). 여기 적힌 것은 "고쳐야 할 것"이지 "따라 해도 되는 패턴"이 아니다:

- ~~Pathfinder 빈 경로의 원인 무구분~~ → 2026-07-09 해소: `ePathFindResult` enum(NullGrid/StartBlocked/GoalBlocked/NoRoute/BrokenPath) out-param 추가, NavigationSystem `[NavAgent]` 트레이스에 result 노출. Server WalkabilityAuthority 결과 구조체로의 전파는 후속.
- ~~CHttpClient 가짜 async~~ → 2026-07-09 해소: future를 `m_PendingRequests`가 소유, worker는 `RequestSnapshot` 복사본만 사용(SetAuthToken race 차단), 소멸자가 전량 드레인(블로킹은 파괴 시점으로 한정).
- CommandExecutor: 죽은/무효 issuer, unknown command kind의 침묵 drop; StartAttackChase 경로 실패 무로그.
- Server: GameRoomCommands의 세션→엔티티 바인딩 실패 침묵 skip; PacketDispatcher의 malformed frame 무로그 단절; suspicion 카운터는 기록만 되고 어떤 enforcement도 없음 (IsSuspicious 호출자 0).
- Client: SnapshotApplier의 대형 yaw 계측 블록 2곳(minion yaw/local yaw)은 여전히 포맷-후-폐기 상태로 보존됨 — yaw 세션에서 재무장 or 삭제 결정.
- Engine: FMOD 실패 무로그 E_FAIL 변환; Initialize_Engine의 매니저별 nullptr → E_FAIL 무로그 캐스케이드; DX11 저수준의 assert 전용 사전조건(릴리즈에서 통과); ModelRenderer Map 실패 시 stale GPU 상태 침묵 유지.
- Loader: Preload_* 결과 폐기 (로딩 화면이 성공을 가장).
- CHttpClient: "Async"가 future 폐기로 사실상 동기 블로킹 (raw this 캡처와 한 세트로 재설계 필요 — 지금 raw this가 안전한 유일한 이유가 그 블로킹임을 주의).
