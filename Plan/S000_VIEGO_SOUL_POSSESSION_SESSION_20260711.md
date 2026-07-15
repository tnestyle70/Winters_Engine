Session - Viego 원혼 생성, 서버 권위 빙의, QWE/기본 공격 탈취, R 복귀를 현재 Winters 코드 기준으로 완성한다.

1. 반영해야 하는 코드

이번 S000은 이후 Viego 작업의 ground-truth 세션이다. 성공 기준은 `Viego kill -> 5초 원혼 -> 우클릭 소비 -> 피해자 외형/BA/QWE -> Viego R 승인 -> 즉시 원형 복귀와 R 실행`이 서버 권위 흐름으로 한 번만 처리되는 것이다.

권위 경계는 다음으로 고정한다.

```text
Client right click / QWER
    -> GameCommand
    -> Server GameSim (kill owner, soul lifetime, possession, skill identity, exit)
    -> Snapshot / ActionStart / Effect event
    -> Client mesh, material, animation, FX, HUD
```

요구 문장의 5초는 원혼의 소비 가능 시간으로 해석한다. 빙의는 별도 5초 타이머와 결합하지 않고 R 사용, Viego 사망, 새 빙의로 종료한다. 이는 "R을 사용하면 Viego 모습으로 돌아오면서 R 사용"이라는 명시 조건을 그대로 보존한다.

1-1. `Shared/GameSim/Components/ViegoSoulComponent.h`

`ViegoSoulComponent`는 팀만 저장하지 않고 실제로 킬을 낸 Viego와 피해자의 QWE 랭크를 박제한다.

기존 구조체를 아래로 교체한다.

```cpp
struct ViegoSoulComponent
{
    EntityID deadChampion = NULL_ENTITY;
    EntityID eligibleViego = NULL_ENTITY;
    eChampion champion = eChampion::END;
    eTeam eligibleTeam = eTeam::TEAM_END;
    u8_t skillRanks[SkillRankComponent::kSlotCount] = {};
    f32_t fRemainingSec = 5.f;
    bool_t bHasSkillRanks = false;
};
```

1-2. `Shared/GameSim/Components/ViegoSimComponent.h`

pending 빙의가 원혼 소멸 뒤에도 대상 랭크를 잃지 않도록 복사하고, 빙의 전 Viego의 QWE rank/runtime을 보관한다.

`ViegoSimComponent`의 possession 필드를 아래 의미로 확장한다.

```cpp
bool_t bPossessionActive = false;
bool_t bPossessionPending = false;
eChampion pendingPossessionChampion = eChampion::END;
EntityID pendingPossessedTarget = NULL_ENTITY;
u8_t pendingSkillRanks[SkillRankComponent::kSlotCount] = {};
bool_t bPendingHasSkillRanks = false;
f32_t possessionApplyTimerSec = 0.f;
f32_t possessionApplyDelaySec = 0.72f;

EntityID possessedTarget = NULL_ENTITY;
eChampion possessionChampion = eChampion::END;

SkillRankComponent originalSkillRanks{};
SkillStateComponent originalSkillState{};
bool_t bHasOriginalSkillRanks = false;
bool_t bHasOriginalSkillState = false;
```

1-3. `Shared/GameSim/Champions/Viego/ViegoGameSim.cpp`

`TrySpawnSoulForKill`은 임의의 아군 킬이 아니라 `killer`가 살아 있는 Viego이고 피해자와 적 팀일 때만 원혼을 만든다. `eligibleViego`를 기록해 같은 팀의 다른 Viego가 소비하지 못하게 한다. 피해자의 QWE 랭크는 원혼 생성 시점에 복사한다.

```cpp
if (killer == NULL_ENTITY ||
    !world.HasComponent<ChampionComponent>(killer) ||
    !world.HasComponent<HealthComponent>(killer))
{
    return;
}

const auto& killerChampion = world.GetComponent<ChampionComponent>(killer);
const auto& killerHealth = world.GetComponent<HealthComponent>(killer);
if (killerChampion.id != eChampion::VIEGO ||
    killerChampion.team == dead.team ||
    killerHealth.bIsDead ||
    killerHealth.fCurrent <= 0.f)
{
    return;
}

ViegoSoulComponent soul{};
soul.deadChampion = deadChampion;
soul.eligibleViego = killer;
soul.champion = dead.id;
soul.eligibleTeam = killerChampion.team;
soul.fRemainingSec = championDef->passiveSoul.lifetimeSec;
```

