# Phase 6A~6E 코드 중심 검토 노트

작성일: 2026-05-06

목적:
- 6A~6E에서 무엇을 구현했거나 구현해야 하는지 코드 기준으로 확인한다.
- 사용자가 직접 검토 후 필요한 부분을 골라 반영할 수 있게, 파일 단위와 코드 스케치 중심으로 정리한다.

상태 기준:

| Phase | 상태 | 핵심 |
|---|---|---|
| 6A | DONE, 현재 워킹트리 반영 | BanPick TCP Lobby 런타임 잠금, 디버그 메시지, netId 안전핀 |
| 6B | DONE, 현재 워킹트리 반영 | LoL 챔피언 렌더링을 PBR이 아닌 diffuse 중심으로 복구 |
| 6C | REVIEW/TODO | Legacy champion spawn/render 경로 제거, roster/ECS/network 단일 경로화 |
| 6D | REVIEW/TODO | InGameScene 구조 분리 |
| 6E | REVIEW/TODO | Hybrid filters 통합, 루트/가상 필터 혼재 정리 |

---

## Phase 6A - BanPick TCP Runtime Lock

### 구현 목적

서버 1개 + 클라이언트 3개 기준으로 BanPick에서 다음 상태를 바로 확인 가능하게 만든다.

- 현재 lobby revision
- lobby phase
- 내 sessionId / netId / slotId
- 마지막으로 보낸 LobbyCommand
- 서버가 방금 수락/거절한 이유
- StartGame 중복 수신 여부
- InGame 진입 후 roster netId와 Hello netId mismatch 여부

### 변경 파일

```txt
Shared/Schemas/LobbyState.fbs
Shared/Schemas/Generated/cpp/LobbyState_generated.h
Shared/Schemas/Generated/go/Shared/Schema/LobbyState.go

Client/Public/Network/Client/GameSessionClient.h
Client/Private/Network/Client/GameSessionClient.cpp
Client/Private/Scene/Scene_BanPick.cpp
Client/Private/Scene/Scene_InGame.cpp

Server/Public/Game/GameRoom.h
Server/Private/Game/GameRoom.cpp
```

### 6A-1. LobbyState에 서버 메시지 추가

파일: `Shared/Schemas/LobbyState.fbs`

```fbs
table LobbyState {
    roomId:uint;
    revision:uint;
    hostSessionId:uint;
    phase:LobbyPhase;
    allPlayersCanEditBots:bool;
    slots:[LobbySlot];
    startCountdownMs:uint;
    debugMessage:string;
}
```

재생성 명령:

```bat
cmd /c Shared\Schemas\run_codegen.bat
```

생성 확인 포인트:

```cpp
// Shared/Schemas/Generated/cpp/LobbyState_generated.h
const ::flatbuffers::String *debugMessage() const;
```

```go
// Shared/Schemas/Generated/go/Shared/Schema/LobbyState.go
func (rcv *LobbyState) DebugMessage() []byte
```

### 6A-2. 클라이언트 세션 디버그 상태 추가

파일: `Client/Public/Network/Client/GameSessionClient.h`

추가된 공개 조회 API:

```cpp
u32_t GetLobbyRevision() const { return m_uLobbyRevision; }
u8_t GetLobbyPhase() const { return m_uLobbyPhase; }
u32_t GetGameStartCount() const { return m_uGameStartCount; }
const char* GetLastLobbyMessage() const { return m_strLastLobbyMessage.c_str(); }
const char* GetLastLobbyCommandText() const { return m_strLastLobbyCommandText.c_str(); }
```

추가된 상태:

```cpp
u32_t m_uLobbyRevision = 0;
u8_t m_uLobbyPhase = 0;
u32_t m_uGameStartCount = 0;
std::string m_strLastLobbyMessage;
std::string m_strLastLobbyCommandText;
```

파일: `Client/Private/Network/Client/GameSessionClient.cpp`

LobbyCommand 송신 시 마지막 명령 문자열 저장:

```cpp
m_strLastLobbyCommandText = BuildLobbyCommandText(kind, slotId, champion, botDifficulty, value);

if (!IsConnected())
{
    m_strLastLobbyMessage = "client reject: lobby server is not connected";
    OutputDebugStringA("[GameSessionClient] lobby command rejected locally: disconnected\n");
    return false;
}
```

`GameStart` 중복 수신 감지:

```cpp
else if (type == ePacketType::GameStart)
{
    ++m_uGameStartCount;
    if (m_uGameStartCount > 1)
        OutputDebugStringA("[GameSessionClient] duplicate GameStart packet received\n");
    m_bGameStarting = true;
}
```

`LobbyState` 수신 시 revision/phase/debugMessage 반영:

```cpp
m_uLobbyRevision = state->revision();
m_uLobbyPhase = static_cast<u8_t>(state->phase());
m_strLastLobbyMessage.clear();
if (const auto* message = state->debugMessage())
    m_strLastLobbyMessage = message->str();
```

### 6A-3. 서버 LobbyCommand accept/reject 메시지

파일: `Server/Public/Game/GameRoom.h`

