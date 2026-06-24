# Session - Client Stale Entity Assertion And Kill Score Dedup

Date: 2026-06-24
Scope: 3-client runtime bug triage, red-team Kindred client ECS assertion, repeated Jax-vs-Ashe kill feed/score inflation

## 1. 반영해야 하는 코드

### 목표

1. Red team Kindred client에서 발생한 `Engine/Public/ECS/Entity.h:106` `assert(IsAlive(id))` 크래시 경로를 끊는다.
2. Jax bot이 Ashe bot을 처치한 뒤 처치 메시지와 화면 킬 스코어가 반복 증가하는 경로를 서버 권위 기준으로 한 번만 처리한다.
3. 클라이언트 HUD는 이벤트 누적값이 아니라 서버 스냅샷으로 복제되는 `MatchScoreComponent`를 우선 표시한다.
4. 수정은 `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual` 흐름을 보존한다.

### 원인 A - stale local EntityID 파괴

증상:
- 런타임 assertion: `CEntityManager::Destroy(EntityID id)` 내부 `assert(IsAlive(id))`.
- 메시지에 표시된 위치는 `Engine/Public/ECS/Entity.h:106`.

판단:
- 엔진 `Destroy`는 죽은 엔티티를 다시 파괴하는 호출을 허용하지 않는다.
- 클라이언트 네트워크 적용부가 `EntityIdMap.FromNet(netId)`로 얻은 local entity를 곧바로 `DestroyEntity`, `GetComponent`, `AddComponent`에 사용하면, snapshot/event 순서 차이로 이미 파괴된 local entity가 다시 들어올 수 있다.
- 3개 클라이언트 환경에서는 projectile hit, effect cue, minion stale cleanup, snapshot ensure 순서가 더 자주 엇갈린다.

반영 파일:
- `Client/Private/Network/Client/EventApplier.cpp`
- `Client/Private/Network/Client/SnapshotApplier.cpp`

반영 방식:
- `ResolveLiveEntity(world, entityMap, netId)`를 통해 `NULL_NET_ENTITY`, 미바인딩, 이미 죽은 local entity를 모두 `NULL_ENTITY`로 정규화한다.
- `DestroyEntityIfAlive(world, entity)`로 client visual/projectile entity 파괴를 생존 확인 뒤 수행한다.
- projectile spawn/hit, action start, effect trigger, damage event의 source/target/attached entity 조회를 live entity 기준으로 통일한다.
- snapshot stale minion cleanup과 `EnsureEntity`에서 dead local mapping을 발견하면 mapping을 해제하고 새 entity 생성 경로로 되돌린다.

### 원인 B - death credit 중복 반영

증상:
- Jax bot이 Ashe bot을 처치한 뒤 kill feed 메시지가 반복 출력된다.
- 화면 팀 킬 스코어가 계속 증가한다.

판단:
- `DamagePipeline`은 이미 죽은 대상 피해를 기본적으로 거부하지만, kill result가 같은 death episode에서 중복 도착하면 `DamageQueueSystem`의 kill reward/feed/score 처리부는 별도 one-shot 가드가 없었다.
- 사망/리스폰 에피소드의 소유자는 서버 GameSim/Server이므로, 중복 방지는 클라이언트 UI가 아니라 서버 사망 상태에 붙어야 한다.

반영 파일:
- `Shared/GameSim/Components/RespawnComponent.h`
- `Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp`
- `Server/Private/Game/GameRoom.cpp`

반영 방식:
- `RespawnComponent`에 `bDeathCredited`를 추가한다.
- `DamageQueueSystem`에서 champion target kill credit을 `TryMarkChampionDeathCredit`으로 한 번만 통과시킨다.
- `GameRoom::Phase_ServerDeathAndRespawn`에서 살아있는 상태 또는 리스폰 완료 시 `bDeathCredited`를 reset한다.
- Champion이 아니거나 respawn component가 없는 비챔피언 처리는 기존 동작을 유지한다.

### 원인 C - HUD가 이벤트 누적값을 우선 표시

증상:
- kill event가 중복 표시될 때 화면 킬 스코어도 함께 증가한다.

판단:
- 서버 권위 점수는 snapshot의 `MatchScoreComponent`에 있다.
- HUD의 `m_GameContextHUD.iBlueKills/iRedKills`는 kill event 기록용 누적값이라, 중복 event 표시 문제와 결합되면 화면 점수가 부풀 수 있다.

반영 파일:
- `Engine/Private/Manager/UI/UI_Manager.cpp`

반영 방식:
- `DrawGameContextHUD`에서 `MatchScoreComponent`가 있으면 blue/red team kill display를 snapshot score로 덮어쓴다.
- local K/D/A도 local champion의 `ChampionScoreComponent`가 있으면 그 값을 우선 표시한다.
- snapshot component가 아직 없을 때만 기존 event 누적값을 fallback으로 사용한다.

## 2. 검증

필수 검증:

1. `git diff --check`
   - 목적: 문서/코드 공백 오류 확인.
2. `msbuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64`
   - 목적: client visual/event/snapshot/HUD 변경 컴파일 및 링크 확인.
3. `msbuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64`
   - 목적: server respawn/death credit 변경 컴파일 및 링크 확인.
4. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug`
   - 목적: definition freshness, ownership audit, visual timing parity, GameSim/Server/Client/SimLab build, deterministic regression, whitespace validation 통합 확인.

수동 검증 권장:

1. 서버 1개와 클라이언트 3개를 실행한다.
2. red-team Kindred client에서 projectile/effect heavy 상황을 재현한다.
3. `Entity.h:106 IsAlive(id)` assertion이 다시 뜨지 않는지 확인한다.
4. Jax bot이 Ashe bot을 처치하는 장면에서 kill feed가 사망 1회당 1회만 뜨는지 확인한다.
5. HUD team kill score가 server snapshot score와 일치하는지 확인한다.

## 3. 완료 기준

- 죽은 local entity는 client event/snapshot path에서 다시 `DestroyEntity`로 들어가지 않는다.
- champion death credit은 respawn episode당 한 번만 score/feed/reward를 발생시킨다.
- HUD score display는 server authoritative replicated score를 우선한다.
- Client/Server Debug x64 build와 LoLDataDriven pipeline이 통과한다.

