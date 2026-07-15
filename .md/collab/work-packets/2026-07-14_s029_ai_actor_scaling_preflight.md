# Work Packet: S029 AI Actor Scaling Preflight

## Metadata

- ID: `2026-07-14_s029_ai_actor_scaling_preflight`
- Status: `Handoff`
- Agent: `Codex`
- Owner: Desktop
- Branch: `main` (shared dirty working tree, no commit created)
- Base: S024 final PASS/source freeze/build-lock handoff
- Started/Finished: `2026-07-14`

## Objective

S024 54,000-tick 5v5 stability 결과를 독립 확인하고, IL/RL 확대 전에 실제 `CGameRoom`과 active-policy canary가 한 호스트에서 process-isolated 1/2/4/8 actor로 결정론을 보존하며 scale하는지 측정한다.

## Owned Paths

- `Tools/Harness/RunGameRoomActorScalingPreflight.ps1`
- `Tools/AIResearch/tests/RunActiveAiActorScalingPreflight.ps1`
- `.md/build/2026-07-14_S029_AI_ACTOR_SCALING_PREFLIGHT_REPORT.md`
- `.md/collab/work-packets/2026-07-14_s029_ai_actor_scaling_preflight.md`
- `.md/collab/ACTIVE_WORK_PACKETS.md` S029 row only
- `.md/build/evidence/s029_ai_actor_scaling/20260714_042900_final/**`

## Read-Only/Excluded Paths

- Engine JobSystem/Chase-Lev/Fiber
- Shared/GameSim gameplay and schema
- Server GameRoom/replay/network implementation
- Client snapshot/F9/network/render
- project files and generated schema

## Final Validation

- S024 54,000-tick evidence/hashes/process-zero/build-lock: verified
- Server Release x64 `/m:1`: PASS
  - SHA-256 `92FB25176E4C1022D1E6FD273ECFEBB94A20132BAB13E618AA347BB7DFA4348E`
- Release GameRoom harness freshness: PASS
  - SHA-256 `9EC4C60B6F5FD232BECF712622F713DAD9F2C41E17FE23C62218DBB15F7EB5D0`
- SimLab Debug x64 `/m:1`: PASS
  - SHA-256 `E9177E894DA67838215251FE1D9210A28959CA6FFCA477AA787593B46C7F04D5`
- SimLab 1,800 tick seed 42: PASS
  - `DB0DC85E451999AD`, seed+1 `57A9B2394575042A`
- GameRoom process-isolated actor 1/2/4/8: PASS
  - 8 actor 6,002.369 world-tick/s, 76.655% efficiency
  - max p99/max 4.971/28.011 ms, deadline miss 0
  - replay/world/final-state `AE0D740A55082FB6` / `D12B7306C1B70685` / `7BDE3420172DB09CC48ECDB4A15C59490E53B393CD60078642C00C6E09BF0EDF`
- active canary actor 1/2/4/8: PASS
  - 8 actor 540.330 transitions/evaluations/s, 93.779% efficiency
  - native trace/metadata/report/validated JSONL/final-state all deterministic
- active A/B export/negative probe: PASS
- AIResearch Python tests: 80/80 PASS
- C++ trace/codec/influence/replay/timeline/shared-boundary: PASS
- measured corpus: 64 records, 32 frozen/mirror groups, four classes x16, promotion PASS
  - SHA-256 `1A823A360D790D7ACD8A277B382C1D35179F3C6BE3C74DEA52EFC475D5C8F8C6`
- PyTorch masked BC A/B: report/binary byte-identical
  - report SHA-256 `74FA508B450ADBC4CD4101ABE442076CD4D9AD4A4BC91323B8549B5C2B220005`
  - `.wbc` SHA-256 `20B810F375FA15C2379DD7F4DBFFC0FC6CF6914817592257D0F120FD63014022`
- Python/C++ max absolute logit delta: `0` (`<= 1e-5`)
- runner AST/trailing whitespace: PASS
- final `git diff --check`: PASS
- final `msbuild/cl/link/GameRoomBotMatchSoak/SimLab/python` process count: 0

## Decision

- Process-isolated headless actor farm: `GO`, recommended maximum proven count `8` on this host.
- Same-process/networked multi-room: `NO-GO`, not validated.
- Current BC artifact: `SHADOW_ONLY`, runtime promotion `NO-GO`.
- Large-scale IL/RL/PPO/self-play: `NO-GO` until decision-native persistent 5v5 capture and seed-manifest gate exist.

## Handoff Notes

- S029 changed no gameplay, Engine, Shared, Server, Client, Network, schema, or project file.
- The active canary is evaluation/DAgger state discovery only and is not expert IL input.
- GameRoom command rate is a proxy, not exact decision/evaluation rate.
- Next slice is persistent decision-native actor capture, not PPO.
- Existing dirty changes were preserved; no stage/commit was created.
- Full report: `.md/build/2026-07-14_S029_AI_ACTOR_SCALING_PREFLIGHT_REPORT.md`
- Evidence: `.md/build/evidence/s029_ai_actor_scaling/20260714_042900_final`
