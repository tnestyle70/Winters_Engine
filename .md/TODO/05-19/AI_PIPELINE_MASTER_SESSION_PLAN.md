Session - ?쒕쾭 沅뚯쐞 BotLaneAI瑜??쇱씤 ?숉뻾, CS, ?뺣쪧 寃ъ젣, ?꾩꽦??AI ?뺤옣 援ъ“濡??뺣━?쒕떎.

1. 諛섏쁺?댁빞 ?섎뒗 肄붾뱶

1-0. ??臾몄꽌??紐⑹쟻

??臾몄꽌??諛붾줈 援ы쁽?섍린 ?꾪븳 理쒖쥌 肄붾뱶 釉붾줉 紐⑥쓬???꾨땲?? ?욎쑝濡?AI ?묒뾽 ?몄뀡?ㅼ씠 ?곕씪???섎뒗 湲곗? 臾몄꽌??

?듭떖? ?섎굹??

```text
AI???꾪닾 ?쒖뒪?쒖씠 ?꾨땲??
AI???쒕쾭 GameSim??GameCommand瑜??ｋ뒗 ?낅젰 ?앹궛?먮떎.
```

?곕씪??Winters AI???뺤떇 援ъ“???꾨옒 ?먮쫫???덈? 踰쀬뼱?섎㈃ ???쒕떎.

```text
Bot Perception
-> Bot Decision
-> GameCommand
-> CDefaultCommandExecutor
-> Server GameSim truth
-> Snapshot / ReplicatedEvent
-> Client Visual / UI Debug
```

AI媛 吏곸젒 ?섎㈃ ???섎뒗 寃?

```text
TransformComponent ?꾩튂 吏곸젒 蹂寃?HealthComponent HP 吏곸젒 蹂寃?SkillStateComponent cooldown 吏곸젒 蹂寃?DamageRequest 吏곸젒 enqueue
MoveTargetComponent瑜?寃곌낵 蹂댁젙?⑹쑝濡?吏곸젒 clear
client visual hook??gameplay 寃곌낵泥섎읆 ?ъ슜
```

AI媛 ?대룄 ?섎뒗 寃?

```text
Move GameCommand ?앹꽦
BasicAttack GameCommand ?앹꽦
CastSkill GameCommand ?앹꽦
?먭린 BotLaneAIComponent ?덉쓽 ?먮떒 ?곹깭/debug 媛?媛깆떊
```

1-1. ?꾩옱 肄붾뱶 湲곗? ?꾩껜 ?먮쫫

湲곗? ?뚯씪:

```text
Server/Private/Game/GameRoom.cpp
```

?꾩옱 ?쒕쾭 tick anchor:

```cpp
void CGameRoom::Tick()
{
    std::lock_guard stateLock(m_stateMutex);

    if (m_roomPhase != eRoomPhase::InGame)
        return;

    ++m_tickIndex;
    m_visibleTickIndex.store(m_tickIndex, std::memory_order_relaxed);

    TickContext tc{
        m_tickIndex,
        DeterministicTime::kFixedDt,
        DeterministicTime::TickToSec(m_tickIndex),
        &m_rng,
        &m_entityMap,
        NULL_ENTITY,
        this
    };
    tc.pLagCompensation = m_pLagCompensation.get();

    Phase_DrainCommands(tc);
    Phase_ServerBotAI(tc);
    Phase_ExecuteCommands(tc);
    Phase_SimulationSystems(tc);
    if (m_pLagCompensation)
        m_pLagCompensation->RecordHistory(m_world, tc.tickIndex);
    Phase_BroadcastEvents(tc);
    Phase_BroadcastSnapshot(tc);
}
```

???쒖꽌??AI ?ㅺ퀎?먯꽌 媛??以묒슂?섎떎.