`ApplyViegoPossession`은 원래 QWE rank/cooldown/stage를 한 번 저장하고, 피해자의 QWE rank를 최소 1로 적용하며 새 폼의 QWE cooldown/stage를 초기화한다. R 슬롯은 건드리지 않는다. `FormOverrideComponent.skillSlotMask`는 BA/Q/W/E 비트만 포함하고 R 비트는 제외한다. 폼은 시간 제한 없이 유지한다.

```cpp
form.baseChampion = eChampion::VIEGO;
form.visualChampion = state.pendingPossessionChampion;
form.skillChampion = state.pendingPossessionChampion;
form.skillSlotMask = static_cast<u8_t>(
    (1u << static_cast<u8_t>(eSkillSlot::BasicAttack)) |
    (1u << static_cast<u8_t>(eSkillSlot::Q)) |
    (1u << static_cast<u8_t>(eSkillSlot::W)) |
    (1u << static_cast<u8_t>(eSkillSlot::E)));
form.fRemainingSec = -1.f;
form.bActive = true;
```

`ClearViegoPossession`은 저장한 QWE rank/runtime을 복구한 뒤 form/pending 상태를 모두 지운다. `OnR`은 이 복구를 먼저 수행하고 기존 Viego R dash/damage를 계속 실행한다. Viego 사망 시에도 동일 helper로 복구한다.

1-4. `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`

원혼 우클릭은 `issuer == soul.eligibleViego`까지 검증한다. 원혼을 제거하기 전에 champion/rank/target을 `ViegoSimComponent` pending 필드로 복사한다.

```cpp
if (issuerChampion.id != eChampion::VIEGO ||
    issuerChampion.team != soul.eligibleTeam ||
    soul.eligibleViego != cmd.issuerEntity)
{
    return true;
}
```

기본 공격은 `ResolveBasicAttack`으로 source champion을 확정하고 그 champion의 attack range, timing, move policy, BA hook/effect identity를 사용한다. 실제 impact까지 `CombatActionComponent.eSourceChampion`에 박제한다.

```cpp
const eChampion baseChampion = ResolveChampion(world, cmd.issuerEntity);
const SkillOverrideResolveResult attackIdentity =
    CSpellbookFormOverrideSystem::ResolveBasicAttack(
        world,
        cmd.issuerEntity,
        baseChampion);
const eChampion champion = attackIdentity.hookChampion;
```

1-5. `Shared/GameSim/Definitions/GameplayDefinitionQuery.h/.cpp`

현재 `FindChampion`/`FindSkill`은 명시된 `fallbackChampion`보다 caster의 `ChampionDefinitionComponent`/`SkillLoadoutComponent`를 먼저 사용한다. 이 때문에 stolen QWE hook 이름만 피해자이고 range/cooldown/effect는 Viego 데이터가 되는 혼합 상태다.

명시 champion이 entity의 base champion과 다르면 명시 identity를 우선하는 분기를 추가한다.

```cpp
const bool_t bExplicitChampionOverride =
    fallbackChampion != eChampion::NONE &&
    fallbackChampion != eChampion::END &&
    fallbackChampion != ResolveEntityChampion(world, entity);

if (!bExplicitChampionOverride &&
    entity != NULL_ENTITY &&
    world.HasComponent<SkillLoadoutComponent>(entity))
{
    if (const SkillGameplayDef* skill =
        pack->FindSkill(world.GetComponent<SkillLoadoutComponent>(entity).skills[slot]))
    {
        return skill;
    }
}
```

같은 규칙을 `FindChampion`과 `ResolveAttackRange`에도 적용해 변신 중 BA range와 stolen QWE 데이터가 같은 source champion을 보게 한다.

1-6. `Shared/GameSim/Systems/SpellbookFormOverride/SpellbookFormOverrideSystem.h/.cpp`

공통 resolver에 BA와 visual identity helper를 추가한다.

```cpp
static SkillOverrideResolveResult ResolveBasicAttack(
    CWorld& world,
    EntityID caster,
    eChampion baseChampion);

static eChampion ResolveVisualChampion(
    CWorld& world,
    EntityID caster,
    eChampion baseChampion);
```

