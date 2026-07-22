# Session - Champion Projectile / Ashe Gameplay / Structure Feed / Shop Designer Implementation Report

Date: 2026-07-16

## 1. 결론과 검증 상태

요청 범위의 코드, 데이터, WFX, UI 편집 경로를 실제 런타임 소유 경로에 반영했다.

- 이즈리얼 Q: `ba_cast.wfx`의 whisp 리소스를 사용하는 폭 `0.5`, 높이 `2.06`의 레이어 3개로 교체했다.
- 이즈리얼 R: 복제 projectile의 최종 yaw offset을 `PI`로 교정했다.
- 애쉬 BA/W/E/R: raw arrow FBX 축을 기준으로 최종 yaw offset과 WFX local rotation을 모두 `-PI/2`로 정렬했다.
- 애쉬 크기: BA/W `0.0105`, E `0.021`(BA 2배), R `0.0315`(BA 3배)다.
- 애쉬 Q: 5초 동안 BA 1회가 0.5초 구간에 5개 화살을 순차 발사한다. 피해와 on-hit은 첫 화살만 적용한다.
- 애쉬 E: 서버 권위 projectile, 이동 시야, 글로벌 사거리, 맵 경계 이탈 제거를 구현했다.
- 애쉬 R: 데이터 정의의 기절을 3초로 변경했다.
- 피오라: BA/Q/W/E/R용 WFX 6개를 만들고 기존 직접 billboard 코드 대신 `CFxCuePlayer::PlayAll` 경로에 연결했다.
- 구조물 feed: 파괴 대상 팀에 따라 blue/red tower 또는 inhibitor PNG를 출력한다.
- 상점 도구: Items 폴더의 PNG 387개를 스캔하고 Lua의 order/x/y/section/hidden을 즉시 reload할 수 있게 했다.

Engine, GameSim, Server, Client Debug x64 빌드는 성공했다. 서버와 클라이언트는 실행하지 않았으므로 아래 시각/플레이 검증 항목은 사용자의 인게임 gate로 남긴다. 이 보고서에서 빌드 성공을 시각 검증 성공으로 간주하지 않는다.

## 2. 반복 yaw 실패의 본질 원인

복제 projectile은 WFX가 만든 초기 transform을 그대로 사용하지 않는다. `Client/Private/Network/Client/EventApplier.cpp`가 매 snapshot/event 적용 때 아래 식으로 최종 transform을 갱신한다.

```cpp
const ProjectileVisualDesc& visual =
    ProjectileVisualCatalog::Resolve(uProjectileKind);
const f32_t yaw =
    WintersMath::YawFromDirectionXZ(direction) + visual.fYawOffset;
```

따라서 WFX의 `rotation` 또는 champion preset의 yaw만 바꾸는 수정은 복제 경로에서 덮어써진다. 이전의 `yaw = 0` 결론이 네 번 체감상 반영되지 않은 직접 원인이다.

정상 동작하던 칼리스타도 같은 최종 catalog offset을 사용한다.

```cpp
constexpr ProjectileVisualDesc kKalistaBasicAttackVisual{
    "Kalista.BA.Projectile", nullptr, "Kalista.Rend.Spear",
    nullptr, nullptr, nullptr, WintersMath::kPi, true
};
```

동일한 소유 경로에서 실제 asset 축을 보정했다.

```cpp
constexpr ProjectileVisualDesc kEzrealGlobalBeamVisual{
    "Ezreal.R.Missile", "Ezreal.R.Hit", nullptr,
    nullptr, nullptr, nullptr, WintersMath::kPi
};

constexpr ProjectileVisualDesc kAsheBasicAttackVisual{
    "Ashe.BA.Arrow", nullptr, "Ashe.BA.Hit",
    nullptr, nullptr, nullptr, -WintersMath::kPi * 0.5f
};

constexpr ProjectileVisualDesc kAsheVolleyArrowVisual{
    "Ashe.W.Arrow", "Ashe.W.Hit", nullptr,
    nullptr, nullptr, nullptr, -WintersMath::kPi * 0.5f
};

constexpr ProjectileVisualDesc kAsheHawkshotVisual{
    "Ashe.E.Hawkshot", nullptr, nullptr,
    nullptr, nullptr, nullptr, -WintersMath::kPi * 0.5f
};

constexpr ProjectileVisualDesc kAsheCrystalArrowVisual{
    "Ashe.R.Arrow", "Ashe.R.Hit", nullptr,
    nullptr, nullptr, nullptr, -WintersMath::kPi * 0.5f
};
```

