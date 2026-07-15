Session - 게임종료 라이프사이클 5버그(넥서스/억제기 HP바, 재입장 유령월드, MMR/RP 계정 업데이트, 재매칭 사이클, 포탑 Z-fighting)를 원인 확정 후 반영한다.

작성일: 2026-07-15 (Agent: Claude)
성격: 세션 문서 — §0 버그 원인(확정) + §1 반영 코드 + §2 검증. 조사는 멀티에이전트 워크플로 2회(조사 5+검증 4+협업맵 1, 포탑 조사 1+적대검증 1)로 수행했고 전 원인이 적대적 검증을 통과했다.
빌드: 사용자 인게임 테스트 중이라 **이번 반영에서는 빌드 실행 보류** — 코드/데이터/문서 반영까지만 하고 빌드·E2E는 후속 신호 후 수행.

## Current sequence

```text
원인 조사(완료) -> 본 문서 박제 -> 코드 반영(빌드 제외) -> [사용자 신호] 빌드 -> 인게임 E2E
```

## Goal

게임 한 판의 수명주기를 닫는다: 넥서스/억제기에도 포탑과 같은 월드 HP바 → 넥서스 파괴 게임종료 → 온라인 계정이면 MMR/RP 반영(비회원/백엔드 다운이면 조용히 스킵) → 마지막 세션 이탈 시 서버 매치 리셋 → 메인메뉴에서 게임시작 시 첫 게임과 동일하게 SeatSelect→밴픽→새 게임. 더불어 포탑 Z-fighting(양 팀 대칭)의 원인을 제거하고 재발 방지 규칙을 남긴다.

## Non-goals

- 빌드/E2E 실행 (보류 — §2)
- `Command.fbs`·`GameRoomTick.cpp` 커맨드 바인딩부·`GameRoomCommands.cpp`·`ChampionTuner.cpp` (M0 데이터드리븐 세션 명시 예약)
- 사운드 세션 소유 파일(`ChampionSoundCatalog.*`, `Scene_InGameNetwork.cpp` 사운드 게이트), 구조물 파괴 세션 산출물(`StructureTunerPanel.*`, EventApplier destruction cue 블록), UDP 4파일(S031), `Scene_InGameRender.cpp`(RHI 레인), `EngineSDK/inc/**`, `.vcxproj/.filters`
- 서버권위 매치결과 보고(Kafka/서버 신원 매핑 — 클라 자기신고의 위변조 리스크는 §0-3에 기록하고 개발 단계 트레이드오프로 수용)
- 멀티 휴먼 엣지케이스(게임종료 후 일부만 이탈한 상태의 재접속 UX — §0-2 문서화만)
- 억제기 wmesh 중첩 점검(동일 클래스 후보 — 잔여로 이관)

## Why this order

문서(원인 박제)→코드는 사용자 지시. 서버 매치 리셋(§0-2)이 재매칭(§0-4)의 전제라 같은 슬라이스로 묶는다. **Bot AI는 GameCommand 생산자이며 게임플레이 truth를 직접 변경하지 않는다** — 이 세션의 서버 변경(매치 리셋)은 시뮬 진행 중의 truth 변경이 아니라 매치 경계(마지막 세션 이탈+게임종료 latch)에서 월드 전체를 교체하는 룸 수명 관리다.

## 협업 경계 (충돌 맵 확정 사항)

- 전 패킷 Handoff 상태, 동시 Active 없음. 단 저장소가 커밋 f9d4d5c 이후 전면 미커밋(수백 파일)이라 이 세션 diff는 타 세션 미검증 작업 위에 얹힌다 — 체크포인트 커밋은 사용자 승인 게이트로 보류.
- `Scene_InGame.h`는 Always-Lock — 이번 반영은 멤버 추가 없이 .cpp만 수정 (HP바 루프는 기존 함수 내부, 업로드 호출은 기존 함수 내부).
- SimLab 골든 해시 정본은 칼리스타 세션(2026-07-15)이 갱신한 **18110C0D** — 이 세션 변경은 GameSim 시뮬 로직 무접촉이라 해시 불변이어야 한다.
- `S034`는 챔피언 스왑 세션용으로 구두 예약(문서 부재 확인) — 본 세션은 S035 선점.

---

## §0 버그 원인 (전부 적대적 검증 통과, rg 재검증 완료)

### 0-1. 넥서스/억제기 월드 HP바 미표시

- 수집 루프 한정이 원인. `CScene_InGame::SyncWorldHealthBarsToEngineUI`(Client/Private/Scene/Scene_InGame.cpp:618-725)에는 ForEach<ChampionComponent>(:639) / <MinionComponent>(:660) / <TurretComponent>(:675) / <JungleComponent>(:692) 4개 루프뿐이다.
- 넥서스/억제기 클라 엔티티에는 `TurretComponent`가 없다 — `EnsureSnapshotStructureRuntimeTags`가 kind==Turret일 때만 부여(Client/Private/Network/Client/SnapshotApplier.cpp:396-403). StructureComponent/TargetableTag/NexusTag/InhibitorTag는 부여됨(:376-394, :405-418).
- HP 데이터 부재는 원인이 아님(배제 확정): 서버가 전 구조물에 HealthComponent 부여(Server/Private/Game/GameRoomSpawn.cpp:434-437), 스냅샷에 hp/maxHp 탑재(SnapshotBuilder.cpp:188-195), 클라 StructureComponent.hp 갱신(SnapshotApplier.cpp:1596-1607).
- Engine 렌더는 포탑 한정이 아니다 — Kind==Structure && !bDead && fMaximum>0 desc면 그린다(Engine/Private/Manager/UI/UI_Manager.cpp:4330-4335, RHI :4404-4409). **수정 지점은 Scene_InGame.cpp 수집 루프 한 곳.**
- 함정: 클라 포탑은 StructureComponent+TurretComponent 동시 보유(Structure_Manager.cpp:416,434-440) — Structure 루프 추가 시 포탑 skip 없으면 포탑 HP바 이중 생성.

