# 2026-07-18 리플레이 Chrono 타임라인 + 프로필/메인메뉴 복귀 + 3클라 RP 계획서

```text
Session - 3클라 정상 종료→공통 RP 1000/전적/리플레이 확인 + Replay Chrono Break 탐색 + 프로필 수동 복귀 + 종료 후 메인메뉴 자동 복귀
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성, C8 검증이 병목
관련: 2026-07-15_AWS_BACKEND_ACCOUNT_REPLAY_LIBRARY_PLAN, 2026-07-16_AWS_BACKEND_ACCOUNT_REPLAY_IMPLEMENTATION_REPORT, 2026-07-12_S015_CODEX_LIVE_AUTHORING_CHRONOBREAK_FOLLOWUP_PLAN, Replay/01_SERVER_REPLAY_RECORD_AND_CLIENT_ENTRY_PLAN
```

## 1. 결정 기록

```text
① 문제·제약: Replay UI는 280×116 FirstUseEver 창이라 이미 배선된 “나의 정보로 돌아가기”가 잘릴 수 있다. CReplayPlayer는 순방향 커서뿐이고 SnapshotApplier는 동일 타임라인의 낮은 tick을 거부한다. 현재 MatchCompleted 보상은 승 150/패 75라 3클라 모두 +1000 촬영 목표와 다르다.
② 순진한 해법의 실패: tick/playhead만 낮추면 낮은 snapshot이 거부되고 이벤트 dedupe·투사체/FX·게임종료 latch가 미래 상태로 남는다. 파일 처음부터 매 드래그마다 선형 재생하면 대형 WRPL에서 O(N) 지연이 생긴다. UI 크기만 늘리는 방식은 저장된 ImGui 창 상태 때문에 다시 잘릴 수 있다.
③ 메커니즘: 로드 시 `deltaBaseTick==0`인 full snapshot 레코드 offset index를 별도로 만들고 upper_bound로 목표 이하 최신 full anchor를 O(log N) 선택한 뒤 목표 tick까지 순방향 적용한다. 탐색 전 SnapshotApplier/씬/이벤트 표현 상태를 rebase-reset한다. 하단 중앙 고정 NoSavedSettings 패널이 U64 timeline·재시작·재생/일시정지·속도·프로필/메인메뉴 버튼을 소유한다.
④ 경계: 이번 Chrono Break는 저장된 WRPL의 클라이언트 재생 위치 탐색이다. 서버 GameSim 체크포인트 복원·분기·명령 재시뮬레이션인 인게임 Chrono Break는 구현했다고 주장하지 않는다. WRPL 포맷과 서버 권위 흐름은 변경하지 않는다.
⑤ 대가: full snapshot index는 full snapshot당 size_t 1개 메모리를 추가한다. full anchor 사이 delta가 생기면 anchor→목표 구간은 선형 적용하지만 현재 서버 WRPL은 매 tick full snapshot이라 실질 seek는 한 tick group이다. 목표 이전에 시작된 일회성 transient FX의 정확한 나이/잔상은 재구성하지 않는다.
```

## 2. 범위와 완료 기준

- 예산: 70%는 seek/reset 정합성, RP 정책 테스트, Debug/Release 빌드와 3클라 정상 종료 절차에 배정한다. 30% ceiling은 하단 타임라인의 읽기 쉬운 배치, 시간 표기, 버튼 가시성에 배정한다.
- 포함: 정상 `MatchCompleted`의 `win/loss/draw` 참가자에게 각각 RP 1000, 리플레이 재실행 시 첫 tick부터 시작, timeline scrub, 수동 프로필/메인메뉴 복귀, 정상 재생 완료 후 2초 뒤 메인메뉴 복귀.
- 제외: ESC 강제 종료를 정상 경기 완료로 승격, F6 로컬 수동 저장을 계정 cloud replay로 승격, WRPL 포맷 변경, 서버 인게임 Chrono 분기, 과거 transient FX 전체 재시뮬레이션.
- 3클라 촬영의 필수 전제: 세 클라이언트가 같은 backend match assignment/signed ticket로 접속하고 넥서스 파괴로 종료해야 한다. F6 `Stop & Save` 또는 ESC 종료는 촬영 성공 경로로 사용하지 않는다.
- 완료 판정: Go 단위 테스트와 전체 서비스 테스트 통과, Client Debug/Release x64 빌드 통과, 유효 WRPL index smoke 통과, 수동 체크리스트에 세 계정 RP +1000/전적/replay와 timeline/복귀 동작을 기록한다.

