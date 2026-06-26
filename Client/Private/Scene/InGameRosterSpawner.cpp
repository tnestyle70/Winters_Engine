#include "Scene/InGameRosterSpawner.h"

#include <Windows.h>
#include <cstdio>
#include <cwchar>
#include <memory>

#include "AI/Blackboard.h"
#include "AI/BTNodes_CombatAgent.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Systems/BehaviorTreeSystem.h"
#include "GameObject/ChampionDef.h"
#include "GamePlay/ChampionRegistry.h"
#include "Dev/SmokeLog.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"

namespace
{
    eChampion ResolveLocalSelectedChampion(eChampion selected)
    {
        const ChampionDef* cd = CChampionRegistry::Instance().Find(selected);
        if (!cd)
            cd = FindChampionDef(selected);
        if (!cd || !cd->fbxPath)
            return eChampion::EZREAL;

        return selected;
    }

    bool_t ShouldSkipSmokeBotSlots()
    {
        static i32_t s_skipBots = -1;
        if (s_skipBots >= 0)
            return s_skipBots == 1;

        const wchar_t* pCommandLine = GetCommandLineW();
        const bool_t bSmoke = pCommandLine && wcsstr(pCommandLine, L"--banpick-smoke");
        const bool_t bFullRoster = pCommandLine && wcsstr(pCommandLine, L"--smoke-full-roster");
        s_skipBots = (bSmoke && !bFullRoster) ? 1 : 0;
        return s_skipBots == 1;
    }

    bool_t ShouldUseSmokeLocalOnlyRoster()
    {
        static i32_t s_localOnly = -1;
        if (s_localOnly >= 0)
            return s_localOnly == 1;

        const wchar_t* pCommandLine = GetCommandLineW();
        const bool_t bSmoke = pCommandLine && wcsstr(pCommandLine, L"--banpick-smoke");
        const bool_t bHumanRoster = pCommandLine && wcsstr(pCommandLine, L"--smoke-human-roster");
        const bool_t bFullRoster = pCommandLine && wcsstr(pCommandLine, L"--smoke-full-roster");
        s_localOnly = (bSmoke && !bHumanRoster && !bFullRoster) ? 1 : 0;
        return s_localOnly == 1;
    }

}

eChampion CInGameRosterSpawner::ResolvePracticeBotChampion()
{
    const wchar_t* pCommandLine = GetCommandLineW();
    const wchar_t* pToken =
        pCommandLine ? wcsstr(pCommandLine, L"--practice-bot=") : nullptr;
    if (!pToken)
        return eChampion::SYLAS;

    pToken += wcslen(L"--practice-bot=");
    wchar_t name[32]{};
    for (u32_t i = 0; i < 31u && pToken[i] && pToken[i] != L' '; ++i)
        name[i] = pToken[i];

    struct { const wchar_t* pName; eChampion champion; } kTable[] = {
        { L"IRELIA", eChampion::IRELIA }, { L"YASUO", eChampion::YASUO },
        { L"KALISTA", eChampion::KALISTA }, { L"SYLAS", eChampion::SYLAS },
        { L"VIEGO", eChampion::VIEGO }, { L"ANNIE", eChampion::ANNIE },
        { L"ASHE", eChampion::ASHE }, { L"FIORA", eChampion::FIORA },
        { L"GAREN", eChampion::GAREN }, { L"RIVEN", eChampion::RIVEN },
        { L"ZED", eChampion::ZED }, { L"EZREAL", eChampion::EZREAL },
        { L"YONE", eChampion::YONE }, { L"JAX", eChampion::JAX },
        { L"MASTERYI", eChampion::MASTERYI }, { L"KINDRED", eChampion::KINDRED },
        { L"LEESIN", eChampion::LEESIN },
    };
    for (const auto& row : kTable)
    {
        if (_wcsicmp(name, row.pName) == 0)
            return row.champion;
    }
    return eChampion::SYLAS;
}