추가 상태:

```cpp
std::string m_strLastLobbyMessage;
```

추가 메서드:

```cpp
void SetLobbyMessageLocked(const std::string& message);
void SetLobbyMessageLocked(const char* message);
```

파일: `Server/Private/Game/GameRoom.cpp`

메시지 포맷 헬퍼:

```cpp
std::string FormatLobbyCommandLog(
    const char* result,
    u32_t sessionId,
    Shared::Schema::LobbyCommandKind kind,
    u8_t slotId,
    eChampion champion,
    const char* reason)
{
    char text[320]{};
    sprintf_s(
        text,
        "%s sid=%u cmd=%s slot=%u champ=%u reason=%s",
        result,
        sessionId,
        GetLobbyCommandKindName(kind),
        static_cast<u32_t>(slotId),
        static_cast<u32_t>(champion),
        reason ? reason : "-");
    return std::string(text);
}
```

Lobby phase가 아니면 명령 거절 후 상태 재전송:

```cpp
if (m_roomPhase != eRoomPhase::Lobby)
{
    SetLobbyMessageLocked(FormatLobbyCommandLog(
        "reject",
        sessionId,
        command->kind(),
        command->slotId(),
        static_cast<eChampion>(command->championId()),
        "room is not in lobby phase"));
    ++m_lobbyRevision;
    BroadcastLobbyStateLocked();
    return;
}
```

명령이 실패해도 이유를 client에 보낸다:

```cpp
if (bChanged)
{
    if (m_strLastLobbyMessage.empty())
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "accept",
            sessionId,
            command->kind(),
            command->slotId(),
            static_cast<eChampion>(command->championId()),
            "state changed"));
    }
    ++m_lobbyRevision;
    BroadcastLobbyStateLocked();
}
else
{
    if (m_strLastLobbyMessage.empty())
    {
        SetLobbyMessageLocked(FormatLobbyCommandLog(
            "reject",
            sessionId,
            command->kind(),
            command->slotId(),
            static_cast<eChampion>(command->championId()),
            "no state change"));
    }
    ++m_lobbyRevision;
    BroadcastLobbyStateLocked();
}
```

### 6A-4. Strict custom lobby 입장 흐름

파일: `Server/Private/Game/GameRoom.cpp`

현재 6A 기준:
- host는 접속 시 slot 0 점유
- 이후 클라이언트는 접속만 하고 직접 슬롯 선택

```cpp
void CGameRoom::OnLobbyJoin(u32_t sessionId)
{
    if (m_hostSessionId == 0)
    {
        m_hostSessionId = sessionId;
        TryJoinSlot(sessionId, 0);
        return;
    }

    SetLobbyMessageLocked(FormatLobbyCommandLog(
        "accept",
        sessionId,
        Shared::Schema::LobbyCommandKind::None,
        kInvalidGameRosterSlot,
        eChampion::END,
        "connected; choose a slot"));
}
```

슬롯 이동 시 이전 슬롯 정리:

```cpp
if (auto prevIt = m_sessionToSlot.find(sessionId); prevIt != m_sessionToSlot.end())
{
    LobbySlotState& oldSlot = m_lobbySlots[prevIt->second];
    previousChampion = oldSlot.champion;
    oldSlot.bHuman = false;
    oldSlot.sessionId = 0;
    oldSlot.netId = NULL_NET_ENTITY;
    oldSlot.champion = eChampion::END;
    oldSlot.botDifficulty = 2;
    oldSlot.bReady = false;
    oldSlot.bLocked = false;
}
```

### 6A-5. BanPick ImGui 디버그 표시

파일: `Client/Private/Scene/Scene_BanPick.cpp`

phase 표시:

```cpp
const char* GetLobbyPhaseLabel(u8_t phase)
{
    switch (static_cast<Shared::Schema::LobbyPhase>(phase))
    {
    case Shared::Schema::LobbyPhase::SeatSelect:
        return "SeatSelect";
    case Shared::Schema::LobbyPhase::ChampionSelect:
        return "ChampionSelect";
    case Shared::Schema::LobbyPhase::Locked:
        return "Locked";
    case Shared::Schema::LobbyPhase::Starting:
        return "Starting";
    case Shared::Schema::LobbyPhase::InGame:
        return "InGame";
    default:
        return "None";
    }
}
```

상단 디버그 표시:

```cpp
ImGui::TextDisabled("rev %u, phase %s, sid %u, net %u, slot %u, slots %u/10, humans %u, bots %u",
    session.GetLobbyRevision(),
    GetLobbyPhaseLabel(session.GetLobbyPhase()),
    session.GetMySessionId(),
    session.GetMyNetId(),
    static_cast<u32_t>(context.MySlotId),
    occupiedCount,
    humanCount,
    botCount);

if (session.GetLastLobbyCommandText()[0] != '\0')
    ImGui::TextDisabled("Last command: %s", session.GetLastLobbyCommandText());
if (session.GetLastLobbyMessage()[0] != '\0')
    ImGui::Text("Lobby: %s", session.GetLastLobbyMessage());
```

