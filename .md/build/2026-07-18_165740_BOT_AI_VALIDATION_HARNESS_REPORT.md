# Bot AI Validation Harness Report

- Date: 2026-07-18 16:59:46 +09:00
- Repo: C:\Users\user\Desktop\Winters
- Configuration: Debug
- Overall: PASS

## Steps

- PASS git diff --check exit=0 seconds=0.51
- PASS ChampionAI dependency boundary audit exit=1 seconds=0.03
  - Notes: No matches.
- PASS Yone E stage-2 contract audit exit=0 seconds=0.11
- PASS LoL data-driven pipeline and SimLab regression exit=0 seconds=125.06

## Output Tail

### git diff --check

```text
 next time Git touches it
warning: in the working copy of 'Shared/GameSim/Definitions/SkillGameplayDef.h', LF will be replaced by CRLF the next t
ime Git touches it
warning: in the working copy of 'Shared/GameSim/Definitions/SkillTypes.h', LF will be replaced by CRLF the next time Gi
t touches it
warning: in the working copy of 'Shared/GameSim/Definitions/SpawnObjectDefinitionPack.h', LF will be replaced by CRLF t
he next time Git touches it
warning: in the working copy of 'Shared/GameSim/Generated/ChampionGameData.generated.cpp', LF will be replaced by CRLF
the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Include/GameSim.vcxproj', LF will be replaced by CRLF the next time Git
 touches it
warning: in the working copy of 'Shared/GameSim/Include/GameSim.vcxproj.filters', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/GameSim/Spawn/ChampionGameplayAssembly.cpp', LF will be replaced by CRLF the ne
xt time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp', LF will be replaced by CRLF t
he next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h', LF will be replaced by CRLF the
 next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Combat/CombatActionSystem.cpp', LF will be replaced by CRLF the
 next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp', LF will be replaced by CR
LF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h', LF will be replaced by CRL
F the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Damage/DamagePipeline.cpp', LF will be replaced by CRLF the nex
t time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Damage/DamagePipeline.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp', LF will be replaced by CRLF the
next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Death/DeathSystem.cpp', LF will be replaced by CRLF the next ti
me Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Experience/ExperienceSystem.cpp', LF will be replaced by CRLF t
he next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Move/MoveSystem.cpp', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.cpp', LF wi
ll be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Rune/RuneSystem.cpp', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Stat/StatSystem.cpp', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.cpp', LF will be replaced by CR
LF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/Event.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Event_generated.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Hello_generated.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/LobbyCommand_generated.h', LF will be replaced by CRLF th
e next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/LobbyTypes_generated.h', LF will be replaced by CRLF the
next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Snapshot_generated.h', LF will be replaced by CRLF the ne
xt time Git touches it
warning: in the working copy of 'Shared/Schemas/Hello.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/LobbyCommand.fbs', LF will be replaced by CRLF the next time Git touche
s it
warning: in the working copy of 'Shared/Schemas/LobbyTypes.fbs', LF will be replaced by CRLF the next time Git touches
it
warning: in the working copy of 'Shared/Schemas/Snapshot.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Tools/ChampionData/build_champion_game_data.py', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Tools/External/LeagueToolkitProbe/Program.cs', LF will be replaced by CRLF the next ti
me Git touches it
warning: in the working copy of 'Tools/LoLData/Build-LoLDefinitionPack.py', LF will be replaced by CRLF the next time G
it touches it
warning: in the working copy of 'Tools/LoLData/Collect-LoLLegacyDataAudit.ps1', LF will be replaced by CRLF the next ti
me Git touches it
warning: in the working copy of 'Tools/LoLData/Export-LoLChampionVisualTimingSeed.ps1', LF will be replaced by CRLF the
 next time Git touches it
warning: in the working copy of 'Tools/LoLData/Export-LoLSounds.ps1', LF will be replaced by CRLF the next time Git tou
ches it
warning: in the working copy of 'Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1', LF will be replaced by CRLF the next t
ime Git touches it
warning: in the working copy of 'Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Tools/UIAtlas/build_itemshop_atlas.py', LF will be replaced by CRLF the next time Git
touches it
```

