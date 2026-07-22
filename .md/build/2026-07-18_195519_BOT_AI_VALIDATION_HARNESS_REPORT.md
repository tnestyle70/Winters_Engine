# Bot AI Validation Harness Report

- Date: 2026-07-18 19:56:28 +09:00
- Repo: C:\Users\user\Desktop\Winters
- Configuration: Debug
- Overall: PASS

## Steps

- PASS git diff --check exit=0 seconds=0.11
- PASS ChampionAI dependency boundary audit exit=1 seconds=0.02
  - Notes: No matches.
- PASS Yone E stage-2 contract audit exit=0 seconds=0.08
- PASS LoL data-driven pipeline and SimLab regression exit=0 seconds=69.15

## Output Tail

### git diff --check

```text
cmd.exe : warning: in the working copy of 'Server/Private/Game/GameRoomChampionAI.cpp', LF will be replaced by CRLF the
 next time Git touches it
At C:\Users\user\Desktop\Winters\Tools\Harness\Run-BotAiValidation.ps1:77 char:20
+             $raw = & cmd.exe /c $CommandLine 2>&1
+                    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (warning: in the... Git touches it:String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError

warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp', LF will be replaced by CRLF t
he next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h', LF will be replaced by CRLF the
 next time Git touches it
warning: in the working copy of 'Tools/SimLab/main.cpp', LF will be replaced by CRLF the next time Git touches it
```

### Yone E stage-2 contract audit

```text
Yone E stage-2 contract is consistent.
```

### LoL data-driven pipeline and SimLab regression

```text
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
[YoneSim] E out caster=9 anchor=(-1.49647,0.742574) end=(-3.20001,4.36168)
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
[ViegoSim] E mist caster=8
[RivenSim] Q caster=6 stage=1 stack=1
[YoneSim] W accepted caster=9
[AsheSim] R crystal arrow caster=3
[ViegoSim] R dash caster=8
[RivenSim] E shield caster=6 amount=70
[AsheSim] E hawkshot caster=3 range=400 sight=10
[YasuoSim] Q accepted caster=1 stage=1 stack=0
[RivenSim] W stun caster=6
[ViegoSim] Q line caster=8
[YoneSim] E out caster=9 anchor=(2.14898,1.14188) end=(-1.80025,1.77716)
[ViegoSim] E mist caster=8
[RivenSim] E shield caster=6 amount=70
[YoneSim] W accepted caster=9
[YasuoSim] E active caster=1 target=10 dashed=1 window=0.5
[RivenSim] Q caster=6 stage=1 stack=1
[AsheSim] E hawkshot caster=3 range=400 sight=10
[ViegoSim] R dash caster=8
[YasuoSim] Q accepted caster=1 stage=1 stack=0
[RivenSim] E shield caster=6 amount=70
[YoneSim] E out caster=9 anchor=(3.97125,1.34304) end=(2.90072,-2.51104)
[RivenSim] W stun caster=6
[ViegoSim] E mist caster=8
[ViegoSim] Q line caster=8
[AsheSim] E hawkshot caster=3 range=400 sight=10
[ViegoSim] R dash caster=8
[YoneSim] W accepted caster=9
[ViegoSim] R dash caster=8
[SimLab][FormulaData] PASS: 17 champions, 85 skills, ranked costs/cooldowns/damage
[SimLab][BORK] PASS: one 10% max-HP on-hit and post-mitigation lifesteal
[SimLab][ManaItems] PASS: Essence Reaver BA/Q mana + Manamune 360 -> Muramana
[SimLab][Resource] PASS: 17-kind matrix, Energy 200/+10, None/Flow, Zed costs, mana-item/restore gate
[SimLab][ActiveItems] PASS: Sundered/Zhonya/QSS/ward/reorder
[SimLab][SkillRank] PASS: Q/W/E 1/3/... and R 6/11/16 gates
[SimLab] same-seed replay OK: hash=B9D22EBCEA88927E
[SimLab] seed sensitivity OK: seed+1 hash=1300E90BC57C3951
[SimLab] PASS
[LoLDataDriven] Whitespace validation
warning: in the working copy of 'Server/Private/Game/GameRoomChampionAI.cpp', LF will be replaced by CRLF the next time
 Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp', LF will be replaced by CRLF t
he next time Git touches it
warning: in the working copy of 'Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h', LF will be replaced by CRLF the
 next time Git touches it
warning: in the working copy of 'Tools/SimLab/main.cpp', LF will be replaced by CRLF the next time Git touches it
[LoLDataDriven] PASS
```
