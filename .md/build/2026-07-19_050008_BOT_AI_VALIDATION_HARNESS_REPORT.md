# Bot AI Validation Harness Report

- Date: 2026-07-19 05:01:05 +09:00
- Repo: C:\Users\user\Desktop\Winters
- Configuration: Debug
- Overall: FAIL

## Steps

- PASS git diff --check exit=0 seconds=0.35
- PASS ChampionAI dependency boundary audit exit=1 seconds=0.02
  - Notes: No matches.
- PASS Yone E stage-2 contract audit exit=0 seconds=0.11
- FAIL LoL data-driven pipeline and SimLab regression exit=1 seconds=56.24

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
[YoneSim] W accepted caster=9
[YasuoSim] Q accepted caster=1 stage=1 stack=0
[RivenSim] W stun caster=6
[LeeSinSim] E slow caster=5
[JaxSim] R active caster=10
[YoneSim] W accepted caster=9
[AsheSim] E hawkshot caster=3 range=400 sight=10
[ViegoSim] E mist caster=8
[YasuoSim] Q accepted caster=1 stage=1 stack=0
[RivenSim] W stun caster=6
[YoneSim] E out caster=9 anchor=(-3.25666,1.19996) end=(-1.86796,-2.55124)
[AsheSim] R crystal arrow caster=3
[JaxSim] R active caster=10
[ViegoSim] Q line caster=8
[LeeSinSim] E slow caster=5
[JaxSim] W empower caster=10
[YoneSim] W accepted caster=9
[AsheSim] E hawkshot caster=3 range=400 sight=10
[YasuoSim] W accepted caster=1
[RivenSim] E shield caster=6 amount=70
[JaxSim] E counter start caster=10
[YoneSim] R accepted caster=9
[ViegoSim] E mist caster=8
[RivenSim] Q caster=6 stage=1 stack=1
[YasuoSim] Q accepted caster=1 stage=1 stack=0
[RivenSim] W stun caster=6
[YoneSim] W accepted caster=9
[ViegoSim] Q line caster=8
[YasuoSim] E active caster=1 target=8 dashed=1 window=0.5
[JaxSim] E counter start caster=10
[AsheSim] W volley caster=3
[YasuoSim] Q accepted caster=1 stage=1 stack=0
[YoneSim] E out caster=9 anchor=(-1.49647,0.742574) end=(-3.20002,4.36168)
[ViegoSim] R dash caster=8
[RivenSim] W stun caster=6
[JaxSim] E counter start caster=10
[AsheSim] Q debug activate without full focus caster=3 stacks=1/4
[YasuoSim] E active caster=1 target=10 dashed=1 window=0.5
[RivenSim] E shield caster=6 amount=70
[YoneSim] W accepted caster=9
[ViegoSim] E mist caster=8
[JaxSim] R active caster=10
[YoneSim] R accepted caster=9
[RivenSim] E shield caster=6 amount=70
[RivenSim] Q caster=6 stage=1 stack=1
[YoneSim] W accepted caster=9
[AsheSim] R crystal arrow caster=3
[RivenSim] E shield caster=6 amount=70
[AsheSim] E hawkshot caster=3 range=400 sight=10
[YasuoSim] Q accepted caster=1 stage=1 stack=0
[RivenSim] W stun caster=6
[ViegoSim] Q line caster=8
[YoneSim] R accepted caster=9
[ViegoSim] E mist caster=8
[RivenSim] W stun caster=6
[RivenSim] E shield caster=6 amount=70
[YoneSim] R accepted caster=9
[AsheSim] E hawkshot caster=3 range=400 sight=10
[ViegoSim] E mist caster=8
[RivenSim] Q caster=6 stage=1 stack=1
[YoneSim] E out caster=9 anchor=(5.15256,2.6659) end=(2.7921,-0.563378)
[RivenSim] E shield caster=6 amount=70
[ViegoSim] Q line caster=8
[YoneSim] W accepted caster=9
[ViegoSim] E mist caster=8
[SimLab][FormulaData] FAIL: scaled skill formula execution drifted
[SimLab][BORK] PASS: one 10% max-HP on-hit and post-mitigation lifesteal
[SimLab][ManaItems] PASS: Essence Reaver BA/Q mana + Manamune 360 -> Muramana
[SimLab][Resource] PASS: 17-kind matrix, Energy 200/+10, None/Flow, Zed costs, mana-item/restore gate
[SimLab][ActiveItems] PASS: Sundered/Zhonya/QSS/ward/reorder
[SimLab][SkillRank] PASS: Q/W/E 1/3/... and R 6/11/16 gates
[SimLab] same-seed replay OK: hash=F58B7AC1780F7EE4
[SimLab] seed sensitivity OK: seed+1 hash=06FEBD7A91DA3F90
[SimLab] FAIL
SimLab deterministic regression failed with exit code 1
At C:\Users\user\Desktop\Winters\Tools\LoLData\Verify-LoLDataDrivenPipeline.ps1:19 char:9
+         throw "$Name failed with exit code $LASTEXITCODE"
+         ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : OperationStopped: (SimLab determin...ith exit code 1:String) [], RuntimeException
    + FullyQualifiedErrorId : SimLab deterministic regression failed with exit code 1
```