```text
Phase_DrainCommands:
  ?щ엺 ?대씪?댁뼵?멸? 蹂대궦 network command瑜?GameCommand濡?蹂?섑븳??

Phase_ServerBotAI:
  遊뉗씠 ?쒕쾭 world瑜?蹂닿퀬 GameCommand瑜?留뚮뱺??

Phase_ExecuteCommands:
  ?щ엺 command? 遊?command媛 媛숈? CDefaultCommandExecutor瑜??듦낵?쒕떎.

Phase_SimulationSystems:
  ?대룞, chase, champion sim, minion, turret, projectile, damage, death/respawn??泥섎━?쒕떎.

Phase_BroadcastEvents / Phase_BroadcastSnapshot:
  寃곌낵瑜?client visual濡?蹂대궦??
```

以묒슂???댁꽍:

```text
遊뉗? ?щ엺蹂대떎 ?밸퀎??沅뚰븳??媛뽰? ?딅뒗??
遊뉗? ?쒕쾭 ?대??먯꽌 ?낅젰??留뚮뱾 肉먯씠怨? 寃곌낵 沅뚰븳? Executor? GameSim??媛吏꾨떎.
```

1-2. ?꾩옱 AI 吏꾩엯??
湲곗? ?뚯씪:

```text
Shared/GameSim/Systems/BotLaneAISystem.cpp
Shared/GameSim/Components/BotLaneAIComponent.h
Shared/GameSim/Systems/BotLaneAIPolicy.h
Shared/GameSim/Systems/BotLaneAIPolicy.cpp
```

?꾩옱 AI 而댄룷?뚰듃 anchor:

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
```

?꾩옱 BotLaneAISystem ??援ъ“:

```text
BuildBotLaneAIContext
-> ChooseBestIntent
-> EmitCommandForIntent
```

?꾩옱 二쇱슂 anchor:

```text
BotLaneAISystem.cpp:24    IsAliveTarget
BotLaneAISystem.cpp:41    MakeBotCommand
BotLaneAISystem.cpp:765   BuildBotLaneAIContext
BotLaneAISystem.cpp:1034  ChooseBestIntent
BotLaneAISystem.cpp:1197  EmitMoveCommand
BotLaneAISystem.cpp:1219  EmitBasicAttackCommand
BotLaneAISystem.cpp:1249  EmitSkillCommand
BotLaneAISystem.cpp:1504  TryEmitChampionFightBT
BotLaneAISystem.cpp:1587  EmitCommandForIntent
BotLaneAISystem.cpp:1731  CBotLaneAISystem::Execute
```

?꾩옱 ?ㅺ퀎??醫뗭? ??

```text
1. 遊?command sequence瑜??먯껜 諛쒓툒?쒕떎.
2. Move / BasicAttack / CastSkill留?outCommands???ｋ뒗??
3. ?ㅼ젣 cooldown/range/target 寃利앹? CommandExecutor媛 ?쒕떎.
4. client BT/MCTS??network authoritative 紐⑤뱶?먯꽌 ?ㅽ궢?쒕떎.
5. SnapshotBuilder媛 AI state瑜?client debug?⑹쑝濡??대젮以??
```

?꾩옱 ?ㅺ퀎?먯꽌 ???뺣━????

```text
1. MoveToLane??"?ы깙 ???덉쟾 ?湲??몄? "?쇱씤 以묎컙 吏꾩엯"?몄? ?섎?媛 ?욎뿬 ?덈떎.
2. FollowWave媛 ?꾩쭅 紐낇솗??root state媛 ?꾨땲??utility ?꾨낫 以??섎굹??媛源앸떎.
3. 90% CS / 10% 梨뷀뵾??寃ъ젣 媛숈? lane behavior policy媛 紐낆떆?곸쑝濡?遺꾨━?섏뼱 ?덉? ?딅떎.
4. difficulty????λ릺吏留??ㅼ젣 decisionInterval, attackChampionChance, mistakeChance??異⑸텇??諛섏쁺?섏? ?딅뒗??
5. debug UI??state/target ?뺣룄留?蹂댁뿬二쇨퀬, ??洹?寃곗젙???덈뒗吏源뚯???蹂댁뿬二쇱? ?딅뒗??
```

1-3. ?꾩옱 command executor 湲곗?

湲곗? ?뚯씪:

```text
Shared/GameSim/Systems/CommandExecutor.cpp
```

?꾩옱 command executor anchor:

```text
CommandExecutor.cpp:1044  CDefaultCommandExecutor::ExecuteCommand
CommandExecutor.cpp:1081  CDefaultCommandExecutor::HandleMove
CommandExecutor.cpp:1156  CDefaultCommandExecutor::HandleCastSkill
CommandExecutor.cpp:1402  CDefaultCommandExecutor::HandleBasicAttack
CommandExecutor.cpp:1649  BuildServerCommand
```

AI 愿?먯뿉??executor???꾨옒 ?섎?瑜?媛吏꾨떎.

```text
Move:
  ?좏슚???대룞 紐⑹쟻吏?몄? ?뺤씤?섍퀬 MoveTargetComponent瑜??ㅼ젙?쒕떎.