## 3. 반영해야 하는 코드

### 3-1. C:/Users/user/Desktop/Winters/Services/internal/profile/consumer.go

`type Consumer struct` 위에 완료 경기 보상 정책을 추가:

```go
const completedMatchRewardRP int64 = 1000

func rewardRPForMatchResult(result string) int64 {
	switch result {
	case "win", "loss", "draw":
		return completedMatchRewardRP
	default:
		return 0
	}
}
```

`handleMessage`의 참가자 보상 계산 기존 코드:

```go
	for _, p := range event.Players {
		rpReward := int64(0)
		if p.Result == "win" {
			rpReward = 150
		} else if p.Result == "loss" {
			rpReward = 75
		}
		if err := c.repo.ReportMatch(ctx, p.UserID, event.MatchID, p, rpReward); err != nil {
```

아래로 교체:

```go
	for _, p := range event.Players {
		rpReward := rewardRPForMatchResult(p.Result)
		if err := c.repo.ReportMatch(ctx, p.UserID, event.MatchID, p, rpReward); err != nil {
```

### 3-2. 새 파일: C:/Users/user/Desktop/Winters/Services/internal/profile/consumer_test.go

전체 파일 본문:

```go
package profile

import "testing"

func TestRewardRPForMatchResult(t *testing.T) {
	tests := []struct {
		name   string
		result string
		want   int64
	}{
		{name: "winner", result: "win", want: 1000},
		{name: "loser", result: "loss", want: 1000},
		{name: "draw", result: "draw", want: 1000},
		{name: "missing", result: "", want: 0},
		{name: "aborted", result: "aborted", want: 0},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			if got := rewardRPForMatchResult(test.result); got != test.want {
				t.Fatalf("rewardRPForMatchResult(%q) = %d, want %d", test.result, got, test.want)
			}
		})
	}
}
```

### 3-3. C:/Users/user/Desktop/Winters/Client/Public/Replay/ReplayPlayer.h

`Update(...)` 선언 아래에 탐색 API 추가:

```cpp
	bool_t SeekToTick(
		u64_t targetTick,
		CWorld& world,
		EntityIdMap& entityMap,
		CSnapshotApplier& snapshotApplier,
		CEventApplier& eventApplier);
```

getter 묶음의 `GetLastTick()` 아래에 추가:

```cpp
	u64_t GetFirstSeekableTick() const
	{
		return m_FullSnapshotRecordIndices.empty()
			? m_Header.firstTick
			: m_RecordIndex[m_FullSnapshotRecordIndices.front()].header.serverTick;
	}
	f32_t GetTickRate() const { return m_fTickRate; }
```

`m_RecordIndex` 아래에 full snapshot index 추가:

```cpp
	std::vector<size_t> m_FullSnapshotRecordIndices{};
```

### 3-4. C:/Users/user/Desktop/Winters/Client/Private/Replay/ReplayPlayer.cpp

표준 include 묶음에 추가:

```cpp
#include <algorithm>
```

로드 시 reserve 기존 코드 아래:

```cpp
	player->m_RecordIndex.reserve(player->m_Header.recordCount);
	player->m_FullSnapshotRecordIndices.reserve(player->m_Header.snapshotCount);
```

record type 카운트 기존 코드:

```cpp
		if (recordType == Winters::Replay::eReplayRecordType::Snapshot)
			++snapshotCount;
```

아래로 교체:

```cpp
		if (recordType == Winters::Replay::eReplayRecordType::Snapshot)
		{
			++snapshotCount;
			const auto* snapshot = Shared::Schema::GetSnapshot(
				player->m_PayloadScratch.data());
			if (snapshot && snapshot->deltaBaseTick() == 0u)
			{
				player->m_FullSnapshotRecordIndices.push_back(
					player->m_RecordIndex.size());
			}
		}
```

header summary 검증 뒤 full anchor가 없으면 재생을 거부하고, 초기 cursor/playhead도 header 첫 record가 아니라 첫 full snapshot tick/group으로 맞춘다:

```cpp
	if (player->m_FullSnapshotRecordIndices.empty())
	{
		outError = "replay has no full snapshots";
		return nullptr;
	}

	const size_t firstSnapshotIndex =
		player->m_FullSnapshotRecordIndices.front();
	const u64_t firstSnapshotTick =
		player->m_RecordIndex[firstSnapshotIndex].header.serverTick;
	player->m_iNextRecord = firstSnapshotIndex;
	while (player->m_iNextRecord > 0u &&
		player->m_RecordIndex[player->m_iNextRecord - 1u].header.serverTick ==
			firstSnapshotTick)
	{
		--player->m_iNextRecord;
	}
	player->m_iCurrentTick = firstSnapshotTick;
	player->m_fPlayheadTick = static_cast<double>(firstSnapshotTick);
```

`SetPlaybackRate` 위에 탐색 구현 추가:

```cpp
bool_t CReplayPlayer::SeekToTick(
	u64_t targetTick,
	CWorld& world,
	EntityIdMap& entityMap,
	CSnapshotApplier& snapshotApplier,
	CEventApplier& eventApplier)
{
	if (m_RecordIndex.empty() || m_FullSnapshotRecordIndices.empty())
	{
		SetPlaybackError("replay seek requires snapshots");
		return false;
	}

	targetTick = (std::max)(m_Header.firstTick, targetTick);
	targetTick = (std::min)(m_Header.lastTick, targetTick);

	const auto upper = std::upper_bound(
		m_FullSnapshotRecordIndices.begin(),
		m_FullSnapshotRecordIndices.end(),
		targetTick,
		[this](u64_t tick, size_t recordIndex)
		{
			return tick < m_RecordIndex[recordIndex].header.serverTick;
		});

	size_t snapshotIndex = m_FullSnapshotRecordIndices.front();
	if (upper != m_FullSnapshotRecordIndices.begin())
		snapshotIndex = *(upper - 1);
	else
		targetTick = m_RecordIndex[snapshotIndex].header.serverTick;

	size_t begin = snapshotIndex;
	const u64_t snapshotTick = m_RecordIndex[snapshotIndex].header.serverTick;
	while (begin > 0u && m_RecordIndex[begin - 1u].header.serverTick == snapshotTick)
		--begin;

	m_strPlaybackError.clear();
	m_bFinished = false;
	m_iNextRecord = begin;
	m_iCurrentTick = snapshotTick;
	m_fPlayheadTick = static_cast<double>(targetTick);
	snapshotApplier.ResetForReplaySeek(world, entityMap, targetTick);

	bool_t bApplied = false;
	while (m_iNextRecord < m_RecordIndex.size() &&
		m_RecordIndex[m_iNextRecord].header.serverTick <= targetTick)
	{
		const u64_t tick = m_RecordIndex[m_iNextRecord].header.serverTick;
		const size_t groupBegin = m_iNextRecord;
		size_t groupEnd = groupBegin;
		while (groupEnd < m_RecordIndex.size() &&
			m_RecordIndex[groupEnd].header.serverTick == tick)
		{
			++groupEnd;
		}

		bApplied = ApplyTickGroup(
			groupBegin,
			groupEnd,
			world,
			entityMap,
			snapshotApplier,
			eventApplier) || bApplied;
		if (!m_strPlaybackError.empty())
			return false;

		m_iCurrentTick = tick;
		m_iNextRecord = groupEnd;
	}

	m_bFinished = m_iNextRecord >= m_RecordIndex.size();
	return bApplied;
}
```

### 3-5. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/SnapshotApplier.h

`OnSnapshot(...)` 선언 아래에 추가:

```cpp
    void ResetForReplaySeek(
        CWorld& world,
        EntityIdMap& entityMap,
        u64_t targetTick);
```

### 3-6. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

`OnSnapshot(...)` 바로 위에 추가. 기존 미커밋 respawn 반영부와 겹치지 않는다:

```cpp
void CSnapshotApplier::ResetForReplaySeek(
    CWorld& world,
    EntityIdMap& entityMap,
    u64_t targetTick)
{
    const SnapshotTimelineState previousTimeline = m_timelineState;
    const u32_t replayLocalNetId = m_localNetId;

    m_localMoveYawProtection = {};
    for (u32_t netId : m_ezrealPassiveNetIds)
    {
        RemoveEzrealPassivePresentation(
            world,
            entityMap.FromNet(netId));
    }
    m_ezrealPassiveNetIds.clear();
    ClearRemoteEntitiesForTimelineRebase(
        world,
        entityMap,
        replayLocalNetId);
    m_seenNetIds.clear();

    m_lastServerTick = 0u;
    m_lastSnapshotTick = 0u;
    m_localNetId = 0u;
    m_lastHelloNetId = 0u;
    m_lastSnapshotNetId = 0u;
    m_lastAckedCommandSequence = 0u;
    m_serverGameplayPackHash = 0u;
    m_serverGameplayPackRevision = 0u;
    m_timelineState = {};
    m_bHasTimelineState = false;

    if (m_onTimelineRebase)
    {
        m_onTimelineRebase(
            previousTimeline,
            SnapshotTimelineState{},
            targetTick);
    }
    else if (m_pEventApplier)
    {
        m_pEventApplier->RebaseTimeline(world, entityMap);
    }
}
```

### 3-7. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

`RebaseTimeline` 말미 기존 코드:

```cpp
    m_reconcileServerTick = 0u;
    m_bReconcileFullSnapshot = false;
}
```

아래로 교체:

```cpp
    m_reconcileServerTick = 0u;
    m_bReconcileFullSnapshot = false;
    m_bGameEndPending = false;
    m_uGameEndWinningTeam = 0u;
}
```

### 3-8. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

replay method 선언 묶음:

```cpp
    void UpdateReplayPlayback(f32_t dt);
    void DrawReplayControlPanel();
    bool_t SendStopReplayRequest();
```

아래로 교체:

```cpp
    void UpdateReplayPlayback(f32_t dt);
    bool_t SeekReplayToTick(u64_t targetTick);
    void DrawReplayControlPanel();
    bool_t SendStopReplayRequest();
```

replay 상태 멤버의 `m_bReplayStopRequested` 아래에 추가:

```cpp
    f32_t m_fReplayAutoReturnRemainingSec = -1.f;
```

### 3-9. C:/Users/user/Desktop/Winters/Client/Public/ClientShell/ClientShellBackendService.h

public request 묶음에 post-match refresh API 추가:

```cpp
	void RequestPostMatchRefresh();
```

private helper 묶음에 추가:

```cpp
	void RequestProfileSync();
	void TryStartPostMatchRefresh();
```

bool 상태 말미에 추가:

```cpp
	bool_t m_bPostMatchRefreshPending = false;
	bool_t m_bMatchHistoryRefreshPending = false;
```

### 3-10. C:/Users/user/Desktop/Winters/Client/Private/ClientShell/ClientShellBackendService.cpp

`ProcessCallbacks` 말미에 pending refresh 시작을 추가:

```cpp
	TryFinishDeferredReset();
	TryStartPostMatchRefresh();
	if (m_bMatchHistoryRefreshPending && !m_bProfileRequestInFlight)
		RequestMatchHistory();
```

`RequestInitialSync`의 inline profile request를 private `RequestProfileSync()` 호출로 추출하고, 아래 post-match refresh를 추가한다. 기존 요청이 남아 있으면 client를 reset하지 않고 완료까지 기다린 뒤 fresh profile/storefront/replay를 시작한다. 같은 profile HTTP client의 history는 profile 완료까지 pending latch로 직렬화한다:

```cpp
void CClientShellBackendService::RequestProfileSync()
{
	if (!m_bConfigured || !m_pProfileClient || m_bProfileRequestInFlight)
		return;

	m_bProfileRequestInFlight = true;
	const u32_t uGeneration = m_uGeneration;
	m_pProfileClient->GetMyProfile(
		[this, uGeneration](const Client::ProfileData& profile)
			{
				m_bProfileRequestInFlight = false;
				if (uGeneration == m_uGeneration)
				{
					CClientShellDataStore::Instance().ApplyProfileData(profile);
					m_strStatus = profile.error.empty()
						? "Profile synced"
						: "Profile sync failed: " + profile.error;
				}
				TryFinishDeferredReset();
			});
}

void CClientShellBackendService::RequestPostMatchRefresh()
{
	if (!m_bConfigured)
		return;

	m_bPostMatchRefreshPending = true;
	TryStartPostMatchRefresh();
}

void CClientShellBackendService::TryStartPostMatchRefresh()
{
	if (!m_bConfigured || HasInFlightRequests())
		return;

	if (m_bPostMatchRefreshPending)
	{
		m_bPostMatchRefreshPending = false;
		RequestProfileSync();
		RequestStorefrontSync();
		RequestReplayLibrary();
		RequestMatchHistory();
		return;
	}
}
```

