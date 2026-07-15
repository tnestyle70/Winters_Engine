Session - S024는 실제 Server `CGameRoom` 5:5 봇전을 30분 simulation-time 동안 실행하고, 발견된 장기 수명·결정론·이동 liveness 결함을 수정한 결과를 정본으로 고정한다.

# S024 5:5 Bot 30-Minute Stability Result

## 1. 최종 판정

`Release x64`, seed `42`, 30 Hz 기준 `54,000` tick 장시간 soak 두 회와 same-seed determinism gate는 모두 **PASS**다.

- 실제 `CGameRoom::Tick`을 run마다 54,000회, 총 108,000회 호출했다.
- 10개 ChampionAI bot이 Blue/Red 5:5로 참여했다.
- minion, structure, projectile, ChampionAI, snapshot, event, replay, respawn 경로가 함께 실행됐다.
- 10/10 bot이 끝까지 command-active였고 inactive bot은 0이었다.
- 사망/부활은 20/20, AI reset 및 mana restore 실패는 0이었다.
- entity initial/peak/final은 53/130/97, zero-component entity는 전 구간 0이었다.
- prepared-to-peak private memory 증가는 2.902/2.594 MiB, stop 후 private memory는 27.750/27.879 MiB였다.
- 각 run의 replay 143,018 records, 1,420,783,432 bytes가 sealed·published·전체 파싱됐고 finalize는 0.666/0.639초였다.
- p99 tick은 3.440/3.551 ms, 최대 tick은 40.926/22.644 ms, 30 Hz deadline miss는 6/0이었다.
- replay hash `F172FA227ACA7576`, raw world hash `3B12304F5110E999`, final-state SHA-256 `EA8CBF81223AB0A31B785596CD31562F5973B100AFBC62BDD3AAD9765982C19F`가 두 run에서 모두 일치했다.

이 결과는 **30분 wall-clock paced 온라인 게임**이 아니라 **30분 simulation-time accelerated headless GameRoom soak**다. Client rendering, TCP/UDP socket, IOCP fanout, JobSystem/Fiber 실행 모드는 포함되지 않는다.

## 2. 문제 정의와 CS적 본질

장시간 안정성은 단순히 crash가 없는 상태가 아니다. 다음 계약이 동시에 유지되어야 한다.

1. **Safety**: NaN, 잘못된 entity binding, component 없는 live entity, 손상 replay가 없어야 한다.
2. **Liveness**: 살아 있는 bot이 영구히 같은 상태나 경로에서 진전 없이 멈추면 안 된다.
3. **Boundedness**: entity, replay 메모리, process memory, handle이 시간에 비례해 무한 증가하면 안 된다.
4. **Determinism**: 같은 seed와 입력은 replay stream과 raw world checkpoint까지 같아야 한다.
5. **Lifecycle closure**: death/respawn/recall/room stop/replay finalize가 이전 상태와 자원을 정확히 닫아야 한다.

짧은 unit test는 한 함수의 결과를 검증하지만 이 계약들은 수천 번의 spawn/despawn, combat, recall, replay write가 누적될 때 깨진다. Stress/soak가 중간에 등장한 이유는 성능 숫자를 만들기 위해서가 아니라, 확률적 interleaving과 시간 누적 결함을 재현 가능한 실패 tick으로 바꾸기 위해서다.

JobSystem·Chase-Lev·Fiber stress도 같은 원리다. submit/shutdown, owner pop/thief steal, wait/wakeup은 특정 thread 순서에서만 실패하므로 반복 경쟁으로 schedule 공간을 넓혀야 한다. Stress PASS는 결함 부재의 수학적 증명이 아니라, 명시한 경쟁·수명 계약을 많은 interleaving에서 위반하지 않았다는 증거다.

## 3. 검증 구조와 gate