### 0-2. 게임종료 후 재입장 = 파괴된 이전 게임 복귀

원인 사슬 4개 전부 코드 확정:
1. `Phase_CheckGameEnd`는 `m_bGameEnded=true` latch만 세운다(GameRoomTick.cpp:181). 참조는 :153/:160/:181 3곳뿐 — 어떤 sim phase도 게이트로 쓰지 않아 **종료 후에도 봇/웨이브 시뮬이 계속 돈다**.
2. 매치 리셋 부재: `m_phase =` 대입은 저장소 전체 4곳(선언 기본 SeatSelect + ChampionSelect/Loading/InGame 전진)뿐 — 역방향 전이 0. `CGameRoom::Create` 호출은 main.cpp:634 단 1곳 — 룸 재생성 0. `m_bGameEnded` 해제 0.
3. 클라가 메인메뉴 복귀 시 Disconnect(Scene_InGame.cpp:1355-1356) → 서버 OnSessionLeave는 InGame phase에서 슬롯을 "reserved for reconnect"로 보존(LobbyAuthority.cpp:349-353,366-368).
4. 게임시작 재클릭 → 새 TCP 접속 → OnSessionJoin의 late-attach 분기(phase가 SeatSelect/ChampionSelect가 아니면, GameRoomLobby.cpp:74-94)가 **끝난 매치의 예약 슬롯에 계정 무관 재부착**(LobbyAuthority.cpp:437-453 첫 끊긴 human 슬롯) → Hello+LobbyState(InGame)+GameStart 재전송(:373-400, bSendGameStart=(phase==InGame) :397) → 클라 GameStart 래치(GameSessionClient.cpp:426-432)로 매칭창을 건너뛰고 즉시 워프.
- 추가 확증: 게임종료 이벤트는 1회성(이벤트 엔티티 송신 후 파괴 GameRoomReplication.cpp:122-123 + latch 재발화 차단) — 재접속 클라는 종료 오버레이조차 못 받고 "계속 진행 중인 끝난 월드"에 갇힌다.
- 리셋 실장 근거: `CWorld`는 move-assign 지원(EngineSDK/inc/ECS/World.h:56) + `Initialize_Spatial` 재초기화 가능. 스테이지/내비/웨이포인트/플로우필드/구조물 스폰은 전부 `SpawnServerGameplayObjects`(GameRoomSpawn.cpp:162-219)에 자기완결로 들어 있고 `m_bGameplayObjectsSpawned` once-가드(:164-167)만 풀면 다음 매치 시작 시 재실행된다. `CCommandIngress::Clear()`(CommandIngress.h) / `EntityIdMap` 재할당 / `CServerMinionWaveRuntime::Clear()` / `InitializeLobbyAuthority()`(=make_unique+InitializeSlots, GameRoomLobby.cpp:39-43) 전부 실재. `Tick()`과 `OnSessionLeave`는 같은 `m_stateMutex`로 직렬화(GameRoomTick.cpp:104)되고, 리셋 후 phase=SeatSelect면 IsInGamePhase 게이트(:106-107)가 sim을 쉬게 한다.

### 0-3. 매치결과 → MMR/RP 계정 업데이트 부재 (+백엔드 미실행 내성)

- 4구간 전부 호출 주체 부재: ①C++ 서버는 감지만 하고 보고 없음(HTTP/Kafka 클라이언트 0건, 세션↔계정 신원 매핑 0건) ②클라 게임종료 처리는 로컬 기록만(SaveEndOfMatchArtifacts, Scene_InGame.cpp:1334-1351 — LocalMatchRecord+AI trace) ③profile 서비스에 쓰기 엔드포인트 없음(handler.go:23-30 GET 4개뿐) ④Kafka MatchCompleted 생산자 전무(소비자만 profile/leaderboard 2곳 — 영원히 굶는 상태).
- 갱신 코드·스키마는 완비: `InsertMatchHistory`/`UpdatePlayerStats(mmr += mmr_change)`(repository.go:99-129), player_stats.mmr(default 1000)/match_history(result CHECK 'win'/'loss'/'draw')/wallets.balance(RP).
- 함정: 클라 라벨은 "victory"/"defeat"(Scene_InGame.cpp:1327) vs DB CHECK('win','loss') — 매핑 필수. match_id는 UUID 타입 — 서버(Go)가 생성.
- 백엔드 미실행 내성은 **이미 확보 검증됨**: 비회원은 HTTP 클라이언트 자체를 안 만들고(ClientShellBackendService.cpp:24-28), 회원+백엔드 다운은 전 호출이 워커 async(CHttpClient LaunchAsyncRequest)라 UI 비차단, 연결 거부는 즉시 실패. 밴픽 챔피언 잠금도 오프라인이면 전부 해제(Scene_BanPick.cpp:277-289). 신규 업로드도 같은 게이트(`!m_bConfigured` → 스킵)를 태우면 내성이 유지된다.
- 리스크(수용): 클라 자기신고는 서버권위 원칙 위반(위변조 가능) — 개발/포트폴리오 단계 트레이드오프로 수용하고, 정공 경로(서버 신원 매핑+Kafka)는 잔여로 남긴다. 재전송 중복 insert 가능(unique 인덱스 없음) — 클라 latch(1회 소비)로 완화.