CastSkill:
  slot, cooldown, stage, target, untargetable, dead target??寃利앺븳??
  accept?섎㈃ cooldown, animation, gameplay hook, fallback damage, effect event瑜?泥섎━?쒕떎.

BasicAttack:
  target, team, cooldown, range瑜?寃利앺븳??
  range 諛뽰씠硫?AttackChase瑜??쒖옉?쒕떎.
  range ?덉씠硫?CombatAction, DamageRequest, cooldown, animation, effect event瑜?泥섎━?쒕떎.
```

?곕씪??AI 肄붾뱶?먯꽌 "?ㅽ궗??留욎븯??, "?됲?媛 ?ㅼ뼱媛붾떎", "?대룞??痍⑥냼?먮떎"瑜?吏곸젒 ?뺤젙?섎㈃ ???쒕떎.

1-4. ?ъ슜?먭? ?쒖븞???됰룞??Winters 援ъ“濡?踰덉뿭

?ъ슜???쒖븞:

```text
泥섏쓬 ?쒖옉 ???ы깙 ?덉쑝濡??ㅼ뼱媛?誘몃땲?몄씠 ?섏삤硫?誘몃땲?멸낵 ?④퍡 ?쇱씤?쇰줈 ?섍컧
??誘몃땲??怨듦꺽
90% ?뺣쪧 tick?쇰줈 誘몃땲??怨듦꺽
10% ?뺣쪧濡?梨뷀뵾??怨듦꺽 ??곸쑝濡??ㅼ젙
梨뷀뵾?몃퀎 遺꾧린:
  Jax - Q
  Fiora - Q
  Ashe - W
  MasterYi - Q
```

Winters ?쒕쾭 沅뚯쐞 湲곗? 踰덉뿭:

```text
泥섏쓬 ?쒖옉 ???ы깙 ?덉쑝濡??ㅼ뼱媛?-> SafeAnchor state
-> ?먭린 ? fountain/outer turret/?쇱씤 safe wait ?꾩튂 以??섎굹瑜?紐⑺몴濡?Move command
-> ???ы깙 ?덉쑝濡?dive?쒕떎???살씠 ?꾨땲??

誘몃땲?몄씠 ?섏삤硫?誘몃땲?멸낵 ?④퍡 ?쇱씤?쇰줈 ?섍컧
-> WaveEscort state
-> alliedWave ?먮뒗 lane waypoint 二쇰??쇰줈 Move command

??誘몃땲??怨듦꺽
-> FarmMinion intent
-> last-hit 媛??誘몃땲???곗꽑
-> ?꾨땲硫?pressure minion
-> BA 媛?ν븯硫?BasicAttack command
-> ?꾨땲硫?targetPos濡?Move command

90% ?뺣쪧 tick?쇰줈 誘몃땲??怨듦꺽
-> 留?frame???꾨땲??decision tick留덈떎 deterministic roll
-> roll < farmChance硫?FarmMinion
-> intent hold timer濡??덈Т ?먯＜ ?붾뱾由щ뒗 寃?諛⑹?

