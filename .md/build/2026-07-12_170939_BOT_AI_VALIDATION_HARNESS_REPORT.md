# Bot AI Validation Harness Report

- Date: 2026-07-12 17:10:23 +09:00
- Repo: C:\Users\user\Desktop\Winters
- Configuration: Debug
- Overall: PASS

## Steps

- PASS git diff --check exit=0 seconds=0.32
- PASS ChampionAI dependency boundary audit exit=1 seconds=0.02
  - Notes: No matches.
- PASS Yone E stage-2 contract audit exit=0 seconds=0.1
- PASS LoL data-driven pipeline and SimLab regression exit=0 seconds=44.3

## Output Tail

### git diff --check

```text
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.h', LF will be replaced by CRLF
the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Combat/CombatActionSystem.cpp', LF will be replaced by CRLF the
 next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp', LF will be replaced by CR
LF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h', LF will be replaced by CRL
F the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Damage/DamagePipeline.cpp', LF will be replaced by CRLF the nex
t time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Experience/ExperienceSystem.cpp', LF will be replaced by CRLF t
he next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Experience/ExperienceSystem.h', LF will be replaced by CRLF the
 next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.cpp', LF will be replaced
 by CRLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h', LF will be replaced b
y CRLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/JungleAI/JungleAISystem.cpp', LF will be replaced by CRLF the n
ext time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Move/DashArrival.h', LF will be replaced by CRLF the next time
Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Move/MoveSystem.cpp', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Recall/RecallSystem.cpp', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.cpp', LF wi
ll be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Rune/RuneSystem.h', LF will be replaced by CRLF the next time G
it touches it
warning: in the working copy of 'Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.cpp', LF will be replaced by
CRLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/SkillRank/SkillRankSystem.h', LF will be replaced by CRLF the n
ext time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/SpellbookFormOverride/SpellbookFormOverrideSystem.cpp', LF will
 be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/SpellbookFormOverride/SpellbookFormOverrideSystem.h', LF will b
e replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Stat/StatSystem.cpp', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Stat/StatSystem.h', LF will be replaced by CRLF the next time G
it touches it
warning: in the working copy of 'Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h', LF will be replaced by CR
LF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.cpp', LF will be replaced by CR
LF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h', LF will be replaced by CRLF
 the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Turret/StructureProjectileSystem.cpp', LF will be replaced by C
RLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Turret/StructureProjectileSystem.h', LF will be replaced by CRL
F the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Turret/TurretAISystem.cpp', LF will be replaced by CRLF the nex
t time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Turret/TurretAISystem.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/WaypointPatrol/WaypointPatrolSystem.cpp', LF will be replaced b
y CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/Command.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/Event.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Command_generated.h', LF will be replaced by CRLF the nex
t time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Event_generated.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Hello_generated.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Snapshot_generated.h', LF will be replaced by CRLF the ne
xt time Git touches it
warning: in the working copy of 'Shared/Schemas/Hello.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/Snapshot.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Tools/ChampionData/build_champion_game_data.py', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Tools/LoLData/Build-LoLDefinitionPack.py', LF will be replaced by CRLF the next time G
it touches it
warning: in the working copy of 'Tools/SimLab/main.cpp', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Tools/cook_map11_ambient_props.py', LF will be replaced by CRLF the next time Git touc
hes it
warning: in the working copy of 'Tools/cook_map11_brush_volumes.py', LF will be replaced by CRLF the next time Git touc
hes it
warning: in the working copy of 'profiler.json', LF will be replaced by CRLF the next time Git touches it
```

### Yone E stage-2 contract audit

```text
Yone E stage-2 contract is consistent.
```

### LoL data-driven pipeline and SimLab regression

```text
F the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Damage/DamagePipeline.cpp', LF will be replaced by CRLF the nex
t time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Experience/ExperienceSystem.cpp', LF will be replaced by CRLF t
he next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Experience/ExperienceSystem.h', LF will be replaced by CRLF the
 next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.cpp', LF will be replaced
 by CRLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h', LF will be replaced b
y CRLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/JungleAI/JungleAISystem.cpp', LF will be replaced by CRLF the n
ext time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Move/DashArrival.h', LF will be replaced by CRLF the next time
Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Move/MoveSystem.cpp', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Recall/RecallSystem.cpp', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.cpp', LF wi
ll be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Rune/RuneSystem.h', LF will be replaced by CRLF the next time G
it touches it
warning: in the working copy of 'Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.cpp', LF will be replaced by
CRLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/SkillRank/SkillRankSystem.h', LF will be replaced by CRLF the n
ext time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/SpellbookFormOverride/SpellbookFormOverrideSystem.cpp', LF will
 be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/SpellbookFormOverride/SpellbookFormOverrideSystem.h', LF will b
e replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Stat/StatSystem.cpp', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Stat/StatSystem.h', LF will be replaced by CRLF the next time G
it touches it
warning: in the working copy of 'Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h', LF will be replaced by CR
LF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.cpp', LF will be replaced by CR
LF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h', LF will be replaced by CRLF
 the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Turret/StructureProjectileSystem.cpp', LF will be replaced by C
RLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Turret/StructureProjectileSystem.h', LF will be replaced by CRL
F the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Turret/TurretAISystem.cpp', LF will be replaced by CRLF the nex
t time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Turret/TurretAISystem.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/WaypointPatrol/WaypointPatrolSystem.cpp', LF will be replaced b
y CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/Command.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/Event.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Command_generated.h', LF will be replaced by CRLF the nex
t time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Event_generated.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Hello_generated.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/LobbyCommand_generated.h', LF will be replaced by CRLF th
e next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/LobbyState_generated.h', LF will be replaced by CRLF the
next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/LobbyTypes_generated.h', LF will be replaced by CRLF the
next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Snapshot_generated.h', LF will be replaced by CRLF the ne
xt time Git touches it
warning: in the working copy of 'Shared/Schemas/Hello.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/Snapshot.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Tools/ChampionData/build_champion_game_data.py', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Tools/LoLData/Build-LoLDefinitionPack.py', LF will be replaced by CRLF the next time G
it touches it
warning: in the working copy of 'Tools/SimLab/main.cpp', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Tools/cook_map11_ambient_props.py', LF will be replaced by CRLF the next time Git touc
hes it
warning: in the working copy of 'Tools/cook_map11_brush_volumes.py', LF will be replaced by CRLF the next time Git touc
hes it
warning: in the working copy of 'profiler.json', LF will be replaced by CRLF the next time Git touches it
[LoLDataDriven] PASS
```