### 0-4. 게임 리셋 → 재매칭 사이클

- 클라는 준비 완료: Disconnect가 세션 상태 전부 리셋(GameSessionClient.cpp:316-335), Scene_CustomMode/Scene_BanPick OnEnter가 상태 리셋(Scene_BanPick.cpp:297-308). 봇 추가는 클라 요청+서버 검증(SetBotChampion, LobbyAuthority.cpp:807-896)으로 첫 게임 경로에 이미 존재.
- **빠진 것은 서버 리셋뿐** — §0-2 리셋이 들어가면 재접속은 phase=SeatSelect의 일반 로비 조인(GameRoomLobby.cpp:108-116)으로 흘러 첫 게임과 동일 사이클(CustomMode 좌석→봇 추가→밴픽→로딩→인게임)을 탄다.

### 0-5. 포탑 텍스처 깨짐 (카메라 이동 시 울렁거림) — 양 팀 대칭

- **확정 원인**: 포탑 wmesh(블루/레드 지오메트리 바이트 동일, MD5 df264569…)에 서브메시 8개([0]Base [1]Stage1 [2]Stage2 [3]Stage3(스텀프) [4]Rubble [5-7]Broken1-3)가 같은 공간에 중첩. Base/Stage1/Stage2는 AABB 완전 동일 + 유니크 정점 93% 공유 + UV/스키닝 일치이나 **삼각형은 12%만 동일(전면 재삼각화)** → 보간 깊이가 ULP 수준으로 어긋나 카메라 이동 시 깊이 경쟁 승자가 픽셀 단위로 뒤바뀌는 고전 Z-fighting. 포탑 비주얼 정의 visibilityStates가 비어 있어(ObjectVisualDefs.json, generated.cpp submeshStateCount=0) `Structure_Manager::Render`의 비-넥서스 분기가 8개 전부 draw(Structure_Manager.cpp:150-153, 깊이 LESS+write ALL DX11Pipeline.cpp:42-44).
- **기각된 가설(중요)**: "블루 wmat이 Base↔Stage에 다른 텍스처를 매핑해 블루만 보인다" — 두 텍스처(turret_base_tx_cm vs turret_tx_cm)는 1024² 전 픽셀 실측 최대 채널 델타 9/255로 사실상 동일 이미지. **wmat 경로 패치는 시각 효과가 없어 적용 금지.** 가시 플리커는 UV 아일랜드가 다른 손상-변형 지오메트리 구간에서 발생하며 wmesh가 바이트 동일이므로 팀 대칭.
- **"블루만"의 정체 = 관측 편향(사용자 확인으로 종결)**: 세션 중 사용자가 레드팀 포탑에도 동일 증상이 존재함을 확인 — 팀 대칭 결함 확정. (부차 관찰: Stage1.dat상 블루 포탑만 수동 회전 ry=-2.16~4.05·높이 py=0.3~1.0 보정 — 접지 차이로 체감이 더 컸을 수 있음.)
- 과거 진단(2026-04-20)과의 관계: "Destroyed 중첩 메시" 골격은 유효했으나 당시 수정안(Model.cpp kSkipMatSubstrings 전역 스킵)은 원복됐고(현재 rg 0건), 이제 넥서스가 Destroyed 서브메시를 파괴 연출(visibleWhenDestroyed 스왑)에 쓰므로 전역 스킵은 부적합. 서브메시 이름도 "meshes[0]-0~7"로 익명화돼 이름 기반 스킵 불가.
- **수정 방향(정공법)**: 살아있는 포탑은 단일 alive 서브메시([0]Base — 삼각형 최다 3358, 이름상 유력)만 렌더. 넥서스가 이미 쓰는 visibilityStates 메커니즘(BuildStructureVisibilityMask: bVisibleWhenDestroyed==bDestroyed일 때만 표시, Structure_Manager.cpp:16-31)을 포탑에 데이터로 등록 — 서브메시 1~7을 visibleWhenDestroyed=true로 걸면 살아있는 동안 자동 숨김, 파괴 시엔 SnapshotApplier가 모델 전체를 숨기므로(hp<=0 bVisible=false, SnapshotApplier.cpp:1629-1633) destroyed 마스크에 실질 도달하지 않는다. 파괴 연출(destruction.wfx+전체 숨김)·넥서스 스왑 경로 완전 무접촉.
- CONFIRM_NEEDED(사용자 시각 게이트): 현재 화면의 포탑 외형은 8겹 중첩의 합성이라 [0]Base 단독이 의도 외형과 일치하는지 인게임/F5 Model&Anim Lab Solo로 1회 확인 필요. 불일치 시 JSON에서 alive 서브메시만 바꾸고 팩 재생성.
- 재발 방지 규칙(→ gotchas 반영): 구조물/오브젝트 wmesh를 추가할 때 서브메시가 2개 이상이면 ①중첩 AABB 여부 덤프 ②visibilityStates로 alive/destroyed 분리 등록 ③팀별 wmat 매핑 대칭 확인 — 카메라 이동 시 울렁거림은 텍스처가 아니라 중첩 지오메트리 Z-fighting부터 의심.