10% ?뺣쪧濡?梨뷀뵾??怨듦꺽 ??곸쑝濡??ㅼ젙
-> roll >= farmChance硫?AttackChampion
-> ??enemy champion, hp, turret danger, cooldown, range 議곌굔???듦낵?댁빞 ??-> ?ㅽ뙣?섎㈃ FarmMinion fallback

梨뷀뵾?몃퀎 遺꾧린
-> ChampionTactic policy
-> Jax Q, Fiora Q, Ashe W, MasterYi Q瑜?CastSkill command濡?emit
-> executor媛 cooldown/range/target validity瑜?理쒖쥌 ?먯젙
```

1-5. Stage1?먯꽌 ?덈줈 ?뺤쓽???곹깭

?꾩옱 enum???ш쾶 諛붽씀吏 ?딄퀬 ?섎?瑜?癒쇱? 怨좎젙?쒕떎.

```text
MoveToLane:
  寃뚯엫 ?쒖옉 ??safeAnchor ?먮뒗 laneGoal濡??대룞?쒕떎.

FollowWave:
  ?꾧뎔 誘몃땲???⑥씠釉뚭? 媛源뚯슦硫??⑥씠釉??욎뿉 ?쇱옄 ??대굹媛吏 ?딄퀬 ?⑥씠釉?洹쇱쿂濡??대룞?쒕떎.

FarmMinion:
  lane combat 湲곕낯 ?됰룞. enemy minion???곗꽑 怨듦꺽?쒕떎.

FightChampion:
  媛뺤젣 all-in???꾨땲??"harass/trade ?쒕룄"濡??쒖옉?쒕떎.

Kite:
  ?먭굅由?梨뷀뵾?몄씠 attack range ?좎? ?먮뒗 ?꾪눜 ?대룞???욌뒗??

AttackStructure:
  allied wave媛 turret/structure瑜?諛쏆븘以??뚮쭔 援ъ“臾?怨듦꺽?쒕떎.

Retreat:
  hp, turretDanger, minionPressure, target loss 議곌굔?쇰줈 safeAnchor/vRetreatGoal濡??대룞?쒕떎.

Dead:
  command emit ?놁쓬.
```

1-6. ?꾩꽦??AI瑜??꾪븳 怨꾩링 援ъ“

泥섏쓬遺??HFSM/BT/GOAP/MCTS瑜?紐⑤몢 ?ｌ쑝硫??꾩옱 ?쒕쾭 沅뚯쐞 ?덉젙?붿? 異⑸룎?쒕떎.

?곕씪???꾨옒 ?쒖꽌濡?怨꾩링???볥뒗??

```text
Layer 0. Perception
  ?쒕쾭 world?먯꽌 ?먮떒 ?щ즺瑜?紐⑥???
  ?? ??HP, ??梨뷀뵾?? ??誘몃땲?? ?꾧뎔 ?⑥씠釉? ?ы깙 ?꾪뿕, ?ㅽ궗 荑⑤떎??

Layer 1. Lane Policy
  吏湲??뱀옣 ?????됰룞??怨좊Ⅸ??
  ?? SafeAnchor, WaveEscort, FarmMinion, AttackChampion, Siege, Retreat.

Layer 2. Champion Tactic
  媛숈? AttackChampion?대씪??梨뷀뵾?몃퀎 ?됰룞??怨좊Ⅸ??
  ?? Jax Q, Fiora Q, Ashe W, MasterYi Q.

Layer 3. Action Builder
  ?좏깮???됰룞??GameCommand濡?留뚮뱺??

Layer 4. Executor
  紐⑤뱺 ?먯젙? CDefaultCommandExecutor媛 ?쒕떎.

Layer 5. Replication / Visual
  Snapshot/Event瑜??듯빐 client媛 animation/fx/ui瑜??ъ깮?쒕떎.
