Session - 17개 챔피언 85개 공격/스킬 수치의 JSON→생성 팩→서버 DamageRequest→최종 피해 경로를 전수 검증하고 조용히 무시되는 계수와 낡은 테스트를 닫는다.
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성 · C8 검증이 병목
관련: 2026-07-16_GAMEPLAY_FORMULA_SKILL_ANIMATION_DATA_DRIVEN_REMEDIATION_PLAN.md · 2026-07-18_F4_BALANCE_TUNER_DAMAGE_RESPAWN_HUD_PLAN.md

## 1. 결정 기록

① 문제·제약: 17 champions/85 skills의 피해가 `params.*`와 `damage.*ByRank`에 중복되고 최종 queue는 후자로 다시 덮는다. 현 JSON은 generated pack보다 새로워 `STALE`이며, 저장돼도 계수가 체감되지 않는다.
② 순진한 해법의 실패: Ezreal Q 기대값을 사용자가 조절한 200/2.0으로 다시 하드코딩하면 다음 튜닝 때 또 깨지고, `Save & Hot Load`가 거절된 매치는 서버 revision이 바뀌지 않아 파일 저장만으로 해결되지 않는다.
③ 메커니즘: 일반 스킬은 여섯 `damage.*ByRank` 배열을 canonical로 두고, custom-flat 4종만 요청 flat을 보존한다. direct pack oracle→Build→DamageQueue를 모든 rank에서 분리 검증하고 F4 ratio/Kindred W rank 누락을 수술한다.
④ 대조: params와 formula schema를 이번에 통합하는 대안은 기존 커스텀 피해 구현 4종과 F4 계약을 크게 흔든다. 이 세션은 서버 권위 경로를 고정하고 중복 schema 제거는 별도 migration으로 남긴다.
⑤ 대가: 두 피해 표현은 당분간 공존하고 미구현 damage request는 데이터만으로 활성화되지 않는다. 이 선택은 params를 canonical로 전환하거나 모든 커스텀 flat을 formula로 이관하는 시점에 틀려진다.

범위: 챔피언 수치 경로와 검증만 수정한다. 포탑/nav, Dragon/Baron, FX, minimap, HealthBar/ImGui 배치는 다른 세션 소유로 두며 해당 파일을 건드리지 않는다.

## 예산

- 바닥 70%: 데이터 소유권/예외 전수검증, 실제 최종 피해 행렬, 낡은 Ezreal 기대값 제거, Kindred rank와 live ratio override 수정, GameSim/Server/SimLab 빌드.
- 천장 30%: 85슬롯 결과를 RESULT의 소유권 표로 환전하고 이후 수치 조절 시 재사용할 명령과 실패 판독법을 남긴다.

## 현재 코드 증거와 경계

- `DamageQueueSystem.cpp::ApplyDataDrivenSkillFormula`는 일반 스킬 요청 전체를 `BuildSkillDamageRequest` 결과로 교체한다. `params` 기반 projectile/request 값은 최종 truth가 아니다.
- `GameplayDefinitionQuery.cpp::BuildSkillDamageRequest`는 formula flat만 `DamageFlatOverride`로 덮고 AD/AP/HP practice override는 읽지 않아, F4 override table에서 ratio를 보내도 최종 formula에 반영되지 않는다.
- `KindredGameSim.cpp` W tick은 `rank=1`을 고정하고 `iSourceSlot/eSourceKind`를 기록하지 않는다. rank별 JSON을 조절해도 W는 1랭크만 사용한다.
- `Tools/SimLab/main.cpp::RunGameplayFormulaDataDrivenProbe`는 데이터 조절과 무관해야 할 계약 테스트에 Ezreal Q 70/1.3/0.4를 박아 두었다.
- 2026-07-19 재검사에서 definition pack과 champion game data `--check`가 모두 `STALE`이다. 최신 JSON은 generated source와 실행 바이너리에 아직 승격되지 않았다.
- 서버 권위 흐름은 `JSON/generated definition -> GameSim request -> DamageQueue -> snapshot/event`로 유지한다. Client/UI는 수정하지 않는다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/GameplayDefinitionQuery.cpp

practice component만 조회하는 helper를 추가한다. 이 helper는 authored `params` fallback을 읽지 않으므로 canonical formula 배열을 가리지 않는다.

기존 코드:

```cpp
u8_t ResolveSkillRankForScaling(CWorld& world, EntityID entity, u8_t slot)
{
    if (entity != NULL_ENTITY &&
        slot < SkillRankComponent::kSlotCount &&
        world.HasComponent<SkillRankComponent>(entity))
    {
        const u8_t rank =
            world.GetComponent<SkillRankComponent>(entity).ranks[slot];
        if (rank > 0u)
            return rank;
    }
    return 1u;
}
```

아래에 추가:

```cpp
bool_t TryResolvePracticeSkillEffectOverride(
    CWorld& world,
    EntityID entity,
    u8_t slot,
    eSkillEffectParamId param,
    f32_t& outValue)
{
    if (entity == NULL_ENTITY ||
        !world.HasComponent<PracticeSkillEffectOverrideComponent>(entity))
    {
        return false;
    }

    const auto& overrides =
        world.GetComponent<PracticeSkillEffectOverrideComponent>(entity);
    const u8_t count = (std::min)(
        overrides.count,
        PracticeSkillEffectOverrideComponent::kMaxEntries);
    for (u8_t index = 0u; index < count; ++index)
    {
        const auto& entry = overrides.entries[index];
        if (entry.slot == slot && entry.paramId == static_cast<u8_t>(param))
        {
            outValue = entry.value;
            return true;
        }
    }
    return false;
}
```

`ResolveSkillEffectParam`의 중복 override 순회를 위 helper 호출로 교체한다.

기존 블록:

```cpp
if (entity != NULL_ENTITY &&
    world.HasComponent<PracticeSkillEffectOverrideComponent>(entity))
{
    const auto& overrides =
        world.GetComponent<PracticeSkillEffectOverrideComponent>(entity);
    const u8_t count = (std::min)(
        overrides.count,
        PracticeSkillEffectOverrideComponent::kMaxEntries);
    for (u8_t index = 0u; index < count; ++index)
    {
        const auto& entry = overrides.entries[index];
        if (entry.slot == slot &&
            entry.paramId == static_cast<u8_t>(param))
        {
            return entry.value;
        }
    }
}
```

아래로 교체:

```cpp
f32_t overrideValue = 0.f;
if (TryResolvePracticeSkillEffectOverride(
        world, entity, slot, param, overrideValue))
{
    return overrideValue;
}
```

`BuildSkillDamageRequest`에서 아래의 정확한 기존 anchor:

```cpp
outRequest.targetMissingHpRatioOverride = ResolveDamageFormulaRankedValue(
    formula, formula.targetMissingHpRatioByRank, rank);
outRequest.skillId = static_cast<u16_t>(
```

를 다음 완전 블록으로 교체해 canonical rank 배열 직후 practice-only ratio override를 적용한다. `DamageFlatOverride`의 -1 sentinel 계약은 유지한다.