`RequestInitialSync` 본문은 아래처럼 단순화:

```cpp
void CClientShellBackendService::RequestInitialSync()
{
	if (!m_bConfigured || m_bInitialSyncRequested)
		return;

	m_bInitialSyncRequested = true;
	RequestProfileSync();
	RequestStorefrontSync();
}
```

`RequestMatchHistory`는 profile 요청 중이면 유실시키지 않고 pending으로 전환:

```cpp
	if (!m_bConfigured || !m_pProfileClient)
		return;
	if (m_bProfileRequestInFlight)
	{
		m_bMatchHistoryRefreshPending = true;
		return;
	}
	m_bMatchHistoryRefreshPending = false;
```

`DestroyClients` 말미에 pending flag도 초기화:

```cpp
	m_bPostMatchRefreshPending = false;
	m_bMatchHistoryRefreshPending = false;
```

### 3-11. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

ClientShell include 묶음에 `#include "ClientShell/ClientShellBackendService.h"` 추가.

`PollGameEndAndSettings` 진입부에 추가하여 replay의 GameEnd를 라이브 승패 overlay/로컬 전적으로 소비하지 않음:

```cpp
void CScene_InGame::PollGameEndAndSettings()
{
    if (m_bReplayPlaybackMode)
        return;

    if (m_pEventApplier && !m_bGameEndActive)
```

`ChangeToMyInfoScene` 전체를 교체:

```cpp
void CScene_InGame::ChangeToMyInfoScene()
{
    CClientShellBackendService::Instance().RequestPostMatchRefresh();

    CGameInstance::Get()->Change_Scene(
        static_cast<uint32_t>(eSceneID::MyInfo),
        CScene_MyInfo::Create());
}
```

### 3-12. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameImGui.cpp

표준 include 추가:

```cpp
#include <algorithm>
```

`UpdateReplayPlayback` 전체를 교체하고 탐색 helper를 아래에 추가:

```cpp
void CScene_InGame::UpdateReplayPlayback(f32_t dt)
{
    if (!m_pReplayPlayer || !m_pEntityIdMap || !m_pSnapshotApplier || !m_pEventApplier)
        return;

    const bool_t bWasFinished = m_pReplayPlayer->IsFinished();
    if (m_pReplayPlayer->Update(
        dt,
        m_World,
        *m_pEntityIdMap,
        *m_pSnapshotApplier,
        *m_pEventApplier))
    {
        ProjectGameplayActorsToMapSurface();
    }

    const std::string& playbackError = m_pReplayPlayer->GetPlaybackError();
    if (!playbackError.empty())
    {
        m_strReplayStatus = playbackError;
        m_fReplayAutoReturnRemainingSec = -1.f;
        return;
    }

    constexpr f32_t kReplayAutoReturnDelaySec = 2.f;
    if (!bWasFinished &&
        m_pReplayPlayer->IsFinished() &&
        !m_pReplayPlayer->IsPaused())
    {
        m_fReplayAutoReturnRemainingSec = kReplayAutoReturnDelaySec;
    }
    else if (m_fReplayAutoReturnRemainingSec >= 0.f)
    {
        m_fReplayAutoReturnRemainingSec =
            (std::max)(0.f, m_fReplayAutoReturnRemainingSec - dt);
    }

    if (m_fReplayAutoReturnRemainingSec < 0.f)
        return;

    m_strReplayStatus = "Replay complete - returning to main menu";
    if (m_fReplayAutoReturnRemainingSec <= 0.f)
        m_bReturnToMainMenuRequested = true;
}

bool_t CScene_InGame::SeekReplayToTick(u64_t targetTick)
{
    if (!m_pReplayPlayer || !m_pEntityIdMap || !m_pSnapshotApplier || !m_pEventApplier)
        return false;

    m_fReplayAutoReturnRemainingSec = -1.f;
    const bool_t bApplied = m_pReplayPlayer->SeekToTick(
        targetTick,
        m_World,
        *m_pEntityIdMap,
        *m_pSnapshotApplier,
        *m_pEventApplier);
    if (bApplied)
    {
        ProjectGameplayActorsToMapSurface();
        m_strReplayStatus = "Replay Chrono seek complete";
        return true;
    }

    const std::string& error = m_pReplayPlayer->GetPlaybackError();
    m_strReplayStatus = error.empty() ? "Replay Chrono seek failed" : error;
    return false;
}
```

