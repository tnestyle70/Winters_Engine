Session - Viego/Sylas steal 구현을 위한 서버 오버라이드 식별자 파이프라인을 먼저 고정한다.

1. 반영해야 하는 코드

이번 세션은 실제 비에고/사일러스 스킬 로직보다 앞에 들어가는 공통 기반이다. 성공 기준은 서버 GameSim에서 원본 챔피언, 비주얼 챔피언, 스킬 훅 챔피언, 쿨다운 슬롯, 기본공격 챔피언을 서로 다른 값으로 해석할 수 있는 것이다.

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/FormOverrideComponent.h

기존 코드:

```cpp
struct FormOverrideComponent
{
	eChampion visualChampion = eChampion::END;
	eChampion skillChampion = eChampion::END;
	u8_t skillSlotMask = 0x0Fu;
	f32_t fRemainingSec = 0.f;
	bool_t bActive = false;
};
```

아래로 교체:

```cpp
struct FormOverrideComponent
{
	eChampion baseChampion = eChampion::END;
	eChampion visualChampion = eChampion::END;
	eChampion skillChampion = eChampion::END;
	u8_t skillSlotMask = 0x0Fu;
	f32_t fRemainingSec = 0.f;
	bool_t bActive = false;
};
```

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/SpellbookOverrideComponent.h

기존 코드:

```cpp
struct SpellbookOverrideComponent
{
	eChampion sourceChampion = eChampion::END;
	u8_t sourceSlot = 4;
	u8_t localSlot = 4;
	f32_t fRemainingSec = 0.f;
	bool_t bActive = false;
};
```

아래로 교체:

```cpp
struct SpellbookOverrideComponent
{
	eChampion sourceChampion = eChampion::END;
	u8_t sourceSlot = 4;
	u8_t localSlot = 4;
	u8_t sourceRank = 0;
	f32_t fRemainingSec = 0.f;
	bool_t bActive = false;
};
```

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/SpellbookFormOverride/SpellbookFormOverrideSystem.h

기존 코드:

```cpp
struct SkillOverrideResolveResult
{
	eChampion baseChampion = eChampion::NONE;
	eChampion hookChampion = eChampion::NONE;
	eChampion cooldownChampion = eChampion::NONE;
	u8_t localSlot = 0;
	u8_t hookSlot = 0;
	u8_t cooldownSlot = 0;
	bool_t bOverridden = false;
	bool_t bConsumeSpellbookOnAccept = false;
};
```

아래로 교체:

```cpp
struct SkillOverrideResolveResult
{
	eChampion baseChampion = eChampion::NONE;
	eChampion visualChampion = eChampion::NONE;
	eChampion hookChampion = eChampion::NONE;
	eChampion cooldownChampion = eChampion::NONE;
	u8_t localSlot = 0;
	u8_t hookSlot = 0;
	u8_t cooldownSlot = 0;
	u8_t sourceRank = 0;
	bool_t bOverridden = false;
	bool_t bFormOverride = false;
	bool_t bSpellbookOverride = false;
	bool_t bConsumeSpellbookOnAccept = false;
};
```

기존 코드:

```cpp
	static SkillOverrideResolveResult ResolveSkill(
		CWorld& world,
		EntityID caster,
		eChampion baseChampion,
		u8_t localSlot);

	static void ConsumeSpellbookOverride(
		CWorld& world,
		EntityID caster,
		u8_t localSlot);
```

아래로 교체:

```cpp
	static SkillOverrideResolveResult ResolveSkill(
		CWorld& world,
		EntityID caster,
		eChampion baseChampion,
		u8_t localSlot);

	static SkillOverrideResolveResult ResolveBasicAttack(
		CWorld& world,
		EntityID caster,
		eChampion baseChampion);

	static eChampion ResolveVisualChampion(
		CWorld& world,
		EntityID caster,
		eChampion baseChampion);

	static void ConsumeSpellbookOverride(
		CWorld& world,
		EntityID caster,
		u8_t localSlot);
```

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/SpellbookFormOverride/SpellbookFormOverrideSystem.cpp

기존 코드:

```cpp
	result.baseChampion = baseChampion;
	result.hookChampion = baseChampion;
	result.cooldownChampion = baseChampion;
	result.localSlot = localSlot;
	result.hookSlot = localSlot;
	result.cooldownSlot = localSlot;
```

아래로 교체:

```cpp
	result.baseChampion = baseChampion;
	result.visualChampion = CSpellbookFormOverrideSystem::ResolveVisualChampion(
		world,
		caster,
		baseChampion);
	result.hookChampion = baseChampion;
	result.cooldownChampion = baseChampion;
	result.localSlot = localSlot;
	result.hookSlot = localSlot;
	result.cooldownSlot = localSlot;
```

기존 코드:

```cpp
			result.hookChampion = form.skillChampion;
			result.cooldownChampion = form.skillChampion;
			result.bOverridden = true;
```

아래로 교체:

```cpp
			result.hookChampion = form.skillChampion;
			result.cooldownChampion = form.skillChampion;
			result.bOverridden = true;
			result.bFormOverride = true;
```

기존 코드:

```cpp
			result.hookChampion = spellbook.sourceChampion;
			result.hookSlot = spellbook.sourceSlot;
			result.cooldownChampion = baseChampion;
			result.cooldownSlot = localSlot;
			result.bOverridden = true;
			result.bConsumeSpellbookOnAccept = true;
```

아래로 교체:

```cpp
			result.hookChampion = spellbook.sourceChampion;
			result.hookSlot = spellbook.sourceSlot;
			result.cooldownChampion = baseChampion;
			result.cooldownSlot = localSlot;
			result.sourceRank = spellbook.sourceRank;
			result.bOverridden = true;
			result.bSpellbookOverride = true;
			result.bConsumeSpellbookOnAccept = true;
```