WFX의 `rotation: [0, -1.5708, 0]`도 local/non-replicated 재생과 복제 재생이 서로 다른 방향을 보이지 않도록 같은 축으로 맞췄다. 최종 판정의 소유자는 WFX가 아니라 `ProjectileVisualCatalog::fYawOffset`이다.

재발 방지 규칙은 `.claude/gotchas.md`에 기록했고, 이전 `yaw = 0` 성공 보고서는 `FAILED / SUPERSEDED`로 명시했다.

## 3. 이즈리얼 WFX

### 3.1 Q

`Data/LoL/FX/Champions/Ezreal/q_projectile.wfx`는 정확히 3개 emitter를 가진다.

```json
{
  "name": "Ezreal.Q.Projectile",
  "emitters": [
    {
      "name": "q_ba_whisp_left",
      "texture": "Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_ba_whisps.png",
      "start_delay": 0.0,
      "width": 0.5,
      "height": 2.06,
      "attach_offset": [-0.1, 1.08, 0.02]
    },
    {
      "name": "q_ba_whisp_center",
      "texture": "Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_ba_whisps.png",
      "start_delay": 0.025,
      "width": 0.5,
      "height": 2.06,
      "attach_offset": [0.0, 1.08, 0.1]
    },
    {
      "name": "q_ba_whisp_right",
      "texture": "Client/Bin/Resource/Texture/Character/Ezreal/particles/ezreal_base_ba_whisps.png",
      "start_delay": 0.05,
      "width": 0.5,
      "height": 2.06,
      "attach_offset": [0.1, 1.08, 0.02]
    }
  ]
}
```

레이어마다 작은 위치/시간 차이를 주어 한 장을 단순 복제한 평면처럼 겹치는 현상을 줄였다.

### 3.2 R

`GlobalBeam`의 최종 catalog offset을 `PI`로 변경했다. WFX만 수정하는 우회 경로를 사용하지 않았다.

## 4. 애쉬 Q/E/R 서버 권위 구현

### 4.1 Q - 5초 buff, 0.5초 동안 5발

`AsheSimComponent::qDurationSec`와 정의 데이터의 `effectDurationSec`는 5초다. BA 생성은 아래처럼 5개 entity를 만들며 0.5초 구간에 delay를 분배한다.

```cpp
constexpr u32_t kQArrowCount = 5u;
constexpr f32_t kQArrowCastWindowSec = 0.5f;
const u32_t arrowCount = state.bQActive ? kQArrowCount : 1u;

for (u32_t arrowIndex = 0u; arrowIndex < arrowCount; ++arrowIndex)
{
    SkillProjectileComponent projectile = baseProjectile;
    projectile.fSpawnDelaySec =
        kQArrowCastWindowSec * static_cast<f32_t>(arrowIndex) /
        static_cast<f32_t>(kQArrowCount - 1u);

    if (arrowIndex > 0u)
    {
        projectile.bApplyDamageOnHit = false;
        projectile.bApplyOnHitStatus = false;
        projectile.damage = 0.f;
    }
    // Add SkillProjectileComponent + TransformComponent.
}
```

서버 projectile tick은 delay가 끝나기 전에는 NetId와 spawn event를 만들지 않는다.

```cpp
if (!projectile.bSpawned && projectile.fSpawnDelaySec > 0.f)
{
    projectile.fSpawnDelaySec =
        (std::max)(0.f, projectile.fSpawnDelaySec - tc.fDt);
    if (projectile.fSpawnDelaySec > 0.f)
        continue;
}
```

중요한 회귀 방지 결정: 5개 모두 피해를 적용하면 Q가 단순 시각 개선이 아니라 기본 공격 5배 피해가 된다. 따라서 첫 화살만 authoritative damage/on-hit을 적용하고 나머지는 서버가 순서를 보장하는 복제 시각 projectile로 유지했다.

### 4.2 E - 이동 시야와 맵 경계 제거

새 projectile kind `AsheHawkshot`을 추가했다. 충돌/피해/장벽 차단을 끄고 서버 entity에 sensor와 vision을 붙인다.

```cpp
projectile.targetKindMask = ProjectileTarget_None;
projectile.bCollidesWithTerrain = false;
projectile.bBlockedByProjectileBarriers = false;
projectile.bPersistAfterSourceDeath = true;
projectile.bApplyDamageOnHit = false;
projectile.bApplyOnHitStatus = false;

SpatialAgentComponent spatial{};
spatial.kind = eSpatialKind::Sensor;
spatial.team = static_cast<u8_t>(ctx.casterTeam);

VisionSourceComponent vision{};
vision.sightRange = sightRange;
vision.bFlying = true;
```

