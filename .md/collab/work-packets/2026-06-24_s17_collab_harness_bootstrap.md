# Work Packet: S17 Collab Harness Bootstrap

## Metadata

- ID: `2026-06-24_s17_collab_harness_bootstrap`
- Status: `Handoff`
- Owner: Desktop
- Branch: `main`
- Base: `origin/main`

## Owned Paths

- `.md/collab/ACTIVE_WORK_PACKETS.md`
- `.md/collab/OWNERSHIP_MATRIX.md`
- `.md/collab/GIT_SYNC_RULES.md`
- `.md/collab/HARNESS_RULES.md`
- `.md/collab/work-packets/2026-06-24_s17_collab_harness_bootstrap.md`
- `Tools/Harness/Run-S17RhiValidation.ps1`
- `.md/build/2026-06-24_COLLAB_HARNESS_BOOTSTRAP_REPORT.md`

## Read-Only Paths

- `Client/**`
- `Engine/**`
- `Shared/**`
- `Server/**`
- `EngineSDK/inc/**`

## Validation

- `Tools/Harness/Run-S17RhiValidation.ps1 -ReportPath .md\build\2026-06-24_S17_RHI_VALIDATION_HARNESS_REPORT.md`
  - PASS
- `git diff --check`
  - PASS
- `git status --short --branch`
  - only this packet's new files before commit

## Handoff Notes

- 목적은 두 장비 동시 작업 충돌을 줄이기 위한 문서와 검증 harness를 추가하는 것이다.
- 코드 migration 자체는 이 packet에서 수정하지 않는다.
- 다음 장비는 `git pull --rebase origin main` 후 `.md/collab/ACTIVE_WORK_PACKETS.md`에 새 packet을 추가하고 진행한다.