선택 슬롯 디버그 표시:

```cpp
ImGui::TextDisabled("slotId=%u team=%u session=%u net=%u botDifficulty=%u",
    static_cast<u32_t>(selected.slotId),
    static_cast<u32_t>(selected.team),
    selected.sessionId,
    selected.netId,
    static_cast<u32_t>(selected.botDifficulty));
```

StartGame 중복 송신 차단:

```cpp
const bool_t bCanSendStart = !session.IsGameStarting() && !m_bTransitionRequested;
if (!bCanSendStart)
    ImGui::TextDisabled("Game start is already in progress.");

if (bCanSendStart && ImGui::Button("Start Game", ImVec2(180.f, 42.f)))
{
    session.SendLobbyCommand(
        Shared::Schema::LobbyCommandKind::StartGame,
        0);
}
```

### 6A-6. InGame netId 안전핀

파일: `Client/Private/Scene/Scene_InGame.cpp`

Hello와 roster netId mismatch 감지:

```cpp
const GameContext& context = CGameInstance::Get()->Get_GameContext();
if (context.bUseNetworkRoster
    && context.MyNetId != 0
    && myNetId != 0
    && context.MyNetId != myNetId)
{
    char mismatch[192]{};
    sprintf_s(mismatch,
        "[Scene_InGame] netId mismatch roster=%u hello=%u sid=%u\n",
        context.MyNetId,
        myNetId,
        mySessionId);
    OutputDebugStringA(mismatch);
}
```

이미 바인딩된 netId는 중복 생성하지 않음:

```cpp
if (slot.netId != 0 && m_pEntityIdMap)
{
    const EntityID existing = m_pEntityIdMap->FromNet(slot.netId);
    if (existing != NULL_ENTITY)
    {
        char dbg[160]{};
        sprintf_s(dbg,
            "[ECS:Roster] reuse existing net=%u entity=%u slot=%u\n",
            slot.netId,
            static_cast<u32_t>(existing),
            static_cast<u32_t>(slot.slotId));
        OutputDebugStringA(dbg);
        return existing;
    }
}
```

### 6A 검증 명령

```bat
cmd /c Shared\Schemas\run_codegen.bat
```

```bat
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:CL_MPCount=1 /m:1 /v:minimal
```

```bat
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:CL_MPCount=1 /m:1 /v:minimal
```

확인 완료:
- FlatBuffers codegen OK
- Server Debug x64 build OK
- Client Debug x64 build OK
- 기존 PostBuild의 `pwsh.exe` 경고는 남아 있으나 MSBuild exit code는 0

---

## Phase 6B - Champion Diffuse Rendering Restore

### 6B 반영 결과

이번 반영은 PBR 파이프라인을 삭제하지 않고, LoL 챔피언 생성 경로만 diffuse 셰이더로 되돌리는 최소 변경이다.

변경 파일:

```txt
Shared/GameSim/Definitions/ChampionDef.h
Client/Private/GameObject/Champion/Yone/Yone_Registration.cpp
Client/Private/ECS/Systems/YoneSoulSpawnSystem.cpp
Client/Private/GameObject/ChampionTable.cpp
Client/Private/Scene/Scene_InGame.cpp
```

핵심 코드:

```cpp
// Shared/GameSim/Definitions/ChampionDef.h
// LoL champion assets are diffuse-only by default; PBR remains opt-in.
const wchar_t* shaderPath = L"Shaders/Mesh3D.hlsl";
```

```cpp
// Client/Private/GameObject/Champion/Yone/Yone_Registration.cpp
cd.shaderPath = L"Shaders/Mesh3D.hlsl";
```

```cpp
// Client/Private/ECS/Systems/YoneSoulSpawnSystem.cpp
if (!pRenderer->Init("Client/Bin/Resource/Texture/Character/Yone/yone.fbx",
    L"Shaders/Mesh3D.hlsl"))
{
    OutputDebugStringA("[YoneSoulSpawnSystem] soul renderer init failed\n");
    return;
}
```

```cpp
// Client/Private/Scene/Scene_InGame.cpp
m_Irelia.Init("Client/Bin/Resource/Texture/Character/Irelia/irelia_fixed.fbx",
    L"Shaders/Mesh3D.hlsl");
```

Scene_InGame의 legacy champion init 7곳도 전부 `Mesh3D.hlsl`로 맞췄다.

```txt
Irelia / Yasuo / Sylas / Viego / Kalista / Garen / Zed
```

LoL 챔피언이 PBR normal/AO pass의 영향을 받지 않도록 Scene_InGame의 SSAO도 기본 비활성으로 둔다. ImGui 튜너나 별도 렌더링 phase에서 다시 켤 수 있다.

```cpp
m_pSSAOPass = Engine::CSSAOPass::Create(pRhiDevice, g_iWinSizeX, g_iWinSizeY);
if (m_pSSAOPass)
    m_pSSAOPass->SetEnabled(false);
```

검증 grep:

```powershell
Get-ChildItem -Path Client\Private,Client\Public,Shared\GameSim -Recurse -File -Include *.cpp,*.h |
  Select-String -Pattern 'Mesh3D_PBR|Skinned3D_PBR'
```