```

?꾩꽦?뺤쑝濡?媛???媛?怨꾩링? ?꾨옒泥섎읆 吏꾪솕?쒕떎.

```text
Stage 1:
  ?꾩옱 BotLaneAISystem ?덉뿉 SafeAnchor/WaveEscort/FarmVsHarass policy瑜??뺣━?쒕떎.

Stage 2:
  BotLaneAIPolicy??difficulty, personality, attackChampionChance, mistakeChance瑜??ｋ뒗??

Stage 3:
  HFSM???꾩엯??Laning / Retreat / Siege / Dead 媛숈? root state瑜?紐낇솗??遺꾨━?쒕떎.

Stage 4:
  Champion tactic??BT ?ㅽ??쇰줈 遺꾨━?쒕떎.
  ?? 理쒖쥌 異쒕젰? 怨꾩냽 GameCommand??

Stage 5:
  Utility score瑜?紐낆떆?곸쑝濡?湲곕줉?섍퀬 debug UI??蹂댁뿬以??

Stage 6:
  Influence map / team blackboard瑜?遺숈씤??

Stage 7:
  MCTS/RL? combat system???덉젙?????좏깮?곸쑝濡?遺숈씤??
```

1-7. ?몄뀡蹂?吏꾪뻾 怨꾪쉷

1-7-1. Session 01 - SafeAnchor / WaveEscort ?섎? 怨좎젙

紐⑺몴:

```text
遊뉗씠 ?쒖옉?섏옄留덉옄 ?쇱옄 ?쇱씤 以묒븰?쇰줈 ?곗? ?딄퀬 safeAnchor濡??대룞?쒕떎.
?꾧뎔 誘몃땲???⑥씠釉뚭? ?깆옣?섎㈃ ?⑥씠釉?洹쇱쿂濡??대룞?쒕떎.
```

?섏젙 ?꾨낫:

```text
Shared/GameSim/Components/BotLaneAIComponent.h
Server/Private/Game/GameRoom.cpp
Shared/GameSim/Systems/BotLaneAISystem.cpp
```

二쇱슂 anchor:

```text
BotLaneAIComponent.h:35  struct BotLaneAIComponent
GameRoom.cpp:5082        if (slot.bBot && !slot.bDummy)
BotLaneAISystem.cpp:765  BuildBotLaneAIContext
BotLaneAISystem.cpp:1587 EmitCommandForIntent
```

?ㅺ퀎:

```text
BotLaneAIComponent??safeAnchor? waveAnchor ?깃꺽??媛믪쓣 異붽??쒕떎.
SpawnChampionForLobbySlot?먯꽌 vRetreatGoal/spawnPos? lane waypoint瑜?湲곗??쇰줈 safeAnchor瑜??〓뒗??
BuildBotLaneAIContext?먯꽌 alliedWave ?꾩튂瑜???紐낇솗???〓뒗??
EmitCommandForIntent?먯꽌 FollowWave媛 吏꾩쭨 ?⑥씠釉?escort濡??숈옉?섍쾶 ?쒕떎.
```

1-7-2. Session 02 - Farm 90 / Harass 10 policy ?꾩엯

紐⑺몴:

```text
LaneCombat ?곹솴?먯꽌 湲곕낯? CS??
寃곗젙 tick留덈떎 deterministic roll濡???? ?뺣쪧??champion harass瑜??쒕룄?쒕떎.
```

?섏젙 ?꾨낫:

```text
Shared/GameSim/Components/BotLaneAIComponent.h
Shared/GameSim/Systems/BotLaneAIPolicy.h
Shared/GameSim/Systems/BotLaneAIPolicy.cpp
Shared/GameSim/Systems/BotLaneAISystem.cpp
```

二쇱슂 anchor:

```text
BotLaneAIPolicy.h:14     struct BotChampionProfile
BotLaneAISystem.cpp:1034 ChooseBestIntent
BotLaneAISystem.cpp:1587 EmitCommandForIntent
```

?ㅺ퀎:

```text
BotChampionProfile??farmChance ?먮뒗 attackChampionChance瑜?異붽??쒕떎.
珥덇린媛믪? Farm 0.90 / Harass 0.10?쇰줈 ?붾떎.
?쒖닔??std::rand媛 ?꾨땲??TickContext??deterministic rng瑜??ъ슜?쒕떎.
roll 寃곌낵??ai debug 媛믪뿉 ??ν븳??
Harass 議곌굔???ㅽ뙣?섎㈃ FarmMinion?쇰줈 fallback?쒕떎.
```

二쇱쓽:

```text
留??꾨젅???뺣쪧??援대━吏 ?딅뒗??
decisionTimer媛 留뚮즺???쒓컙?먮쭔 援대┛??
fIntentHoldTimer濡?理쒖냼 ?좎? ?쒓컙???붾떎.
```

1-7-3. Session 03 - 梨뷀뵾?몃퀎 Harass tactic

紐⑺몴:

```text
10% AttackChampion intent媛 ?좏깮?먯쓣 ??梨뷀뵾?몃퀎濡?紐낇솗??1李??ㅽ궗???ъ슜?쒕떎.
```

珥덇린 梨뷀뵾??

```text
Jax      -> Q
Fiora    -> Q
Ashe     -> W
MasterYi -> Q
```

?섏젙 ?꾨낫:

```text
Shared/GameSim/Systems/BotLaneAISystem.cpp
Shared/GameSim/Systems/BotLaneAIPolicy.cpp
```

二쇱슂 anchor:

```text
BotLaneAISystem.cpp:1504 TryEmitChampionFightBT
BotLaneAISystem.cpp:1219 EmitBasicAttackCommand
BotLaneAISystem.cpp:1249 EmitSkillCommand
```

?ㅺ퀎:

```text
TryEmitChampionFightBT ?덉뿉??champion蹂?tactic???몄텧?쒕떎.
媛?tactic? target, range, hp, turretDanger瑜??뺤씤?섍퀬 EmitSkillCommand瑜??몄텧?쒕떎.
?ㅽ궗 command媛 reject?????덉쑝誘濡?fallback?쇰줈 BasicAttack ?먮뒗 FarmMinion???덉슜?쒕떎.
```

1-7-4. Session 04 - Debug UI 媛뺥솕

紐⑺몴:

```text
AI媛 ??洹?寃곗젙???덈뒗吏 client?먯꽌 蹂????덇쾶 ?쒕떎.
```

?섏젙 ?꾨낫:

```text
Shared/GameSim/Components/BotLaneAIComponent.h
Server/Private/Game/SnapshotBuilder.cpp
Client/Private/Network/Client/SnapshotApplier.cpp
Client/Private/UI/AIDebugPanel.cpp
```

?꾩옱 anchor:

```text
SnapshotBuilder.cpp:273       BotLaneAIComponent debug encode
SnapshotApplier.cpp:456       BotLaneAIDebugComponent update
AIDebugPanel.cpp:54           BotLaneStateName
```

異붽? ?쒖떆 ?꾨낫:

```text
intent
lastIntent
lastCommandKind
lastRoll
scoreFarm
scoreFight
scoreRetreat
targetNetId
safeAnchor / laneGoal
```

1-7-5. Session 05 - Difficulty / Personality

紐⑺몴:

```text
botDifficulty媛 ?ㅼ젣 AI ?됰룞 李⑥씠??諛섏쁺?섍쾶 ?쒕떎.
```

?ㅺ퀎:

```text
Beginner:
  decisionInterval ??  attackChampionChance ??쓬
  last-hit ?ㅼ닔 留롮쓬
  turretDanger??誘쇨컧