```cpp
outRequest.targetMissingHpRatioOverride = ResolveDamageFormulaRankedValue(
    formula, formula.targetMissingHpRatioByRank, rank);
f32_t formulaOverride = 0.f;
if (TryResolvePracticeSkillEffectOverride(
        world, source, slot, eSkillEffectParamId::TotalAdRatio, formulaOverride))
    outRequest.adRatioOverride = formulaOverride;
if (TryResolvePracticeSkillEffectOverride(
        world, source, slot, eSkillEffectParamId::BonusAdRatio, formulaOverride))
    outRequest.bonusAdRatioOverride = formulaOverride;
if (TryResolvePracticeSkillEffectOverride(
        world, source, slot, eSkillEffectParamId::ApRatio, formulaOverride))
    outRequest.apRatioOverride = formulaOverride;
if (TryResolvePracticeSkillEffectOverride(
        world, source, slot, eSkillEffectParamId::TargetMaxHpRatio, formulaOverride))
    outRequest.targetMaxHpRatioOverride = formulaOverride;
if (TryResolvePracticeSkillEffectOverride(
        world, source, slot,
        eSkillEffectParamId::MissingHealthDamageRatio, formulaOverride))
    outRequest.targetMissingHpRatioOverride = formulaOverride;
outRequest.skillId = static_cast<u16_t>(
```

### 2-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/KindredSimComponent.h

아래 기존 2줄을 완전히 교체해 W 시전 시점 rank를 저장한다. 진행 중 레벨업이 이미 발동한 W의 피해 rank를 바꾸지 못하게 한다.

기존 블록:

```cpp
f32_t fWTickAccumulatorSec = 0.f;
Vec3 vWCenter{};
```

아래로 교체:

```cpp
f32_t fWTickAccumulatorSec = 0.f;
u8_t uWCastRank = 1u;
Vec3 vWCenter{};
```

### 2-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Kindred/KindredGameSim.cpp

`EnqueuePhysicalDamage`의 request 작성 블록을 아래로 교체한다.

기존 블록:

```cpp
request.skillId = static_cast<u16_t>((static_cast<u32_t>(eChampion::KINDRED) << 8) | slot);
request.rank = rank;
request.flags = DamageFlag_OnHit;
EnqueueDamageRequest(world, request);
```

아래로 교체:

```cpp
request.skillId = static_cast<u16_t>(
    (static_cast<u32_t>(eChampion::KINDRED) << 8) | slot);
request.rank = rank > 0u ? rank : 1u;
request.iSourceSlot = slot;
request.eSourceKind = eDamageSourceKind::Skill;
request.flags = DamageFlag_OnHit;
EnqueueDamageRequest(world, request);
```

`OnW`에서 duration/tick accumulator를 설정한 직후 시전 rank를 캡처한다.

기존 블록:

```cpp
state.fWTickAccumulatorSec = ResolveKindredSkillEffectParam(
    ctx, eSkillSlot::W, eSkillEffectParamId::TickIntervalSec);
state.vWCenter = center;
```

아래로 교체:

```cpp
state.fWTickAccumulatorSec = ResolveKindredSkillEffectParam(
    ctx, eSkillSlot::W, eSkillEffectParamId::TickIntervalSec);
state.uWCastRank = ctx.skillRank > 0u ? ctx.skillRank : 1u;
state.vWCenter = center;
```

W tick의 고정 `rank=1` 전달을 저장된 시전 rank로 교체한다.

기존 블록:

```cpp
EnqueuePhysicalDamage(
    world,
    entity,
    target,
    champion.team,
    wDamage,
    static_cast<u8_t>(eSkillSlot::W),
    1);
```

아래로 교체:

```cpp
EnqueuePhysicalDamage(
    world,
    entity,
    target,
    champion.team,
    wDamage,
    static_cast<u8_t>(eSkillSlot::W),
    state.uWCastRank);
```

### 2-4. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

기존 코드:

```cpp
bool_t RunGameplayFormulaDataDrivenProbe()
```

아래로 교체한다. 새 matrix 전체 뒤에 기존 선언을 다시 내보내므로 기존 함수 본문은 그대로 이어진다. 독립 world를 매 case 생성해 inventory/objective buff/Fiora 강화 상태를 비우고, crit 0/armor 0/MR 0으로 격리한다. 일반 스킬은 요청 수치를 일부러 0으로 훼손해 DamageQueue가 canonical formula를 복원하는지 검사하고, custom-flat 4종은 sentinel flat 37을 보존하면서 ratio만 canonical로 복원하는지 검사한다.