현재 결과: LoL client/source 경로에서 PBR champion 참조 없음.

빌드 검증:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:CL_MPCount=1 /m:1 /v:minimal
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:CL_MPCount=1 /m:1 /v:minimal
```

현재 결과: 둘 다 exit code 0.

주의: 기존 DLL interface warning(C4251/C4275)과 PostBuild의 `pwsh.exe` 경고는 남아 있다.

### 구현 목적

LoL 챔피언 에셋은 현재 diffuse 텍스처 중심이다.
normal/metallic/roughness 텍스처가 없는 상태에서 PBR shader를 쓰면 점토색/찰흙빛이 나기 쉽다.

따라서 LoL 경로는 우선 diffuse 중심 렌더링으로 복구한다.

정책:
- LoL champion: `Shaders/Mesh3D.hlsl` 또는 기존 diffuse shader 사용
- PBR shader는 EldenRing 타겟 또는 PBR 텍스처가 실제 준비된 asset에 사용
- normal map 없음: 비활성
- metallic 없음: 0
- roughness 없음: 고정값이더라도 LoL champion path에서는 영향 최소화
- AO/SSAO는 챔피언을 어둡게 뭉개면 비활성 또는 약화

### 6B-1. 현재 PBR 판정 위치

파일: `Engine/Private/Renderer/ModelRenderer.cpp`

현재 구조:

```cpp
m_pImpl->bUsePBR = (shaderPath.find(L"PBR") != std::wstring::npos);

m_pImpl->pSharedMeshShader   = m_pImpl->bUsePBR ? app.GetMeshPBRShader() : app.GetMeshShader();
m_pImpl->pSharedMeshPipeline = m_pImpl->bUsePBR ? app.GetMeshPBRPipeline() : app.GetMeshPipeline();

m_pImpl->pSharedSkinnedShader   = m_pImpl->bUsePBR ? app.GetSkinnedPBRShader() : app.GetSkinnedShader();
m_pImpl->pSharedSkinnedPipeline = m_pImpl->bUsePBR ? app.GetSkinnedPBRPipeline() : app.GetSkinnedPipeline();
```

즉, `ChampionDef::shaderPath`가 `Mesh3D_PBR.hlsl`이면 PBR 경로를 탄다.

### 6B-2. 우선 반영 후보 - ChampionDef shaderPath 정리

이미 대부분 신규 pure ECS champion은 diffuse shader다.

확인된 예:

```cpp
// Ezreal/Fiora/Jax/Annie/Ashe
cd.shaderPath = L"Shaders/Mesh3D.hlsl";
```

현재 Yone은 PBR:

```cpp
// Client/Private/GameObject/Champion/Yone/Yone_Registration.cpp
cd.shaderPath = L"Shaders/Mesh3D_PBR.hlsl";
```

권장 변경:

```cpp
cd.shaderPath = L"Shaders/Mesh3D.hlsl";
```

### 6B-3. Legacy champion init 중 PBR 제거 후보

파일: `Client/Private/Scene/Scene_InGame.cpp`

현재 legacy path에 남아 있는 PBR init 예:

```cpp
m_Irelia.Init("Client/Bin/Resource/Texture/Character/Irelia/irelia_fixed.fbx",
    L"Shaders/Mesh3D_PBR.hlsl");

m_Yasuo.Init("Client/Bin/Resource/Texture/Character/Yasuo/yasuo_fixed.fbx",
    L"Shaders/Mesh3D_PBR.hlsl");

m_Kalista.Init("Client/Bin/Resource/Texture/Character/Kalista/kalista.fbx",
    L"Shaders/Mesh3D_PBR.hlsl");
```

6C에서 legacy path를 삭제할 계획이므로 둘 중 하나로 처리한다.

Option A - 6C에서 곧 지울 코드면 6B에서는 건드리지 않는다.

Option B - 사용자 눈으로 바로 확인해야 하면 임시로 diffuse shader로 바꾼다.

```cpp
m_Irelia.Init("Client/Bin/Resource/Texture/Character/Irelia/irelia_fixed.fbx",
    L"Shaders/Mesh3D.hlsl");
