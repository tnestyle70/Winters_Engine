Session - BotLaneAI瑜?ChampionAI濡??대쫫 ?뺣━?섍퀬 Session 1 ?쇱씤 梨뷀뵾??AI濡??⑥닚?뷀븳??

1. 諛섏쁺?댁빞 ?섎뒗 肄붾뱶

?꾩젣:
- ?대쾲 ?몄뀡?먯꽌 諛붽씀???대쫫? AI 紐⑤뱢??`BotLaneAI*` 怨꾩뿴?대떎.
- 濡쒕퉬/?ㅽ궎留덉쓽 `botLane`, `SetBotLane`, `GetBotLaneLabel`? "遊뉗씠 ?대뒓 ?쇱씤??媛덉?"瑜??삵븯???꾨줈?좎퐳/濡쒖뒪???대쫫?대?濡??대쾲 Session 1?먯꽌???좎??쒕떎. ???곸뿭源뚯? 諛붽씀?ㅻ㈃ FlatBuffers schema/generated, lobby command, UI源뚯? 嫄대뱶由щ뒗 蹂꾨룄 ?몄뀡?쇰줈 遺꾨━?쒕떎.
- AI??怨꾩냽 `GameCommand` ?앹궛?먮떎. ?꾩튂, 泥대젰, 荑⑦??? ?쇳빐?? ?ㅼ젣 怨듦꺽 ?먯젙? `CommandExecutor`? GameSim ?쒖뒪?쒖씠 泥섎━?쒕떎.
- Session 1 紐⑺몴??蹂듭옟???좏떥由ы떚/BT ?뺤옣???꾨땲???ㅼ쓬 ?먮쫫?대떎.
  - ?ㅽ룿 ??吏???쇱씤??理쒖쇅媛??꾧뎔 ?ы깙 ???덉쟾 吏?먯쑝濡??대룞?쒕떎.
  - ?꾧뎔 誘몃땲???⑥씠釉뚭? ?쇱젙 嫄곕━ ?덉뿉 ?ㅼ뼱?ㅻ㈃ `LaneCombat`?쇰줈 ?꾪솚?쒕떎.
  - `LaneCombat`? 寃곗젙 ?⑥닔 ?섎굹?먯꽌 ?꾪눜, ?ы깙 怨듦꺽, 誘몃땲??怨듦꺽, 10% 梨뷀뵾??寃ъ젣瑜?怨좊Ⅸ??
  - 梨뷀뵾??寃ъ젣 ?ㅽ궗 ?곗꽑?쒖쐞????뒪 Q, ?쇱삤??Q, ?좎돩 W, 留덉뒪?곗씠 Q??
  - ??梨뷀뵾?몄씠 ?녾퀬 ?꾧뎔 誘몃땲?몄씠 ???ы깙 ?ш굅由??덉뿉 ?덉쑝硫??ы깙??吏꾩엯??援ъ“臾쇱쓣 怨듦꺽?쒕떎.
  - ???ы깙 洹쇱쿂?먯꽌 ??梨뷀뵾?몄씠 媛먯??섎㈃ ?꾪눜?쒕떎.

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/BotLaneAIComponent.h -> C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h

?뚯씪紐?蹂寃?
```text
Shared/GameSim/Components/BotLaneAIComponent.h
-> Shared/GameSim/Components/ChampionAIComponent.h
```

湲곗〈 肄붾뱶:
```cpp
enum class eBotLaneAIState : u8_t
{
	MoveToLane,
	FollowWave,
	FarmMinion,
	FightChampion,
	Kite,
	AttackStructure,
	Retreat,
	Dead,
};
```

?꾨옒濡?援먯껜:
```cpp
enum class eChampionAIState : u8_t
{
	MoveToOuterTurret,
	WaitForWave,
	LaneCombat,
	Retreat,
	Dead,
};
```

湲곗〈 肄붾뱶:
```cpp
enum class eBotLaneAIIntent : u8_t
{
	MoveToLane,
	FollowWave,
	FarmMinion,
	FightChampion,
	Kite,
	AttackStructure,
	Retreat,
};
```

?꾨옒濡?援먯껜:
```cpp
enum class eChampionAIAction : u8_t
{
	MoveToSafeAnchor,
	FollowWave,
	AttackMinion,
	AttackChampion,
	AttackStructure,
	Retreat,
};
```

湲곗〈 肄붾뱶:
```cpp
inline constexpr u32_t kBotLaneAIDebugPresentFlag = 1u << 7;
inline constexpr u32_t kBotLaneAIStateShift = 8u;
inline constexpr u32_t kBotLaneAIStateMask = 0xFu << kBotLaneAIStateShift;
```

?꾨옒濡?援먯껜:
```cpp
inline constexpr u32_t kChampionAIDebugPresentFlag = 1u << 7;
inline constexpr u32_t kChampionAIStateShift = 8u;
inline constexpr u32_t kChampionAIStateMask = 0xFu << kChampionAIStateShift;
```