```cpp
bool_t RunChampionDamageAuthorityMatrixProbe()
{
    const GameplayDefinitionPack& pack =
        ServerData::GetLoLGameplayDefinitionPack();
    constexpr f32_t kSourceBaseAd = 80.f;
    constexpr f32_t kSourceBonusAd = 20.f;
    constexpr f32_t kSourceTotalAd = 100.f;
    constexpr f32_t kSourceAp = 50.f;
    constexpr f32_t kTargetMaxHp = 1000000.f;
    constexpr f32_t kTargetCurrentHp = 900000.f;
    constexpr f32_t kCustomFlatSentinel = 37.f;
    u32_t rankCaseCount = 0u;

    const auto IsCustomFlat = [](eChampion champion, u8_t slot)
    {
        return
            (champion == eChampion::YASUO &&
                slot == static_cast<u8_t>(eSkillSlot::Q)) ||
            (champion == eChampion::KALISTA &&
                slot == static_cast<u8_t>(eSkillSlot::E)) ||
            (champion == eChampion::LEESIN &&
                slot == static_cast<u8_t>(eSkillSlot::Q)) ||
            (champion == eChampion::EZREAL &&
                slot == static_cast<u8_t>(eSkillSlot::R));
    };
    const auto ResolveRawDamage = [](
        const DamageRequest& request,
        const StatComponent& sourceStat,
        f32_t targetMaxHp,
        f32_t targetCurrentHp)
    {
        return request.flatAmount +
            request.adRatioOverride * sourceStat.ad +
            request.bonusAdRatioOverride * sourceStat.bonusAd +
            request.apRatioOverride * sourceStat.ap +
            request.targetMaxHpRatioOverride * targetMaxHp +
            request.targetMissingHpRatioOverride *
                (targetMaxHp - targetCurrentHp);
    };
    const auto BuildPackOracle = [](
        const DamageFormulaDef& formula,
        EntityID source,
        EntityID target,
        eTeam sourceTeam,
        eChampion champion,
        u8_t slot,
        u8_t rank,
        eDamageSourceKind sourceKind)
    {
        DamageRequest oracle{};
        oracle.source = source;
        oracle.target = target;
        oracle.sourceTeam = sourceTeam;
        oracle.type = formula.type;
        oracle.flatAmount = ResolveDamageFormulaRankedValue(
            formula, formula.flatByRank, rank);
        oracle.adRatioOverride = ResolveDamageFormulaRankedValue(
            formula, formula.totalAdRatioByRank, rank);
        oracle.bonusAdRatioOverride = ResolveDamageFormulaRankedValue(
            formula, formula.bonusAdRatioByRank, rank);
        oracle.apRatioOverride = ResolveDamageFormulaRankedValue(
            formula, formula.apRatioByRank, rank);
        oracle.targetMaxHpRatioOverride = ResolveDamageFormulaRankedValue(
            formula, formula.targetMaxHpRatioByRank, rank);
        oracle.targetMissingHpRatioOverride = ResolveDamageFormulaRankedValue(
            formula, formula.targetMissingHpRatioByRank, rank);
        oracle.skillId = static_cast<u16_t>(
            (static_cast<u32_t>(champion) << 8u) | slot);
        oracle.rank = rank > 0u ? rank : 1u;
        oracle.iSourceSlot = slot;
        oracle.eSourceKind = sourceKind;
        oracle.flags = formula.flags;
        return oracle;
    };
    const auto RequestsMatchFormula = [](
        const DamageRequest& actual,
        const DamageRequest& expected)
    {
        constexpr f32_t kTolerance = 0.0001f;
        return actual.source == expected.source &&
            actual.target == expected.target &&
            actual.sourceTeam == expected.sourceTeam &&
            actual.type == expected.type &&
            std::fabs(actual.flatAmount - expected.flatAmount) <= kTolerance &&
            std::fabs(actual.adRatioOverride - expected.adRatioOverride) <= kTolerance &&
            std::fabs(actual.bonusAdRatioOverride - expected.bonusAdRatioOverride) <= kTolerance &&
            std::fabs(actual.apRatioOverride - expected.apRatioOverride) <= kTolerance &&
            std::fabs(actual.targetMaxHpRatioOverride - expected.targetMaxHpRatioOverride) <= kTolerance &&
            std::fabs(actual.targetMissingHpRatioOverride - expected.targetMissingHpRatioOverride) <= kTolerance &&
            actual.skillId == expected.skillId &&
            actual.rank == expected.rank &&
            actual.iSourceSlot == expected.iSourceSlot &&
            actual.eSourceKind == expected.eSourceKind &&
            actual.flags == expected.flags;
    };

    for (std::size_t championIndex = 0u;
        championIndex < pack.championCount;
        ++championIndex)
    {
        const ChampionGameplayDef& champion = pack.champions[championIndex];
        for (u8_t slot = 0u; slot < kChampionSkillSlotCount; ++slot)
        {
            const SkillGameplayDef* pSkill =
                pack.FindSkill(champion.skillLoadout[slot]);
            if (!pSkill || !pSkill->effect.damage.bValid)
            {
                std::printf(
                    "[SimLab][DamageAuthority] FAIL: champion=%u slot=%u missing formula\n",
                    static_cast<unsigned>(champion.legacyChampion),
                    static_cast<unsigned>(slot));
                return false;
            }

            const u8_t rankCount = pSkill->effect.damage.rankCount;
            for (u8_t rank = 1u; rank <= rankCount; ++rank)
            {
                CWorld world;
                DeterministicRng rng(
                    2026071901ull + rankCaseCount);
                EntityIdMap entityMap;
                FlatWalkable walkable;
                const EntityID source = SpawnChampion(
                    world,
                    entityMap,
                    champion.legacyChampion,
                    static_cast<u8_t>(eTeam::Blue),
                    0u);
                const EntityID target = SpawnChampion(
                    world,
                    entityMap,
                    eChampion::GAREN,
                    static_cast<u8_t>(eTeam::Red),
                    5u);
                TickContext tc = MakeProbeTickContext(
                    1ull,
                    rng,
                    entityMap,
                    walkable);

                StatComponent& sourceStat =
                    world.GetComponent<StatComponent>(source);
                sourceStat.baseAd = kSourceBaseAd;
                sourceStat.bonusAd = kSourceBonusAd;
                sourceStat.ad = kSourceTotalAd;
                sourceStat.ap = kSourceAp;
                sourceStat.critChance = 0.f;
                sourceStat.lifesteal = 0.f;
                sourceStat.bDirty = false;
                if (world.HasComponent<InventoryComponent>(source))
                    world.GetComponent<InventoryComponent>(source) = {};
                if (world.HasComponent<FioraSimComponent>(source))
                    world.GetComponent<FioraSimComponent>(source) = {};
                StatComponent& targetStat =
                    world.GetComponent<StatComponent>(target);
                targetStat.baseArmor = 0.f;
                targetStat.bonusArmor = 0.f;
                targetStat.armor = 0.f;
                targetStat.baseMr = 0.f;
                targetStat.bonusMr = 0.f;
                targetStat.mr = 0.f;
                targetStat.bDirty = false;
                HealthComponent& targetHealth =
                    world.GetComponent<HealthComponent>(target);
                targetHealth.fMaximum = kTargetMaxHp;
                targetHealth.fCurrent = kTargetCurrentHp;
                targetHealth.bIsDead = false;
                ChampionComponent& targetChampion =
                    world.GetComponent<ChampionComponent>(target);
                targetChampion.maxHp = kTargetMaxHp;
                targetChampion.hp = kTargetCurrentHp;

                const eDamageSourceKind sourceKind = slot ==
                    static_cast<u8_t>(eSkillSlot::BasicAttack)
                    ? eDamageSourceKind::BasicAttack
                    : eDamageSourceKind::Skill;
                const DamageRequest oracle = BuildPackOracle(
                    pSkill->effect.damage,
                    source,
                    target,
                    eTeam::Blue,
                    champion.legacyChampion,
                    slot,
                    rank,
                    sourceKind);
                DamageRequest built{};
                if (!GameplayDefinitionQuery::BuildSkillDamageRequest(
                        world,
                        source,
                        target,
                        tc,
                        champion.legacyChampion,
                        slot,
                        rank,
                        eTeam::Blue,
                        sourceKind,
                        built) ||
                    !RequestsMatchFormula(built, oracle))
                {
                    std::printf(
                        "[SimLab][DamageAuthority] FAIL: champion=%u slot=%u rank=%u build-vs-pack\n",
                        static_cast<unsigned>(champion.legacyChampion),
                        static_cast<unsigned>(slot),
                        static_cast<unsigned>(rank));
                    return false;
                }

                DamageRequest submitted = built;
                DamageRequest expected = oracle;
                if (slot != static_cast<u8_t>(eSkillSlot::BasicAttack))
                {
                    submitted.flatAmount = 0.f;
                    submitted.adRatioOverride = 0.f;
                    submitted.bonusAdRatioOverride = 0.f;
                    submitted.apRatioOverride = 0.f;
                    submitted.targetMaxHpRatioOverride = 0.f;
                    submitted.targetMissingHpRatioOverride = 0.f;
                    if (IsCustomFlat(champion.legacyChampion, slot))
                    {
                        submitted.flatAmount = kCustomFlatSentinel;
                        expected.flatAmount = kCustomFlatSentinel;
                    }
                }

                const f32_t expectedRaw = (std::max)(
                    0.f,
                    ResolveRawDamage(
                        expected,
                        sourceStat,
                        kTargetMaxHp,
                        kTargetCurrentHp));
                if (expectedRaw >= kTargetCurrentHp)
                {
                    std::printf(
                        "[SimLab][DamageAuthority] FAIL: champion=%u slot=%u rank=%u nonlethal fixture overflow=%.3f\n",
                        static_cast<unsigned>(champion.legacyChampion),
                        static_cast<unsigned>(slot),
                        static_cast<unsigned>(rank),
                        expectedRaw);
                    return false;
                }
                EnqueueDamageRequest(world, submitted);
                CDamageQueueSystem::Execute(world, tc);
                const f32_t actualApplied = kTargetCurrentHp -
                    world.GetComponent<HealthComponent>(target).fCurrent;
                const f32_t tolerance = (std::max)(
                    0.05f,
                    expectedRaw * 0.00001f);
                if (std::fabs(actualApplied - expectedRaw) > tolerance)
                {
                    std::printf(
                        "[SimLab][DamageAuthority] FAIL: champion=%u slot=%u rank=%u expected=%.3f actual=%.3f\n",
                        static_cast<unsigned>(champion.legacyChampion),
                        static_cast<unsigned>(slot),
                        static_cast<unsigned>(rank),
                        expectedRaw,
                        actualApplied);
                    return false;
                }
                ++rankCaseCount;
            }
        }
    }

    struct OverrideCase
    {
        eSkillEffectParamId param = eSkillEffectParamId::None;
        f32_t value = 0.f;
    };
    const OverrideCase overrideCases[] = {
        { eSkillEffectParamId::TotalAdRatio, 0.11f },
        { eSkillEffectParamId::BonusAdRatio, 0.13f },
        { eSkillEffectParamId::ApRatio, 0.17f },
        { eSkillEffectParamId::TargetMaxHpRatio, 0.19f },
        { eSkillEffectParamId::MissingHealthDamageRatio, 0.23f },
    };
    const auto ResolveOverrideField = [](
        const DamageRequest& request,
        eSkillEffectParamId param)
    {
        switch (param)
        {
        case eSkillEffectParamId::TotalAdRatio:
            return request.adRatioOverride;
        case eSkillEffectParamId::BonusAdRatio:
            return request.bonusAdRatioOverride;
        case eSkillEffectParamId::ApRatio:
            return request.apRatioOverride;
        case eSkillEffectParamId::TargetMaxHpRatio:
            return request.targetMaxHpRatioOverride;
        case eSkillEffectParamId::MissingHealthDamageRatio:
            return request.targetMissingHpRatioOverride;
        default:
            return -1.f;
        }
    };

    for (u8_t overrideIndex = 0u; overrideIndex < 5u; ++overrideIndex)
    {
        CWorld world;
        DeterministicRng rng(2026071902ull + overrideIndex);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        const EntityID source = SpawnChampion(
            world, entityMap, eChampion::EZREAL,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID target = SpawnChampion(
            world, entityMap, eChampion::GAREN,
            static_cast<u8_t>(eTeam::Red), 5u);
        TickContext tc = MakeProbeTickContext(1ull, rng, entityMap, walkable);
        auto& sourceStat = world.GetComponent<StatComponent>(source);
        sourceStat.baseAd = kSourceBaseAd;
        sourceStat.bonusAd = kSourceBonusAd;
        sourceStat.ad = kSourceTotalAd;
        sourceStat.ap = kSourceAp;
        sourceStat.critChance = 0.f;
        sourceStat.lifesteal = 0.f;
        sourceStat.bDirty = false;
        if (world.HasComponent<InventoryComponent>(source))
            world.GetComponent<InventoryComponent>(source) = {};
        auto& targetStat = world.GetComponent<StatComponent>(target);
        targetStat.baseArmor = 0.f;
        targetStat.bonusArmor = 0.f;
        targetStat.armor = 0.f;
        targetStat.baseMr = 0.f;
        targetStat.bonusMr = 0.f;
        targetStat.mr = 0.f;
        targetStat.bDirty = false;
        auto& targetHealth = world.GetComponent<HealthComponent>(target);
        targetHealth.fMaximum = kTargetMaxHp;
        targetHealth.fCurrent = kTargetCurrentHp;
        targetHealth.bIsDead = false;
        auto& targetChampion = world.GetComponent<ChampionComponent>(target);
        targetChampion.maxHp = kTargetMaxHp;
        targetChampion.hp = kTargetCurrentHp;

        auto& overrides =
            world.AddComponent<PracticeSkillEffectOverrideComponent>(
                source,
                PracticeSkillEffectOverrideComponent{});
        overrides.entries[0].slot = static_cast<u8_t>(eSkillSlot::Q);
        overrides.entries[0].paramId = static_cast<u8_t>(
            overrideCases[overrideIndex].param);
        overrides.entries[0].value = overrideCases[overrideIndex].value;
        overrides.count = 1u;

        const ChampionGameplayDef* pEzreal =
            pack.FindChampion(eChampion::EZREAL);
        const SkillGameplayDef* pEzrealQ = pEzreal
            ? pack.FindSkill(
                pEzreal->skillLoadout[static_cast<u8_t>(eSkillSlot::Q)])
            : nullptr;
        if (!pEzrealQ || !pEzrealQ->effect.damage.bValid)
        {
            std::printf(
                "[SimLab][DamageAuthority] FAIL: Ezreal Q oracle missing\n");
            return false;
        }
        DamageRequest expected = BuildPackOracle(
            pEzrealQ->effect.damage,
            source,
            target,
            eTeam::Blue,
            eChampion::EZREAL,
            static_cast<u8_t>(eSkillSlot::Q),
            3u,
            eDamageSourceKind::Skill);
        switch (overrideCases[overrideIndex].param)
        {
        case eSkillEffectParamId::TotalAdRatio:
            expected.adRatioOverride = overrideCases[overrideIndex].value;
            break;
        case eSkillEffectParamId::BonusAdRatio:
            expected.bonusAdRatioOverride = overrideCases[overrideIndex].value;
            break;
        case eSkillEffectParamId::ApRatio:
            expected.apRatioOverride = overrideCases[overrideIndex].value;
            break;
        case eSkillEffectParamId::TargetMaxHpRatio:
            expected.targetMaxHpRatioOverride = overrideCases[overrideIndex].value;
            break;
        case eSkillEffectParamId::MissingHealthDamageRatio:
            expected.targetMissingHpRatioOverride = overrideCases[overrideIndex].value;
            break;
        default:
            return false;
        }

        DamageRequest built{};
        if (!GameplayDefinitionQuery::BuildSkillDamageRequest(
                world,
                source,
                target,
                tc,
                eChampion::EZREAL,
                static_cast<u8_t>(eSkillSlot::Q),
                3u,
                eTeam::Blue,
                eDamageSourceKind::Skill,
                built) ||
            !RequestsMatchFormula(built, expected) ||
            std::fabs(ResolveOverrideField(
                built,
                overrideCases[overrideIndex].param) -
                overrideCases[overrideIndex].value) > 0.0001f)
        {
            std::printf(
                "[SimLab][DamageAuthority] FAIL: live override build index=%u\n",
                static_cast<unsigned>(overrideIndex));
            return false;
        }
        DamageRequest submitted = built;
        submitted.adRatioOverride = 0.f;
        submitted.bonusAdRatioOverride = 0.f;
        submitted.apRatioOverride = 0.f;
        submitted.targetMaxHpRatioOverride = 0.f;
        submitted.targetMissingHpRatioOverride = 0.f;
        const f32_t expectedApplied = ResolveRawDamage(
            expected,
            sourceStat,
            kTargetMaxHp,
            kTargetCurrentHp);
        if (expectedApplied >= kTargetCurrentHp)
        {
            std::printf(
                "[SimLab][DamageAuthority] FAIL: live override nonlethal fixture index=%u damage=%.3f\n",
                static_cast<unsigned>(overrideIndex),
                expectedApplied);
            return false;
        }
        EnqueueDamageRequest(world, submitted);
        CDamageQueueSystem::Execute(world, tc);
        const f32_t actualApplied = kTargetCurrentHp -
            world.GetComponent<HealthComponent>(target).fCurrent;
        if (std::fabs(actualApplied - expectedApplied) > 0.05f)
        {
            std::printf(
                "[SimLab][DamageAuthority] FAIL: live override index=%u expected=%.3f actual=%.3f\n",
                static_cast<unsigned>(overrideIndex),
                expectedApplied,
                actualApplied);
            return false;
        }
    }

    {
        CWorld world;
        DeterministicRng rng(2026071903ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        const EntityID kindred = SpawnChampion(
            world, entityMap, eChampion::KINDRED,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID target = SpawnChampion(
            world, entityMap, eChampion::GAREN,
            static_cast<u8_t>(eTeam::Red), 5u);
        world.GetComponent<TransformComponent>(kindred).SetPosition({ 0.f, 0.f, 0.f });
        world.GetComponent<TransformComponent>(target).SetPosition({ 0.f, 0.f, 0.f });
        TickContext tc = MakeProbeTickContext(1ull, rng, entityMap, walkable);
        GameCommand wCommand{};
        wCommand.groundPos = { 0.f, 0.f, 0.f };
        GameplayHookContext wHook{};
        wHook.pWorld = &world;
        wHook.casterEntity = kindred;
        wHook.casterTeam = eTeam::Blue;
        wHook.casterChampion = eChampion::KINDRED;
        wHook.skillRank = 4u;
        wHook.pCommand = &wCommand;
        wHook.pTickCtx = &tc;
        KindredGameSim::RegisterHooks();
        if (!CGameplayHookRegistry::Instance().Dispatch(
                MakeGameplayHookId(
                    eChampion::KINDRED,
                    GameplayHookVariant::W_CastFrame),
                wHook))
        {
            std::printf(
                "[SimLab][DamageAuthority] FAIL: Kindred W hook dispatch\n");
            return false;
        }
        auto& state = world.GetComponent<KindredSimComponent>(kindred);
        if (state.uWCastRank != 4u || state.fWRemainingSec <= 0.f)
        {
            std::printf(
                "[SimLab][DamageAuthority] FAIL: Kindred W cast-rank capture\n");
            return false;
        }
        state.fWTickAccumulatorSec = 0.f;
        world.GetComponent<SkillRankComponent>(kindred)
            .ranks[static_cast<u8_t>(eSkillSlot::W)] = 1u;
        KindredGameSim::Tick(world, tc);
        bool_t foundRankFourRequest = false;
        world.ForEach<DamageRequestComponent>(
            [&](EntityID, DamageRequestComponent& request)
            {
                foundRankFourRequest = foundRankFourRequest ||
                    (request.source == kindred &&
                        request.target == target &&
                        request.iSourceSlot ==
                            static_cast<u8_t>(eSkillSlot::W) &&
                        request.rank == 4u &&
                        request.eSourceKind == eDamageSourceKind::Skill);
            });
        if (!foundRankFourRequest)
        {
            std::printf(
                "[SimLab][DamageAuthority] FAIL: Kindred W cast-rank route\n");
            return false;
        }
    }

    std::printf(
        "[SimLab][DamageAuthority] PASS: 17 champions / 85 skills / %u rank cases + live overrides + Kindred cast rank\n",
        rankCaseCount);
    return true;
}

bool_t RunGameplayFormulaDataDrivenProbe()
```