### Yone E stage-2 contract audit

```text
Yone E stage-2 contract is consistent.
```

### LoL data-driven pipeline and SimLab regression

```text
warning: in the working copy of 'Shared/GameSim/Definitions/SkillGameplayDef.h', LF will be replaced by CRLF the next t
ime Git touches it
warning: in the working copy of 'Shared/GameSim/Definitions/SkillTypes.h', LF will be replaced by CRLF the next time Gi
t touches it
warning: in the working copy of 'Shared/GameSim/Definitions/SpawnObjectDefinitionPack.h', LF will be replaced by CRLF t
he next time Git touches it
warning: in the working copy of 'Shared/GameSim/Generated/ChampionGameData.generated.cpp', LF will be replaced by CRLF
the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Include/GameSim.vcxproj', LF will be replaced by CRLF the next time Git
 touches it
warning: in the working copy of 'Shared/GameSim/Include/GameSim.vcxproj.filters', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/GameSim/Spawn/ChampionGameplayAssembly.cpp', LF will be replaced by CRLF the ne
xt time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp', LF will be replaced by CRLF t
he next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h', LF will be replaced by CRLF the
 next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Combat/CombatActionSystem.cpp', LF will be replaced by CRLF the
 next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp', LF will be replaced by CR
LF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h', LF will be replaced by CRL
F the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Damage/DamagePipeline.cpp', LF will be replaced by CRLF the nex
t time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Damage/DamagePipeline.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp', LF will be replaced by CRLF the
next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Death/DeathSystem.cpp', LF will be replaced by CRLF the next ti
me Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Experience/ExperienceSystem.cpp', LF will be replaced by CRLF t
he next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Move/MoveSystem.cpp', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.cpp', LF wi
ll be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Rune/RuneSystem.cpp', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Stat/StatSystem.cpp', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.cpp', LF will be replaced by CR
LF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/Event.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Event_generated.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Hello_generated.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/LobbyCommand_generated.h', LF will be replaced by CRLF th
e next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/LobbyTypes_generated.h', LF will be replaced by CRLF the
next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Snapshot_generated.h', LF will be replaced by CRLF the ne
xt time Git touches it
warning: in the working copy of 'Shared/Schemas/Hello.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/LobbyCommand.fbs', LF will be replaced by CRLF the next time Git touche
s it
warning: in the working copy of 'Shared/Schemas/LobbyTypes.fbs', LF will be replaced by CRLF the next time Git touches
it
warning: in the working copy of 'Shared/Schemas/Snapshot.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Tools/ChampionData/build_champion_game_data.py', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Tools/External/LeagueToolkitProbe/Program.cs', LF will be replaced by CRLF the next ti
me Git touches it
warning: in the working copy of 'Tools/LoLData/Build-LoLDefinitionPack.py', LF will be replaced by CRLF the next time G
it touches it
warning: in the working copy of 'Tools/LoLData/Collect-LoLLegacyDataAudit.ps1', LF will be replaced by CRLF the next ti
me Git touches it
warning: in the working copy of 'Tools/LoLData/Export-LoLChampionVisualTimingSeed.ps1', LF will be replaced by CRLF the
 next time Git touches it
warning: in the working copy of 'Tools/LoLData/Export-LoLSounds.ps1', LF will be replaced by CRLF the next time Git tou
ches it
warning: in the working copy of 'Tools/LoLData/Get-LoLDataDrivenGoalStatus.ps1', LF will be replaced by CRLF the next t
ime Git touches it
warning: in the working copy of 'Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Tools/UIAtlas/build_itemshop_atlas.py', LF will be replaced by CRLF the next time Git
touches it
[LoLDataDriven] PASS
```