湲곗〈 肄붾뱶:
```cpp
struct BotLaneAIComponent
{
	eChampion champion = eChampion::NONE;
	eTeam team = eTeam::Blue;
	u8_t difficulty = 1;
	u8_t lane = 1;

	eBotLaneAIState state = eBotLaneAIState::MoveToLane;
	Vec3 laneGoal{ 0.f, 1.f, 0.f };

	EntityID lockedChampion = NULL_ENTITY;
	EntityID targetMinion = NULL_ENTITY;
	EntityID targetStructure = NULL_ENTITY;

	f32_t decisionTimer = 0.f;
	f32_t decisionInterval = 0.15f;
	f32_t championScanRange = 9.f;
	f32_t minionScanRange = 12.f;
	f32_t structureScanRange = 18.f;
	f32_t leashRange = 14.f;
	u32_t nextCommandSequence = 1;

	f32_t scoreFight = 0.f;
	f32_t scoreKite = 0.f;
	f32_t scoreFarm = 0.f;
	f32_t scoreSiege = 0.f;
	f32_t scoreRetreat = 0.f;

	bool_t bLastHitReady = false;
	bool_t bSiegeSafe = false;
	EntityID lastHitTarget = NULL_ENTITY;

	eBotLaneAIIntent intent = eBotLaneAIIntent::MoveToLane;
	eBotLaneAIIntent lastIntent = eBotLaneAIIntent::MoveToLane;

	f32_t fIntentHoldTimer = 0.f;
	f32_t fRetreatTimer = 0.f;
	f32_t fRetreatUntilHpRatio = 0.55f;

	EntityID threatChampion = NULL_ENTITY;
	Vec3 vRetreatGoal{ 0.f, 1.f, 0.f };
};
```

?꾨옒濡?援먯껜:
```cpp
struct ChampionAIComponent
{
	eChampion champion = eChampion::NONE;
	eTeam team = eTeam::Blue;
	u8_t difficulty = 1;
	u8_t lane = 1;

	eChampionAIState state = eChampionAIState::MoveToOuterTurret;
	eChampionAIAction lastAction = eChampionAIAction::MoveToSafeAnchor;

	Vec3 laneGoal{ 0.f, 1.f, 0.f };
	Vec3 safeAnchor{ 0.f, 1.f, 0.f };
	Vec3 retreatGoal{ 0.f, 1.f, 0.f };

	EntityID lockedChampion = NULL_ENTITY;
	EntityID targetMinion = NULL_ENTITY;
	EntityID targetStructure = NULL_ENTITY;
	EntityID alliedWave = NULL_ENTITY;

	f32_t decisionTimer = 0.f;
	f32_t decisionInterval = 0.20f;
	f32_t championScanRange = 9.f;
	f32_t minionScanRange = 12.f;
	f32_t structureScanRange = 18.f;
	f32_t waveJoinRange = 8.f;
	f32_t leashRange = 14.f;
	f32_t attackChampionChance = 0.10f;
	f32_t retreatHpRatio = 0.35f;
	f32_t reengageHpRatio = 0.55f;
	u32_t nextCommandSequence = 1;

	bool_t bWaveJoined = false;
	bool_t bStructureWaveTanking = false;
	bool_t bInsideEnemyTurretDanger = false;
};
```

湲곗〈 肄붾뱶:
```cpp
struct BotLaneAIDebugComponent
{
	bool_t bPresent = false;
	eBotLaneAIState state = eBotLaneAIState::MoveToLane;
	u32_t targetNetId = 0;
	f32_t moveSpeed = 0.f;
	Vec3 snapshotPos{ 0.f, 0.f, 0.f };
};
```

?꾨옒濡?援먯껜:
```cpp
struct ChampionAIDebugComponent
{
	bool_t bPresent = false;
	eChampionAIState state = eChampionAIState::MoveToOuterTurret;
	u32_t targetNetId = 0;
	f32_t moveSpeed = 0.f;
	Vec3 snapshotPos{ 0.f, 0.f, 0.f };
};
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/BotLaneAISystem.h -> C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAISystem.h

?뚯씪紐?蹂寃?
```text
Shared/GameSim/Systems/BotLaneAISystem.h
-> Shared/GameSim/Systems/ChampionAISystem.h
```

湲곗〈 肄붾뱶:
```cpp
#pragma once

#include "Shared/GameSim/Systems/ICommandExecutor.h"
#include <vector>

class CWorld;

class CBotLaneAISystem final
{
public:
	static void Execute(CWorld& world,
		const TickContext& tc, std::vector<GameCommand>& outCommands);

private:
    CBotLaneAISystem() = delete;
};
```

?꾨옒濡?援먯껜:
```cpp
#pragma once

#include "Shared/GameSim/Systems/ICommandExecutor.h"

#include <vector>

class CWorld;

class CChampionAISystem final
{
public:
	static void Execute(CWorld& world,
		const TickContext& tc,
		std::vector<GameCommand>& outCommands);

private:
	CChampionAISystem() = delete;
};
```

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/BotLaneAIPolicy.h -> C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAIPolicy.h

?뚯씪紐?蹂寃?
```text
Shared/GameSim/Systems/BotLaneAIPolicy.h
-> Shared/GameSim/Systems/ChampionAIPolicy.h
```

湲곗〈 肄붾뱶:
```cpp
struct BotSkillRule
{
    u8_t slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
    f32_t minRange = 0.f;
    f32_t score = 0.f;
};

struct BotChampionProfile
{
    eChampion champion = eChampion::END;
    f32_t preferredRange = 1.5f;
    f32_t championScanRange = 6.f;
    f32_t minionScanRange = 10.f;
    f32_t structureScanRange = 18.f;
    f32_t leashRange = 14.f;
    f32_t aggression = 1.f;
    f32_t kiteBias = 0.f;
    f32_t retreatHpRatio = 0.35f;
    f32_t reengageHpRatio = 0.55f;
    f32_t minionPressureWeight = 1.f;
    f32_t turretRiskWeight = 1.f;
    f32_t lastHitWeight = 1.f;
    f32_t siegeWeight = 1.f;
    BotSkillRule skillRules[4]{};
    u8_t skillRuleCount = 0;
};

