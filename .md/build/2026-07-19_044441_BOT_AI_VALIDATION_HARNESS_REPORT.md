# Bot AI Validation Harness Report

- Date: 2026-07-19 04:44:42 +09:00
- Repo: C:\Users\user\Desktop\Winters
- Configuration: Debug
- Overall: FAIL

## Steps

- PASS git diff --check exit=0 seconds=0.37
- PASS ChampionAI dependency boundary audit exit=1 seconds=0.05
  - Notes: No matches.
- PASS Yone E stage-2 contract audit exit=0 seconds=0.15
- FAIL LoL data-driven pipeline and SimLab regression exit=1 seconds=1.05

## Output Tail

### git diff --check

```text
ime Git touches it
warning: in the working copy of 'Shared/GameSim/Champions/Yone/YoneGameSim.h', LF will be replaced by CRLF the next tim
e Git touches it
warning: in the working copy of 'Shared/GameSim/Components/ChampionAIComponent.h', LF will be replaced by CRLF the next
 time Git touches it
warning: in the working copy of 'Shared/GameSim/Components/DamageRequestComponent.h', LF will be replaced by CRLF the n
ext time Git touches it
warning: in the working copy of 'Shared/GameSim/Components/IreliaSimComponent.h', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/GameSim/Components/YoneSimComponent.h', LF will be replaced by CRLF the next ti
me Git touches it
warning: in the working copy of 'Shared/GameSim/Core/Checkpoint/WorldKeyframe.cpp', LF will be replaced by CRLF the nex
t time Git touches it
warning: in the working copy of 'Shared/GameSim/Definitions/DamageTypes.h', LF will be replaced by CRLF the next time G
it touches it
warning: in the working copy of 'Shared/GameSim/Definitions/SkillAtomData.h', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Shared/GameSim/Definitions/SnapshotStateFlags.h', LF will be replaced by CRLF the next
 time Git touches it
warning: in the working copy of 'Shared/GameSim/Definitions/SpawnLoadoutPolicyDef.h', LF will be replaced by CRLF the n
ext time Git touches it
warning: in the working copy of 'Shared/GameSim/Generated/ChampionAIPolicyData.generated.inl', LF will be replaced by C
RLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Generated/ChampionGameData.generated.cpp', LF will be replaced by CRLF
the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Include/GameSim.vcxproj', LF will be replaced by CRLF the next time Git
 touches it
warning: in the working copy of 'Shared/GameSim/Include/GameSim.vcxproj.filters', LF will be replaced by CRLF the next
time Git touches it
warning: in the working copy of 'Shared/GameSim/Spawn/ChampionGameplayAssembly.cpp', LF will be replaced by CRLF the ne
xt time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.cpp', LF will be replaced by CRLF th
e next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIPerception.h', LF will be replaced by CRLF
 the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp', LF will be replaced by CRLF t
he next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h', LF will be replaced by CRLF the
 next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIResearchTypes.h', LF will be replaced by C
RLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp', LF will be replaced by CRLF t
he next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h', LF will be replaced by CRLF the
 next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.cpp', LF will be replaced by CRL
F the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.h', LF will be replaced by CRLF
the next time Git touches it
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
warning: in the working copy of 'Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.cpp', LF will be replaced by
CRLF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.h', LF will be replaced by CR
LF the next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/Stat/StatSystem.cpp', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Shared/Schemas/Command.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Command_generated.h', LF will be replaced by CRLF the nex
t time Git touches it
warning: in the working copy of 'Shared/Schemas/Generated/cpp/Snapshot_generated.h', LF will be replaced by CRLF the ne
xt time Git touches it
warning: in the working copy of 'Shared/Schemas/Snapshot.fbs', LF will be replaced by CRLF the next time Git touches it
warning: in the working copy of 'Shared/Schemas/run_codegen.bat', LF will be replaced by CRLF the next time Git touches
 it
warning: in the working copy of 'Tools/AIResearch/AI_EPISODE_V1.md', LF will be replaced by CRLF the next time Git touc
hes it
warning: in the working copy of 'Tools/Harness/GameRoomBotMatchSoak.cpp', LF will be replaced by CRLF the next time Git
 touches it
warning: in the working copy of 'Tools/LoLData/Build-LoLDefinitionPack.py', LF will be replaced by CRLF the next time G
it touches it
warning: in the working copy of 'Tools/SimLab/main.cpp', LF will be replaced by CRLF the next time Git touches it
```

### Yone E stage-2 contract audit

```text
Yone E stage-2 contract is consistent.
```

### LoL data-driven pipeline and SimLab regression

```text
[LoLDataDriven] Definition pack freshness
STALE Data\LoL\ServerPrivate\Gameplay\ChampionGameplayDefs.json
STALE Data\LoL\ServerPrivate\Gameplay\SkillGameplayDefs.json
STALE Data\LoL\SharedContract\DefinitionManifest.json
STALE Server\Private\Data\Generated\LoLGameplayDefinitions.generated.cpp
STALE Client\Private\Data\Generated\LoLVisualDefinitions.generated.cpp
STALE .md\TODO\06-22\LOL_DEFINITION_PACK_PARITY.json
cmd.exe : [LoLDefinitionPack] generated definition pack is stale; run without --check
At C:\Users\user\Desktop\Winters\Tools\Harness\Run-BotAiValidation.ps1:77 char:20
+             $raw = & cmd.exe /c $CommandLine 2>&1
+                    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: ([LoLDefinitionP...without --check:String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError

Definition pack freshness failed with exit code 1
At C:\Users\user\Desktop\Winters\Tools\LoLData\Verify-LoLDataDrivenPipeline.ps1:19 char:9
+         throw "$Name failed with exit code $LASTEXITCODE"
+         ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : OperationStopped: (Definition pack...ith exit code 1:String) [], RuntimeException
    + FullyQualifiedErrorId : Definition pack freshness failed with exit code 1
```
