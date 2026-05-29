# Phase 6C Implementation Report - Legacy Champion Full Removal

Date: 2026-05-06
Scope: BanPick roster -> InGame pure ECS champion spawn path

---

## 1. Goal

Phase 6C target:

- Remove legacy Scene-owned champion spawn/render path from `Scene_InGame`.
- Make InGame champion creation depend only on `GameContext.Roster[10]`.
- Preserve direct/local InGame entry by synthesizing a local roster fallback.
- Fix the class of bugs where BanPick selected champions differ from InGame spawned champions.

---

## 2. Main Flow After Change

```txt
BanPick / MatchLoading
  -> GameContext.Roster[10]
  -> Scene_InGame::CreateECSEntities()
  -> EnsureLocalRosterFallback()
  -> CreateRosterChampionsFromGameContext()
  -> CreateRosterChampion(slot)
  -> CreateECSChampion(championId, team)
  -> BindPlayerToECSChampion(local entity)
```

`Scene_InGame` no longer spawns Irelia/Yasuo/Sylas/Viego/Kalista/Garen/Zed through direct `ModelRenderer` members.

---

## 3. Core Code

### 3.1 Roster-only InGame entry

File: `Client/Private/Scene/Scene_InGame.cpp`

```cpp
void CScene_InGame::CreateECSEntities()
{
    EnsureLocalRosterFallback();
    m_PlayerEntity = NULL_ENTITY;

    const bool_t bCreatedRoster = CreateRosterChampionsFromGameContext();
    CreateMapEntity();

    if (m_PlayerEntity == NULL_ENTITY)
    {
        OutputDebugStringA("[ECS:RosterOnly] no local player entity after roster creation\n");
        return;
    }

    BindPlayerToECSChampion(m_PlayerEntity);

    if (m_World.HasComponent<ChampionComponent>(m_PlayerEntity))
    {
        m_PlayerTeam = m_World.GetComponent<ChampionComponent>(m_PlayerEntity).team;
    }

    if (m_bUsingSharedNetwork)
    {
        CGameSessionClient::Instance().ReplayLastHelloToGameFrameCallback();
    }

    char dbg[192]{};
    sprintf_s(dbg, "[ECS:RosterOnly] created=%d total=%u player=%u champion=%u\n",
        bCreatedRoster ? 1 : 0,
        m_World.GetEntityCount(),
        static_cast<u32_t>(m_PlayerEntity),
        static_cast<u32_t>(GetPlayerChampionId()));
    OutputDebugStringA(dbg);
}
```

### 3.2 Local/direct fallback roster

File: `Client/Private/Scene/Scene_InGame.cpp`

```cpp
void CScene_InGame::EnsureLocalRosterFallback()
{
    GameContext& context = CGameInstance::Get()->Get_GameContext();
    if (context.bUseNetworkRoster)
        return;

    eChampion selected = context.SelectedChampion;
    const ChampionDef* cd = CChampionRegistry::Instance().Find(selected);
    if (!cd)
        cd = FindChampionDef(selected);
    if (!cd || !cd->fbxPath)
        selected = eChampion::EZREAL;

    context = GameContext{};
    context.bUseNetworkRoster = true;
    context.SelectedChampion = selected;
    context.MySessionId = 1;
    context.MyNetId = 1;
    context.MySlotId = 0;
    context.MyTeam = 0;

    GameRosterSlot& player = context.Roster[0];
    player.slotId = 0;
    player.team = 0;
    player.bHuman = true;
    player.sessionId = context.MySessionId;
    player.netId = context.MyNetId;
    player.champion = selected;

    GameRosterSlot& practiceBot = context.Roster[5];
    practiceBot.slotId = 5;
    practiceBot.team = 1;
    practiceBot.bBot = true;
    practiceBot.netId = 1005;
    practiceBot.champion = eChampion::SYLAS;
    practiceBot.botDifficulty = 2;

    OutputDebugStringA("[ECS:RosterOnly] synthesized local practice roster\n");
}
```

### 3.3 Local player identity rule

File: `Client/Private/Scene/Scene_InGame.cpp`

```cpp
bool_t IsLocalRosterSlot(const GameContext& context, const GameRosterSlot& slot)
{
    if (!slot.bHuman)
        return false;

    if (context.MySessionId != 0)
        return slot.sessionId == context.MySessionId;

    if (context.MyNetId != 0)
        return slot.netId == context.MyNetId;

    return context.MySlotId != kInvalidGameRosterSlot
        && slot.slotId == context.MySlotId;
}
```

Important: `slotId` is only a fallback. If `MySessionId` or `MyNetId` exists, a stale/default slot id cannot bind the local player.