---

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

`SyncWorldHealthBarsToEngineUI`의 포탑 루프 아래에 추가.

기존 코드:

```cpp
    m_World.ForEach<TurretComponent, TransformComponent>(
        [&](EntityID Entity, TurretComponent& Turret, TransformComponent& Transform)
        {
            Engine::UIWorldHealthBarDesc Bar{};
            Bar.Entity = Entity;
            Bar.Kind = Engine::UIWorldHealthBarKind::Structure;
            Bar.vWorldPos = Transform.GetPosition();
            Bar.fCurrent = Turret.hp;
            Bar.fMaximum = Turret.maxHp;
            Bar.iTeam = ToLoLUITeamId(Turret.team);
            Bar.bDead = Bar.fCurrent <= 0.f;
            ApplyHealthOverride(Entity, Bar.fCurrent, Bar.fMaximum, Bar.bDead);
            Bars.push_back(Bar);
        });
```

아래에 추가:

```cpp
    // 넥서스/억제기: TurretComponent가 없어 위 포탑 루프에 안 걸린다 — StructureComponent로 수집.
    // 클라 포탑은 두 컴포넌트를 동시 보유하므로 이중 desc 방지를 위해 포탑은 건너뛴다 (S035).
    m_World.ForEach<StructureComponent, TransformComponent>(
        [&](EntityID Entity, StructureComponent& Structure, TransformComponent& Transform)
        {
            if (m_World.HasComponent<TurretComponent>(Entity))
                return;

            Engine::UIWorldHealthBarDesc Bar{};
            Bar.Entity = Entity;
            Bar.Kind = Engine::UIWorldHealthBarKind::Structure;
            Bar.vWorldPos = Transform.GetPosition();
            Bar.fCurrent = Structure.hp;
            Bar.fMaximum = Structure.maxHp;
            Bar.iTeam = ToLoLUITeamId(Structure.team);
            Bar.bDead = Bar.fCurrent <= 0.f;
            ApplyHealthOverride(Entity, Bar.fCurrent, Bar.fMaximum, Bar.bDead);
            Bars.push_back(Bar);
        });
```

`SaveEndOfMatchArtifacts`에서 온라인 계정 매치결과 업로드. 파일 상단 include 블록의 아래 기존 코드 근처(ClientShell 계열 include)에 `#include "ClientShell/ClientShellBackendService.h"`가 없으면 추가.

기존 코드:

```cpp
    Winters::LocalMatchRecord record{};
    record.strUser = CClientShellSession::Instance().GetDisplayName();
    record.strResult = pResultLabel ? pResultLabel : "unknown";
    record.uEndTick = m_pSnapshotApplier
        ? m_pSnapshotApplier->GetLastAppliedServerTick()
        : 0ull;
    Winters::AppendLocalMatchRecord(record);
```

아래에 추가:

```cpp
    // 온라인 계정이면 매치결과를 백엔드에 보고 (MMR/RP 반영, S035).
    // 비회원/백엔드 미실행이면 서비스 내부 게이트가 조용히 스킵 — 게임 흐름 비차단.
    // "aborted"(강제 이탈)는 보고하지 않는다.
    if (record.strResult == "victory" || record.strResult == "defeat")
    {
        CClientShellBackendService::Instance().RequestReportMatchResult(
            record.strResult == "victory");
    }
```

### 1-2. C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h

기존 코드:

```cpp
    //Replay
    void FinalizeReplayRecorder();
```

아래에 추가:

```cpp
    // S035: 게임종료 후 마지막 세션 이탈 시 룸을 새 매치 대기 상태(SeatSelect)로 리셋.
    // m_stateMutex를 잡은 문맥에서만 호출한다.
    void ResetMatchStateLocked();
```

### 1-3. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomLobby.cpp

late-attach 분기에 게임종료 게이트. `OnSessionJoin`에서:

기존 코드:

```cpp
    if (m_pLobbyAuthority &&
        m_pLobbyAuthority->GetPhase() != eRoomPhase::SeatSelect &&
        m_pLobbyAuthority->GetPhase() != eRoomPhase::ChampionSelect)
```

아래로 교체:

```cpp
    if (m_pLobbyAuthority &&
        !m_bGameEnded &&
        m_pLobbyAuthority->GetPhase() != eRoomPhase::SeatSelect &&
        m_pLobbyAuthority->GetPhase() != eRoomPhase::ChampionSelect)
```

`OnSessionLeave`에서:

기존 코드:

```cpp
    // ESC/강제 종료 저장 보증 — 마지막 세션이 떠나면 룸 teardown을 기다리지 않고
    // 리플레이를 즉시 발행한다 (S030, FinalizeReplayRecorder는 멱등).
    if (m_sessionIds.empty())
        FinalizeReplayRecorder();
```