void CInGameRosterSpawner::EnsureLocalRosterFallback(MatchContext& context)
{
    if (context.bUseNetworkRoster)
        return;

    const eChampion selected = ResolveLocalSelectedChampion(context.SelectedChampion);

    context = MatchContext{};
    context.bUseNetworkRoster = true;
    context.SelectedChampion = selected;
    context.MySessionId = 1;
    context.MyNetId = 1;
    context.MySlotId = 0;
    context.MyTeam = 0;

    GameRosterSlot& player = context.Roster[0];
    player.slotId = 0;
    player.team = 0;
    player.bHuman = true;
    player.sessionId = context.MySessionId;
    player.netId = context.MyNetId;
    player.champion = selected;

    GameRosterSlot& practiceBot = context.Roster[5];
    practiceBot.slotId = 5;
    practiceBot.team = 1;
    practiceBot.bBot = true;
    practiceBot.netId = 1005;
    practiceBot.champion = ResolvePracticeBotChampion();
    practiceBot.botDifficulty = 2;

    Winters::DevSmoke::Log(
        "[ECS:RosterFallback] local practice roster playerSlot=0 champion=%u net=%u botSlot=5 botChampion=%u botNet=%u\n",
        static_cast<u32_t>(selected),
        player.netId,
        static_cast<u32_t>(practiceBot.champion),
        practiceBot.netId);
}

bool_t CInGameRosterSpawner::IsLocalRosterSlot(const MatchContext& context, const GameRosterSlot& slot)
{
    if (!slot.bHuman)
        return false;

    if (context.MySessionId != 0)
        return slot.sessionId == context.MySessionId;

    if (context.MyNetId != 0)
        return slot.netId == context.MyNetId;

    return context.MySlotId != kInvalidGameRosterSlot
        && slot.slotId == context.MySlotId;
}

EntityID CInGameRosterSpawner::SpawnSlot(InGameRosterSpawnDesc& desc, const GameRosterSlot& slot)
{
    if (slot.champion == eChampion::END || slot.champion == eChampion::NONE)
        return NULL_ENTITY;

    if (slot.netId != 0 && desc.pEntityIdMap)
    {
        const EntityID existing = desc.pEntityIdMap->FromNet(slot.netId);
        if (existing != NULL_ENTITY)
        {
            char dbg[160]{};
            sprintf_s(dbg,
                "[ECS:Roster] reuse existing net=%u entity=%u slot=%u\n",
                slot.netId,
                static_cast<u32_t>(existing),
                static_cast<u32_t>(slot.slotId));
            Winters::DevSmoke::Log("%s", dbg);
            return existing;
        }
    }

    if (!desc.createChampion)
        return NULL_ENTITY;

    const eTeam team = static_cast<eTeam>(slot.team);
    const EntityID entity = desc.createChampion(slot.champion, team);
    if (entity == NULL_ENTITY)
        return NULL_ENTITY;

    if (slot.netId != 0 && desc.pEntityIdMap)
        desc.pEntityIdMap->Bind(slot.netId, entity);

    Vec3 spawnPos{};
    bool_t bHasSpawnPos = false;

    if (slot.slotId != kInvalidGameRosterSlot
        && desc.world.HasComponent<TransformComponent>(entity))
    {
        spawnPos = GetGameSimRosterSpawnPosition(slot.slotId, slot.team, slot.bBot);
        desc.world.GetComponent<TransformComponent>(entity).SetPosition(spawnPos);
        desc.networkChampionPrevPos[entity] = spawnPos;
        bHasSpawnPos = true;
    }

    if (slot.bBot && !desc.bNetworkAuthoritative)
    {
        auto pTree = Engine::BuildStandardCombatAgentBT();
        auto& bot = desc.world.AddComponent<Engine::BotComponent>(entity);
        bot.difficulty = slot.botDifficulty ? slot.botDifficulty : 2;
        bot.bUseRL = false;
        bot.pBT = std::shared_ptr<Engine::CBehaviorTree>(std::move(pTree));

        auto& bb = desc.world.AddComponent<BlackboardComponent>(entity);
        bb.bb.Set("difficulty", static_cast<i32_t>(bot.difficulty));
        bb.bb.Set("lanePushPos", team == eTeam::Red
            ? Vec3{ -30.f, 0.f, 30.f }
            : Vec3{ 30.f, 0.f, -30.f });
    }
    else if (slot.bBot)
    {
        Winters::DevSmoke::Log(
            "[ECS:Roster] network authoritative bot slot=%u: skip local BT/MCTS components\n",
            static_cast<u32_t>(slot.slotId));
    }

    if (desc.assignAlias)
        desc.assignAlias(slot.champion, entity);

    Winters::DevSmoke::Log(
        "[ECS:Roster] created slot=%u entity=%u champ=%u team=%u human=%u bot=%u net=%u pos=(%.2f,%.2f,%.2f) hasPos=%u\n",
        static_cast<u32_t>(slot.slotId),
        static_cast<u32_t>(entity),
        static_cast<u32_t>(slot.champion),
        static_cast<u32_t>(slot.team),
        slot.bHuman ? 1u : 0u,
        slot.bBot ? 1u : 0u,
        slot.netId,
        spawnPos.x,
        spawnPos.y,
        spawnPos.z,
        bHasSpawnPos ? 1u : 0u);

    return entity;
}