```

### 6B-4. PBR shader를 보존하되 LoL 기본값을 안전하게 둔다

파일: `Engine/Public/Renderer/CBPerMaterial.h`

현재 기본값:

```cpp
material.fMetallic = 0.0f;
material.fRoughness = 0.5f;
material.fAmbientOcclusion = 1.0f;
material.fEmissiveIntensity = 0.0f;
```

이 자체는 나쁘지 않지만, shader 쪽 ambient가 낮다.

파일: `Shaders/Mesh3D_PBR.hlsl`, `Shaders/Skinned3D_PBR.hlsl`

현재 ambient:

```hlsl
float3 ambient = float3(0.03f, 0.03f, 0.03f) * albedo * (g_fAmbientOcclusion * aoVisibility);
```

LoL diffuse 복구에서는 PBR shader 자체를 우회하는 게 1순위다.
PBR shader를 수정한다면 EldenRing용 품질에도 영향을 줄 수 있으므로 별도 shader로 분기한다.

권장 이름:

```txt
Shaders/Mesh3D_DiffuseLit.hlsl
Shaders/Skinned3D_DiffuseLit.hlsl
```

단, 현재 엔진에는 이미 `Mesh3D.hlsl` 계열 diffuse 경로가 있으므로 신규 shader 추가보다 `ChampionDef::shaderPath` 정리가 먼저다.

### 6B 완료 기준

- Yone/Fiora/Ezreal/Annie/Ashe/Jax가 `Mesh3D.hlsl` 경로로 렌더링
- `ModelRenderer::UsesPBR()`가 LoL champion에서 false
- diffuse texture가 mesh별로 정상 적용
- 찰흙빛/과도한 회색감 감소
- PBR shader 파일은 삭제하지 않음

---

## Phase 6C - Legacy Champion Full Removal

### 구현 목적

InGameScene의 champion 생성 경로를 하나로 만든다.

최종 경로:

```txt
BanPick LobbyState
-> GameContext.Roster[10]
-> Scene_InGame::CreateRosterChampionsFromGameContext()
-> CreateECSChampion()
-> m_ChampionRenderers[entity]
-> Server netId binding
```

삭제할 legacy 경로:
- `WINTERS_MIN_SCENE` 기반 분기
- `m_Irelia`, `m_Yasuo`, `m_Kalista`, `m_Viego`, `m_Garen`, `m_Zed` 같은 Scene 소유 champion renderer
- `CreateChampionEntity(ModelRenderer&, CTransform&, ...)` 중심 경로
- selectedChampion switch로 직접 champion을 만드는 fallback
- legacy renderer 직접 `Update/Render/Shutdown`

### 6C-1. Local fallback도 roster로 만든다

현재 local fallback은 이미 BanPick에서 `GameContext.Roster`를 만든다.
추가로 InGame 단독 진입 방어용 fallback을 둔다.

파일 후보: `Client/Private/Scene/Scene_InGame.cpp`

```cpp
void CScene_InGame::EnsureLocalRosterFallback()
{
    GameContext& context = CGameInstance::Get()->Get_GameContext();
    if (context.bUseNetworkRoster)
        return;

    const eChampion selected =
        context.SelectedChampion != eChampion::END && context.SelectedChampion != eChampion::NONE
            ? context.SelectedChampion
            : eChampion::EZREAL;

    context = GameContext{};
    context.bUseNetworkRoster = true;
    context.MySessionId = 1;
    context.MyNetId = 1;
    context.MySlotId = 0;
    context.MyTeam = 0;
    context.SelectedChampion = selected;

    GameRosterSlot& slot = context.Roster[0];
    slot.slotId = 0;
    slot.team = 0;
    slot.bHuman = true;
    slot.sessionId = context.MySessionId;
    slot.netId = context.MyNetId;
    slot.champion = selected;
}
```

OnEnter 초반에서 호출:

```cpp
EnsureLocalRosterFallback();
const GameContext& gameContext = CGameInstance::Get()->Get_GameContext();
```

### 6C-2. CreateECSEntities를 roster-only로 축소

현재는 network roster면 early return, 아니면 legacy path로 내려간다.

권장 구조:

```cpp
void CScene_InGame::CreateECSEntities()
{
    EnsureLocalRosterFallback();

    const bool_t bCreatedRoster = CreateRosterChampionsFromGameContext();
    CreateMapEntity();

    if (m_PlayerEntity != NULL_ENTITY)
    {
        BindPlayerToECSChampion(m_PlayerEntity);
        if (m_World.HasComponent<ChampionComponent>(m_PlayerEntity))
            m_PlayerTeam = m_World.GetComponent<ChampionComponent>(m_PlayerEntity).team;
    }

    if (m_bUsingSharedNetwork)
        CGameSessionClient::Instance().ReplayLastHelloToGameFrameCallback();

    char dbg[192]{};
    sprintf_s(dbg, "[ECS:RosterOnly] created=%d total=%u player=%u champion=%u\n",
        bCreatedRoster ? 1 : 0,
        m_World.GetEntityCount(),
        static_cast<u32_t>(m_PlayerEntity),
        static_cast<u32_t>(GetPlayerChampionId()));
    OutputDebugStringA(dbg);
}
```

### 6C-3. legacy renderer 멤버 제거 후보

파일: `Client/Public/Scene/Scene_InGame.h`

삭제 후보:

```cpp
ModelRenderer   m_Irelia;   CTransform m_IreliaTransform;
ModelRenderer   m_Yasuo;    CTransform m_YasuoTransform;
ModelRenderer   m_Sylas;    CTransform m_SylasTransform;
ModelRenderer   m_Viego;    CTransform m_ViegoTransform;
ModelRenderer   m_Kalista;  CTransform m_KalistaTransform;
ModelRenderer   m_Garen;    CTransform m_GarenTransform;
ModelRenderer   m_Zed;      CTransform m_ZedTransform;
```

주의:
- `m_SylasEntity`, `m_YasuoEntity`, `m_IreliaEntity` 같은 EntityID alias는 곧바로 삭제하지 않는다.
- 스킬/FX 코드가 champion별 entity alias를 참조하므로 먼저 `AssignPureECSChampionAlias()`로 계속 공급한다.
- 삭제 1순위는 renderer/transform 멤버다.

남겨도 되는 alias:

```cpp
EntityID m_IreliaEntity = NULL_ENTITY;
EntityID m_YasuoEntity = NULL_ENTITY;
EntityID m_SylasEntity = NULL_ENTITY;
EntityID m_FioraEntity = NULL_ENTITY;
EntityID m_YoneEntity = NULL_ENTITY;
```

최종적으로는 아래처럼 map 기반으로 바꾼다.

```cpp
std::unordered_map<eChampion, EntityID> m_ChampionAlias{};
```

### 6C-4. legacy Update/Render/Shutdown 제거 후보

삭제 후보 패턴:

```cpp
#if !WINTERS_MIN_SCENE
    m_Irelia.Update(dt);
    m_Yasuo.Update(dt);
    m_Kalista.Update(dt);
