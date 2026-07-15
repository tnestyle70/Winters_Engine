# Work Packet: S023 UDP + JobSystem + Fiber Runtime

## Metadata

- ID: `2026-07-13_s023_udp_job_fiber_runtime`
- Status: `Handoff`
- Outcome: 구현·검증 완료, production UDP cutover와 GameRoom workload jobification은 후속 packet
- Agent: `Codex`
- Owner: Desktop
- Branch: `main` (shared dirty working tree, 미커밋)
- Base: `8676251583d607b8d0cbab6357d4b4e33da75958`
- Verified: `2026-07-13`

## Delivered Scope

### JobSystem / Chase-Lev / Fiber

- immutable heap `WorkItem*` publication과 counter-before-publish/rollback
- Submit admission, Shutdown drain, external wait lifetime 경계
- owner-system identity와 foreign/global injection 경계
- 4,096-slot atomic pointer Chase-Lev deque
- last-item owner Pop/thief Steal CAS expected-mutation 수정
- callable exception의 exactly-once counter/node completion
- `ThreadOnly`, `FiberShell`, `FiberFull`
- worker당 64개 pooled fiber, 64 KiB commit/256 KiB reserve
- counter suspend/resume, origin-worker ready inbox, lost-wakeup 방지
- fiber-local이어야 하는 scheduler/profiler context의 FLS 전환
- `CServerEntry` lifecycle, Server job CLI와 startup self-test

### UDP runtime migration

- v3 40-byte big-endian packet header, 16-byte fragment header, 1,200-byte datagram budget
- Control/Lobby/Command/Event reliable-ordered, Snapshot/Heartbeat unreliable-sequenced lane
- lane별 ACK/retry/ordered receive와 bounded reassembly
- Hello/Retry/Connect/Accept/Confirm cookie handshake와 activation barrier
- Server UDP `WSARecvFrom`/`WSASendTo` IOCP core
- transport-neutral `CServerSessionHub`, logical session와 bounded owned-frame ingress
- Client TCP/UDP facade와 strict transport selection
- Server `tcp|udp|dual`, Client `tcp|udp` CLI
- Debug-only empty-ticket gate와 validator 부재 시 exit 5 fail-closed
- TCP rollback 기본 경로 유지
- UDP activation 거절 peer 제거와 ReliableOrdered backpressure fail-closed
- TCP send queue 256 packet/8 MiB cap, partial `WSASend` offset 재게시, atomic/idempotent socket close와 manager-level terminal teardown
- transport IOCP worker를 GameRoom stop/finalize 전에 join하는 shutdown 순서

### Evidence and documentation

- implementation plan: [UDP·JobSystem·Fiber 계획](../../plan/2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_IMPLEMENTATION_PLAN.md)
- canonical result: [UDP·JobSystem·Fiber 결과](../../build/2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_RESULT.md)
- payload data: [Replay payload JSON](../../build/2026-07-13_NETWORK_REPLAY_PAYLOAD_MEASUREMENT.json)
- S022 boundary result: [PyTorch BC Shadow Policy 보고서](../../build/2026-07-13_S022_PYTORCH_BC_SHADOW_POLICY_REPORT.md)

## Ownership and S022 Boundary

S023 primary ownership:

- Engine JobSystem, JobCounter, Chase-Lev, Fiber and related stress/profiler support
- Shared network packet semantics, UDP codec/reliability/ordering/reassembly/handshake
- Server UDP transport, session hub, ServerEntry and runtime integration
- Client UDP transport and TCP/UDP facade
- `Tools/Harness`의 Job/UDP payload·loopback·F5 smoke
- S023 plan/result/work packet

S022 primary ownership:

- `Tools/AIResearch/**`, `Tools/SimLab/main.cpp`
- Shared/GameSim ChampionAI와 policy artifact
- Snapshot schema/generated code, Server builder, Client F9 debug seam
- Server policy injection과 교차 `GameRoom`/`main.cpp`

두 packet은 같은 dirty checkout을 사용했다. Engine `WorkItem -> WorkItem*` 전환 중 한 번의 partial-source SimLab build를 발견한 뒤 다음 gate로 교정했다.

```text
MSBuild process lock
+ dependency source freeze
+ generated/project wiring freeze
```

