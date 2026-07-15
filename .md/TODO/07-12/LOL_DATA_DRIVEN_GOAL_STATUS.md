# LoL Data-Driven Goal Status

generatedAtUtc: 2026-07-12T08:09:43.7546111Z
buildHash: 592494486
complete: False
completedGoalCount: 2 / 6

## Next Focus

- phase: P5
- key: P5AiPolicyHardcode
- remaining: 217
- action: Move ChampionAIPolicy profile, combo, and bot skill-rank constants to ServerPrivate AI definition data.

## Goals

- [PASS] P3 P3SkillEffectHardcode: current=0, targetMax=0
- [PASS] P4 P4VisualAuthorityLeak: current=0, targetMax=0
- [TODO] P5 P5AiPolicyHardcode: current=217, targetMax=0
- [TODO] P6 P6ObjectWaveHardcode: current=33, targetMax=0
- [TODO] P7 P7NetworkIdentityLegacy: current=579, targetMax=0
- [TODO] P8 P8LegacyValueOwner: current=194, targetMax=0

## Gate

- powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1
- All goals have currentCount <= targetMax and the full verification command passes.