아래로 교체:

```cpp
    // ESC/강제 종료 저장 보증 — 마지막 세션이 떠나면 룸 teardown을 기다리지 않고
    // 리플레이를 즉시 발행한다 (S030, FinalizeReplayRecorder는 멱등).
    if (m_sessionIds.empty())
    {
        FinalizeReplayRecorder();

        // 게임종료 후 마지막 세션까지 떠나면 매치를 리셋해 SeatSelect로 되돌린다 —
        // 재접속이 파괴된 월드로 워프되는 대신 첫 게임과 동일 경로를 타게 한다 (S035).
        if (m_bGameEnded)
            ResetMatchStateLocked();
    }
```

### 1-4. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

`InitializeServerSimSystems` 함수 정의 바로 아래에 추가:

```cpp
void CGameRoom::ResetMatchStateLocked()
{
    // 월드 통째 교체 — 파괴된 구조물/미니언/챔피언/이벤트 엔티티 전부 소멸.
    // 스테이지/내비/웨이포인트/구조물 스폰은 SpawnServerGameplayObjects가 자기완결이라
    // m_bGameplayObjectsSpawned 가드만 풀면 다음 매치 시작(bBeginLoading) 때 재구축된다.
    m_world = CWorld{};
    m_world.Initialize_Spatial(DefaultSpatialGridDesc());
    EnsureMatchScoreEntity(m_world);

    m_entityMap = EntityIdMap{};
    m_rng = DeterministicRng{ 0xC0FFEEull };
    m_tickIndex = 0;
    m_visibleTickIndex.store(0, std::memory_order_relaxed);

    m_pExecutor = CDefaultCommandExecutor::Create();
    m_pSnapBuilder = CSnapshotBuilder::Create();
    m_pLagCompensation = std::make_unique<CLagCompensation>();
    m_pReplayRecorder = CReplayRecorder::Create(m_roomId, 30);
    m_bReplayFinalized = false;
    m_bGameEnded = false;

    m_pSpatialSystem = Engine::CSpatialHashSystem::Create();
    m_pTurretAI = GameplayTurret::CTurretAISystem::Create();

    m_commandIngress.Clear();
    m_pendingExecCommands.clear();
    m_pendingReplayCommands.clear();
    m_bPracticeModeEnabled = false;
    m_PracticeSpawnedEntities.clear();
    m_PendingPracticeControlChange = {};

    m_bSimPaused = false;
    m_simStepBudget = 0;
    m_simSpeedMul.store(1.f, std::memory_order_relaxed);
    m_timelineEpoch = 1;
    m_timelineBranchId = 1;
    m_lastReplaySnapshotTick = ~0ull;
    m_lastReplayToolRevision = ~0ull;
    m_keyframes.clear();
    m_pendingRewindToTick = 0;

    m_sessionBinding = CSessionBinding{};
    m_lastBroadcastActionSeq.clear();
    m_lastSimCommandSeqBySession.clear();

    m_bGameplayObjectsSpawned = false;
    m_serverMinionWaves.Clear();

    // 로비를 SeatSelect부터 다시 — 다음 접속은 첫 게임과 동일 경로를 탄다.
    InitializeLobbyAuthority();

    OutputServerAITrace("[GameRoom] Match reset after game end; lobby back to SeatSelect\n");
}
```

### 1-5. C:/Users/user/Desktop/Winters/Services/internal/profile/handler.go

기존 코드:

```go
	r.Get("/me", h.GetMyProfile)
```

아래에 추가:

```go
	r.Post("/me/matches", h.ReportMyMatch)
```

파일 말미에 추가 (MMR/RP 상수는 서버 소유 — 클라는 승패만 보고):