`DrawReplayControlPanel`의 replay playback 분기 전체를 고정형 하단 패널로 교체. recording 분기는 기존 `Stop & Save Replay` 창을 유지:

```cpp
void CScene_InGame::DrawReplayControlPanel()
{
    if (m_bReplayPlaybackMode)
    {
        ImGuiIO& io = ImGui::GetIO();
        const f32_t availableWidth = (std::max)(320.f, io.DisplaySize.x - 40.f);
        const ImVec2 windowSize((std::min)(920.f, availableWidth), 176.f);
        ImGui::SetNextWindowPos(
            ImVec2(
                (io.DisplaySize.x - windowSize.x) * 0.5f,
                io.DisplaySize.y - windowSize.y - 20.f),
            ImGuiCond_Always);
        ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

        constexpr ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings;
        if (!ImGui::Begin("Replay Chrono Break", nullptr, flags))
        {
            ImGui::End();
            return;
        }

        if (m_pReplayPlayer)
        {
            const u64_t firstTick = m_pReplayPlayer->GetFirstSeekableTick();
            const u64_t lastTick = m_pReplayPlayer->GetLastTick();
            u64_t selectedTick = (std::max)(
                firstTick,
                m_pReplayPlayer->GetCurrentTick());
            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::SliderScalar(
                "##ReplayTimeline",
                ImGuiDataType_U64,
                &selectedTick,
                &firstTick,
                &lastTick,
                "%llu"))
            {
                m_pReplayPlayer->SetPaused(true);
                SeekReplayToTick(selectedTick);
            }

            const f32_t tickRate = m_pReplayPlayer->GetTickRate();
            const double currentSec = tickRate > 0.f
                ? static_cast<double>(selectedTick - firstTick) / tickRate
                : 0.0;
            const double totalSec = tickRate > 0.f
                ? static_cast<double>(lastTick - firstTick) / tickRate
                : 0.0;
            ImGui::Text("Time: %.2f / %.2f sec", currentSec, totalSec);

            if (ImGui::Button("Restart"))
            {
                if (SeekReplayToTick(firstTick))
                    m_pReplayPlayer->SetPaused(false);
            }
            ImGui::SameLine();
            const bool_t bPaused = m_pReplayPlayer->IsPaused();
            if (ImGui::Button(bPaused ? "Play" : "Pause"))
                m_pReplayPlayer->SetPaused(!bPaused);
            ImGui::SameLine();
            f32_t speed = m_pReplayPlayer->GetPlaybackRate();
            ImGui::SetNextItemWidth(180.f);
            if (ImGui::SliderFloat("Speed", &speed, 0.25f, 4.f, "%.2fx"))
                m_pReplayPlayer->SetPlaybackRate(speed);

            if (ImGui::Button("프로필로 돌아가기"))
                m_bExitReplayToMyInfoRequested = true;
            ImGui::SameLine();
            if (ImGui::Button("메인 메뉴로"))
                m_bReturnToMainMenuRequested = true;
        }

        ImGui::TextUnformatted(
            m_strReplayStatus.empty() ? "Replay playback" : m_strReplayStatus.c_str());
        ImGui::End();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(320.f, 100.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Replay Recording"))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Stop & Save Replay"))
        SendStopReplayRequest();
    ImGui::TextUnformatted(
        m_strReplayStatus.empty() ? "Recording on server" : m_strReplayStatus.c_str());
    ImGui::End();
}
```

### 3-13. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameLifecycle.cpp

코드 변경 없음. `OnEnter`의 replay 분기가 매 scene 진입마다 `CReplayPlayer::LoadFromFile`로 새 인스턴스를 만들고 `firstTick`으로 초기화하는 기존 근거를 회귀 확인한다. 따라서 프로필에서 같은 replay를 다시 실행하면 처음부터 시작한다.

## 4. 검증 예측과 명령

1. RP 단위/서비스 테스트
   - 예측: `win/loss/draw`는 1000, 빈 값/`aborted`는 0. 기존 profile repository/consumer 테스트도 통과.
   - 명령: `gofmt -w internal/profile/consumer.go internal/profile/consumer_test.go`
   - 명령: `go test ./...` (`Services` 작업 디렉터리)