`RunGameplayFormulaDataDrivenProbe`의 `DamageRequest request{};`부터 기존 Ezreal 실패 `if`의 닫는 중괄호까지를 아래 블록 전체로 교체한다. 기존 선언을 남기지 않으므로 `request` 중복 선언이 생기지 않는다.

```cpp
const ChampionGameplayDef* pEzreal = pack.FindChampion(eChampion::EZREAL);
const SkillGameplayDef* pEzrealQ = pEzreal
    ? pack.FindSkill(
        pEzreal->skillLoadout[static_cast<u8_t>(eSkillSlot::Q)])
    : nullptr;
const DamageFormulaDef* pEzrealQDamage = pEzrealQ
    ? &pEzrealQ->effect.damage
    : nullptr;
DamageRequest request{};
if (!pEzrealQDamage ||
    !GameplayDefinitionQuery::BuildSkillDamageRequest(
        world,
        source,
        target,
        tc,
        eChampion::EZREAL,
        static_cast<u8_t>(eSkillSlot::Q),
        3u,
        eTeam::Blue,
        eDamageSourceKind::Skill,
        request) ||
    std::fabs(request.flatAmount - ResolveDamageFormulaRankedValue(
        *pEzrealQDamage, pEzrealQDamage->flatByRank, 3u)) > 0.001f ||
    std::fabs(request.adRatioOverride - ResolveDamageFormulaRankedValue(
        *pEzrealQDamage, pEzrealQDamage->totalAdRatioByRank, 3u)) > 0.001f ||
    std::fabs(request.apRatioOverride - ResolveDamageFormulaRankedValue(
        *pEzrealQDamage, pEzrealQDamage->apRatioByRank, 3u)) > 0.001f ||
    request.type != pEzrealQDamage->type ||
    request.flags != pEzrealQDamage->flags)
{
    std::printf(
        "[SimLab][FormulaData] FAIL: Ezreal Q rank-3 request drifted from current pack\n");
    return false;
}
```

