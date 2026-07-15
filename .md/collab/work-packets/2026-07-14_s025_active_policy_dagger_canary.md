# Work Packet: S025 Active Policy / DAgger Canary

## Metadata

- ID: `2026-07-14_s025_active_policy_dagger_canary`
- Status: `Handoff`
- Agent: `Codex`
- Owner: Desktop
- Branch: `main` (shared dirty working tree, 미커밋)
- Base: `8676251`

## Owned Paths

- `.md/build/2026-07-14_S025_ACTIVE_POLICY_DAGGER_CANARY_REPORT.md`
- `.md/collab/work-packets/2026-07-14_s025_active_policy_dagger_canary.md`
- `Tools/SimLab/main.cpp`
- `Tools/AIResearch/AiImitationDatasetSchema.py`
- `Tools/AIResearch/MaterializeImitationDataset.py`
- `Tools/AIResearch/TrainImitationRankingBaseline.py`
- `Tools/AIResearch/fixtures/ai_decision_correction_sidecar_v1_golden.json`
- `Tools/AIResearch/tests/BuildActiveMacroCanaryPolicy.py`
- `Tools/AIResearch/tests/RunActiveAiPolicyEpisodeProbe.ps1`
- `Tools/AIResearch/tests/test_materialize_imitation_dataset.py`
- `Tools/AIResearch/tests/test_train_imitation_ranking_baseline.py`
- `Tools/AIResearch/RunValidation.ps1`
- `Tools/AIResearch/README.md`
- `Tools/AIResearch/AI_EPISODE_V1.md`

## Read-Only / Excluded Paths

- S024 `Server/GameRoom/ReplayRecorder`, `Tools/Harness`, S024 문서
- `Shared/**`, `Server/**`, `Client/**`, `Engine/**`
- `.md/collab/ACTIVE_WORK_PACKETS.md` (S024 소유 중이어서 수정하지 않음)

## Final Validation

- SimLab Debug x64 `/m:1`: PASS
- SimLab SHA-256: `677E657C0075F92F3FD64175247605198C6FBB5E7DCAB99CD54FF3D08275477B`
- Active two-pass canary repeat A/B: PASS
- active transitions/evaluated/interventions/applied/safety: `41/41/41/21/20`
- active JSONL SHA: `043F2AA728A80CA8F8B544C8C05668D2835642B334033FF6E0F3DCCDC5B6AD04`
- active report SHA: `874A683B6A405EB749ABE79C07949FA94633B81006CA7A10CA0CDE03F758E1DB`
- truncated WBC decode fail-closed: exit 1, output 없음
- existing live episode repeat/promotion: PASS
- `SimLab.exe 1800 42`: `DB0DC85E451999AD`, seed+1 `57A9B2394575042A`, PASS
- Python unittest: `80/80 PASS`
- PS parse/Python AST/trailing whitespace/scoped diff-check: PASS
- 종료 시 `msbuild/cl/link/SimLab/python`: 0

## Handoff Notes

- active episode는 expert corpus가 아니라 evaluation/DAgger state discovery다.
- active source action을 직접 BC하지 않고 `--corrected-only` 인간 교정만 학습한다.
- canary WBC는 active path 증명용 고정 artifact이며 실력·승급 artifact가 아니다.
- production active policy, PPO, 5v5 league, 자동 F9 correction capture는 후속이다.
- build-lock은 검증 직후 S024 owner에게 반환했다.
- 기존 dirty 변경을 reset/revert하지 않았고 commit은 만들지 않았다.

