# Minion Collider Application Plan

작성일: 2026-05-07

## 목표

미니언 겹침 완화의 기준을 `ColliderComponent`로 옮긴다. 기존 `SpatialAgentComponent`는 시야, 타겟팅, spatial query용 반경으로 유지하고, 실제 유닛끼리 겹쳤을 때 위치를 밀어내는 처리는 collider가 붙은 엔티티만 대상으로 한다.

## 현재 판단

- 미니언에는 `SpatialAgentComponent`만 있고 `ColliderComponent`가 없어 DebugDraw collider도 나오지 않는다.
- 기존 `CMinionSeparationSystem`은 속도 방향을 살짝 비트는 steering이다. 공격/추격 중 겹침을 완화할 수는 있지만 “몸이 있는 유닛” 처리는 아니다.
- 스폰 위치와 라인 이동은 의도대로 동작하고 있었으므로, spawn fan-out은 제거하고 collider push-out으로 해결한다.
- 스케줄러 안에서 collision을 돌리면 `CMinion_Manager::Tick`보다 먼저 실행되어 한 프레임 늦게 보정된다. 따라서 인게임 틱 끝, 미니언 이동 후에 resolver를 한 번 더 실행한다.

## 적용 범위

1. `ColliderComponent` 부착
   - Champion: dynamic solid
   - Minion: dynamic solid
   - Turret / Inhibitor / Nexus: static solid
   - Jungle monster: static solid
   - Turret projectile: trigger collider, non-blocking

2. `CGameplayCollisionSystem` 추가
   - `TransformComponent + ColliderComponent + SpatialAgentComponent`를 가진 엔티티를 snapshot.
   - XZ 평면 원형 반경으로 겹침 계산. 반경은 `max(SpatialAgent.radius, Collider.halfExtents.x/z)` 사용.
   - `bIsTrigger == true`는 위치 보정에서 제외.
   - dynamic vs dynamic은 양쪽 절반씩 밀고, dynamic vs static은 dynamic만 민다.
   - 1차 구현은 O(N^2). 현재 미니언 수에서는 충분하며, 이후 `CSpatialIndex` broadphase로 교체 가능.

3. 디버그/튜닝
   - Render Debug의 기존 `ECS Colliders`가 바로 표시할 수 있게 collider 데이터를 채운다.
   - ImGui에 Gameplay Collision 튜너를 추가한다.
   - 기존 Minion Separation은 보조 steering으로 남기되 기본값은 꺼서 collider가 주 동작이 되게 한다.

## 완료 기준

- 미니언, 챔피언, 구조물, 정글몹, 포탑 투사체에 collider가 붙는다.
- 미니언이 공격/추격 중 같은 지점에 포개져도 틱 끝에서 분리된다.
- 스폰 좌표와 라인 이동 로직은 변경하지 않는다.
- Engine / Client Debug x64 빌드가 통과한다.

## 적용 결과

- `CGameplayCollisionSystem`을 추가했다.
- `Scene_InGame`이 시스템을 직접 소유하고 `CMinion_Manager::Tick` 직후 실행한다.
- 미니언, 챔피언, 구조물, 정글몹은 solid collider를 가진다.
- 포탑 투사체는 trigger collider를 가진다.
- 정글몹은 포탑 spatial target 후보가 되지 않도록 `SpatialAgentComponent` 없이 collider만 부착한다.
- `CMinionSeparationSystem`은 보조 steering으로 남기되 기본값을 off로 변경했다.
- Engine Debug x64 빌드는 통과했다.
- Client Debug x64는 컴파일/링크 로그와 `WintersGame.exe` 생성까지 확인했다. 단, 기존 post-build의 `pwsh.exe` 미설치 메시지 때문에 MSBuild 종료 코드는 1로 반환된다.