`RunGameplayFormulaDataDrivenProbe` 끝의 아래 기존 블록을 완전히 교체해 행렬 호출을 최종 PASS 출력 전에 추가한다.

기존 블록:

```cpp
std::printf(
    "[SimLab][FormulaData] PASS: ranked data, scaled damage execution, respawn contract\n");
return true;
```

아래로 교체:

```cpp
if (!RunChampionDamageAuthorityMatrixProbe())
    return false;

std::printf(
    "[SimLab][FormulaData] PASS: ranked data, scaled damage execution, respawn contract\n");
return true;
```

### 2-5. C:/Users/user/Desktop/Winters/Tools/LoLData/Test-F4BalanceContracts.py

이 파일은 수치 자체를 하드코딩하지 않고 경로 계약만 검사한다.

기존 코드:

```python
damage_queue = (root / "Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp").read_text(encoding="utf-8")
```

아래로 교체:

```python
query_cpp = (root /
    "Shared/GameSim/Definitions/GameplayDefinitionQuery.cpp").read_text(
        encoding="utf-8")
kindred_cpp = (root /
    "Shared/GameSim/Champions/Kindred/KindredGameSim.cpp").read_text(
        encoding="utf-8")
kindred_component = (root /
    "Shared/GameSim/Components/KindredSimComponent.h").read_text(
        encoding="utf-8")
require("uWCastRank = 1u" in kindred_component,
        "Kindred W cast rank is checkpoint-owned state")
require(
    "TryResolvePracticeSkillEffectOverride" in query_cpp,
    "formula live overrides must bypass authored runtime params",
)
for token in (
    "eSkillEffectParamId::TotalAdRatio",
    "eSkillEffectParamId::BonusAdRatio",
    "eSkillEffectParamId::ApRatio",
    "eSkillEffectParamId::TargetMaxHpRatio",
    "eSkillEffectParamId::MissingHealthDamageRatio",
):
    require(token in query_cpp, f"missing final formula override: {token}")
require(
    "request.eSourceKind = eDamageSourceKind::Skill" in kindred_cpp,
    "Kindred W requests must enter the skill formula path explicitly",
)
require("state.uWCastRank = ctx.skillRank > 0u ? ctx.skillRank : 1u" in kindred_cpp,
        "Kindred W captures rank at cast time")
damage_queue = (root / "Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp").read_text(encoding="utf-8")
```