```text
GameRoomBotMatchSoak
  -> CGameRoom 준비/5:5 bot roster
  -> 30 Hz TickContext
  -> ChampionAI -> GameCommand -> CommandExecutor
  -> Move/Combat/Skill/Minion/Turret/Respawn systems
  -> SnapshotBuilder + replicated events
  -> ReplayRecorder spool
  -> room Stop -> sealed replay atomic publish
  -> replay full-stream parse/hash + world keyframe/hash
```

Roster:

- Blue: Yasuo, Zed, Ashe, Annie, Lee Sin
- Red: Riven, Sylas, Viego, Yone, Jax

Acceptance gate:

- finite position/rotation/HP/mana/timer
- live entity cap 512, zero-component entity 0, entity-map 양방향 일관성
- replay write health, header/count/tick/sequence 및 full record stream 검증
- 동일 seed replay hash와 raw world checkpoint hash A/B 일치
- 10개 bot command activity, 60 simulation-second inactivity fail-closed
- death/respawn 대응, respawn AI commitment reset, full mana restore
- p99 < 33.333 ms, single tick < 500 ms, deadline miss rate <= 0.5%
- private memory <= 1,024 MiB, steady handle delta 제한
- room stop/replay finalize <= 10초

명시적 비범위는 Client render/input/interpolation, TCP/UDP socket·IOCP·session hub, packet loss/reorder, JobSystem/Fiber GameRoom 병렬화, wall-clock pacing과 실제 10개 외부 client 및 multi-seed soak다.

## 4. 실제 수정

### 4.1 Replay bounded lifetime과 atomic finalize

모든 record byte를 process vector에 누적하면 54,000 tick에서 1 GiB 이상 RAM을 시간에 비례해 소비한다. 수정 후 record header/payload를 temp spool에 순차 기록하고, Stop 시 seal한 뒤 destination staging file과 `MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH)`로 최종 publish한다. Publish 실패 후에도 sealed spool을 보존해 retry할 수 있고, GameRoom은 실제 save 성공 전에는 finalize 완료로 간주하지 않는다.

### 4.2 Entity와 lifecycle closure

- 소비된 TowerAggroNotify marker는 component만 제거하지 않고 `DestroyEntity`로 lifetime을 닫았다.
- death/respawn 시 target, combo, dive, last-seen, trace, timer 등 transient ChampionAI commitment를 중앙 reset한다.
- respawn과 recall 완료 시 mana를 full restore한다.

최종 soak에서 zero-component entity와 잔존 aggro notification은 0, respawn AI/mana failure도 0이었다.

### 4.3 Raw checkpoint padding 결정론

`PodSave<T>`는 trivially-copyable object representation을 raw copy한다. C++ implicit padding은 의미 상태가 아니며 초기화가 보장되지 않으므로 replay hash가 같아도 raw world hash가 달라질 수 있었다. 초기 A/B의 393개 차이 byte를 component/offset에 매핑해 implicit gap과 empty tag representation을 명시적 zero-initialized reserved byte로 바꿨다. 이후 두 v3 54,000-tick blob의 교차 감사에서 의미 필드는 전부 같지만 `VisibilityComponent` 2 bytes × 10 entities와 `ZedSimComponent` 3 bytes × 2 entities, 총 26 bytes의 잔여 padding drift를 다시 발견했다. 두 gap도 명시적 zero-reserved field로 닫고 Engine public/EngineSDK mirror를 동기화했다. `sizeof`/중요 `offsetof` assertion으로 ABI를 고정했으며 최종 v4 1,800 및 54,000 A/B는 replay, world hash, raw final-state SHA까지 일치한다. raw layout 변경마다 keyframe version을 올려 현재는 v4이며, 이전 v2/v3 blob은 잘못 해석하지 않고 fail-closed한다.

### 4.4 Retreat movement progress

첫 실패는 active path가 남아 AI가 equivalent move를 재발행하지 않지만 unit avoidance 후보가 모두 막혀 영구 정지하는 문제였다. 180도 후보는 한 배치를 풀었으나, 다른 배치에서는 장애물 주변 미세 이동이 계속되어 좌표 변화만으로는 진전을 판단할 수 없었다.