```go
// ReportMyMatch records the caller's finished match from JWT claims:
// inserts match history, applies MMR delta, credits RP to the wallet.
// Client self-report (dev-stage trade-off) — the C++ game server does not
// yet carry account identity, so the client is the only reporter available.
func (h *Handler) ReportMyMatch(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return
	}

	var req struct {
		Result string `json:"result"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		response.Error(w, http.StatusBadRequest, "invalid body")
		return
	}
	if req.Result != "win" && req.Result != "loss" {
		response.Error(w, http.StatusBadRequest, "result must be win or loss")
		return
	}

	const (
		mmrWinDelta  = 25
		mmrLossDelta = -25
		rpWinReward  = 150
		rpLossReward = 75
	)

	player := MatchCompletedPlayer{
		UserID: claims.UserID,
		Result: req.Result,
	}
	rpReward := int64(rpLossReward)
	if req.Result == "win" {
		player.MMRChange = mmrWinDelta
		rpReward = rpWinReward
	} else {
		player.MMRChange = mmrLossDelta
	}

	matchID := uuid.New()
	if err := h.repo.ReportMatch(r.Context(), claims.UserID, matchID, player, rpReward); err != nil {
		response.Error(w, http.StatusInternalServerError, "failed to report match")
		return
	}

	response.JSON(w, http.StatusOK, map[string]any{
		"match_id":   matchID,
		"mmr_change": player.MMRChange,
		"rp_reward":  rpReward,
	})
}
```

import 블록에 `"encoding/json"` 추가 (기존 코드 `"errors"` 아래).

### 1-6. C:/Users/user/Desktop/Winters/Services/internal/profile/repository.go

파일 말미(`InvalidateCache` 아래)에 추가:

```go
// ReportMatch applies one finished match atomically: history row + player_stats
// (wins/losses/mmr) + RP wallet credit, then invalidates the profile cache.
func (r *Repository) ReportMatch(ctx context.Context, userId, matchID uuid.UUID, p MatchCompletedPlayer, rpReward int64) error {
	tx, err := r.db.Begin(ctx)
	if err != nil {
		return fmt.Errorf("begin report match: %w", err)
	}
	defer tx.Rollback(ctx)

	if _, err := tx.Exec(ctx,
		`INSERT INTO match_history (user_id, match_id, result, kills, deaths, assists, mmr_change)
		 VALUES ($1, $2, $3, $4, $5, $6, $7)`,
		userId, matchID, p.Result, p.Kills, p.Deaths, p.Assists, p.MMRChange); err != nil {
		return fmt.Errorf("insert match history: %w", err)
	}

	winAdd, lossAdd := 0, 0
	if p.Result == "win" {
		winAdd = 1
	} else if p.Result == "loss" {
		lossAdd = 1
	}
	if _, err := tx.Exec(ctx,
		`UPDATE player_stats
		 SET wins = wins + $2, losses = losses + $3,
		     mmr = GREATEST(0, mmr + $4), updated_at = NOW()
		 WHERE user_id = $1`,
		userId, winAdd, lossAdd, p.MMRChange); err != nil {
		return fmt.Errorf("update player_stats: %w", err)
	}

	if _, err := tx.Exec(ctx,
		`UPDATE wallets SET balance = balance + $2, updated_at = NOW()
		 WHERE user_id = $1`,
		userId, rpReward); err != nil {
		return fmt.Errorf("credit wallet: %w", err)
	}

	if err := tx.Commit(ctx); err != nil {
		return fmt.Errorf("commit report match: %w", err)
	}

	r.InvalidateCache(ctx, userId)
	return nil
}
```

### 1-7. C:/Users/user/Desktop/Winters/Client/Public/Network/Backend/ProfileClient.h

기존 코드:

```cpp
using ProfileCallback = function<void(const ProfileData&)>;
using HistoryCallback = function<void(const vector<MatchRecord>&)>;
```

아래로 교체:

```cpp
using ProfileCallback = function<void(const ProfileData&)>;
using HistoryCallback = function<void(const vector<MatchRecord>&)>;

struct MatchReportResult
{
	bool_t success = false;
	i32_t  mmrChange = 0;
	i32_t  rpReward = 0;
	string error;
};

using MatchReportCallback = function<void(const MatchReportResult&)>;
```

기존 코드:

```cpp
	void GetMyHistory(HistoryCallback callback);
```

아래에 추가:

```cpp
	// 매치결과 자기신고 (POST /profile/me/matches, JWT claims 기반) — S035.
	void ReportMyMatch(bool_t bVictory, MatchReportCallback callback);
```

### 1-8. C:/Users/user/Desktop/Winters/Client/Private/Network/Backend/ProfileClient.cpp

`GetMyHistory` 정의 아래에 추가:

```cpp
void CProfileClient::ReportMyMatch(bool_t bVictory, MatchReportCallback callback)
{
	json body;
	body["result"] = bVictory ? "win" : "loss";
	m_pHttp->AsyncPost("/profile/me/matches", body.dump(), [callback](const HttpResponse& resp) {
		MatchReportResult result;
		try
		{
			auto j = json::parse(resp.body);
			if (!resp.success)
			{
				result.error = j.value("error", "report failed");
				callback(result);
				return;
			}
			auto data = j["data"];
			result.success = true;
			result.mmrChange = data.value("mmr_change", 0);
			result.rpReward = data.value("rp_reward", 0);
		}
		catch (const json::exception& e) { result.error = e.what(); }
		callback(result);
		});
}
```

### 1-9. C:/Users/user/Desktop/Winters/Client/Public/ClientShell/ClientShellBackendService.h

기존 코드:

```cpp
	void RequestMatchHistory();
```

아래에 추가:

```cpp
	// 게임종료 매치결과 보고 — 오프라인/미구성이면 조용히 스킵 (S035).
	void RequestReportMatchResult(bool_t bVictory);
```

### 1-10. C:/Users/user/Desktop/Winters/Client/Private/ClientShell/ClientShellBackendService.cpp

`RequestMatchHistory` 정의 아래에 추가:

```cpp
void CClientShellBackendService::RequestReportMatchResult(bool_t bVictory)
{
	// 비회원/백엔드 미실행이면 보고하지 않는다 — 게임 흐름은 로컬 전적만으로 완결 (S035).
	if (!m_bConfigured || !m_pProfileClient)
		return;

	const u32_t uGeneration = m_uGeneration;
	m_bProfileRequestInFlight = true;
	m_pProfileClient->ReportMyMatch(
		bVictory,
		[this, uGeneration](const Client::MatchReportResult& result)
		{
			m_bProfileRequestInFlight = false;
			if (uGeneration == m_uGeneration)
			{
				if (result.success)
				{
					m_strStatus = "Match reported";
					// MMR/RP가 바뀌었으니 다음 메인메뉴 진입 sync가 최신 값을 받도록
					// 프로필/상점 재요청 래치를 푼다.
					m_bInitialSyncRequested = false;
				}
				else
				{
					m_strStatus = result.error.empty()
						? "Match report failed"
						: "Match report failed: " + result.error;
				}
			}
			TryFinishDeferredReset();
		});
}
```

### 1-11. C:/Users/user/Desktop/Winters/Data/LoL/ClientPublic/Visual/ObjectVisualDefs.json

`structure.turret.blue`와 `structure.turret.red` 두 항목의:

기존 코드:

```json
   "visibilityStates": []
