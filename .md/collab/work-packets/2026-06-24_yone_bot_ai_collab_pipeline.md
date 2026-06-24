# Work Packet: Yone Bot AI Collab Pipeline

## Metadata

- ID: `2026-06-24_yone_bot_ai_collab_pipeline`
- Status: `Active`
- Owner: Desktop
- Branch: `main`
- Base: `origin/main`

## Owned Paths

- `.md/collab/ACTIVE_WORK_PACKETS.md`
- `.md/collab/OWNERSHIP_MATRIX.md`
- `.md/collab/GIT_SYNC_RULES.md`
- `.md/collab/HARNESS_RULES.md`
- `.md/collab/work-packets/2026-06-24_yone_bot_ai_collab_pipeline.md`
- `.md/plan/Champion/2026-06-24_YONE_BOT_AI_CODEBASE_AUDIT_AND_CONTINUATION_PLAN.md`
- `Tools/Harness/Run-BotAiValidation.ps1`
- `.md/build/2026-06-24_BOT_AI_COLLAB_PIPELINE_REPORT.md`

## Read-Only Paths

- `Shared/GameSim/**`
- `Server/**`
- `Client/**`
- `Engine/**`
- `EngineSDK/inc/**`
- `Data/LoL/**`
- `Tools/SimLab/**`

## Validation

- `Tools/Harness/Run-BotAiValidation.ps1 -SkipFullPipeline -AllowKnownYoneEContractGap -ReportPath .md\build\2026-06-24_BOT_AI_COLLAB_PIPELINE_REPORT.md`

## Handoff Notes

- 이 packet은 협업 규칙과 Bot AI validation harness를 세팅한다.
- GameSim/Server gameplay code는 이 packet에서 수정하지 않는다.
- 현재 Yone E return stage-2 data contract mismatch는 known gap으로 보고서에 남긴다.
- 다음 packet은 `skill.yone.e` stage-2 contract fix와 targeted scenario를 소유한다.