2. 정적 검사
   - 예측: whitespace error 0, replay 변경 이외 기존 dirty diff 보존.
   - 명령: `git diff --check`
   - 명령: `git diff -- <이번 대상 파일들>`로 변경선 소유권 확인.
3. Client 빌드
   - 예측: `Client.vcxproj` Debug|x64, Release|x64 각각 오류 0.
   - 명령: MSBuild `Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1`
   - 명령: MSBuild `Client/Include/Client.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1`
4. WRPL smoke
   - 예측: 기존 유효 replay가 Debug/Release 모두 index-load되고 trailing/FlatBuffer 검증 오류가 없으며 full snapshot anchor가 1개 이상이다.
   - 명령: 각 산출물의 `--replay-index-smoke=<유효 WRPL 절대 경로>`.
5. 수동 Replay Chrono acceptance
   - 프로필에서 replay 실행 → 첫 tick부터 재생.
   - 하단 중앙 timeline을 전/후로 드래그 → 캐릭터/구조물/스코어가 선택 시간 snapshot으로 이동, 과거 snapshot 거부 로그 없음.
   - `Restart` → 첫 tick으로 이동 후 재생. `프로필로 돌아가기` → MyInfo에서 최신 RP/전적/replay 재요청. `메인 메뉴로` → MainMenu.
   - 자연 재생으로 끝 tick 도달 → 2초 상태 표시 후 MainMenu 자동 복귀. slider로 끝 tick을 직접 선택한 경우에는 일시정지 상태를 유지하고 자동 복귀하지 않는다.
6. 3클라 계정 E2E/촬영 체크리스트
   - 동일 backend match assignment의 서버 1 + 클라 3으로 시작하고 F6/ESC를 사용하지 않은 채 넥서스를 파괴.
   - replay service 상태 `ready`, match completion 소비 완료 후 세 계정 각각 RP 이전값 대비 +1000, 전적 1건, cloud replay 1건 확인.
   - 각 클라 MyInfo에서 독립적으로 전적과 replay를 열고 timeline seek/Restart/프로필 복귀/자동 메인메뉴 복귀를 촬영.

## 5. 서브 에이전트 비평

```text
검토: /root/replay_plan_fast_critique 읽기 전용 비평 완료. source edit 전 반영.
P0 수용 — 모든 snapshot index를 full snapshot(`deltaBaseTick==0`) anchor index로 교체하고 anchor부터 목표까지 적용한다. 현재 replay snapshot이 full이라는 서버 근거가 있어도 포맷 소비자는 delta를 안전하게 처리해야 한다.
P1 수용 — `ConfigureFromSession` 재호출을 폐기하고 in-flight 완료를 기다리는 `RequestPostMatchRefresh` latch를 ClientShellBackendService에 추가한다. profile/storefront/replay를 새로 요청하고 같은 profile client의 history는 profile 완료 직후 pending latch로 시작한다.
P1 수용 — 사용자가 끝 tick으로 scrub한 것은 자연 완료가 아니므로 `false→true` finished 전이이면서 재생 중일 때만 2초 자동 복귀 타이머를 시작한다.
P1 부분 수용 — RP helper unit test는 유지하고, `ReportMatch`의 `(user_id, match_id) ON CONFLICT DO NOTHING` + 단일 transaction 근거와 3클라 E2E의 history 1행/RP 정확히 +1000 검증을 명시한다. 별도 DB consumer 통합 test harness 신설은 이번 UI/replay 슬라이스를 넘고 기존 repository가 재전달 멱등 경계를 이미 소유하므로 기각한다.
P2 수용 — slider/Restart 최소값은 header first record가 아니라 첫 full snapshot tick인 `GetFirstSeekableTick()`을 사용한다.
```

## 6. 인계/중단 기준

- snapshot reset이 같은 파일의 기존 respawn 작업과 충돌하면 해당 메서드 추가 위치만 재선정하고 기존 변경은 수정하지 않는다.
- post-match refresh는 backend client reset 없이 pending latch로 직렬화한다. fresh profile/storefront/replay와 profile 완료 후 history가 실제로 시작되지 않으면 완료로 판정하지 않는다.
- 실제 3클라 계정 E2E는 계정/서버 런타임 조작이 필요한 수동 acceptance다. 빌드·로컬 smoke만 수행한 경우 결과서에서 이를 통과로 과장하지 않는다.