#endif
```

대체 원칙:
- ECS champion은 `m_ChampionRenderers`가 소유한다.
- 렌더링은 `RenderComponent`를 순회하는 helper만 사용한다.
- animation update도 `RenderComponent.pRenderer->Update(dt)`로 통일한다.

예상 통합 코드:

```cpp
m_World.ForEach<RenderComponent>(
    [&](EntityID entity, RenderComponent& rc)
    {
        if (!rc.bVisible || !rc.pRenderer || rc.bSceneManaged)
            return;

        rc.pRenderer->Update(dt);
    });
```

### 6C 완료 기준

- `Scene_InGame.cpp`에서 `WINTERS_MIN_SCENE` 제거
- legacy champion `ModelRenderer` 멤버 제거
- `CreateECSEntities()`가 roster-only
- BanPick에서 선택한 Yone/Fiora/Ezreal이 InGame에서 그대로 생성
- local fallback도 roster를 만들어 같은 path 사용
- 서버 netId가 있는 슬롯은 `EntityIdMap::Bind()`로 연결
- 첫 Snapshot 이후 동일 netId 중복 생성 없음

---

## Phase 6D - InGameScene 구조 정리

### 구현 목적

`Scene_InGame.cpp`가 너무 많은 책임을 가진 상태다.
Legacy 삭제 이후 다음 책임을 작은 단위로 나눈다.

분리 후보:

| 책임 | 새 파일 후보 |
|---|---|
| roster -> ECS champion spawn | `Client/Public/Scene/InGame/InGameRosterSpawner.h` |
| shared TCP session / snapshot / hello | `Client/Public/Scene/InGame/InGameNetworkBridge.h` |
| RenderComponent update/render helper | `Client/Public/Scene/InGame/InGameRenderBridge.h` |
| map spawn/create | `Client/Public/Scene/InGame/InGameMapBootstrap.h` |
| debug ImGui | `Client/Public/Scene/InGame/InGameDebugPanel.h` |

### 6D-1. RosterSpawner 스케치

```cpp
class CInGameRosterSpawner final
{
public:
    struct Desc
    {
        CWorld* pWorld = nullptr;
        EntityIdMap* pEntityMap = nullptr;
        std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>>* pRenderers = nullptr;
    };

    bool Initialize(const Desc& desc);
    bool SpawnFromContext(const GameContext& context, EntityID* pOutPlayerEntity);

private:
    EntityID SpawnOne(const GameRosterSlot& slot);
};
```

`Scene_InGame` 사용 형태:

```cpp
CInGameRosterSpawner::Desc desc{};
desc.pWorld = &m_World;
desc.pEntityMap = m_pEntityIdMap.get();
desc.pRenderers = &m_ChampionRenderers;
m_RosterSpawner.Initialize(desc);

m_RosterSpawner.SpawnFromContext(
    CGameInstance::Get()->Get_GameContext(),
    &m_PlayerEntity);
```

### 6D-2. NetworkBridge 스케치

```cpp
class CInGameNetworkBridge final
{
public:
    using BindPlayerFn = std::function<void(EntityID)>;
    using CreateNetworkEntityFn = std::function<EntityID(u32_t netId, u8_t championId, u8_t team)>;

    bool Initialize(
        CWorld* pWorld,
        EntityIdMap* pEntityMap,
        CClientNetwork* pNetwork,
        CSnapshotApplier* pSnapshotApplier);