기존 코드:

```cpp
	return result;
}

void CSpellbookFormOverrideSystem::ConsumeSpellbookOverride(
```

아래로 교체:

```cpp
	return result;
}

SkillOverrideResolveResult CSpellbookFormOverrideSystem::ResolveBasicAttack(
	CWorld& world,
	EntityID caster,
	eChampion baseChampion)
{
	return ResolveSkill(
		world,
		caster,
		baseChampion,
		static_cast<u8_t>(eSkillSlot::BasicAttack));
}

eChampion CSpellbookFormOverrideSystem::ResolveVisualChampion(
	CWorld& world,
	EntityID caster,
	eChampion baseChampion)
{
	if (world.HasComponent<FormOverrideComponent>(caster))
	{
		const auto& form = world.GetComponent<FormOverrideComponent>(caster);
		if (form.bActive && IsValidChampion(form.visualChampion))
			return form.visualChampion;
	}

	return baseChampion;
}

void CSpellbookFormOverrideSystem::ConsumeSpellbookOverride(
```

1-5. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/CombatActionComponent.h

기존 코드:

```cpp
#include "ECS/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"
```

아래로 교체:

```cpp
#include "ECS/Entity.h"
#include "GameContext.h"
#include "WintersMath.h"
#include "WintersTypes.h"
```

기존 코드:

```cpp
    eCombatActionKind eKind = eCombatActionKind::None;
    eCombatActionMovePolicy eMovePolicy = eCombatActionMovePolicy::None;
    u8_t uSlot = 0;
```

아래로 교체:

```cpp
    eCombatActionKind eKind = eCombatActionKind::None;
    eCombatActionMovePolicy eMovePolicy = eCombatActionMovePolicy::None;
    eChampion eSourceChampion = eChampion::NONE;
    u8_t uSlot = 0;
```

1-6. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

기존 코드:

```cpp
    const eChampion champion = ResolveChampion(world, cmd.issuerEntity);

    ClearAttackChase(world, cmd.issuerEntity);
    ClearMoveTarget(world, cmd.issuerEntity);
```

아래로 교체:

```cpp
    const eChampion baseChampion = ResolveChampion(world, cmd.issuerEntity);
    const SkillOverrideResolveResult attackIdentity =
        CSpellbookFormOverrideSystem::ResolveBasicAttack(
            world,
            cmd.issuerEntity,
            baseChampion);
    const eChampion champion = attackIdentity.hookChampion;

    ClearAttackChase(world, cmd.issuerEntity);
    ClearMoveTarget(world, cmd.issuerEntity);
```

기존 코드:

```cpp
    action.eKind = eCombatActionKind::BasicAttack;
    action.eMovePolicy = ResolveBasicAttackMovePolicy(champion);
    action.uSlot = static_cast<u8_t>(eSkillSlot::BasicAttack);
```

아래로 교체:

```cpp
    action.eKind = eCombatActionKind::BasicAttack;
    action.eMovePolicy = ResolveBasicAttackMovePolicy(champion);
    action.eSourceChampion = champion;
    action.uSlot = static_cast<u8_t>(eSkillSlot::BasicAttack);
```

1-7. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/Combat/CombatActionSystem.cpp

기존 코드:

```cpp
    f32_t ResolveBasicAttackDamage(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        EntityID target,
        eTeam sourceTeam,
        u16_t actionFlags)
```

아래로 교체:

```cpp
    f32_t ResolveBasicAttackDamage(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        EntityID target,
        eTeam sourceTeam,
        eChampion champion,
        u16_t actionFlags)
```

기존 코드:

```cpp
        const eChampion champion = ResolveChampion(world, source);
        if (champion == eChampion::FIORA)
```

아래로 교체:

```cpp
        if (champion == eChampion::NONE || champion == eChampion::END)
            champion = ResolveChampion(world, source);
        if (champion == eChampion::FIORA)
```

기존 코드:

```cpp
    u32_t BuildBasicAttackEffectId(CWorld& world, EntityID entity)
    {
        const eChampion champion = ResolveChampion(world, entity);
        const u8_t slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
```

아래로 교체:

```cpp
    u32_t BuildBasicAttackEffectId(CWorld& world, EntityID entity, eChampion champion)
    {
        if (champion == eChampion::NONE || champion == eChampion::END)
            champion = ResolveChampion(world, entity);
        const u8_t slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
```

기존 코드:

```cpp
        const f32_t damage =
            ResolveBasicAttackDamage(world, tc, source, target, sourceTeam, action.uFlags);
```

아래로 교체:

```cpp
        const eChampion actionChampion = action.eSourceChampion;
        const f32_t damage = ResolveBasicAttackDamage(
            world,
            tc,
            source,
            target,
            sourceTeam,
            actionChampion,
            action.uFlags);
```

기존 코드:

```cpp
        effectEvent.effectId = BuildBasicAttackEffectId(world, source);
```

아래로 교체:

```cpp
        effectEvent.effectId = BuildBasicAttackEffectId(world, source, actionChampion);
```

2. 검증

`Shared/GameSim/Systems/SpellbookFormOverride/SpellbookFormOverrideSystem.cpp`에서 `ResolveSkill`, `ResolveBasicAttack`, `ResolveVisualChampion`이 모두 빌드되는지 확인한다.

비에고가 폼 오버라이드 중일 때 기본공격을 누르면 `CombatActionComponent.eSourceChampion`이 훔친 챔피언으로 저장되고, R 슬롯은 이 세션에서 아직 바뀌지 않아야 한다.

`git diff --check`를 실행한다.

Server와 Client를 빌드한다. Engine public header는 이번 세션에서 변경하지 않으므로 `UpdateLib.bat`는 필요 없다.