최종 정의:

```text
현재 waypoint까지의 best distance를 MoveTarget에 저장
-> 누적 5 cm 이상 개선이면 blocked counter reset
-> 새 path/waypoint면 progress state reset
-> action/skill movement lock 동안은 blocked로 세지 않음
-> 3초 동안 best distance가 개선되지 않은 Retreat만 기존 Recall로 fail over
```

거짓 command 재발행이나 teleport 대신 authoritative GameSim progress를 기록하며, 저체력 Retreat에서만 기존 safe-state transition을 사용한다. AttackChase, CombatAction, WaypointPatrol 및 command 경계에서도 새 목표가 이전 progress state를 물려받지 않도록 함께 reset한다. `MoveTargetComponent` checkpoint ABI는 6,204 bytes로 고정했다.

## 5. 실패 → 원인 → 수정 타임라인

| 단계 | 결과 | 발견/조치 |
|---|---|---|
| Debug 1,800 1차 | FAIL, p99 34.383 ms | Debug 계측/외부 부하; 기능과 Release 성능 gate 분리 |
| Debug 1,800 2차 | FAIL, deadline miss 18 | 0.5% limit 초과; Release를 장시간 acceptance config로 고정 |
| Release 1,800 A/B 초기 | 개별 기능 PASS, world hash 불일치 | replay hash 동일 → gameplay가 아니라 raw POD padding; gap 전수 초기화 |
| Release 54,000 1차 | tick 16,920 FAIL | Red Yone Retreat, path active, 60초 inactivity; 180도 avoidance와 상세 진단 추가 |
| Release 18,000 | PASS | 첫 failure tick 통과 |
| Release 54,000 2차 | tick 41,190 FAIL | Blue Yasuo의 다른 Retreat dead-end; blocked state 설계 |
| displacement counter 버전 | tick 18,210 FAIL | 좌표는 미세 변화하지만 goal progress 없음; best distance로 재정의 |
| Release 20,000 | PASS | 18,210 회귀 지점 통과 |
| Release 1,800 A/B v3 | PASS/PASS | replay/world raw hash 동시 일치; 짧은 lifecycle 범위 통과 |
| Release 54,000 v3 외부 경합 run | 기능·liveness·memory·replay PASS, 성능 gate FAIL | 보호된 Windows service child가 약 2.4 GiB와 3개 안팎 CPU core를 점유; p99 35.501 ms, miss 588 |
| Release 54,000 v3 clean run | PASS | 임계값 변경 없이 30분 simulation-time gate 통과 |
| v3 54,000 cross-run 감사 | replay 동일, raw world 불일치 | 의미 상태는 같고 `Visibility`/`ZedSim` implicit padding 26 bytes만 drift; zero-reserved + v4 전환 |
| Release 1,800 A/B v4 | PASS/PASS | replay/world/raw blob 일치 |
| Release 54,000 A/B v4 최종 | PASS/PASS | 두 30분 simulation-time run의 모든 gate 및 장기 raw 결정론 통과 |

일부 중간 run에는 별도 Windows service child process가 CPU와 약 2.4 GiB private memory를 사용해 성능 수치가 오염됐다. 해당 외부 process는 종료하지 않았고, liveness 실패는 clean run에서도 재현해 product defect로 수정했다. 오염된 v3 run은 tick 54,000까지 기능·수명 gate를 통과했지만 성능 gate 때문에 전체 FAIL로 보존했다(`release_ticks_54000_seed_42_20260714_030453_173_ff1de38c`). 그 v3 blob과 다음 clean v3 blob의 26-byte padding drift도 숨기지 않고 위 v4 수정의 직접 원인으로 삼았다. 최종 v4 장기 A/B는 외부 process를 조작하거나 임계값을 완화하지 않고 모두 통과했다.

## 6. 빌드·실행 명령과 artifact

Runner의 freshness build:

```powershell
MSBuild.exe Server\Include\Server.vcxproj /m:1 /nr:false /t:Build /p:Configuration=Release /p:Platform=x64 /verbosity:minimal
```

```powershell
# Final source freshness build + regression
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\Harness\RunGameRoomBotMatchSoak.ps1 -Configuration Release -TickCount 20000 -Seed 42 -Runs 1 -HeartbeatTicks 1800 -PrivateLimitMiB 1024

# Final same-seed A/B
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\Harness\RunGameRoomBotMatchSoak.ps1 -Configuration Release -TickCount 1800 -Seed 42 -Runs 2 -HeartbeatTicks 1800 -PrivateLimitMiB 1024 -SkipServerBuild

# Exact 30-minute simulation-time soak
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\Harness\RunGameRoomBotMatchSoak.ps1 -Configuration Release -TickCount 54000 -Seed 42 -Runs 2 -HeartbeatTicks 1800 -PrivateLimitMiB 1024 -SkipServerBuild

# Replay contract
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\Harness\RunReplayCommandContractProbe.ps1

# Keyframe v3 restore/determinism regression
MSBuild.exe Tools\SimLab\SimLab.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
.\Tools\Bin\Debug\SimLab.exe 1800 42
```

- harness EXE SHA-256: `9F34F8EB9850AE15167048D42BDCB1ABC60444C78B12A3E1DF96335F49C50CAD`
- Release Server SHA-256: `9E47D01CE85355BFDD4C566EEDE671F4904ED859B740CEE4A451B5F89C10BBCA`
- Debug SimLab SHA-256: `2709797ACB10BFB4251D20C7CBF7EC355CE3D2285DDBE47FA6ECD1281D790030`
- Release Server/GameSim build: PASS; 기존 C4275 DLL-interface warning만 존재
- SharedBoundary: PASS
- Replay v1/v2, command journal, sealed publish retry contract: PASS
- SimLab keyframe atomic/restore/same-seed replay regression: PASS (`DB0DC85E451999AD`, seed+1 `57A9B2394575042A`)

## 7. 최종 수치

### 7.1 동일 seed 1,800 tick A/B

Evidence: `.md/build/evidence/s024_bot_soak/release_ticks_1800_seed_42_20260714_032135_467_469c2b93`

| 항목 | Run 1 | Run 2 |
|---|---:|---:|
| replay hash | `9D4F43DF5201865E` | `9D4F43DF5201865E` |
| world hash | `D789194186577787` | `D789194186577787` |
| p50/p95/p99 us | 1419.050/1907.620/2775.241 | 1391.900/1967.410/2591.248 |
| max us/deadline miss | 7394.700/0 | 4410.800/0 |
| peak private/growth MiB | 45.734/1.742 | 45.633/2.391 |
| deaths/respawns | 2/1 | 2/1 |

Raw final keyframe도 byte-identical, 198,063 bytes다.

### 7.2 v4 이전 20,000 tick 기능·liveness 회귀

Evidence: `.md/build/evidence/s024_bot_soak/release_ticks_20000_seed_42_20260714_025140_288_f6a84e51`

- replay/world: `0712ACC989E97CFA` / `C34ADBC5EA932646`
- p99/max: 3.255/8.320 ms, deadline miss 0
- deaths/respawns: 16/16, peak entities 129, private growth 2.645 MiB, inactive bot 0

### 7.3 정확히 54,000 tick 장기 A/B

Evidence: `.md/build/evidence/s024_bot_soak/release_ticks_54000_seed_42_20260714_032420_855_acca44c0`