InGameRosterSpawnResult CInGameRosterSpawner::SpawnFromContext(
    InGameRosterSpawnDesc& desc,
    const MatchContext& context)
{
    InGameRosterSpawnResult result{};
    EntityID firstHumanEntity = NULL_ENTITY;
    EntityID firstCreatedEntity = NULL_ENTITY;

    for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
    {
        const GameRosterSlot& slot = context.Roster[i];
        if (!slot.bHuman && !slot.bBot)
            continue;

        if (ShouldUseSmokeLocalOnlyRoster()
            && !CInGameRosterSpawner::IsLocalRosterSlot(context, slot))
        {
            Winters::DevSmoke::Log(
                "[ECS:Roster] smoke skip nonlocal slot=%u champion=%u human=%d bot=%d\n",
                i,
                static_cast<u32_t>(slot.champion),
                slot.bHuman ? 1 : 0,
                slot.bBot ? 1 : 0);
            continue;
        }
        if (slot.bBot && ShouldSkipSmokeBotSlots())
        {
            Winters::DevSmoke::Log(
                "[ECS:Roster] smoke skip bot slot=%u champion=%u\n",
                i,
                static_cast<u32_t>(slot.champion));
            continue;
        }

        ++result.requestedSlots;
        if (slot.bHuman)
            ++result.humanSlots;
        if (slot.bBot)
            ++result.botSlots;

        const EntityID entity = SpawnSlot(desc, slot);
        if (entity == NULL_ENTITY)
        {
            char failDbg[192]{};
            sprintf_s(failDbg, "[ECS:Roster] slot %u failed champion=%u human=%d bot=%d\n",
                i,
                static_cast<u32_t>(slot.champion),
                slot.bHuman ? 1 : 0,
                slot.bBot ? 1 : 0);
            Winters::DevSmoke::Log("%s", failDbg);
            continue;
        }

        result.bCreatedAny = true;
        ++result.createdSlots;
        if (firstCreatedEntity == NULL_ENTITY)
            firstCreatedEntity = entity;
        if (slot.bHuman && firstHumanEntity == NULL_ENTITY)
            firstHumanEntity = entity;

        if (IsLocalRosterSlot(context, slot))
        {
            result.playerEntity = entity;

            char localDbg[192]{};
            sprintf_s(localDbg,
                "[ECS:Roster] local slot=%u sid=%u net=%u champ=%u entity=%u\n",
                static_cast<u32_t>(slot.slotId),
                slot.sessionId,
                slot.netId,
                static_cast<u32_t>(slot.champion),
                static_cast<u32_t>(entity));
            Winters::DevSmoke::Log("%s", localDbg);
        }
    }

    const bool_t bHasAuthoritativeIdentity =
        context.bUseNetworkRoster &&
        (context.MySessionId != 0 || context.MyNetId != 0);

    if (result.playerEntity == NULL_ENTITY && !bHasAuthoritativeIdentity)
        result.playerEntity = firstHumanEntity;
    if (result.playerEntity == NULL_ENTITY && !bHasAuthoritativeIdentity)
        result.playerEntity = firstCreatedEntity;

    if (result.playerEntity == NULL_ENTITY && bHasAuthoritativeIdentity)
    {
        char noLocalDbg[224]{};
        sprintf_s(noLocalDbg,
            "[ECS:Roster] local slot not found sid=%u net=%u mySlot=%u; no first-human fallback\n",
            context.MySessionId,
            context.MyNetId,
            static_cast<u32_t>(context.MySlotId));
        Winters::DevSmoke::Log("%s", noLocalDbg);
    }

    if (result.playerEntity != NULL_ENTITY
        && !desc.world.HasComponent<LocalPlayerTag>(result.playerEntity))
    {
        desc.world.AddComponent<LocalPlayerTag>(result.playerEntity);
    }

    char dbg[224]{};
    sprintf_s(dbg, "[ECS:Roster] requested=%u created=%u humans=%u bots=%u player=%u mySlot=%u\n",
        result.requestedSlots,
        result.createdSlots,
        result.humanSlots,
        result.botSlots,
        static_cast<u32_t>(result.playerEntity),
        static_cast<u32_t>(context.MySlotId));
    Winters::DevSmoke::Log("%s", dbg);

    return result;
}