```

아래로 교체 (두 항목 동일):

```json
   "visibilityStates": [
    { "name": "Stage1", "submeshIndex": 1, "visibleWhenDestroyed": true },
    { "name": "Stage2", "submeshIndex": 2, "visibleWhenDestroyed": true },
    { "name": "Stage3Stump", "submeshIndex": 3, "visibleWhenDestroyed": true },
    { "name": "Rubble", "submeshIndex": 4, "visibleWhenDestroyed": true },
    { "name": "Broken1", "submeshIndex": 5, "visibleWhenDestroyed": true },
    { "name": "Broken2", "submeshIndex": 6, "visibleWhenDestroyed": true },
    { "name": "Broken3", "submeshIndex": 7, "visibleWhenDestroyed": true }
   ]
```

의미: 살아있는 동안(bDestroyed=false) 위 7개 숨김 → [0]Base만 렌더. 파괴 시엔 SnapshotApplier가 모델 전체를 숨기므로 destroyed 마스크는 실질 미도달.

### 1-12. C:/Users/user/Desktop/Winters/Client/Private/Data/LoLVisualDefinitionPack.h

기존 코드:

```cpp
    inline constexpr u8_t kVisualSubmeshStateCount = 4u;
```

아래로 교체:

```cpp
    inline constexpr u8_t kVisualSubmeshStateCount = 8u;   // S035: 포탑 7상태 수용
```

### 1-13. C:/Users/user/Desktop/Winters/Client/Private/Data/Generated/LoLVisualDefinitions.generated.cpp

직접 수정 금지 — `Tools/LoLData/Build-LoLDefinitionPack.py` 재실행으로 재생성 완료. 결과: 팩 0x266FF061, `--check` PASS, 포탑 상태 7×2 반영(generated 파일 내 submeshStates 대입 36라인 = 넥서스 8 + 포탑 28).

### 1-13b. C:/Users/user/Desktop/Winters/Tools/LoLData/Build-LoLDefinitionPack.py

생성기의 visibilityStates 상한이 4로 하드코딩 — 1-12와 정렬.

기존 코드:

```python
        if len(states) > 4:
            fail(f"structures[{index}].visibilityStates has too many entries: {len(states)} > 4")
```

아래로 교체 (적용 완료):

```python
        # S035: kVisualSubmeshStateCount(LoLVisualDefinitionPack.h)와 정렬 — 포탑 7상태 수용.
        if len(states) > 8:
            fail(f"structures[{index}].visibilityStates has too many entries: {len(states)} > 8")
```

**⚠충돌 고지**: 이 파일은 반영 시점에 Active 전환된 `2026-07-15_data_driven_balance_m0_m6_execution` 패킷의 owned path였다(조사 시점에는 문서 전용 Handoff). 수정은 가산 2줄이라 병합 위험 낮음 — M0 세션은 재생성 시 이 상한(8)을 유지해야 한다. 또한 팩 재생성(0x266FF061)이 M0 세션 진행 중 gameplay JSON 상태를 함께 구웠을 수 있음 — M0 세션이 JSON 편집 완료 후 재생성하면 자연 수렴하며, 게임플레이 수치 자체는 이 세션이 건드리지 않았다(비주얼 JSON만 편집, kBuildHash는 정체성 상수).

### 1-14. C:/Users/user/Desktop/Winters/Client/Private/Manager/Structure_Manager.cpp

`Render`의 넥서스 분기:

기존 코드:

```cpp
            // 넥서스는 양 팀 모두 컬링 없이 항상 렌더한다.
            // (이전엔 Blue 넥서스만 우회해 Red 넥서스가 프러스텀 경계에서 비대칭 컬링/pop-in.)
            const bool_t bNexus =
                structure.kind == static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Nexus);
            if (bNexus)
            {
                const auto kind = static_cast<Winters::Map::eObjectKind>(structure.kind);
                const ClientData::StructureVisualDefinition* pVisual =
                    ClientData::FindStructureVisualDefinition(kind, structure.team);
                const VisibilityMask mask = BuildStructureVisibilityMask(structure, pVisual);
                rc.pRenderer->RenderWithVisibility(mask);
            }
            else
            {
                rc.pRenderer->RenderFrustumCulled(matViewProj);
            }