`FormOverrideComponent.fRemainingSec < 0.f`는 명시적 무기한 폼으로 취급하고 timer system이 제거하지 않는다.

1-7. `Shared/GameSim/Components/CombatActionComponent.h`와 `Shared/GameSim/Systems/Combat/CombatActionSystem.cpp`

기본 공격 impact가 폼 만료/해제 뒤에도 시작 당시 source identity를 잃지 않도록 아래 필드를 추가한다.

```cpp
eChampion eSourceChampion = eChampion::NONE;
```

damage passive dispatch, Irelia 예외, BA effect hook은 `ResolveChampion(world, source)`가 아니라 `action.eSourceChampion`을 사용한다. 유효하지 않을 때만 base champion으로 fallback한다.

1-8. `Server/Private/Game/SnapshotBuilder.cpp`, `Client/Private/Network/Client/SnapshotApplier.cpp`, `Client/Private/Scene/Scene_InGameNetwork.cpp`

서버는 기존 `baseChampionId`, `visualChampionId`, `skillChampionId`, `skillSlotMask`, `kSnapshotStateViegoSoulFlag`를 유지한다. 클라이언트는 snapshot 외형이 바뀌면 해당 champion renderer를 attach하되, locomotion/idle/run은 `FormOverride.visualChampion`, action은 ActionStart의 source champion으로 해석한다.

R처럼 ActionStart가 visual swap snapshot보다 먼저 도착하는 경우 새 renderer attach 직후 현재 replicated action을 한 번 재생해 Viego R animation을 복구한다.

1-9. `Engine/Private/Renderer/ModelRenderer.cpp`, `Shaders/Mesh3D.hlsl`, `Shaders/Skinned3D.hlsl`

현재 alpha는 override enable flag일 뿐 출력 alpha에 반영되지 않는다. override alpha를 실제 pixel alpha로 출력하고, alpha가 1보다 작을 때 DX11 alpha blend + depth read-only state를 draw 범위에만 적용하고 이전 state를 복구한다.

shader 분기를 아래로 교체한다.

```hlsl
if (g_vMaterialOverrideColor.a > 0.001f)
{
    return float4(
        saturate(g_vMaterialOverrideColor.rgb),
        texColor.a * saturate(g_vMaterialOverrideColor.a));
}
```

원혼 본체는 기존 초록 tint `Vec4{ 0.20f, 1.05f, 0.72f, 0.80f }`를 유지하되 이제 실제 80% alpha blend가 된다.

1-10. `Client/Private/GameObject/Champion/Viego/Viego_FxPresets.cpp`와 stale cleanup

`SpawnSoulIdle`은 `PlayAll` 결과 entity를 owner별로 저장한다. 원혼이 5초 전에 소비되면 stale entity cleanup callback이 `StopSoulIdle`을 호출해 남은 안개/오라를 즉시 제거한다.

```cpp
void StopSoulIdle(CWorld& world, EntityID owner);
```

2. 검증

정적/데이터 검증:

```text
python Tools/LoLData/Build-LoLDefinitionPack.py --check
Shared/Schemas/run_codegen.bat
git diff --check
```

서버 결정론 smoke에 다음 케이스를 추가하거나 기존 GameSim smoke에서 동일 상태를 직접 검증한다.

```text
1. Viego가 아닌 아군의 kill -> soul 0개
2. Viego kill -> 피해자 위치 soul 1개, lifetime 5초, eligibleViego == killer
3. 다른 Viego consume -> 거절, owner Viego consume -> pending 후 form 활성
4. form mask == BA|Q|W|E, R bit == 0
5. stolen QWE FindSkill/range/cooldown/effect identity == 피해자 champion
6. BA CombatAction.eSourceChampion == 피해자 champion
7. R cast hookChampion == VIEGO, form 제거, Viego R dash 생성
8. R 뒤 Viego QWE rank/runtime 원상 복구
```

빌드:

```text
MSBuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
MSBuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

2대 PC runtime 확인:

```text
서버 PC에서 kill -> 두 클라이언트 모두 동일 위치에 피해자 mesh 원혼 1개
원혼 mesh는 초록 tint + 실제 반투명, 5초 만료 또는 소비 즉시 mesh/FX 동시 제거
소비 후 두 클라이언트 모두 동일 champion 외형과 BA/QWE animation
R 입력 승인 후 Viego 외형으로 복귀하면서 Viego R animation/effect/dash 실행
```
