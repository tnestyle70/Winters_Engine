Session - S017 NYPC-to-Winters AI research pipeline implementation and validation result.

# S017 AI Research Pipeline Validation

작성일: 2026-07-12  
검증 명령:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File Tools/AIResearch/RunValidation.ps1 `
  -FullBuild -SimTicks 1800 -Seed 42
```

## 결과

- bridge manifest: PASS, 10 selected/blocked entries with source SHA/provenance checks.
- Python contracts: PASS, 61/61 tests.
- C++ contract probes: PASS, research POD, AiEpisode codec, influence map, replay command journal, timeline rebase.
- Shared dependency boundary: PASS.
- GameSim Debug x64: PASS.
- SimLab Debug x64: PASS.
- live AiEpisode smoke: PASS, 240 ticks, 8 final team-filtered records, raw/promotion validation, two-run byte equality.
- deterministic 5v5: PASS, 1,800 ticks; same-seed hash `DB0DC85E451999AD`, seed+1 hash `57A9B2394575042A`.
- Server Debug x64: PASS.
- Client Debug x64: PASS.
- ReplayPlayer Snapshot/Event fail-closed validation 후 Client Debug x64 재빌드: PASS.

검증된 live artifact:

```text
AiEpisode JSONL SHA-256
BECC9A151ABD8C24E45D79C787EA37FDB7BAFFA513896723128F912C98B0130E

SimLab.exe SHA-256
D8E4228D231CBCD7DDC81FADCBE585147D618625AC6E370E649032047A0AA100

WintersServer.exe SHA-256
AAE771319C5FBAF8B7211B424A7CFE60D56AE1013E21B266323242D4536ED712

WintersGame.exe SHA-256 (ReplayPlayer hardening rebuild)
CC9335842E33B480ABCDEB672FA6B33EC23B6BB77B4C3EFF251178B17D78EC4D
```

## 이번 패킷에서 닫은 경계

- NYPC 파일 일괄 복사 대신 allow/deny manifest와 SHA 검증을 사용한다.
- AI observation/candidate/action-mask/executor outcome을 versioned typed record로 남긴다.
- 현재 적 사실은 팀 시야의 반경·원뿔·은신/은폐 경계를 통과한 경우만 기록한다. 숨은 적의 현재 HP/레벨/경제/거리는 0으로 정규화한다.
- 마지막 목격은 5초 confidence decay memory와 `ThreatBelief` 연구 계측으로 분리한다.
- Influence v1은 9x9 `ThreatNow/ThreatBelief/SupportEta/EscapeCost` 진단 계층이며 authoritative state/keyframe에서 분리한 transient component다.
- command executor가 실제 sequence의 Accepted/Rejected 결과를 trace에 되돌려 쓴다.
- checkpoint restore는 allocator/component/EntityIdMap topology를 검증한 뒤 transactional swap한다.
- timeline epoch/branch/tool revision이 Snapshot을 통해 전달되고 Client는 분기 변경 시 prediction/event/visual cache와 모든 non-local NetId binding을 rebase한다.
- `EntityIdMap::Bind`는 재바인딩 뒤에도 net/entity 1:1 대응을 보존한다.
- WRPL v2는 command domain/tool revision 계약과 크기·개수·단조 tick·trailing-byte·FlatBuffer payload 검증에서 fail closed한다.
- NumPy pairwise imitation baseline은 frozen scenario group split과 promotion-valid expert label만 사용한다.

## 아직 완료로 주장하지 않는 범위

- 현재 FOW는 Champion AI observation 경계다. terrain wall LOS와 클라이언트별 network snapshot confidentiality는 아직 아니다.
- Influence v1은 연구/디버그 계측이다. Brain feature, AiEpisode export, Client overlay에는 아직 연결되지 않았다.
- AiEpisodeV1은 decision-event/BC 계약이다. next observation, variable delta, dense per-tick trajectory를 갖춘 PPO rollout 계약은 아니다.
- Chrono는 checkpoint rewind와 client rebase까지다. journal exact re-simulation, faithful/reactive A/B, first-divergence runner는 후속이다.
- trainer는 deterministic NumPy pairwise baseline이다. 실제 PyTorch BC/DAgger/PPO, self-play opponent pool, measured league promotion, ShadowCoach는 후속이다.
- 빌드에는 기존 DLL interface 계열 C4251/C4275 warning이 남아 있으나 오류와 링크 실패는 없었다.

지원은 위 후속 항목을 기다리지 않고 2026-07-15에 먼저 진행한다. 미구현 항목은 포트폴리오에서 구현 완료로 표기하지 않는다.