    void SetBindPlayerCallback(BindPlayerFn fn);
    void SetCreateNetworkEntityCallback(CreateNetworkEntityFn fn);
    void AttachSharedSessionIfNeeded(bool_t bUsingSharedNetwork);
    void Pump(bool_t bUsingSharedNetwork);
    void Shutdown(bool_t bUsingSharedNetwork);
};
```

핵심 목표:
- `Scene_InGame::OnEnter()` 안의 lambda가 줄어든다.
- `Hello`, `Snapshot`, `GameStart ignore`가 한 곳으로 모인다.

### 6D-3. RenderBridge 스케치

```cpp
class CInGameRenderBridge final
{
public:
    static void UpdateAnimatedRenderers(CWorld& world, f32_t dt);
    static void RenderMainPass(CWorld& world, const Mat4& viewProj);
    static void RenderNormalPass(CWorld& world, DX11Shader* pMeshShader, DX11Pipeline* pMeshPipeline);
};
```

`Scene_InGame`에서는 직접 legacy renderer를 호출하지 않는다.

```cpp
CInGameRenderBridge::UpdateAnimatedRenderers(m_World, dt);
CInGameRenderBridge::RenderMainPass(m_World, vp);
```

### 6D-4. Scene_InGame 잔여 책임 정리 순서

권장 분리 순서:

```txt
1. RosterSpawner 먼저 분리
2. NetworkBridge 분리
3. RenderBridge 분리
4. DebugPanel 분리
5. MapBootstrap 분리
```

Scene_InGame에 남길 책임:

```cpp
class CScene_InGame final : public IScene
{
public:
    bool OnEnter() override;
    void OnExit() override;
    void OnUpdate(f32_t dt) override;
    void OnLateUpdate(f32_t dt) override;
    void OnRender() override;
    void OnImGui() override;

private:
    void BootstrapWorld();
    void RegisterSystems();
    void UpdateGameplay(f32_t dt);
    void RenderFrame();
};
```

### 6D 완료 기준

- `Scene_InGame.cpp` 줄 수 감소
- BanPick/InGame network callback이 `CInGameNetworkBridge`로 이동
- roster spawn이 `CInGameRosterSpawner`로 이동
- legacy renderer 멤버 제거 이후 render/update/shutdown helper 통일

---

## Phase 6E - Hybrid Filters 통합

### 구현 목적

Visual Studio Solution Explorer의 가상 필터와 실제 디스크 폴더가 따로 놀지 않게 정리한다.

정책:

```txt
최상위 = 의존성/도메인 번호
내부 = 실제 디스크 폴더 미러
```

즉, 완전 mirror도 아니고 현재처럼 순수 module tree도 아니다.
F12 이동 경로와 Solution Explorer 위치를 동시에 이해할 수 있게 만드는 정리 단계다.

### 6E-1. 적용 대상 파일

```txt
Engine/Include/Engine.vcxproj.filters
Client/Include/Client.vcxproj.filters
Server/Include/Server.vcxproj.filters
Tools/WintersAssetConverter/WintersAssetConverter.vcxproj.filters
```

AssetConverter `.filters`가 없으면 신규 생성한다.

### 6E-2. Engine filters 목표 구조

```txt
00. RHI
  Interface
  DX11
  DX12
01. Core
  Fiber
  JobSystem
  Profiler
02. Framework
03. Renderer
  FX
04. Editor
05. ECS
  Components
  Systems
06. Resource
  AssetFormat
    Common
    Mesh
    Anim
07. Physics
08. Sound
09. Network
10. Platform
11. Scene
  Manager
12. Collision
13. AI
14. Tools
  AssetConverter
15. Shaders
```

모듈명 갱신 방향:

```txt
00. Manager   -> 00. RHI
02. Structure -> 02. Framework
08. Audio     -> 08. Sound
10. JobSystem -> 01. Core\JobSystem
10. Platform  -> 10. Platform
14. Tools     -> 신규
15. Shaders   -> 신규
```

### 6E-3. Client filters 목표 구조

```txt
00. App
01. Scene
  InGame
    Bootstrap
    Roster
    Network
    Render
    Debug
  BanPick
  MatchLoading
  MainMenu
  Loading
  Editor
02. GameObject
  Champion
    Irelia
    Yasuo
    Kalista
    Garen
    Zed
    Riven
    Ezreal
    Fiora
    Jax
    Annie
    Ashe
    Yone
  FX
  Projectile
03. GamePlay
  Skill
  Champion
  System
04. Manager
05. UI
06. Network
  Backend
  Client
07. Shared
  Components
  Systems
  Registries
99. Defines
```

현재 평면 노출 보정 후보:

```xml
<ClCompile Include="..\..\Shared\GameSim\Systems\GameplayHookRegistry.cpp">
  <Filter>07. Shared\Systems</Filter>
</ClCompile>

<ClCompile Include="..\..\Shared\GameSim\Registries\ChampionStatsRegistry.cpp">
  <Filter>07. Shared\Registries</Filter>
</ClCompile>

<ClCompile Include="..\Private\GamePlay\VisualHookRegistry.cpp">
  <Filter>03. GamePlay\System</Filter>
</ClCompile>
```

### 6E-4. Server filters 목표 구조

```txt
00. App
01. Network
  IOCP
  Session
  PacketDispatcher
  FrameParser
02. Game
  GameRoom
  GameLogic
  ServerWorld
  AOI
  CommandDispatcher
  SnapshotBuilder
03. Security
04. Shared
  Components
  Systems
  Registries
```

Server Shared 보정 후보:

```xml
<ClCompile Include="..\..\Shared\GameSim\Systems\MoveSystem.cpp">
  <Filter>04. Shared\Systems</Filter>