기존 코드:

```python
simlab = (root / "Tools/SimLab/main.cpp").read_text(encoding="utf-8")
f4_only = simlab.split(
    'std::strcmp(argv[1], "--f4-balance-only")', 1)[1].split(
        'std::strcmp(argv[1], "--irelia-q-only")', 1)[0]
```

아래로 교체한다. `simlab` 계약은 입력 로드 뒤, `f4_only` 파생 전에 실행된다.

```python
simlab = (root / "Tools/SimLab/main.cpp").read_text(encoding="utf-8")
require("state.uWCastRank" in simlab and
        "wHook.skillRank = 4u" in simlab and
        "request.rank == 4u" in simlab,
        "Kindred W cast hook and request rank route are probed")
require("RunChampionDamageAuthorityMatrixProbe" in simlab,
        "all champion damage formulas execute through DamageQueue")
require("BuildPackOracle" in simlab and
        "RequestsMatchFormula" in simlab,
        "damage matrix oracle is built directly from pack arrays")
f4_only = simlab.split(
    'std::strcmp(argv[1], "--f4-balance-only")', 1)[1].split(
        'std::strcmp(argv[1], "--irelia-q-only")', 1)[0]
```

## 3. 검증

예측:

- 최신 JSON은 generator 실행 후 두 `--check`에서 PASS한다. STALE이 남으면 실행 중 source hash 변경 또는 generator coverage 문제다.
- pack coverage는 17 champions/85 skills이고, 모든 유효 rank의 direct pack oracle·Build request·DamageQueue final damage가 일치한다. `[DamageAuthority] champion/slot/rank`는 최초로 어긋난 소유권 경로를 특정한다.
- F4 practice total AD/bonus AD/AP/max HP/missing HP override가 각각 final damage에 반영된다. `live override index`는 실패한 param mapping을 특정한다.
- Kindred W rank 4 시전 후 현재 W rank를 1로 바꿔도 request는 rank 4/slot W/source kind Skill을 유지한다. `Kindred W cast-rank` 로그는 캡처와 전달 중 깨진 단계를 구분한다.
- stale Ezreal 70/1.3 하드코딩 없이 현재 pack 값으로 FormulaData가 PASS하고, GameSim/SimLab/Server/Client Debug x64가 빌드된다.
- Bot AI는 GameCommand 생산자이며 gameplay truth를 직접 변경하지 않는다. 동일 서버 `BuildSkillDamageRequest -> DamageQueue` 게이트가 player/bot 공통 수치 경계를 잡는다.

구현/검증 순서:

1. 현 JSON/생성 팩 parity와 사용자 변경 key 목록을 캡처한다 → 현재 `STALE` 목록과 17/85를 기록한다.
2. 독립 critique P0/P1을 0으로 만든다 → 구현 전 plan disposition 기록.
3. query/Kindred component+system/SimLab/test 다섯 파일만 수술한다 → 적용 직전 anchor/hash 재검사와 scoped diff 확인.
4. shared-generated owner handoff를 먼저 확인하고, 다른 세션의 source JSON 변경이 멈춘 동일 시점 snapshot에서 두 generator를 직렬 실행한다 → 11개 canonical source hash가 실행 중 바뀌면 산출물을 폐기하고 owner와 다시 합의한 뒤 재실행한다.
5. 정적/시뮬레이션 검증을 실행한다 → F4 contract, 두 `--check`, SimLab `--f4-balance-only`와 전체 SimLab.
6. GameSim/Server Debug x64를 `/m:1`로 빌드한다 → 사용자/다른 세션 변경을 되돌리지 않고 결과와 잔여 실패를 RESULT에 분리한다.

검증 명령:

생성 전 선행 조건: 이 세션이 아래 shared-generated 파일의 단일 owner임을 다른 작업 세션과 합의한다. 합의 전에는 generator를 실행하지 않는다.

- `Shared/GameSim/Generated/ChampionGameData.generated.h`
- `Shared/GameSim/Generated/ChampionGameData.generated.cpp`
- `Data/LoL/ServerPrivate/Gameplay/ChampionGameplayDefs.json`
- `Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json`
- `Data/LoL/SharedContract/DefinitionManifest.json`
- `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp`
- `Client/Private/Data/Generated/LoLVisualDefinitions.generated.cpp`
- `Shared/GameSim/Generated/ChampionAIPolicyData.generated.inl`
- `.md/TODO/06-22/LOL_DEFINITION_PACK_PARITY.json`

아래 블록은 generator의 `canonical_source_table`과 동일한 11개 authoring input만 SHA-256으로 감시한다. generated output은 입력 hash에서 제외한다. 각 프로세스 직후 exit code를 확인하며, 전후 hash 불일치 시 현재 산출물을 채택하지 않고 즉시 실패한다.

```powershell
$canonicalInputs = @(
    "Data/Gameplay/ChampionGameData/champions.json",
    "Data/LoL/ServerPrivate/Gameplay/SummonerSpellGameplayDefs.json",
    "Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json",
    "Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json",
    "Data/LoL/ServerPrivate/Gameplay/EconomyGameplayDefs.json",
    "Data/LoL/ServerPrivate/Gameplay/ItemGameplayDefs.json",
    "Data/LoL/ServerPrivate/Gameplay/RuneGameplayDefs.json",
    "Data/LoL/ServerPrivate/AI/ChampionAIGameplayDefs.json",
    "Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json",
    "Data/LoL/ClientPublic/Visual/ObjectVisualDefs.json",
    "Data/LoL/ClientPublic/Visual/ChampionAssetVisualDefs.json"
)
$beforeHashes = @{}
foreach ($path in $canonicalInputs) {
    $beforeHashes[$path] = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
}

python Tools/ChampionData/build_champion_game_data.py --root .
if ($LASTEXITCODE -ne 0) { throw "ChampionGameData generation failed" }
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
if ($LASTEXITCODE -ne 0) { throw "LoL definition-pack generation failed" }

$changedInputs = @()
foreach ($path in $canonicalInputs) {
    $afterHash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    if ($afterHash -ne $beforeHashes[$path]) { $changedInputs += $path }
}
if ($changedInputs.Count -ne 0) {
    throw "Canonical inputs changed during generation; discard outputs and reacquire owner handoff: $($changedInputs -join ', ')"
}

python Tools/LoLData/Test-F4BalanceContracts.py --root .
if ($LASTEXITCODE -ne 0) { throw "F4 balance contracts failed" }
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
if ($LASTEXITCODE -ne 0) { throw "LoL definition-pack check failed" }
python Tools/ChampionData/build_champion_game_data.py --root . --check
if ($LASTEXITCODE -ne 0) { throw "ChampionGameData check failed" }
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild `
    -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
