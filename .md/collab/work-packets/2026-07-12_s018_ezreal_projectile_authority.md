# S018 Ezreal Projectile Authority Work Packet

ID: `2026-07-12_s018_ezreal_projectile_authority`

상태: `Handoff`

Agent: `Codex`

Owner: `Desktop`

Branch: `main`

Base: 현재 dirty worktree의 AI Handoff 변경을 보존한 상태

Owned paths:

- `Shared/GameSim/Champions/Ezreal/**`
- 투사체·버프·불연속 이동에 직접 필요한 `Shared/GameSim/Components/**`, `Systems/**`, keyframe registry
- `Server/Private/Game/GameRoomProjectiles.cpp`
- `Server/Private/Game/ServerProjectileAuthority.cpp`
- Debug 통합 probe seam인 `Server/Public/Game/GameRoom.h`, `Server/Public/Game/LobbyAuthority.h`, `Shared/GameSim/Systems/Turret/TurretAISystem.h`
- projectile spawn/replication/snapshot에 직접 필요한 `Server/Private/Game/GameRoom*.cpp`
- `Shared/Schemas/Event.fbs`, `Shared/Schemas/Snapshot.fbs`와 generated C++/Go
- 이즈리얼 gameplay/visual canonical data와 generated definition pack
- Client projectile event/snapshot presentation과 Ezreal/Yasuo 관련 visual routing
- `Tools/SimLab/main.cpp`, S018 contract harnesses
- `Tools/Harness/GameRoomProjectileIntegrationProbe.cpp`, `Tools/Harness/RunGameRoomProjectileIntegrationProbe.ps1`

Validation:

- Shared/GameSim, SimLab, Server, Client Debug x64 build: PASS
- Ezreal BA/Q/W/E/R와 Rising Spell Force SimLab: PASS
- 실제 GameRoom Ezreal BA·포탑 projectile lifecycle, skill/structure target generation 재사용, serializer delayed-unbind, passive pre-command expiry: PASS
- moving-target CCD, skill/structure generation lifetime, barrier, typed contact: PASS
- append-only wire field ID와 current-schema omitted-tail defaults: PASS
- production presentation mutation ordering comparator: PASS
- Shared boundary와 definition/hash checks: PASS

Report: `.md/build/2026-07-13_S018_EZREAL_PROJECTILE_AUTHORITY_REPORT.md`

Handoff notes:

- 기존 uncommitted 변경을 reset/revert하지 않는다.
- S018 코드는 구현·빌드·계약 검증 완료 상태다.
- 실제 다중 클라이언트 packet-delivery/렌더 스모크와 historical old-schema byte fixture, 승인 FX 에셋, designer projectile profile, attack-proc receipt, keyframe v1->v2 migration은 보고서의 후속 경계로 남긴다.
