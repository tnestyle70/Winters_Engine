# Phase 6D-2 Implementation Report - InGame Network Bridge

Date: 2026-05-06
Scope: split InGame network session/callback handling out of `Scene_InGame`

---

## 1. Goal

Phase 6D-2 target:

- Keep the Phase 6C/6D-1 roster-only champion spawn behavior.
- Move `GameSessionClient` frame callback setup/teardown out of `Scene_InGame`.
- Move `Hello` / `Snapshot` dispatch policy into a narrow helper.
- Preserve local net entity rebinding through a callback back into `Scene_InGame`.

---

## 2. New Files

```txt
Client/Public/Scene/InGameNetworkBridge.h
Client/Private/Scene/InGameNetworkBridge.cpp
```

Registered in:

```txt
Client/Include/Client.vcxproj
Client/Include/Client.vcxproj.filters
```

Filter:

```txt
01. Scene\InGame
```

---

## 3. New API

File: `Client/Public/Scene/InGameNetworkBridge.h`

```cpp
struct InGameNetworkBridgeDesc
{
    CWorld& world;
    const GameContext& context;
    std::unique_ptr<EntityIdMap>& entityIdMap;
    std::unique_ptr<CClientNetwork>& ownedNetwork;
    CClientNetwork*& networkView;
    bool_t& bUsingSharedNetwork;
    std::unique_ptr<CSnapshotApplier>& snapshotApplier;
    std::unique_ptr<CCommandSerializer>& commandSerializer;
    std::function<EntityID(eChampion, eTeam)> createChampion;
    std::function<void(EntityID)> bindLocalEntity;
};

class CInGameNetworkBridge final
{
public:
    static void Initialize(InGameNetworkBridgeDesc& desc);
    static bool_t Pump(CClientNetwork* pNetworkView, bool_t bUsingSharedNetwork);
    static void ReplayLastHelloIfShared(bool_t bUsingSharedNetwork);
    static void ApplySnapshot(
        CWorld& world,
        CSnapshotApplier* pSnapshotApplier,
        EntityIdMap* pEntityIdMap,
        const u8_t* bytes,
        u32_t len);
    static void Shutdown(...);
};
```

The bridge owns session/callback policy only. `Scene_InGame` still owns actual scene state.

---

## 4. Scene Call Site After Refactor

File: `Client/Private/Scene/Scene_InGame.cpp`

```cpp
const GameContext& gameContext = CGameInstance::Get()->Get_GameContext();
InGameNetworkBridgeDesc networkDesc{
    m_World,
    gameContext,
    m_pEntityIdMap,
    m_pNetwork,
    m_pNetworkView,
    m_bUsingSharedNetwork,
    m_pSnapshotApplier,
    m_pCommandSerializer,
    [this](eChampion champion, eTeam team)
    {
        return CreateECSChampion(champion, team);
    },
    [this](EntityID entity)
    {
        m_PlayerEntity = entity;
        BindPlayerToECSChampion(m_PlayerEntity);
    }
};
CInGameNetworkBridge::Initialize(networkDesc);
```

`Scene_InGame` no longer constructs the `FrameCallback` lambda directly.

---

## 5. Logic Moved Out Of Scene_InGame

Moved to `CInGameNetworkBridge`:

```txt
EntityIdMap allocation
shared BanPick TCP session reuse decision
local CClientNetwork creation
SnapshotApplier / CommandSerializer creation
SnapshotApplier OnNewEntity callback setup
GameSessionClient frame callback creation
Hello packet handling
Snapshot packet handling
netId/sessionId mismatch debug logging
local net entity rebind callback
shared/local network pump
shared Hello replay
OnExit callback detach / disconnect / reset
```

Remaining in `Scene_InGame`:

```txt
CreateECSChampion
BindPlayerToECSChampion
movement command send via CCommandSerializer
combat, render, camera, UI, systems
```

---

## 6. Header Include Gotcha Fixed

Initial build failed because the new cpp included a header path that allowed `Windows.h` to enter before `WinSock2.h`.

Fixed order:

```cpp
#include "Network/Client/ClientNetwork.h"
#include "Scene/InGameNetworkBridge.h"

#include <Windows.h>
```

Reason:

```txt
WinSock2.h must be included before Windows.h.
```

---

## 7. Verification

### Scene stale search

Command:

```powershell
Select-String -Path Client\Private\Scene\Scene_InGame.cpp,Client\Public\Scene\Scene_InGame.h `
  -Pattern 'GameSessionClient|FrameCallback|SetGameFrameCallback|PumpReceivedFrames|OnHello\(|OnSnapshot\(m_World|m_pNetwork->Disconnect|SetOnNewEntityCallback'
```

Result:

```txt
No direct callback/setup/teardown hits in Scene_InGame.
Only CInGameNetworkBridge::ReplayLastHelloIfShared call remains.
```

### Build

Command:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  Client\Include\Client.vcxproj `
  /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

Result:

```txt
Build succeeded.
Output: Client\Bin\Debug\WintersGame.exe
```

Known warnings:

```txt
C4275/C4251 DLL-interface warnings from existing EngineSDK headers.
C4834 nodiscard warning in Scene_InGame.cpp.
PostBuild: pwsh.exe not found.
```

No new build blocker.

---

## 8. Next Slice

Recommended next 6D-3:

```txt
InGameRenderBridge
  - ECS champion normal pass loop
  - ECS champion main render loop
  - map direct render remains separate until map ECS render ownership is decided

or

InGamePlayerControlBridge
  - GetPlayerChampionId / BindPlayerToECSChampion / local transform sync
  - skill dispatch still stays in Scene until champion skill systems are fully extracted
```