| 항목 | Run 1 | Run 2 |
|---|---:|---:|
| seed/ticks/sim time | 42/54,000/1,800.000 sec | 42/54,000/1,800.000 sec |
| accelerated wall/stop finalize sec | 106.216/0.666 | 112.543/0.639 |
| p50/p95/p99/max ms | 1.868/3.043/3.440/40.926 | 1.979/3.294/3.551/22.644 |
| deadline misses | 6/54,000 | 0/54,000 |
| prepared RSS/private MiB | 49.859/43.359 | 49.852/43.949 |
| peak RSS/private MiB | 53.293/46.262 | 53.215/46.543 |
| private growth/post-stop MiB | 2.902/27.750 | 2.594/27.879 |
| initial/peak/final entities | 53/130/97 | 53/130/97 |
| peak/final minions | 69/39 | 69/39 |
| peak/final projectiles | 14/3 | 14/3 |
| deaths/respawns | 20/20 | 20/20 |
| AI reset/mana failures | 0/0 | 0/0 |
| command-active/inactive bots | 10/0 | 10/0 |
| max command inactivity | 538 ticks | 538 ticks |
| final command sequence sum | 47,750 | 47,750 |
| replay records | 143,018 | 143,018 |
| snapshots/events/external commands | 54,000/89,018/0 | 54,000/89,018/0 |
| replay file/hashed record bytes | 1,420,783,432/1,420,783,384 | 1,420,783,432/1,420,783,384 |
| final keyframe bytes | 213,466 | 213,466 |
| replay/world hash | `F172FA227ACA7576`/`3B12304F5110E999` | `F172FA227ACA7576`/`3B12304F5110E999` |
| final-state SHA-256 | `EA8CBF81223AB0A31B785596CD31562F5973B100AFBC62BDD3AAD9765982C19F` | `EA8CBF81223AB0A31B785596CD31562F5973B100AFBC62BDD3AAD9765982C19F` |
| steady handle delta | 9 | 9 |

External replay command 0은 의도된 결과다. 이 headless match에는 player input이 없고 bot AI command는 deterministic regeneration 대상이라 command journal에 기록하지 않는다.

## 8. S023 Job/Fiber/UDP 및 S025 경계

S023 정본 상태는 S024가 바꾸지 않았다.

- JobSystem submit race/lifetime과 Chase-Lev fixed pointer deque: 구현·stress 검증 완료
- FiberFull scheduler와 Server startup/shutdown lifecycle: 구현·runtime probe 완료
- UDP v3 feature-gated vertical slice와 `tcp|udp|dual` integration: 구현 완료
- 남은 것: Application Verifier급 concurrency soak, GameRoom workload jobification/Fiber speedup 측정, production UDP cutover와 실제 loss/reorder 5:5 soak

따라서 S024는 authoritative gameplay/replay/lifecycle의 장시간 안정성을 닫았지만 UDP/Fiber production readiness를 대신 증명하지 않는다.

S025 active policy/DAgger canary는 `Tools/SimLab`과 `Tools/AIResearch`에서 별도 Handoff됐다. 두 lane은 source/build lock을 직렬화했고 S025가 Server/Shared/Engine/Network 경로를 수정하지 않는 경계를 보존했다.

## 9. 남은 위험과 다음 gate

1. 실제 Server + 10 external clients를 `tcp`, `udp`, `dual` 각각 wall-clock 30분 실행한다.
2. UDP에 loss, reorder, duplicate, fragmentation, reconnect, address rebinding을 주입한다.
3. GameRoom phase를 job graph로 나누기 전에 read/write set과 deterministic merge point를 명시하고 ThreadOnly/FiberFull A/B를 측정한다.
4. multi-seed와 다른 champion 조합으로 장기 결정론의 입력 공간을 확장한다.
5. Application Verifier/ASan 계열 도구와 hang watchdog으로 heap/handle/UAF 및 bounded termination을 보강한다.

이 항목들은 현재 PASS를 무효화하지 않지만 “실제 온라인 UDP/Fiber 5:5 production-ready” 주장에는 별도 gate가 필요하다.

## 10. Handoff

- shared dirty worktree의 기존 변경은 reset/revert/stage/commit하지 않았다.
- S024 범위 밖 AIResearch/SimLab, Client, Network, Job/Fiber 기능은 추가 수정하지 않았다.
- 최종 source는 freeze 상태다.
- 최종 process 0 확인 후 build lock을 다음 lane으로 반환한다.