```

아래로 교체:

```cpp
            // 넥서스는 양 팀 모두 컬링 없이 항상 렌더한다.
            // (이전엔 Blue 넥서스만 우회해 Red 넥서스가 프러스텀 경계에서 비대칭 컬링/pop-in.)
            // S035: visibilityStates를 가진 구조물(포탑 Z-fight 수복)도 상태 마스크 경로를 탄다.
            const auto kind = static_cast<Winters::Map::eObjectKind>(structure.kind);
            const ClientData::StructureVisualDefinition* pVisual =
                ClientData::FindStructureVisualDefinition(kind, structure.team);
            const bool_t bNexus =
                structure.kind == static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Nexus);
            if (bNexus || (pVisual && pVisual->submeshStateCount > 0u))
            {
                const VisibilityMask mask = BuildStructureVisibilityMask(structure, pVisual);
                rc.pRenderer->RenderWithVisibility(mask);
            }
            else
            {
                rc.pRenderer->RenderFrustumCulled(matViewProj);
            }
```

`AppendRenderSnapshotMeshes`의 동일 분기:

기존 코드:

```cpp
            const bool_t bNexus =
                structure.kind == static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Nexus);
            if (bNexus)
            {
                const auto kind = static_cast<Winters::Map::eObjectKind>(structure.kind);
                const ClientData::StructureVisualDefinition* pVisual =
                    ClientData::FindStructureVisualDefinition(kind, structure.team);
                const VisibilityMask mask = BuildStructureVisibilityMask(structure, pVisual);
                appendedCount += rc.pRenderer->AppendRenderSnapshotMeshes(snapshot, mask);
            }
            else
            {
                appendedCount += rc.pRenderer->AppendRenderSnapshotMeshesFrustumCulled(snapshot, matViewProj);
            }
```

아래로 교체:

```cpp
            const auto kind = static_cast<Winters::Map::eObjectKind>(structure.kind);
            const ClientData::StructureVisualDefinition* pVisual =
                ClientData::FindStructureVisualDefinition(kind, structure.team);
            const bool_t bNexus =
                structure.kind == static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Nexus);
            if (bNexus || (pVisual && pVisual->submeshStateCount > 0u))
            {
                const VisibilityMask mask = BuildStructureVisibilityMask(structure, pVisual);
                appendedCount += rc.pRenderer->AppendRenderSnapshotMeshes(snapshot, mask);
            }
            else
            {
                appendedCount += rc.pRenderer->AppendRenderSnapshotMeshesFrustumCulled(snapshot, matViewProj);
            }
```

---

## 2. 검증

미검증 (이번 반영 범위 밖 — 빌드 보류):
- C++ 빌드 미실행 (사용자 인게임 테스트 중 — 신호 후 수행)
- 인게임 E2E 미실행
- 포탑 [0]Base 단독 외형이 의도 외형과 일치하는지 (시각 게이트)

이번 턴 수행 (결과):
- 팩 재생성 exit 0 + `--check` PASS (0x266FF061) + generated.cpp 포탑 상태 28라인 반영 확인
- `git diff --check` / `go vet ./internal/profile/` 통과
- ACTIVE_WORK_PACKETS.md에 S035 패킷 등록(+M0 Active 충돌 고지), gotchas 재발 방지 1건 추가

빌드/E2E (사용자 신호 후):
- 솔루션 빌드 (전체 — MSBuild `-t:프로젝트명`은 이 솔루션에서 MSB4057로 깨짐)
- `Tools/Bin/Debug/SimLab.exe 1800 42` — 골든 해시 **18110C0D**(칼리스타 세션 갱신 정본) 불변 확인 (이 세션은 GameSim 시뮬 로직 무접촉)
- 인게임 E2E: ①넥서스/억제기/포탑 HP바 표시 ②포탑 카메라 이동 시 울렁거림 소멸(양 팀)+외형 확인 ③F4 Low HP→넥서스 파괴→승리 오버레이→메인메뉴 ④게임시작 재클릭→SeatSelect/밴픽부터 새 게임(파괴 상태 잔존 없음) ⑤서버 로그 "[GameRoom] Match reset after game end" ⑥회원 로그인+백엔드 기동 상태에서 게임종료→`/profile/me` MMR ±25·RP +150/75 반영, MyInfo 전적에 서버 기록 표시 ⑦백엔드 미기동+비회원으로 동일 사이클이 에러 없이 완주
- 백엔드 단독: `curl -X POST http://127.0.0.1:8084/profile/me/matches -H "Authorization: Bearer <token>" -d '{"result":"win"}'` → 200 + mmr_change=25

확인 필요:
- 팩 생성 스크립트(Build-LoLDefinitionPack.py 계열)의 위치/실행법 및 상태 수 상한 검증 로직
- 억제기 wmesh도 동일 클래스(중첩 서브메시) 여부 — 후속 점검 (glb 덤프로 확인 후 동일 패턴 등록)

후속(잔여):
- 포탑 alive 서브메시 시각 게이트 (F5 Model&Anim Lab Solo)
- 서버권위 매치결과 보고(신원 매핑+Kafka 생산자) — 클라 자기신고 대체
- match_history (user_id, match_id) unique 인덱스 (중복 방어)
- 멀티 휴먼: 게임종료 후 일부만 이탈 시 잔류자가 나갈 때까지 재입장 불가 (게이트가 워프만 차단) — UX 정리 별도
- ModelRenderer 자동 애니[0]=break1 잠재 지뢰 (구조물 애니 배선 시 idle 명시 필요)

## Next slice

사용자 빌드 신호 → 전체 솔루션 빌드 → SimLab 골든 → 인게임 E2E §2 순서. 실패 시 롤백 범위 = 본 문서 1-1~1-14 diff (팩은 스크립트 재실행 복원).