</ClCompile>

<ClCompile Include="..\..\Shared\GameSim\Registries\SkillScalingRegistry.cpp">
  <Filter>04. Shared\Registries</Filter>
</ClCompile>
```

### 6E-5. AssetConverter filters 신규 생성 스케치

파일 후보:

```txt
Tools/WintersAssetConverter/WintersAssetConverter.vcxproj.filters
```

구조:

```xml
<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup>
    <Filter Include="00. Engine Shared">
      <UniqueIdentifier>{00000000-0000-0000-0000-000000000001}</UniqueIdentifier>
    </Filter>
    <Filter Include="00. Engine Shared\AssetFormat">
      <UniqueIdentifier>{00000000-0000-0000-0000-000000000002}</UniqueIdentifier>
    </Filter>
    <Filter Include="00. Engine Shared\AssetFormat\Common">
      <UniqueIdentifier>{00000000-0000-0000-0000-000000000003}</UniqueIdentifier>
    </Filter>
    <Filter Include="00. Engine Shared\AssetFormat\Mesh">
      <UniqueIdentifier>{00000000-0000-0000-0000-000000000004}</UniqueIdentifier>
    </Filter>
    <Filter Include="00. Engine Shared\AssetFormat\Anim">
      <UniqueIdentifier>{00000000-0000-0000-0000-000000000005}</UniqueIdentifier>
    </Filter>
    <Filter Include="01. Tools">
      <UniqueIdentifier>{00000000-0000-0000-0000-000000000006}</UniqueIdentifier>
    </Filter>
  </ItemGroup>
</Project>
```

실제 반영 시 `UniqueIdentifier`는 `New-Guid`로 새로 생성한다.

### 6E-6. 필터 매핑 자동 검증 스크립트 후보

문서용 검증 스케치:

```powershell
$projects = @(
  "Engine\Include\Engine.vcxproj.filters",
  "Client\Include\Client.vcxproj.filters",
  "Server\Include\Server.vcxproj.filters"
)

foreach ($filters in $projects) {
  [xml]$xml = Get-Content $filters
  $items = @()
  $items += $xml.Project.ItemGroup.ClCompile
  $items += $xml.Project.ItemGroup.ClInclude
  $items |
    Where-Object { $_ -and -not $_.Filter } |
    ForEach-Object { "$filters : missing filter -> $($_.Include)" }
}
```

완료 시 기대 결과:

```txt
출력 없음 = ClCompile/ClInclude 평면 노출 없음
```

### 6E 완료 기준

- Engine DX12 파일이 `00. RHI\DX12`에 표시
- Engine Fiber/JobSystem 파일이 `01. Core\Fiber`, `01. Core\JobSystem`에 표시
- Client Shared/GameSim 파일이 `07. Shared` 하위에 표시
- Server Shared/GameSim 파일이 `04. Shared` 하위에 표시
- AssetConverter filters 파일 존재
- `.vcxproj`의 compile/include 경로는 변경하지 않고 `.filters`만 변경
- `msbuild` 결과에 영향 없음

---

## 전체 진행 추천 순서

```txt
1. 6A 현재 반영분 런타임 확인
   - server 1 + client 3
   - slot move, champion pick, bot fill, StartGame

2. 6B 최소 반영
   - Yone shaderPath Mesh3D.hlsl 전환
   - 필요 시 legacy PBR init도 Mesh3D.hlsl로 임시 전환

3. 6C legacy removal
   - local fallback도 roster로 생성
   - CreateECSEntities roster-only
   - legacy ModelRenderer 멤버 제거

4. 6D 구조 분리
   - RosterSpawner
   - NetworkBridge
   - RenderBridge

5. 6E Hybrid filters 정리
   - Engine/Client/Server .vcxproj.filters 정리
   - Shared/GameSim 평면 노출 제거
   - AssetConverter .filters 신규 생성
```

## 런타임 체크리스트

```txt
[ ] Client 1 접속: host, slot 0 자동 점유
[ ] Client 2 접속: 빈 상태, 직접 slot 선택
[ ] Client 3 접속: 빈 상태, 직접 slot 선택
[ ] C2/C3 slot 이동 시 이전 slot 비움
[ ] Yone/Fiora/Ezreal 선택 후 LobbyState championId 정상
[ ] Fill Empty Slots With Bots 후 10 slot 점유
[ ] StartGame 연타 시 중복 전환 없음
[ ] MatchLoading roster가 3클라 동일
[ ] InGame 내 조작 대상 netId == Hello netId
[ ] 첫 Snapshot 후 local player 중복 생성 없음
[x] LoL champion UsesPBR false
[ ] Diffuse texture 색감이 회색/찰흙빛에서 복구
[ ] Engine DX12 파일이 00. RHI\DX12에 표시
[ ] Client Shared/GameSim 파일이 07. Shared 하위에 표시
[ ] Server Shared/GameSim 파일이 04. Shared 하위에 표시
[ ] AssetConverter .vcxproj.filters 존재
```
