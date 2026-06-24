# Bot AI Validation Harness Report

- Date: 2026-06-24 09:36:06 +09:00
- Repo: C:\Users\user\Desktop\Winters
- Configuration: Debug
- Overall: WARN

## Steps

- PASS git diff --check exit=0 seconds=0.18
- PASS ChampionAI dependency boundary audit exit=1 seconds=0.02
  - Notes: No matches.
- PASS Yone E stage-2 contract audit exit=0 seconds=0.09
- WARN LoL data-driven pipeline and SimLab regression exit=0 seconds=0
  - Notes: Skipped by -SkipFullPipeline.

## Output Tail

### git diff --check

```text
warning: in the working copy of 'Client/Public/Scene/Loader.h', LF will be replaced by CRLF the next time Git touches i
t
warning: in the working copy of 'Client/Public/Scene/Scene_InGame.h', LF will be replaced by CRLF the next time Git tou
ches it
warning: in the working copy of 'Data/Gameplay/ChampionGameData/champions.json', LF will be replaced by CRLF the next t
ime Git touches it
warning: in the working copy of 'Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json', LF will be replaced by CRLF the
 next time Git touches it
warning: in the working copy of 'Data/LoL/FX/Champions/Viego/ba_hit.wfx', LF will be replaced by CRLF the next time Git
 touches it
warning: in the working copy of 'Data/LoL/FX/Champions/Viego/e_mist.wfx', LF will be replaced by CRLF the next time Git
 touches it
warning: in the working copy of 'Data/LoL/FX/Champions/Viego/q_slash.wfx', LF will be replaced by CRLF the next time Gi
t touches it
warning: in the working copy of 'Data/LoL/FX/Champions/Viego/r_impact.wfx', LF will be replaced by CRLF the next time G
it touches it
warning: in the working copy of 'Data/LoL/FX/Champions/Viego/w_cast.wfx', LF will be replaced by CRLF the next time Git
 touches it
warning: in the working copy of 'Data/LoL/FX/Champions/Viego/w_missile.wfx', LF will be replaced by CRLF the next time
Git touches it
warning: in the working copy of 'Data/LoL/FX/Champions/Yone/e_soul_out.wfx', LF will be replaced by CRLF the next time
Git touches it
warning: in the working copy of 'Data/LoL/FX/Champions/Yone/e_soul_return.wfx', LF will be replaced by CRLF the next ti
me Git touches it
warning: in the working copy of 'Data/LoL/FX/Champions/Yone/q_mortal_steel.wfx', LF will be replaced by CRLF the next t
ime Git touches it
warning: in the working copy of 'Data/LoL/ServerPrivate/Gameplay/ChampionGameplayDefs.json', LF will be replaced by CRL
F the next time Git touches it
warning: in the working copy of 'Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json', LF will be replaced by
CRLF the next time Git touches it
warning: in the working copy of 'Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json', LF will be replaced by CRLF t
he next time Git touches it
warning: in the working copy of 'Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json', LF will be replaced by
CRLF the next time Git touches it
warning: in the working copy of 'Data/LoL/ServerPrivate/Gameplay/SummonerSpellGameplayDefs.json', LF will be replaced b
y CRLF the next time Git touches it
warning: in the working copy of 'Data/LoL/SharedContract/DefinitionManifest.json', LF will be replaced by CRLF the next
 time Git touches it
warning: in the working copy of 'Engine/Private/ECS/Systems/VisionSystem.cpp', LF will be replaced by CRLF the next tim
e Git touches it
warning: in the working copy of 'Engine/Private/Manager/UI/UI_Manager.cpp', LF will be replaced by CRLF the next time G
it touches it
warning: in the working copy of 'Engine/Public/ECS/Components/VisionComponents.h', LF will be replaced by CRLF the next
 time Git touches it
warning: in the working copy of 'EngineSDK/inc/ECS/Components/VisionComponents.h', LF will be replaced by CRLF the next
 time Git touches it
warning: in the working copy of 'Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp', LF will be replac
ed by CRLF the next time Git touches it
warning: in the working copy of 'Server/Private/Game/GameRoom.cpp', LF will be replaced by CRLF the next time Git touch
es it
warning: in the working copy of 'Server/Private/Game/SnapshotBuilder.cpp', LF will be replaced by CRLF the next time Gi
t touches it
warning: in the working copy of 'Server/Private/Network/IOCPCore.cpp', LF will be replaced by CRLF the next time Git to
uches it
warning: in the working copy of 'Server/Private/Network/Session_Manager.cpp', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Server/Public/Network/Session_Manager.h', LF will be replaced by CRLF the next time Gi
t touches it
warning: in the working copy of 'Shared/GameSim/Champions/Kalista/KalistaGameSim.cpp', LF will be replaced by CRLF the
next time Git touches it
warning: in the working copy of 'Shared/GameSim/Champions/Viego/ViegoGameSim.cpp', LF will be replaced by CRLF the next
 time Git touches it
warning: in the working copy of 'Shared/GameSim/Components/RespawnComponent.h', LF will be replaced by CRLF the next ti
me Git touches it
warning: in the working copy of 'Shared/GameSim/Components/ViegoSimComponent.h', LF will be replaced by CRLF the next t
ime Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp', LF will be replaced by CRLF the
next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Command_generated.h', LF will be replaced by CRLF the nex
t time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Event_generated.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/LobbyTypes_generated.h', LF will be replaced by CRLF the
next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Snapshot_generated.h', LF will be replaced by CRLF the ne
xt time Git touches it
warning: in the working copy of 'Tools/EldenAssetPipeline/elden_pipeline.py', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Tools/External/LeagueToolkitProbe/Program.cs', LF will be replaced by CRLF the next ti
me Git touches it
```

### Yone E stage-2 contract audit

```text
Yone E stage-2 contract is consistent.
```

