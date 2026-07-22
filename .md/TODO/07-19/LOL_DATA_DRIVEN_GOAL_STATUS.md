# LoL Data-Driven Goal Status

generatedAtUtc: 2026-07-19T10:28:53.2084946Z
buildHash: 119905831
complete: False
completedGoalCount: 11 / 12

## Next Focus

- phase: P3
- key: P3GameplayTuningLiteral
- remaining: 6
- action: Move remaining champion, item, economy, rune and formula fallback values to canonical ServerPrivate data.

## Goals

- [TODO] P3 P3GameplayTuningLiteral: current=6, comparison=max, target=0
- [PASS] P3 P3PackMissFallback: current=0, comparison=max, target=0
- [PASS] P4 P4ClientGameplayLiteral: current=0, comparison=max, target=0
- [PASS] P4 P4ChampionModelCoverage: current=17, comparison=min, target=17
- [PASS] P5 P5AiPolicyHardcode: current=0, comparison=max, target=0
- [PASS] P5 P5AiProfileCoverage: current=17, comparison=min, target=17
- [PASS] P6 P6ObjectWaveHardcode: current=0, comparison=max, target=0
- [PASS] P7 P7NetworkIdentityRuntimeReader: current=0, comparison=max, target=0
- [PASS] P8 P8LegacyValueOwnerReader: current=0, comparison=max, target=0
- [PASS] P9 P9SchemaCoverage: current=11, comparison=min, target=11
- [PASS] P9 P9RuntimeReloadCoverage: current=11, comparison=min, target=11
- [PASS] P9 P9DraftRoundTrip: current=0, comparison=max, target=0

## Gate

- powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -RequireComplete
- All goals are achieved, all legacy reader counts are zero, and the read-only full verification command passes.