Normal:
  Farm 90 / Harass 10
  湲곕낯 ?덉쟾 ?뺤콉

Hard:
  attackChampionChance 利앷?
  low HP target 異붽꺽 利앷?
  last-hit ?뺥솗??利앷?

Master:
  utility score 湲곕컲?쇰줈 harass/farm/siege瑜???怨듦꺽?곸쑝濡??좏깮
```

?섏젙 ?꾨낫:

```text
Shared/GameSim/Systems/BotLaneAIPolicy.h
Shared/GameSim/Systems/BotLaneAIPolicy.cpp
Shared/GameSim/Systems/BotLaneAISystem.cpp
```

1-7-6. Session 06 - HFSM 遺꾨━

紐⑺몴:

```text
?꾩옱 intent 湲곕컲 AI瑜?Laning / Retreat / Siege / Dead root state濡??뺣━?쒕떎.
```

二쇱쓽:

```text
泥섏쓬遺??Engine/Public/AI??湲곗〈 BT/MCTS瑜?server path??遺숈씠吏 ?딅뒗??
癒쇱? Shared/GameSim ?덉쓽 server-authoritative BotLaneAI瑜??뺣━?쒕떎.
```

?꾨낫 援ъ“:

```text
BotRootState:
  Laning
  Retreat
  Siege
  Dead