if (-not $msbuild) { throw "MSBuild not found" }
& $msbuild Shared/GameSim/Include/GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
if ($LASTEXITCODE -ne 0) { throw "GameSim build failed" }
& $msbuild Tools/SimLab/SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
if ($LASTEXITCODE -ne 0) { throw "SimLab build failed" }
& .\Tools\Bin\Debug\SimLab.exe --f4-balance-only
if ($LASTEXITCODE -ne 0) { throw "SimLab F4 probe failed" }
& .\Tools\Bin\Debug\SimLab.exe
if ($LASTEXITCODE -ne 0) { throw "Full SimLab failed" }
& $msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
if ($LASTEXITCODE -ne 0) { throw "Server build failed" }
& $msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
if ($LASTEXITCODE -ne 0) { throw "Client build failed" }
git diff --check
if ($LASTEXITCODE -ne 0) { throw "git diff --check failed" }
```

미검증:

- Riven Q/W, Master Yi Q/W/E, Lee Sin E처럼 현재 damage request 자체가 없는 기능은 이번 데이터 전달 수정으로 새 gameplay를 만들지 않는다. RESULT에서 `NOT_IMPLEMENTED`로 분리한다.
- F4 per-entity override 32-entry 용량 확장은 이번 범위 밖이다. 한 번에 모든 slot×모든 ratio를 live override하는 UX는 `CONFIRM_NEEDED`다.

확인 필요:

- 다른 작업 세션이 11개 canonical input 또는 shared generated output을 동시에 수정 중이면 owner handoff 전 generator를 실행하지 않는다.

## 4. 인계

- 직접 수정 owner: `GameplayDefinitionQuery.cpp`, `KindredSimComponent.h`, `KindredGameSim.cpp`, `Tools/SimLab/main.cpp`의 새 probe/FormulaData anchor, `Test-F4BalanceContracts.py`의 damage-authority contract, 본 PLAN/RESULT.
- shared generated owner: generator가 쓰는 `ChampionGameplayDefs.json`, `SkillGameplayDefs.json`, manifest, Server/Client generated cpp, `ChampionGameData.generated.h/.cpp`, AI generated inl, parity report는 손편집하지 않는다. 다른 세션과 단일 owner handoff가 끝나고 11개 source JSON의 즉시 전/후 SHA-256이 동일한 직렬 cook만 채택한다.
- 사용자 수치(`SkillEffectGameplayDefs.json`)와 generated pack은 임의 복원하지 않는다. generator가 필요할 때만 현재 JSON으로 재생성한다.
- F5 체감 검증은 Debug room host에서 `Save & Hot Load` 후 status가 `Hot load applied`인지 먼저 확인하고, 실패하면 서버 재빌드/재시작 후 같은 스킬을 비교한다.

## 서브 에이전트 비평

- 독립 critique 1차(Huygens): P0=0, P1=4로 구현 gate 불통과.
  - 수용: §2-4를 placeholder/prose에서 완전한 격리 matrix 코드로 교체하고 custom-flat sentinel 검사를 추가했다.
  - 수용: Kindred W rank를 tick 현재값이 아니라 `OnW` 시전값으로 캡처하고 request metadata까지 검사한다.
  - 수용: 두 generator 실행과 shared output 직렬 cook/hash gate를 명시했다.
  - 수용: 직접 수정 anchor와 generated 산출물 owner를 분리하고 적용 직전 anchor/hash 재검사를 추가했다.
  - 수용: vswhere 기반 MSBuild 탐색, 명시적 SimLab 호출, Client Debug 빌드를 추가했다.
  - 수용: query/Kindred/test의 기존 anchor와 완전 교체 블록, 예측/실패 판독, 미구현 스킬과 override 용량 미검증을 명시했다.
- 독립 critique 2차(Huygens): P0=0, P1=4로 구현 gate 불통과.
  - 수용: Build 결과를 expected로 재사용하지 않고 `DamageFormulaDef` 여섯 rank 배열/type/flags/metadata로 독립 `BuildPackOracle`을 구성하며, Build 전체 필드와 queue 결과를 각각 대조한다.
  - 수용: override 5종도 direct oracle의 정확히 한 필드만 교체하고 나머지 필드 불변을 검사한다.
  - 수용: Kindred W는 `W_CastFrame` registry dispatch로 rank 4를 캡처한 뒤 live rank를 1로 바꾸고 tick request metadata를 검증한다.
  - 수용: generator 11개 canonical input SHA-256 전후 비교, 모든 exit code, shared-generated 단일 owner handoff와 누락된 generated header를 명령/목록으로 고정했다.
  - 수용: query/component/SimLab Ezreal/PASS/Test-F4의 정확한 anchor와 `simlab` 로드 이후 정적 계약 삽입 순서를 명시했다.
- 독립 critique 3차(Huygens): P0=0, P1=2, P2=2로 구현 gate 불통과.
  - 수용: helper는 `ResolveSkillRankForScaling` 전체 기존 코드 아래에 추가하고, matrix는 기존 FormulaData 선언을 `matrix 전체 + 기존 선언`으로 완전 교체하는 strict anchor로 바꿨다.
  - 수용: Test-F4의 `damage_queue`와 `simlab/f4_only` 실제 기존 블록을 인용하고 입력/계약/원본 선언을 포함한 완전 교체 블록으로 바꿨다.
  - 수용: §3을 예측(Bot AI 경계 포함) → 구현/검증 순서 → 검증 명령 → 미검증 → 확인 필요 순으로 재배치했다.
  - 기각(P2): rank clamp helper 독립 재구현은 formula 소유권/전달 검증 범위를 넘어 production 로직을 테스트에 복제하므로 이번에는 공유 helper를 쓴다.
  - 보류(P2): Kindred keyframe round-trip은 유용하지만 이번 결함은 cast-rank 캡처와 request metadata 누락이며, raw ECS checkpoint 범위 자체는 별도 keyframe 계약에서 다룬다.
- 독립 critique 4차(Huygens): P0=0, P1=0, P2=2. strict anchor와 검증 순서를 재대조해 구현 gate 통과.

## 5. 2026-07-19 Release 수치 미반영 재개방 델타

### 5-1. 결정 기록

① 문제·제약: 22:42~22:43에 저장된 authoring JSON보다 Release가 소비하는 generated pack(20:02)이 오래됐다. 현재 Yasuo Q는 authoring `flat=60,totalAD=2.0`이지만 generated는 `flat=60,totalAD=0.0`, Lee Sin Q도 `totalAD=1.0→0.0`, Yasuo E cooldown도 `0.1→3.0`이다.
② 순진한 해법의 실패: flat damage를 다시 채우거나 서버 cooldown clamp를 제거해도 결함을 못 고친다. 세 스킬 모두 flat은 이미 존재하고, 일반 스킬 cooldown 경로는 `max(0,cooldown)`만 적용하며 Release는 `_DEBUG` practice/hot-load 명령을 의도적으로 무시한다.
③ 메커니즘: 현 authoring 값을 보존한 채 두 generator를 직렬 실행하고 Release Server/Client/SimLab을 다시 빌드한다. `--check`와 named-field parity를 Release 실행 전 게이트로 둔다.
④ 대조: Release에서 F4 JSON을 런타임 hot-load하는 방식은 촬영 빌드의 불변성과 서버 권위를 깨므로 채택하지 않는다. Debug F4는 즉시 실험, Release는 cook+rebuild로 승격한다.
⑤ 대가: F4 저장 뒤 Release 재생성·재빌드가 필요하다. 이 선택은 향후 signed runtime balance bundle을 Release 서버가 명시적으로 지원할 때 틀려진다.

### 5-2. 확정 원인과 오진 기각

- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`
  - `skill.yasuo.q`: `flatByRank=[60×5]`, `totalAdRatioByRank=[2.0×5]` — flat 비어 있음 가설 기각.
  - `skill.leesin.q`: `flatByRank=[55,80,105,130,155]`, `totalAdRatioByRank=[1.0×5]` — flat 비어 있음 가설 기각.