S022 shadow byte equality 실패는 policy interference가 아니라 `JaxSimComponent` raw POD의 다섯 3-byte implicit padding이었다. 이를 zero-initialized reserved array로 바꾸면서 `sizeof == 60`과 field offset `4/20/32/44/56`을 보존했다. S022 71/71과 deterministic keyframe 검증 뒤 통합했다.

## Final Validation

### Build

- `GameSim` Debug x64 `/m:1`: PASS
- `SimLab` Debug x64 `/m:1`: PASS
- `Server` Debug x64 `/m:1`: PASS
- `Client` Debug x64 `/m:1`: PASS
- `Winters.sln` Debug x64 `/m:1`: PASS
  - Engine, GameSim, Server, Client, AssetConverter, SimLab, EldenRingClient
- 기존 warning만 관찰: C4251/C4275, 범위 밖 `ChampionSpawnService.cpp:190` C4477

### Job/Fiber

- Chase-Lev last-item 100,000회: owner 59,659 / thief 40,341 / violation 0 / 59.2859 ms
- ThreadOnly fan-out + help 100,000: 243.940 ms / 409,936 jobs/s
- ThreadOnly pure worker 100,000: 227.589 ms / 439,389 jobs/s
- FiberFull fan-out + help 100,000: 197.613 ms / 506,038 jobs/s
- FiberFull pure worker 100,000: 181.521 ms / 550,902 jobs/s
- nested waits/resumes: 16/16
- interleave waits/resumes: 4/4, inline 5
- saturation: parents 80, waits/resumes 64/64, pool misses 17

### Network

- UDP loopback: `codec=1 ordered=1 laneAck=1 reassembly22104=1 live=1`, PASS
- UDP F5 + FiberFull:
  - startup jobs 2/2, switches 6, waits/resumes 1/1
  - Client Hello/LobbyState 1/1, invalid 0, recv/send 5/5, retransmit/drop 0/0
  - Server stale/overflow/outbound reject 0/0/0
- TCP smoke: receive 480, PASS
- TCP+UDP `dual`: 양쪽 PASS
- UDP auth flag absent: fail-closed exit 5, PASS
- FiberShell Server probe: switches 4, PASS

### Payload baseline

- replay snapshots 1,786
- average 15,415.77 B, p50 16,256 B, p95 19,808 B, max 22,104 B
- payload 451.634 KiB/s/client, application UDP wire 474.345 KiB/s/client
- datagrams average 13.843, p95 18, max 20
- IPv4+UDP downstream 485.70 + immediate ACK uplink 27.58 KiB/s/client
- 5 clients 약 2.506 MiB/s
- p95를 4 datagram 이하로 만들려면 약 76.9% payload reduction 필요

### Static/regression

- S022 Python: 71/71 PASS
- Shared boundary/project XML: PASS
- scoped `git diff --check`: PASS

## Handoff Boundary

완료로 주장하는 범위:

- JobSystem Submit/lifetime, Chase-Lev last-item, Fiber scheduler correctness 수직 슬라이스
- Server Fiber lifecycle과 실제 startup wait/resume probe
- UDP v3 feature-gated runtime migration과 TCP/UDP/dual localhost smoke
- shared checkout의 S022 통합과 전체 solution build

완료로 주장하지 않는 범위:

- production UDP 기본 전환 또는 TCP 제거
- post-handshake MAC/AEAD와 production ticket authentication
- RTT estimator, pacing, congestion control, fast retransmit
- IPv6, authenticated NAT rebinding, PMTU, reconnect/resume, graceful close
- AOI/delta/quantization/baseline 기반 Snapshot diet와 WAN chaos/soak
- authoritative GameRoom workload jobification 또는 FiberFull speedup
- Fiber 6주 mastery 프로그램

후속 runtime packet은 mutable GameRoom/live session/socket을 job에 넘기지 말고, tick 경계의 immutable replication DTO encode부터 병렬화해야 한다. UDP production packet은 보안과 혼잡 제어를 Snapshot 최적화보다 먼저 명시적으로 소유해야 한다.

No commit, reset, checkout, destructive cleanup을 수행하지 않았다. 이 packet은 shared dirty `main`의 구현·검증 완료 상태를 다음 integration owner에게 handoff한다.