const BotChampionProfile& GetBotChampionProfile(eChampion champion);
```

?꾨옒濡?援먯껜:
```cpp
struct ChampionAISkillRule
{
	u8_t slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
	f32_t minRange = 0.f;
	f32_t score = 0.f;
};

struct ChampionAIProfile
{
	eChampion champion = eChampion::END;
	f32_t preferredRange = 1.5f;
	f32_t championScanRange = 6.f;
	f32_t minionScanRange = 10.f;
	f32_t structureScanRange = 18.f;
	f32_t leashRange = 14.f;
	f32_t aggression = 1.f;
	f32_t kiteBias = 0.f;
	f32_t retreatHpRatio = 0.35f;
	f32_t reengageHpRatio = 0.55f;
	f32_t minionPressureWeight = 1.f;
	f32_t turretRiskWeight = 1.f;
	f32_t lastHitWeight = 1.f;
	f32_t siegeWeight = 1.f;
	ChampionAISkillRule skillRules[4]{};
	u8_t skillRuleCount = 0;
};

const ChampionAIProfile& GetChampionAIProfile(eChampion champion);
```

1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/BotLaneAIPolicy.cpp -> C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAIPolicy.cpp

?뚯씪紐?蹂寃?
```text
Shared/GameSim/Systems/BotLaneAIPolicy.cpp
-> Shared/GameSim/Systems/ChampionAIPolicy.cpp
```

湲곗〈 肄붾뱶:
```cpp
#include "Shared/GameSim/Systems/BotLaneAIPolicy.h"
```

?꾨옒濡?援먯껜:
```cpp
#include "Shared/GameSim/Systems/ChampionAIPolicy.h"
```

湲곗〈 肄붾뱶:
```cpp
constexpr BotChampionProfile MakeDefaultProfile()
```

?꾨옒濡?援먯껜:
```cpp
constexpr ChampionAIProfile MakeDefaultProfile()
```

湲곗〈 肄붾뱶:
```cpp
const BotChampionProfile& GetBotChampionProfile(eChampion champion)
```

?꾨옒濡?援먯껜:
```cpp
const ChampionAIProfile& GetChampionAIProfile(eChampion champion)
```

異붽? ?곸슜:
```text
BotSkillRule -> ChampionAISkillRule
BotChampionProfile -> ChampionAIProfile
GetBotChampionProfile -> GetChampionAIProfile
```

Session 1?먯꽌???뺤콉 cpp??梨뷀뵾???꾨줈???꾨뱶 ?쒖꽌瑜??좎??섍퀬 ?대쫫留?援먯껜?쒕떎. ?댁쑀??initializer ????섏젙 ?놁씠 而댄뙆???꾪뿕??以꾩씠湲??꾪빐?쒕떎. 10% 寃ъ젣 ?뺣쪧怨??⑥씠釉??⑸쪟 嫄곕━??`ChampionAIComponent` 湲곕낯媛믪쓣 ?ъ슜?쒕떎.

1-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/BotLaneAISystem.cpp -> C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAISystem.cpp

?뚯씪紐?蹂寃?
```text
Shared/GameSim/Systems/BotLaneAISystem.cpp
-> Shared/GameSim/Systems/ChampionAISystem.cpp
```

湲곗〈 肄붾뱶:
```cpp
#include "Shared/GameSim/Systems/BotLaneAISystem.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/BotLaneAIComponent.h"
```

?꾨옒濡?援먯껜:
```cpp
#include "Shared/GameSim/Systems/ChampionAISystem.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
```

湲곗〈 肄붾뱶:
```cpp
#include "Shared/GameSim/Systems/BotLaneAIPolicy.h"
```

?꾨옒濡?援먯껜:
```cpp
#include "Shared/GameSim/Systems/ChampionAIPolicy.h"
```

?꾩껜 援ъ“ 援먯껜 諛⑺뼢:
```text
湲곗〈 `BuildUtilityScores`, `ChooseBestIntent`, `TryEmit*FightBT`, `EmitCommandForIntent` 以묒떖 援ъ“瑜??쒓굅?쒕떎.
Session 1??`ChampionAISystem.cpp`???꾨옒 ?⑥닔 臾띠쓬留??④릿??
- BuildChampionAIContext
- ExecuteMoveToOuterTurret
- ExecuteWaitForWave
- ExecuteLaneCombat
- EmitMoveCommand / EmitBasicAttackCommand / EmitSkillCommand
- TryEmitAttackChampion
```

湲곗〈 肄붾뱶:
```cpp
void CBotLaneAISystem::Execute(CWorld& world, const TickContext& tc, std::vector<GameCommand>& outCommands)
{
    world.ForEach<BotLaneAIComponent, ChampionComponent, TransformComponent>(
        [&](EntityID self, BotLaneAIComponent& ai, ChampionComponent& champion, TransformComponent& selfTf)
```

?꾨옒濡?援먯껜:
```cpp
void CChampionAISystem::Execute(CWorld& world, const TickContext& tc, std::vector<GameCommand>& outCommands)
{
	world.ForEach<ChampionAIComponent, ChampionComponent, TransformComponent>(
		[&](EntityID self, ChampionAIComponent& ai, ChampionComponent& champion, TransformComponent& selfTf)
```

湲곗〈 肄붾뱶:
```cpp
ai.state = eBotLaneAIState::Dead;
ai.intent = eBotLaneAIIntent::Retreat;
```

?꾨옒濡?援먯껜:
```cpp
ai.state = eChampionAIState::Dead;
ai.lastAction = eChampionAIAction::Retreat;
```

援ы쁽 ??`Execute` 蹂몃Ц? ?꾨옒 ?섎?濡??⑥닚?뷀븳??
```cpp
ai.decisionTimer -= tc.fDt;
if (ai.decisionTimer > 0.f)
	return;
ai.decisionTimer = ai.decisionInterval;

const ChampionAIProfile& profile = GetChampionAIProfile(champion.id);
ai.champion = champion.id;
ai.team = champion.team;
ai.championScanRange = profile.championScanRange;
ai.minionScanRange = profile.minionScanRange;
ai.structureScanRange = profile.structureScanRange;
ai.leashRange = profile.leashRange;
ai.retreatHpRatio = profile.retreatHpRatio;
ai.reengageHpRatio = profile.reengageHpRatio;

const ChampionAIContext ctx = BuildChampionAIContext(world, self, ai, champion, selfTf.GetPosition());

switch (ai.state)
{
case eChampionAIState::MoveToOuterTurret:
	ExecuteMoveToOuterTurret(world, tc, self, ai, champion, selfTf.GetPosition(), ctx, outCommands);
	break;
case eChampionAIState::WaitForWave:
	ExecuteWaitForWave(world, tc, self, ai, champion, selfTf.GetPosition(), ctx, outCommands);
	break;
case eChampionAIState::Retreat:
case eChampionAIState::LaneCombat:
default:
	ExecuteLaneCombat(world, tc, self, ai, champion, selfTf.GetPosition(), ctx, profile, outCommands);
	break;
}
```

LaneCombat 寃곗젙 ?⑥닔???곗꽑?쒖쐞:
```cpp
// 1. ??? 泥대젰 ?먮뒗 ???ы깙 ?꾪뿕 + ??梨뷀뵾??媛먯?硫??꾪눜
// 2. ??梨뷀뵾?몄씠 ?녾퀬 ?꾧뎔 誘몃땲?몄씠 ???ы깙??留욎븘二쇰㈃ 援ъ“臾?怨듦꺽
// 3. ??梨뷀뵾?몄씠 ?덇퀬 10% 寃곗젙 濡ㅼ씠 ?깃났?섎㈃ 梨뷀뵾??寃ъ젣
// 4. ??誘몃땲?몄씠 ?덉쑝硫?誘몃땲??怨듦꺽
// 5. ?꾧뎔 ?⑥씠釉뚭? ?덉쑝硫??⑥씠釉?異붿쥌
// 6. ?놁쑝硫?laneGoal濡??대룞
```

梨뷀뵾??寃ъ젣 ?ㅽ궗 遺꾧린:
```cpp
u8_t ResolveAttackChampionSlot(eChampion champion)
{
	switch (champion)
	{
	case eChampion::JAX:
	case eChampion::FIORA:
	case eChampion::MASTERYI:
		return static_cast<u8_t>(eSkillSlot::Q);
	case eChampion::ASHE:
		return static_cast<u8_t>(eSkillSlot::W);
	default:
		return static_cast<u8_t>(eSkillSlot::BasicAttack);
	}
}
```

湲덉?:
```text
ChampionAISystem.cpp ?덉뿉??TransformComponent, HealthComponent, SkillStateComponent, DamageRequest瑜?吏곸젒 ?깃났 寃곌낵濡?蹂寃쏀븯吏 ?딅뒗??
?덉슜?섎뒗 蹂寃쎌? ChampionAIComponent???곹깭/?源???대㉧ 媛깆떊怨?GameCommand push肉먯씠??
```

1-6. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

湲곗〈 肄붾뱶:
```cpp
#include "Shared/GameSim/Components/BotLaneAIComponent.h"
```

?꾨옒濡?援먯껜:
```cpp
#include "Shared/GameSim/Components/ChampionAIComponent.h"
```

湲곗〈 肄붾뱶:
```cpp
#include "Shared/GameSim/Systems/BotLaneAIPolicy.h"
#include "Shared/GameSim/Systems/BotLaneAISystem.h"
```

?꾨옒濡?援먯껜:
```cpp
#include "Shared/GameSim/Systems/ChampionAIPolicy.h"
#include "Shared/GameSim/Systems/ChampionAISystem.h"
```

湲곗〈 肄붾뱶:
```cpp
constexpr f32_t kBotInitialDecisionDelaySec = 0.35f;
constexpr f32_t kBotLaneWaitBehindTurret = 3.f;
```

?꾨옒濡?援먯껜:
```cpp
constexpr f32_t kChampionAIInitialDecisionDelaySec = 0.35f;
constexpr f32_t kChampionAISafeAnchorBehindTurret = 3.f;
```

湲곗〈 肄붾뱶:
```cpp
CBotLaneAISystem::Execute(m_world, tc, m_pendingExecCommands);
```

?꾨옒濡?援먯껜:
```cpp
CChampionAISystem::Execute(m_world, tc, m_pendingExecCommands);
```

湲곗〈 肄붾뱶:
```cpp
if (m_world.HasComponent<BotLaneAIComponent>(entity))
{
    auto& ai = m_world.GetComponent<BotLaneAIComponent>(entity);
    ai.state = eBotLaneAIState::Dead;
    ai.intent = eBotLaneAIIntent::Retreat;
    ai.lastIntent = eBotLaneAIIntent::Retreat;
    ai.lockedChampion = NULL_ENTITY;
    ai.targetMinion = NULL_ENTITY;
    ai.targetStructure = NULL_ENTITY;
    ai.threatChampion = NULL_ENTITY;
    ai.fIntentHoldTimer = 0.f;
    ai.fRetreatTimer = 0.f;
}
```

?꾨옒濡?援먯껜:
```cpp
if (m_world.HasComponent<ChampionAIComponent>(entity))
{
	auto& ai = m_world.GetComponent<ChampionAIComponent>(entity);
	ai.state = eChampionAIState::Dead;
	ai.lastAction = eChampionAIAction::Retreat;
	ai.lockedChampion = NULL_ENTITY;
	ai.targetMinion = NULL_ENTITY;
	ai.targetStructure = NULL_ENTITY;
	ai.alliedWave = NULL_ENTITY;
}
```

湲곗〈 肄붾뱶:
```cpp
if (m_world.HasComponent<BotLaneAIComponent>(entity))
{
    auto& ai = m_world.GetComponent<BotLaneAIComponent>(entity);
    ai.state = eBotLaneAIState::MoveToLane;
    ai.intent = eBotLaneAIIntent::MoveToLane;
    ai.lastIntent = eBotLaneAIIntent::MoveToLane;
    ai.lockedChampion = NULL_ENTITY;
    ai.targetMinion = NULL_ENTITY;
    ai.targetStructure = NULL_ENTITY;
    ai.threatChampion = NULL_ENTITY;
    ai.fIntentHoldTimer = 0.f;
    ai.fRetreatTimer = 0.f;
    ai.decisionTimer = 0.25f;
}
```

?꾨옒濡?援먯껜:
```cpp
if (m_world.HasComponent<ChampionAIComponent>(entity))
{
	auto& ai = m_world.GetComponent<ChampionAIComponent>(entity);
	ai.state = eChampionAIState::MoveToOuterTurret;
	ai.lastAction = eChampionAIAction::MoveToSafeAnchor;
	ai.lockedChampion = NULL_ENTITY;
	ai.targetMinion = NULL_ENTITY;
	ai.targetStructure = NULL_ENTITY;
	ai.alliedWave = NULL_ENTITY;
	ai.bWaveJoined = false;
	ai.bStructureWaveTanking = false;
	ai.bInsideEnemyTurretDanger = false;
	ai.decisionTimer = 0.25f;
}
```

湲곗〈 肄붾뱶:
```cpp
RefreshBotLaneWaitGoals();
```

?꾨옒濡?援먯껜:
```cpp
RefreshChampionAIGoals();
```

湲곗〈 肄붾뱶:
```cpp
Vec3 CGameRoom::ResolveBotLaneAdvanceGoal(eTeam team, u8_t lane) const
```

?꾨옒濡?援먯껜:
```cpp
Vec3 CGameRoom::ResolveChampionAILaneGoal(eTeam team, u8_t lane) const
```

湲곗〈 肄붾뱶:
```cpp
Vec3 CGameRoom::ResolveBotLaneWaitGoal(eTeam team, u8_t lane)
```

?꾨옒濡?援먯껜:
```cpp
Vec3 CGameRoom::ResolveChampionAISafeAnchor(eTeam team, u8_t lane)
```

湲곗〈 肄붾뱶:
```cpp
best.x - laneDir.x * kBotLaneWaitBehindTurret,
```

?꾨옒濡?援먯껜:
```cpp
best.x - laneDir.x * kChampionAISafeAnchorBehindTurret,
```

湲곗〈 肄붾뱶:
```cpp
best.z - laneDir.z * kBotLaneWaitBehindTurret
```

?꾨옒濡?援먯껜:
```cpp
best.z - laneDir.z * kChampionAISafeAnchorBehindTurret
```

湲곗〈 肄붾뱶:
```cpp
best.x += (team == eTeam::Blue) ? -kBotLaneWaitBehindTurret : kBotLaneWaitBehindTurret;
```

?꾨옒濡?援먯껜:
```cpp
best.x += (team == eTeam::Blue) ? -kChampionAISafeAnchorBehindTurret : kChampionAISafeAnchorBehindTurret;
```

湲곗〈 肄붾뱶:
```cpp
void CGameRoom::RefreshBotLaneWaitGoals()
{
    m_world.ForEach<BotLaneAIComponent>(
        [this](EntityID, BotLaneAIComponent& ai)
        {
            const Vec3 waitGoal = ResolveBotLaneWaitGoal(ai.team, ai.lane);
            ai.laneGoal = ResolveBotLaneAdvanceGoal(ai.team, ai.lane);
```

?꾨옒濡?援먯껜:
```cpp
void CGameRoom::RefreshChampionAIGoals()
{
	m_world.ForEach<ChampionAIComponent>(
		[this](EntityID, ChampionAIComponent& ai)
		{
			ai.safeAnchor = ResolveChampionAISafeAnchor(ai.team, ai.lane);
			ai.retreatGoal = ai.safeAnchor;
			ai.laneGoal = ResolveChampionAILaneGoal(ai.team, ai.lane);
```

湲곗〈 肄붾뱶:
```cpp
"[BotAI] lane goal team=%u champ=%u lane=%u wpLane=%u advance=(%.2f,%.2f,%.2f) wait=(%.2f,%.2f,%.2f)\n",
```

?꾨옒濡?援먯껜:
```cpp
"[ChampionAI] lane goal team=%u champ=%u lane=%u wpLane=%u advance=(%.2f,%.2f,%.2f) safe=(%.2f,%.2f,%.2f)\n",
```

湲곗〈 肄붾뱶:
```cpp
waitGoal.x,
waitGoal.y,
waitGoal.z);
```

?꾨옒濡?援먯껜:
```cpp
ai.safeAnchor.x,
ai.safeAnchor.y,
ai.safeAnchor.z);
```

湲곗〈 肄붾뱶:
```cpp
BotLaneAIComponent ai{};
const BotChampionProfile& profile = GetBotChampionProfile(slot.champion);
```

?꾨옒濡?援먯껜:
```cpp
ChampionAIComponent ai{};
const ChampionAIProfile& profile = GetChampionAIProfile(slot.champion);
```

湲곗〈 肄붾뱶:
```cpp
ai.decisionTimer = kBotInitialDecisionDelaySec;
ai.vRetreatGoal = spawnPos;
```

?꾨옒濡?援먯껜:
```cpp
ai.decisionTimer = kChampionAIInitialDecisionDelaySec;
ai.retreatGoal = spawnPos;
```

湲곗〈 肄붾뱶:
```cpp
ai.fRetreatUntilHpRatio = profile.reengageHpRatio;
```

?꾨옒濡?援먯껜:
```cpp
ai.reengageHpRatio = profile.reengageHpRatio;
ai.retreatHpRatio = profile.retreatHpRatio;
```

湲곗〈 肄붾뱶:
```cpp
ai.laneGoal = waypointCount > 0u
    ? GetServerMinionWaypoint(ai.team, waypointLane, waypointCount / 2u)
    : GetGameSimLaneGatherPosition(ai.lane, slot.team);

m_world.AddComponent<BotLaneAIComponent>(entity, ai);
```

?꾨옒濡?援먯껜:
```cpp
ai.laneGoal = waypointCount > 0u
	? GetServerMinionWaypoint(ai.team, waypointLane, waypointCount / 2u)
	: GetGameSimLaneGatherPosition(ai.lane, slot.team);
ai.safeAnchor = ResolveChampionAISafeAnchor(ai.team, ai.lane);
ai.retreatGoal = ai.safeAnchor;

m_world.AddComponent<ChampionAIComponent>(entity, ai);
```

1-7. C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h

湲곗〈 肄붾뱶:
```cpp
Vec3 ResolveBotLaneAdvanceGoal(eTeam team, u8_t lane) const;
Vec3 ResolveBotLaneWaitGoal(eTeam team, u8_t lane);
void RefreshBotLaneWaitGoals();
```

?꾨옒濡?援먯껜:
```cpp
Vec3 ResolveChampionAILaneGoal(eTeam team, u8_t lane) const;
Vec3 ResolveChampionAISafeAnchor(eTeam team, u8_t lane);
void RefreshChampionAIGoals();
```

`ResolveInitialBotLane`, `TrySetBotLane`? ?대쾲 ?몄뀡?먯꽌 ?좎??쒕떎. ?댁쑀??AI 紐⑤뱢 ?대쫫???꾨땲??濡쒕퉬 ?쇱씤 ?좏깮 ?꾨줈?좎퐳?닿린 ?뚮Ц?대떎.

1-8. C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

湲곗〈 肄붾뱶:
```cpp
#include "Shared/GameSim/Components/BotLaneAIComponent.h"
```

?꾨옒濡?援먯껜:
```cpp
#include "Shared/GameSim/Components/ChampionAIComponent.h"
```

湲곗〈 肄붾뱶:
```cpp
if (world.HasComponent<BotLaneAIComponent>(entity))
{
    const auto& ai = world.GetComponent<BotLaneAIComponent>(entity);
    stateFlags |= kBotLaneAIDebugPresentFlag;
    stateFlags |= (static_cast<u32_t>(ai.state) << kBotLaneAIStateShift) & kBotLaneAIStateMask;
```

?꾨옒濡?援먯껜:
```cpp
if (world.HasComponent<ChampionAIComponent>(entity))
{
	const auto& ai = world.GetComponent<ChampionAIComponent>(entity);
	stateFlags |= kChampionAIDebugPresentFlag;
	stateFlags |= (static_cast<u32_t>(ai.state) << kChampionAIStateShift) & kChampionAIStateMask;
```

湲곗〈 肄붾뱶:
```cpp
EntityID target = ai.lockedChampion;
if (target == NULL_ENTITY)
    target = ai.targetMinion;
if (target == NULL_ENTITY)
    target = ai.targetStructure;
```

?꾨옒濡?援먯껜:
```cpp
EntityID target = ai.lockedChampion;
if (target == NULL_ENTITY)
	target = ai.targetMinion;
if (target == NULL_ENTITY)
	target = ai.targetStructure;
if (target == NULL_ENTITY)
	target = ai.alliedWave;
```

1-9. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

湲곗〈 肄붾뱶:
```cpp
#include "Shared/GameSim/Components/BotLaneAIComponent.h"
```

?꾨옒濡?援먯껜:
```cpp
#include "Shared/GameSim/Components/ChampionAIComponent.h"
```

湲곗〈 肄붾뱶:
```cpp
const bool_t bHasAIDebug = (es->stateFlags() & kBotLaneAIDebugPresentFlag) != 0u;
if (bHasAIDebug)
{
    if (!world.HasComponent<BotLaneAIDebugComponent>(e))
        world.AddComponent<BotLaneAIDebugComponent>(e, BotLaneAIDebugComponent{});

    auto& debug = world.GetComponent<BotLaneAIDebugComponent>(e);
    debug.bPresent = true;
    debug.state = static_cast<eBotLaneAIState>(
        (es->stateFlags() & kBotLaneAIStateMask) >> kBotLaneAIStateShift);
```

?꾨옒濡?援먯껜:
```cpp
const bool_t bHasAIDebug = (es->stateFlags() & kChampionAIDebugPresentFlag) != 0u;
if (bHasAIDebug)
{
	if (!world.HasComponent<ChampionAIDebugComponent>(e))
		world.AddComponent<ChampionAIDebugComponent>(e, ChampionAIDebugComponent{});

	auto& debug = world.GetComponent<ChampionAIDebugComponent>(e);
	debug.bPresent = true;
	debug.state = static_cast<eChampionAIState>(
		(es->stateFlags() & kChampionAIStateMask) >> kChampionAIStateShift);
```

湲곗〈 肄붾뱶:
```cpp
else if (world.HasComponent<BotLaneAIDebugComponent>(e))
{
    world.GetComponent<BotLaneAIDebugComponent>(e).bPresent = false;
}
```

?꾨옒濡?援먯껜:
```cpp
else if (world.HasComponent<ChampionAIDebugComponent>(e))
{
	world.GetComponent<ChampionAIDebugComponent>(e).bPresent = false;
}
```

1-10. C:/Users/user/Desktop/Winters/Client/Private/UI/AIDebugPanel.cpp

湲곗〈 肄붾뱶:
```cpp
#include "Shared/GameSim/Components/BotLaneAIComponent.h"
```

?꾨옒濡?援먯껜:
```cpp
#include "Shared/GameSim/Components/ChampionAIComponent.h"
```

湲곗〈 肄붾뱶:
```cpp
const char* BotLaneStateName(eBotLaneAIState state)
{
	switch (state)
	{
	case eBotLaneAIState::MoveToLane: return "MoveToLane";
	case eBotLaneAIState::FollowWave: return "FollowWave";
	case eBotLaneAIState::FarmMinion: return "FarmMinion";
	case eBotLaneAIState::FightChampion: return "FightChampion";
	case eBotLaneAIState::Kite: return "Kite";
	case eBotLaneAIState::AttackStructure: return "AttackStructure";
	case eBotLaneAIState::Retreat: return "Retreat";
	case eBotLaneAIState::Dead: return "Dead";
	default: return "Unknown";
	}
}
```

?꾨옒濡?援먯껜:
```cpp
const char* ChampionAIStateName(eChampionAIState state)
{
	switch (state)
	{
	case eChampionAIState::MoveToOuterTurret: return "MoveToOuterTurret";
	case eChampionAIState::WaitForWave: return "WaitForWave";
	case eChampionAIState::LaneCombat: return "LaneCombat";
	case eChampionAIState::Retreat: return "Retreat";
	case eChampionAIState::Dead: return "Dead";
	default: return "Unknown";
	}
}
```

湲곗〈 肄붾뱶:
```cpp
ImGui::TextUnformatted("BotLane AI");
```

?꾨옒濡?援먯껜:
```cpp
ImGui::TextUnformatted("Champion AI");
```

湲곗〈 肄붾뱶:
```cpp
if (!world.HasComponent<BotLaneAIDebugComponent>(entity))
    return;

auto& debug = world.GetComponent<BotLaneAIDebugComponent>(entity);
```

?꾨옒濡?援먯껜:
```cpp
if (!world.HasComponent<ChampionAIDebugComponent>(entity))
	return;

auto& debug = world.GetComponent<ChampionAIDebugComponent>(entity);
```

湲곗〈 肄붾뱶:
```cpp
ImGui::Text("State: %s", BotLaneStateName(debug.state));
```

?꾨옒濡?援먯껜:
```cpp
ImGui::Text("State: %s", ChampionAIStateName(debug.state));
```

湲곗〈 肄붾뱶:
```cpp
"No BotLane AI snapshot yet. If bots are visible, check server SnapshotBuilder debug flags.");
```

?꾨옒濡?援먯껜:
```cpp
"No Champion AI snapshot yet. If bots are visible, check server SnapshotBuilder debug flags.");
```

1-11. C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj

湲곗〈 肄붾뱶:
```xml
<ClCompile Include="..\..\Shared\GameSim\Systems\BotLaneAIPolicy.cpp" />
<ClCompile Include="..\..\Shared\GameSim\Systems\BotLaneAISystem.cpp" />
```

?꾨옒濡?援먯껜:
```xml
<ClCompile Include="..\..\Shared\GameSim\Systems\ChampionAIPolicy.cpp" />
<ClCompile Include="..\..\Shared\GameSim\Systems\ChampionAISystem.cpp" />
```

湲곗〈 肄붾뱶:
```xml
<ClInclude Include="..\..\Shared\GameSim\Components\BotLaneAIComponent.h" />
```

?꾨옒濡?援먯껜:
```xml
<ClInclude Include="..\..\Shared\GameSim\Components\ChampionAIComponent.h" />
```

湲곗〈 肄붾뱶:
```xml
<ClInclude Include="..\..\Shared\GameSim\Systems\BotLaneAIPolicy.h" />
<ClInclude Include="..\..\Shared\GameSim\Systems\BotLaneAISystem.h" />
```

?꾨옒濡?援먯껜:
```xml
<ClInclude Include="..\..\Shared\GameSim\Systems\ChampionAIPolicy.h" />
<ClInclude Include="..\..\Shared\GameSim\Systems\ChampionAISystem.h" />
```

1-12. C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj.filters

湲곗〈 肄붾뱶:
```xml
<ClCompile Include="..\..\Shared\GameSim\Systems\BotLaneAIPolicy.cpp">
  <Filter>04. Shared\GameSim\Systems</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Systems\BotLaneAISystem.cpp">
  <Filter>04. Shared\GameSim\Systems</Filter>
</ClCompile>
```

?꾨옒濡?援먯껜:
```xml
<ClCompile Include="..\..\Shared\GameSim\Systems\ChampionAIPolicy.cpp">
  <Filter>04. Shared\GameSim\Systems</Filter>
</ClCompile>
<ClCompile Include="..\..\Shared\GameSim\Systems\ChampionAISystem.cpp">
  <Filter>04. Shared\GameSim\Systems</Filter>
</ClCompile>
```

湲곗〈 肄붾뱶:
```xml
<ClInclude Include="..\..\Shared\GameSim\Components\BotLaneAIComponent.h">
  <Filter>04. Shared\GameSim\Components</Filter>
</ClInclude>
```

?꾨옒濡?援먯껜:
```xml
<ClInclude Include="..\..\Shared\GameSim\Components\ChampionAIComponent.h">
  <Filter>04. Shared\GameSim\Components</Filter>
</ClInclude>
```

湲곗〈 肄붾뱶:
```xml
<ClInclude Include="..\..\Shared\GameSim\Systems\BotLaneAIPolicy.h">
  <Filter>04. Shared\GameSim\Systems</Filter>
</ClInclude>
<ClInclude Include="..\..\Shared\GameSim\Systems\BotLaneAISystem.h">
  <Filter>04. Shared\GameSim\Systems</Filter>
</ClInclude>
```

?꾨옒濡?援먯껜:
```xml
<ClInclude Include="..\..\Shared\GameSim\Systems\ChampionAIPolicy.h">
  <Filter>04. Shared\GameSim\Systems</Filter>
</ClInclude>
<ClInclude Include="..\..\Shared\GameSim\Systems\ChampionAISystem.h">
  <Filter>04. Shared\GameSim\Systems</Filter>
</ClInclude>
```

1-13. ?대쫫 蹂寃???寃??湲곗?

援ы쁽 ???꾨옒 寃?됱뿉??AI 怨꾩뿴 `BotLaneAI` ?붿뿬媛 ?놁뼱???쒕떎.
```text
rg -n "BotLaneAI|CBotLaneAISystem|BotLaneAIDebug|eBotLaneAI|kBotLaneAI" Shared Server Client
```

?꾨옒 寃?됱? 濡쒕퉬/?ㅽ궎留??⑹뼱媛 ?⑤뒗 寃껋씠 ?뺤긽?대떎.
```text
rg -n "botLane|SetBotLane|GetBotLaneLabel|ResolveInitialBotLane|TrySetBotLane" Shared Server Client Engine
```

2. 寃利?
寃利?紐낅졊:
```text
git diff --check
rg -n "BotLaneAI|CBotLaneAISystem|BotLaneAIDebug|eBotLaneAI|kBotLaneAI" Shared Server Client
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /t:Server
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /t:Client
```

?섎룞 ?뺤씤:
```text
1. ?쒕쾭 沅뚯쐞 紐⑤뱶濡?寃뚯엫 ?쒖옉.
2. 遊?梨뷀뵾?몄씠 ?ㅽ룿 吏곹썑 吏???쇱씤??理쒖쇅媛??꾧뎔 ?ы깙 ??safeAnchor濡??대룞?섎뒗吏 ?뺤씤.
3. ?꾧뎔 誘몃땲???⑥씠釉뚭? safeAnchor/遊?洹쇱쿂???ㅼ뼱?ㅻ㈃ AI Debug Panel ?곹깭媛 LaneCombat?쇰줈 諛붾뚮뒗吏 ?뺤씤.
4. LaneCombat?먯꽌 ??誘몃땲?몄쓣 ?곗꽑 怨듦꺽?섍퀬, ??梨뷀뵾?몄씠 ?덉쓣 ????? 鍮덈룄濡???뒪 Q / ?쇱삤??Q / ?좎돩 W / 留덉뒪?곗씠 Q 紐낅졊???쒕쾭?먯꽌 accept ?먮뒗 reject 濡쒓렇濡?寃利앸릺?붿? ?뺤씤.
5. ??梨뷀뵾?몄씠 ?녾퀬 ?꾧뎔 誘몃땲?몄씠 ???ы깙 ?ш굅由??덉뿉 ?덉쑝硫?援ъ“臾?怨듦꺽 紐낅졊???섍??붿? ?뺤씤.
6. ???ы깙 洹쇱쿂?먯꽌 ??梨뷀뵾?몄씠 媛먯??섎㈃ Retreat ?곹깭濡??꾪솚?섍퀬 safeAnchor 履??대룞 紐낅졊???섍??붿? ?뺤씤.
```

誘멸?利?
```text
?꾩쭅 ?ㅼ젣 C++ 蹂寃쎄낵 鍮뚮뱶???섑뻾?섏? ?딆븯?? ??臾몄꽌??/plan-rules 湲곗? Session 1 援ы쁽 怨꾪쉷?대떎.
```