- `Data/Gameplay/ChampionGameData/champions.json`
  - Yasuo E `cooldownSecByRank=[0.1×5]`.
- stale generated truth
  - `SkillGameplayDefs.json`: Yasuo Q/Lee Sin Q total AD ratio가 모두 0, Yasuo E cooldown 3초.
  - `Build-LoLDefinitionPack.py --check`: 6 generated outputs `STALE`.
  - `build_champion_game_data.py --check`: `ChampionGameData.generated.cpp` `STALE`.
- runtime 메커니즘
  - `DamageQueueSystem.cpp::UsesParamDrivenDamageVariant`는 Yasuo Q/Lee Sin Q의 custom flat을 보존하고 `BuildSkillDamageRequest`에서 AD ratio를 다시 합친다.
  - `DamagePipeline.cpp::BuildRawDamage`는 최종 `StatComponent::ad * adRatioOverride`를 더한다. 비율만 0인 stale pack 때문에 아이템/레벨 AD 증가가 Q에 안 보인 것이다.
  - `CommandExecutor.cpp::ResolveCastSkillCooldown`에는 스킬 0.1초를 3초로 올리는 clamp가 없다. 단, Yasuo E는 cooldown과 별도로 0.4초 action lock 및 동일 대상 10초 lockout을 가진다.
  - `GameRoomCommands.cpp::TryHandlePracticeControl`은 `!_DEBUG`에서 practice 명령을 처리하지 않고 반환한다. 따라서 Debug F4 저장은 Release 실행 바이너리의 compiled pack을 바꾸지 않는다.

### 5-3. 반영해야 하는 코드/산출물

소스 수치나 GameSim 공식을 추가 수정하지 않는다. 아래 authoring 파일은 현재 사용자 값이므로 손편집하지 않는다.

- `C:/Users/user/Desktop/Winters/Data/Gameplay/ChampionGameData/champions.json`
- `C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`

아래 명령으로만 generated 산출물을 갱신한다.

```powershell
python Tools/ChampionData/build_champion_game_data.py --root .
if ($LASTEXITCODE -ne 0) { throw "ChampionGameData generation failed" }
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
if ($LASTEXITCODE -ne 0) { throw "LoL definition-pack generation failed" }
```

generator가 쓰는 기존 산출물 전체를 채택하되 손편집하지 않는다. 다른 세션이 canonical input/generated output을 수정 중이면 기존 §3의 11-input SHA-256/단일-owner 게이트를 그대로 적용한다.

Release 촬영 진입점 owner에게는 `Tools/Harness/StartReleaseAccountReplayCapture.ps1`가 실행 전 아래 두 freshness check를 통과시키거나 즉시 중단하도록 인계한다. 해당 파일은 replay 세션 소유이므로 이 델타에서 직접 수정하지 않는다.

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
python Tools/ChampionData/build_champion_game_data.py --root . --check
```

### 5-4. 검증

예측:

- 재생성 뒤 generated `skill.yasuo.q.totalAdRatioByRank=2.0`, `skill.leesin.q.totalAdRatioByRank=1.0`, Yasuo E cooldown `0.1`이며 두 `--check`가 PASS한다.
- Release SimLab damage-authority 행렬은 Yasuo/Lee Sin Q에서 source AD 증가분을 ratio만큼 반영한다. flat은 Yasuo Q variant/Lee Sin Q recast가 제출한 값을 계속 보존한다.
- Release 서버에서 Yasuo E cooldown slot은 0.1초가 된다. 실제 같은 대상 재사용은 10초 lockout, 새 대상 연속 사용은 0.4초 action lock이 별도 상한이다.
- Bot AI는 GameCommand 생산자이며 gameplay truth를 직접 변경하지 않는다. player/bot 모두 같은 generated pack→DamageQueue 경로를 소비한다.

named-field parity:

```powershell
python -c "import json; a=json.load(open('Data/Gameplay/ChampionGameData/champions.json',encoding='utf-8')); e=json.load(open('Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json',encoding='utf-8')); g=json.load(open('Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json',encoding='utf-8')); cs={x['champion']:x for x in a['champions']}; es={x['key']:x for x in e['skillEffects']}; gs={x['key']:x for x in g['skills']}; assert gs['skill.yasuo.q']['effect']['damage']['totalAdRatioByRank']==es['skill.yasuo.q']['damage']['totalAdRatioByRank']; assert gs['skill.leesin.q']['effect']['damage']['totalAdRatioByRank']==es['skill.leesin.q']['damage']['totalAdRatioByRank']; assert gs['skill.yasuo.e']['cooldown']['secondsByRank']==cs['YASUO']['skills'][3]['cooldownSecByRank']; print('PASS Yasuo/LeeSin authoring-to-generated parity')"
```

Release 빌드/회귀:

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
if ($LASTEXITCODE -ne 0) { throw "LoL definition-pack check failed" }
python Tools/ChampionData/build_champion_game_data.py --root . --check
if ($LASTEXITCODE -ne 0) { throw "ChampionGameData check failed" }
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
& $msbuild Tools/SimLab/SimLab.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
if ($LASTEXITCODE -ne 0) { throw "Release SimLab build failed" }
& Tools/Bin/Release/SimLab.exe --f4-balance-only
if ($LASTEXITCODE -ne 0) { throw "Release damage authority probe failed" }
& $msbuild Server/Include/Server.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
if ($LASTEXITCODE -ne 0) { throw "Release Server build failed" }
& $msbuild Client/Include/Client.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
if ($LASTEXITCODE -ne 0) { throw "Release Client build failed" }
git diff --check
```

미검증:

- 실클라 방어력·아이템 조합 체감은 Release 3클라 재현에서 확인한다. 자동 행렬은 armor 0의 raw/formula 소유권을 검증한다.
- 동일 대상 Yasuo E 10초 lockout은 cooldown 0.1과 별도 규약이므로 이번 수치 cook에서 변경하지 않는다.

### 5-5. 독립 비평 게이트

- 1차 `/root/replay_plan_critique`: P0 0 / P1 1, FAIL. §5-3은 launcher 인계만 기록하고 §5-4의 실제 Release acceptance block에 freshness check가 없어 강제되지 않는다는 지적을 수용했다.
- 수용 — 두 generator `--check`와 exit-code gate를 named-field parity 및 Release build 전에 §5-4에 직접 추가했다.
- 수정본의 residual P0/P1 0을 짧게 재확인한 뒤 cook/build를 시작한다.
- 최종 `/root/replay_plan_critique`: residual P0 0 / P1 0, PASS. 두 freshness check가 Release acceptance에 직접 강제됨을 확인했다.