client snapshot mirror도 `projectileRadius()`를 시야 반경으로 사용해 해당 projectile에 `VisionSourceComponent`를 구성한다. 서버 map sampler 경계를 다음 위치가 벗어나면 `RangeExpired` contact를 만들고 entity를 제거한다.

```cpp
if (projectile.kind == eProjectileKind::AsheHawkshot &&
    m_pMapSurfaceSampler && m_pMapSurfaceSampler->IsReady() &&
    (end.x < m_pMapSurfaceSampler->GetMinX() ||
     end.x > m_pMapSurfaceSampler->GetMaxX() ||
     end.z < m_pMapSurfaceSampler->GetMinZ() ||
     end.z > m_pMapSurfaceSampler->GetMaxZ()))
{
    EnqueueProjectileContact(
        NULL_ENTITY, start, ProjectileContactReason::RangeExpired, true);
    m_world.DestroyEntity(entity);
    continue;
}
```

스킬 데이터는 direction target, range `400`, speed `24`, sight radius `10`이다. map sampler가 준비되지 않은 예외 경로에서는 max range가 lifetime 상한 역할을 한다.

### 4.3 R

`skill.ashe.r.stunDurationSec`를 `3.0`으로 변경했다. R 크기는 BA 축소 기준 3배다.

## 5. 애쉬 WFX 크기 계약

| Cue | Mesh scale | BA 기준 | 최종 yaw |
|---|---:|---:|---:|
| `Ashe.BA.Arrow` | `0.0105` | 1x | `-PI/2` |
| `Ashe.W.Arrow` | `0.0105` | 1x | `-PI/2` |
| `Ashe.E.Hawkshot` | `0.0210` | 2x | `-PI/2` |
| `Ashe.R.Arrow` | `0.0315` | 3x | `-PI/2` |

이 값은 WFX MeshParticle scale과 replicated projectile catalog offset에 모두 반영되어 있다.

## 6. 피오라 WFX 실적용

추가된 cue는 다음과 같다.

| 파일 | cue |
|---|---|
| `ba_hit.wfx` | `Fiora.BA.Hit` |
| `q_cast.wfx` | `Fiora.Q.Cast` |
| `w_cast.wfx` | `Fiora.W.Cast` |
| `e_buff.wfx` | `Fiora.E.Buff` |
| `r_mark.wfx` | `Fiora.R.Mark` |
| `r_heal.wfx` | `Fiora.R.Heal` |

기존의 코드 직접 billboard 생성은 제거하고 WFX registry/player 경로를 사용한다.

```cpp
void PlayAttached(CWorld& world, const char* cueName, EntityID owner,
    const Vec3& direction, f32_t lifetime)
{
    FxCueContext cue{};
    cue.attachTo = owner;
    cue.vWorldPos = ResolvePosition(world, owner);
    cue.vForward = ResolveForward(direction);
    cue.bOverrideLifetime = lifetime > 0.f;
    cue.fLifetimeOverride = lifetime;
    CFxCuePlayer::PlayAll(world, cueName, cue, nullptr);
}
```

WFX registry가 챔피언 디렉터리를 재귀 로드하므로 별도의 hardcoded cue registration을 만들지 않았다.

## 7. 구조물 kill feed

구조물 파괴 event에서 `targetTeam`을 `GameInstance -> UI_Manager`로 전달하고 banner에 보존한다. 렌더 시 object kind와 target team을 함께 사용한다.

```cpp
if (void* pObjectIcon = FindOrLoadKillFeedObjectIcon(
        banner.iObjectKind,
        banner.iTargetTeam))
{
    DrawKillFeedCircleImage(/* ... */);
}
```

매핑은 아래와 같다.

| object kind | target team | texture |
|---|---|---|
| structure | blue | `minimap_tower_blue.png` |
| structure | red | `minimap_tower_red.png` |
| objective/inhibitor | blue | `minimap_inhibitor_blue.png` |
| objective/inhibitor | red | `minimap_inhibitor_red.png` |

SRV는 최초 사용 시 lazy load하고 `Shutdown`에서 release한다. texture load 실패 시 기존 텍스트 fallback을 유지한다.

## 8. Items 폴더 + Lua 상점 디자이너

### 8.1 폴더 스캔과 구매 권위 분리

`LoLUIContentRegistry`는 runtime 기준 `Resource/Texture/UI/Items`, repository 기준 `Client/Bin/Resource/Texture/UI/Items`를 순서대로 검사한다. PNG filename의 leading number를 item id로 읽고, 기존 seed 순서를 먼저 보존한 뒤 나머지를 section/id/filename으로 정렬한다.