### 3.4 Runtime champion id

File: `Client/Private/Scene/Scene_InGame.cpp`

```cpp
eChampion CScene_InGame::GetPlayerChampionId()
{
    if (m_PlayerEntity != NULL_ENTITY
        && m_World.HasComponent<ChampionComponent>(m_PlayerEntity))
    {
        return m_World.GetComponent<ChampionComponent>(m_PlayerEntity).id;
    }

    return CGameInstance::Get()->Get_GameContext().SelectedChampion;
}
```

Skill dispatch, attack action, local prediction, and recovery logic now read the player entity champion first instead of trusting `GameContext.SelectedChampion`.

---

## 4. Legacy Path Removed

Removed from `Client/Public/Scene/Scene_InGame.h`:

```cpp
ModelRenderer   m_Irelia;   CTransform m_IreliaTransform;
ModelRenderer   m_Yasuo;    CTransform m_YasuoTransform;
ModelRenderer   m_Sylas;    CTransform m_SylasTransform;
ModelRenderer   m_Viego;    CTransform m_ViegoTransform;
ModelRenderer   m_Kalista;  CTransform m_KalistaTransform;
ModelRenderer   m_Garen;    CTransform m_GarenTransform;
ModelRenderer   m_Zed;      CTransform m_ZedTransform;

EntityID CreateChampionEntity(...);
EntityID CreateChampionEntity_FromBlueprint(...);
void SyncECSTransformsFromLegacy();
```

Removed from `Client/Private/Scene/Scene_InGame.cpp`:

```txt
WINTERS_MIN_SCENE
CreateChampionEntity
CreateChampionEntity_FromBlueprint
SyncECSTransformsFromLegacy
legacy champion direct Update
legacy champion direct Render
legacy champion direct Shutdown
debug hotkey animation calls for legacy ModelRenderer members
```

The map render path remains direct for now because map rendering is not part of the champion ECS renderer path yet.

---

## 5. Champion Table Completion

File: `Client/Private/GameObject/ChampionTable.cpp`

Added real asset definitions for legacy champions so they can be spawned through `CreateECSChampion`:

```txt
IRELIA
YASUO
KALISTA
SYLAS
VIEGO
GAREN
ZED
RIVEN
```

This keeps older champions usable after removing the direct Scene-owned renderer path.

---

## 6. Additional Fixes

### Yasuo state update

The old update depended on a single `m_YasuoEntity`. That member is removed, so the timer update now iterates ECS components:

```cpp
m_World.ForEach<YasuoStateComponent>(
    [dt](EntityID, YasuoStateComponent& ys)
    {
        if (ys.qStackTimer > 0.f)
        {
            ys.qStackTimer -= dt;
            if (ys.qStackTimer <= 0.f) ys.qStackCount = 0;
        }
        if (ys.eActiveTimer > 0.f)
        {
            ys.eActiveTimer -= dt;
            if (ys.eActiveTimer <= 0.f) ys.bEActive = false;
        }
    });
```

### BanPick -> InGame binding gotcha

`CLAUDE.md` now records the identity rule:

```txt
sessionId/netId first, slotId fallback only.
Do not bind InGame local player by first human slot.
```

---

## 7. Verification

### Stale code search

Command:

```powershell
Select-String -Path Client\Private\Scene\Scene_InGame.cpp,Client\Public\Scene\Scene_InGame.h,Client\Private\GameObject\ChampionTable.cpp `
  -Pattern 'WINTERS_MIN_SCENE|#if 0|m_Irelia\.|m_Yasuo\.|m_Kalista\.|m_Viego\.|m_Garen\.|m_Zed\.|m_Sylas\.|m_IreliaEntity|m_YasuoEntity|m_KalistaEntity|m_ViegoEntity|m_GarenEntity|m_ZedEntity|m_SylasTransform|CreateChampionEntity|SyncECSTransformsFromLegacy'
```

Result:

```txt
No hits.
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
C4275 DLL interface warnings from EngineSDK ECS system headers.
PostBuild: pwsh.exe not found.
```

These are pre-existing warnings and did not block the Debug Client build.

---

## 8. Manual Runtime Checklist

```txt
[ ] Client 1 picks Yone -> InGame local player is Yone
[ ] Client 2 picks Fiora -> InGame local player is Fiora
[ ] Client 3 picks Ezreal -> InGame local player is Ezreal
[ ] All three clients see the same roster champions
[ ] Skill dispatch uses each local player's own champion
[ ] Direct/local InGame entry still spawns selected champion + Sylas bot fallback
[ ] Map still renders
```
