# Phase 6D Implementation Report - InGame Roster Spawn Slice

Date: 2026-05-06
Scope: split roster-based InGame champion spawning out of `Scene_InGame`

---

## 1. Goal

Phase 6D first slice:

- Keep the Phase 6C roster-only behavior.
- Move BanPick roster -> ECS champion spawn rules out of `Scene_InGame`.
- Preserve existing player binding, bot spawn, net id binding, and local fallback behavior.
- Keep the refactor small enough that combat/render/network frame logic remains untouched.

---

## 2. New Files

```txt
Client/Public/Scene/InGameRosterSpawner.h
Client/Private/Scene/InGameRosterSpawner.cpp
```

The files are registered in:

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

File: `Client/Public/Scene/InGameRosterSpawner.h`

```cpp
struct InGameRosterSpawnDesc
{
    CWorld& world;
    EntityIdMap* pEntityIdMap = nullptr;
    std::unordered_map<EntityID, Vec3>& networkChampionPrevPos;
    std::function<EntityID(eChampion, eTeam)> createChampion;
    std::function<void(eChampion, EntityID)> assignAlias;
};

struct InGameRosterSpawnResult
{
    bool_t bCreatedAny = false;
    u32_t requestedSlots = 0;
    u32_t createdSlots = 0;
    u32_t humanSlots = 0;
    u32_t botSlots = 0;
    EntityID playerEntity = NULL_ENTITY;
};

class CInGameRosterSpawner final
{
public:
    static void EnsureLocalRosterFallback(GameContext& context);
    static bool_t IsLocalRosterSlot(const GameContext& context, const GameRosterSlot& slot);
    static InGameRosterSpawnResult SpawnFromContext(
        InGameRosterSpawnDesc& desc,
        const GameContext& context);

private:
    static EntityID SpawnSlot(InGameRosterSpawnDesc& desc, const GameRosterSlot& slot);
};
```

The helper owns only roster-spawn policy. It does not own renderers, camera, combat state, or scene lifecycle.

---

## 4. Scene Call Site After Refactor

File: `Client/Private/Scene/Scene_InGame.cpp`

```cpp
void CScene_InGame::CreateECSEntities()
{
    GameContext& context = CGameInstance::Get()->Get_GameContext();
    CInGameRosterSpawner::EnsureLocalRosterFallback(context);
    m_PlayerEntity = NULL_ENTITY;

    InGameRosterSpawnDesc rosterDesc{
        m_World,
        m_pEntityIdMap.get(),
        m_NetworkChampionPrevPos,
        [this](eChampion champion, eTeam team)
        {
            return CreateECSChampion(champion, team);
        },
        [this](eChampion champion, EntityID entity)
        {
            AssignPureECSChampionAlias(champion, entity);
        }
    };

    const InGameRosterSpawnResult rosterResult =
        CInGameRosterSpawner::SpawnFromContext(rosterDesc, context);
    m_PlayerEntity = rosterResult.playerEntity;

    CreateMapEntity();

    if (m_PlayerEntity == NULL_ENTITY)
    {
        OutputDebugStringA("[ECS:RosterOnly] no local player entity after roster creation\n");
        return;
    }

    BindPlayerToECSChampion(m_PlayerEntity);
}
```

`Scene_InGame` now provides two callbacks:

```txt
createChampion: Scene-owned renderer/model setup stays in Scene for now.
assignAlias: Scene-owned debug aliases stay in Scene for now.
```

This keeps the extraction narrow and avoids moving combat/render state prematurely.

---

## 5. Logic Moved Out Of Scene_InGame

Moved to `CInGameRosterSpawner`:

```txt
local practice roster fallback
local player identity matching
netId -> EntityID reuse/bind
slot-based spawn position override
bot behavior tree + blackboard setup
LocalPlayerTag assignment
roster spawn debug counters
```

Removed from `Scene_InGame` declarations:

```cpp
EntityID CreateRosterChampion(const GameRosterSlot& slot);
bool_t CreateRosterChampionsFromGameContext();
void EnsureLocalRosterFallback();
```

Also removed direct AI bot setup includes from `Scene_InGame.cpp`:

```cpp
#include "AI/BTNodes_Champion.h"
#include "AI/Blackboard.h"
```

`Scene_InGame` still includes `BehaviorTreeSystem.h` because the scheduler registers `CBehaviorTreeSystem`.

---

## 6. Preserved Rules

### Identity binding

```txt
MySessionId first
MyNetId second
MySlotId only when no authoritative identity exists
no first-human fallback when authoritative identity exists
```

### Local fallback

```txt
Direct InGame entry:
slot 0 = selected human champion
slot 5 = Sylas practice bot
```

### Bot setup

```txt
slot.bBot -> BuildStandardChampionBT()
BotComponent.difficulty = slot.botDifficulty or 2
Blackboard difficulty + lanePushPos
```

---

## 7. Verification

### Stale symbol check

Command:

```powershell
Select-String -Path Client\Private\Scene\Scene_InGame.cpp,Client\Public\Scene\Scene_InGame.h `
  -Pattern 'CreateRosterChampion|CreateRosterChampionsFromGameContext|EnsureLocalRosterFallback|IsLocalRosterSlot'
```

Result:

```txt
Only call site remains:
Scene_InGame.cpp: CInGameRosterSpawner::EnsureLocalRosterFallback(context)
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

Recommended next 6D slice:

```txt
6D-2: InGameNetworkBridge
  - shared GameSessionClient frame callback setup/teardown
  - SnapshotApplier + EntityIdMap handling
  - local net entity rebind -> BindPlayerToECSChampion call boundary

6D-3: InGameRenderBridge
  - ECS champion render loop
  - normal pass loop
  - map direct render remains separate until map ECS render policy is decided
```