```cpp
for (std::filesystem::directory_iterator it(itemDirectory, iterateError), end;
     !iterateError && it != end;
     it.increment(iterateError))
{
    if (extension != ".png")
        continue;
    discovered.push_back(MakeRuntimeShopEntry(
        ParseItemId(assetKey), assetKey, true));
}
```

PNG가 존재한다고 구매 가능한 아이템이 되지는 않는다.

```cpp
const ItemDef* pItem = CItemRegistry::Instance().Find(Entry.iItemId);
Desc.iPrice = pItem ? pItem->price : 0u;
Desc.bPurchasable = pItem != nullptr && Entry.iItemId != 0u;
```

즉 가격/효과/구매 판정의 authoritative owner는 계속 ItemDef/서버이고, 폴더에만 있는 0원 resource entry는 미리보기만 가능하다.

### 8.2 Lua 배치

`Client/Bin/Resource/UI/Lua/itemshop_catalog.lua`의 designer-owned 표를 직접 편집한다.

```lua
WintersItemShopLayoutOverrides = {
    -- ["1055_marksman_t1_doransblade.png"] = {
    --     order = 1, x = 172, y = 201,
    --     section = "starter", hidden = false,
    -- },
}
```

지원 field는 `order`, `x`, `y`, `section`, `hidden`이다. 명시되지 않은 PNG는 자동 grid 배치된다. `ChampionTuner > Shop Layout`에서 다음 작업을 제공한다.

- filename/display-name 필터
- PNG 폴더 재스캔
- Lua layout reload
- 위/아래 순서 이동과 enable toggle
- server 등록 아이템과 resource-only 아이템 표시
- 현재 가격/section 표시와 Item Balance 이동
- 현재 layout 재적용

가격 조정 자체는 기존 Item Balance/ItemDef 데이터 경로를 유지한다. Lua가 서버 가격을 덮어쓰지 않게 하여 클라이언트 표시와 실제 구매 가격 불일치를 막았다.

## 9. 검증 결과

완료된 정적/빌드 검증:

- 신규/수정 WFX 11개 JSON parse: PASS
- Items PNG 개수: `387`
- 4개 structure feed PNG 존재: PASS
- LoL definition generator/check: PASS
- 최종 gameplay definition build hash: `0x4C664D66`
- Engine Debug x64: PASS
- Engine SDK `UpdateLib.bat`: PASS
- GameSim Debug x64: PASS
- SharedBoundary: PASS
- Server Debug x64: PASS
- Client Debug x64: PASS
- `git diff --check`: whitespace error 없음; 기존 CRLF 변환 경고만 존재

Client 첫 빌드에서 `SnapshotApplier.cpp`의 `eProjectileKind` include 누락을 검출했고 이를 추가한 뒤 Client를 다시 빌드해 통과했다. 최종 데이터 재생성 뒤 Server와 Client도 다시 빌드했다.

## 10. 회귀 위험과 인게임 검증 gate

### 높은 우선순위

1. 애쉬 BA/W/E/R 및 이즈리얼 R을 각기 동/서/남/북으로 발사해 진행 방향과 mesh nose가 일치하는지 확인한다.
2. 애쉬 Q 5발이 0.5초 내 순차로 보이되, 대상 체력은 일반 BA 1회분만 감소하는지 확인한다.
3. E가 지나간 암흑 시야를 계속 밝히고 map 밖에서 사라지는지 확인한다.
4. R 적중 시 정확히 3초간 행동 불가이며 크기가 축소 BA의 3배인지 확인한다.

### 중간 우선순위

5. 이즈리얼 Q 3개 whisp가 폭 0.5의 하나의 응집된 투사체처럼 보이는지 확인한다.
6. 피오라 BA/Q/W/E/R cue가 중복 재생 없이 실제 WFX로 나타나는지 확인한다.
7. blue/red tower와 inhibitor를 각각 파괴해 대상 팀/종류별 icon이 올바른지 확인한다.
8. Shop Layout에서 387개 목록, resource-only 구매 불가, Lua reload 후 위치 변경을 확인한다.

### 알려진 fallback

- E map exit 즉시 제거는 map surface sampler가 준비된 정상 서버 경로의 동작이다. sampler가 준비되지 않으면 정의된 max range로 제거된다.
- structure icon texture load가 실패하면 Tower/Inhibitor text fallback을 사용한다.
- 폴더에 PNG만 추가한 item은 ItemDef가 생길 때까지 구매할 수 없다.

이 gate를 통과하기 전 상태 표기는 `코드/데이터 반영 및 빌드 완료, 인게임 시각/플레이 검증 대기`다.