BotLaneSubState:
  SafeAnchor
  WaveEscort
  Farm
  Harass
  Chase
  Kite
```

1-7-7. Session 07 - BT / Utility 遺꾨━

紐⑺몴:

```text
梨뷀뵾?몃퀎 tactic??而ㅼ?湲??쒖옉?섎㈃ BotLaneAISystem.cpp?먯꽌 遺꾨━?쒕떎.
```

遺꾨━ ?꾨낫:

```text
Shared/GameSim/AI/BotPerception.h/.cpp
Shared/GameSim/AI/BotDecision.h/.cpp
Shared/GameSim/AI/BotChampionTactics.h/.cpp
```

遺꾨━ 湲곗?:

```text
BotLaneAISystem.cpp媛 2000以꾩쓣 ?섍굅??
champion蹂?tactic??5媛??댁긽 ?섏뼱?섍굅??
debug score breakdown??蹂듭옟?댁쭏 ??遺꾨━?쒕떎.
```

1-7-8. Session 08 - ?꾩꽦??AI ?뺤옣

紐⑺몴:

```text
?쇱씤?꾨쭔 ?섎뒗 遊뉗뿉??理쒖냼 ????猷⑦봽瑜??댄빐?섎뒗 遊뉗쑝濡??뺤옣?쒕떎.
```

?뺤옣 ?쒖꽌:

```text
1. Tower destroy awareness
2. Inhibitor / Nexus objective awareness
3. Recall / buy item ?먮떒
4. Team blackboard
5. Influence map
6. Objective push/defense
7. Replay 湲곕컲 decision 遺꾩꽍
```

1-8. 吏湲??뱀옣 ?섏? 留먯븘??????
```text
1. AI媛 吏곸젒 ?곕?吏瑜??ｋ뒗 援ъ“
2. AI媛 吏곸젒 cooldown??諛붽씀??援ъ“
3. AI媛 吏곸젒 Transform???쒓컙?대룞?쒗궎??援ъ“
4. Client BT/MCTS瑜?network authoritative 遊??먮떒?쇰줈 ?ъ궗?⑺븯??援ъ“
5. 90/10 ?뺣쪧??留?frame留덈떎 援대젮???됰룞???붾뱾由щ뒗 援ъ“
6. ?쒕쾭 cue ?놁씠 client local visual hook?쇰줈 AI 寃곌낵瑜?蹂댁뿬二쇰뒗 援ъ“
```

1-9. ?ㅼ쓬 ?몄뀡?먯꽌 ?묒꽦??援ы쁽 怨꾪쉷 ?⑥쐞

?ㅼ쓬 援ы쁽 ?몄뀡? ?꾨옒 ?섎굹留???곸쑝濡??쒕떎.

```text
Session 01 - SafeAnchor / WaveEscort ?섎? 怨좎젙
```

?ㅼ쓬 援ы쁽 怨꾪쉷?먯꽌 諛섎뱶???ы븿???뚯씪:

```text
Shared/GameSim/Components/BotLaneAIComponent.h
Server/Private/Game/GameRoom.cpp
Shared/GameSim/Systems/BotLaneAISystem.cpp
Server/Private/Game/SnapshotBuilder.cpp
Client/Private/Network/Client/SnapshotApplier.cpp
Client/Private/UI/AIDebugPanel.cpp
```

?? Snapshot/UI debug 媛뺥솕??Session 01?먯꽌 理쒖냼 ?꾨뱶留?異붽??섍퀬, ?곸꽭 score breakdown? Session 04濡?誘몃，??

2. 寃利?
誘멸?利?

```text
??臾몄꽌???ㅺ퀎 怨좎젙 臾몄꽌??
肄붾뱶 蹂寃? 鍮뚮뱶, ?고???smoke???꾩쭅 ?섑뻾?섏? ?딆븯??
```

Session 01 援ы쁽 ??寃利?紐낅졊:

```powershell
git diff --check
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

