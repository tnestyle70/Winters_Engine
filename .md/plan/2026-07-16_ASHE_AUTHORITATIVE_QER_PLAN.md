Session - Ashe Authoritative Q E R Gameplay

## 1. 반영해야 하는 코드

### 서버 권위 경계

입력은 기존 `GameCommand`를 유지하고 Q 다중 화살, E 이동/시야/소멸, R 기절은 `Shared/GameSim`과 Server projectile phase가 소유한다. Client cast hook은 복제 발사체와 중복되는 E를 만들지 않는다.

### 기존 파일: `Shared/GameSim/Components/SkillProjectileComponent.h`

아래 필드를 `direction` 아래에 추가한다.

```cpp
f32_t fSpawnDelaySec = 0.f;
```

### 기존 파일: `Shared/GameSim/Champions/Ashe/AsheGameSim.cpp`

Q active BA는 0.5초 창에 다섯 projectile을 만든다. 첫 발만 gameplay hit를 적용하여 피해가 5배가 되는 회귀를 막는다.

```cpp
constexpr u32_t kQArrowCount = 5u;
constexpr f32_t kQArrowCastWindowSec = 0.5f;
projectile.fSpawnDelaySec = kQArrowCastWindowSec *
    static_cast<f32_t>(arrowIndex) /
    static_cast<f32_t>(kQArrowCount - 1u);
if (arrowIndex > 0u)
{
    projectile.bApplyDamageOnHit = false;
    projectile.bApplyOnHitStatus = false;
    projectile.damage = 0.f;
}
```

E는 `AsheHawkshot` projectile에 sensor와 vision을 붙인다. 충돌, 피해, 장벽 차단은 없고 source 사망 후에도 진행한다.

```cpp
projectile.targetKindMask = ProjectileTarget_None;
projectile.bCollidesWithTerrain = false;
projectile.bBlockedByProjectileBarriers = false;
projectile.bPersistAfterSourceDeath = true;
projectile.bApplyDamageOnHit = false;

SpatialAgentComponent spatial{};
spatial.kind = eSpatialKind::Sensor;
spatial.team = static_cast<u8_t>(ctx.casterTeam);
VisionSourceComponent vision{};
vision.sightRange = sightRange;
vision.bFlying = true;
```

### 기존 파일: `Server/Private/Game/GameRoomProjectiles.cpp`

spawn delay가 끝나기 전에는 NetId와 spawn event를 만들지 않는다.

```cpp
if (!projectile.bSpawned && projectile.fSpawnDelaySec > 0.f)
{
    projectile.fSpawnDelaySec =
        (std::max)(0.f, projectile.fSpawnDelaySec - tc.fDt);
    if (projectile.fSpawnDelaySec > 0.f)
        continue;
}
```

Hawkshot의 다음 위치가 map surface bounds 밖이면 `RangeExpired`로 제거한다.

```cpp
if (projectile.kind == eProjectileKind::AsheHawkshot &&
    m_pMapSurfaceSampler && m_pMapSurfaceSampler->IsReady() &&
    (end.x < m_pMapSurfaceSampler->GetMinX() ||
     end.x > m_pMapSurfaceSampler->GetMaxX() ||
     end.z < m_pMapSurfaceSampler->GetMinZ() ||
     end.z > m_pMapSurfaceSampler->GetMaxZ()))
{
    EnqueueProjectileContact(NULL_ENTITY, start,
        ProjectileContactReason::RangeExpired, true);
    m_world.DestroyEntity(entity);
    continue;
}
```

### 기존 파일: `Client/Private/Network/Client/SnapshotApplier.cpp`

Hawkshot snapshot entity에도 같은 팀 sensor와 snapshot `projectileRadius` 기반 vision을 붙여 클라이언트 FOW 표현이 서버 발사체 위치를 따른다.

### 기존 파일: `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`

정확한 기존 `skill.ashe.q/r` key를 수정하고 `skill.ashe.e` key를 추가한다.

```json
{"key":"skill.ashe.q","params":{"effectDurationSec":5.0}}
{"key":"skill.ashe.e","params":{"radius":10.0,"range":400.0,"speed":24.0}}
{"key":"skill.ashe.r","params":{"stunDurationSec":3.0}}
```

## 2. 검증

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
msbuild Shared/GameSim/Include/GameSim.vcxproj /m /p:Configuration=Debug /p:Platform=x64
msbuild Server/Include/Server.vcxproj /m /p:Configuration=Debug /p:Platform=x64
```

인게임 gate는 Q가 5초 유지되고 한 BA당 5발이 0.5초에 걸쳐 나오되 피해는 1회, E가 이동 경로를 밝히고 맵 밖에서 사라짐, R 적중 대상이 3초 stun인 것이다.