Session 01 ?섎룞 ?뺤씤:

```text
1. ?쒕쾭 InGame ?쒖옉 ??bot??利됱떆 ???ы깙 履쎌쑝濡??곗? ?딅뒗??
2. bot??safeAnchor ?먮뒗 lane wait 吏?먯쑝濡??대룞?쒕떎.
3. 誘몃땲???⑥씠釉뚭? 媛源뚯썙吏硫?bot??wave escort濡??꾪솚?쒕떎.
4. AI Debug Panel?먯꽌 MoveToLane / FollowWave / FarmMinion ?곹깭媛 援щ텇?쒕떎.
5. [BotAI] 濡쒓렇?먯꽌 command媛 Move ?먮뒗 BasicAttack?쇰줈留??뺤긽 異쒕젰?쒕떎.
6. CommandExecutor reject log媛 諛섎났 spam?섏? ?딅뒗??
```

Session 02 援ы쁽 ???섎룞 ?뺤씤:

```text
1. decision tick留덈떎 farm/harass roll??湲곕줉?쒕떎.
2. ???90%??FarmMinion, 10%??AttackChampion?쇰줈 ?좏깮?쒕떎.
3. harass 議곌굔??遺덇??ν븯硫?FarmMinion?쇰줈 fallback?쒕떎.
4. 遊??됰룞??留?frame ?붾뱾由ъ? ?딅뒗??
```

Session 03 援ы쁽 ???섎룞 ?뺤씤:

```text
1. Jax bot??harass intent?먯꽌 Q command瑜??몃떎.
2. Fiora bot??harass intent?먯꽌 Q command瑜??몃떎.
3. Ashe bot??harass intent?먯꽌 W command瑜??몃떎.
4. MasterYi bot??harass intent?먯꽌 Q command瑜??몃떎.
5. 紐⑤뱺 ?ㅽ궗? CDefaultCommandExecutor::HandleCastSkill???듦낵?쒕떎.
6. client??Snapshot/Event瑜??듯빐 animation/fx留??ъ깮?쒕떎.
```

?κ린 ?꾨즺 湲곗?:

```text
1. 遊뉗? lane start, wave escort, farm, harass, retreat, siege瑜??덉젙?곸쑝濡?諛섎났?쒕떎.
2. 遊뉕낵 ?щ엺??command媛 媛숈? executor path瑜??꾨떎.
3. 二쎌? target, stale projectile, respawn ?곹깭?먯꽌 AI媛 target??怨꾩냽 臾쇱? ?딅뒗??
4. AI Debug Panel?먯꽌 寃곗젙 ?댁쑀媛 異붿쟻 媛?ν븯??
5. 理쒖냼 ????猷⑦봽??lane -> tower -> inhibitor/nexus -> result/reset 諛⑺뼢?쇰줈 ?뺤옣 媛?ν븯??
```
