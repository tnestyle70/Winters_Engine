#include "GameObject/ChampionDef.h"
#include "GamePlay/ChampionRegistry.h"
#include "Client/Private/Data/LoLVisualDefinitionPack.h"

namespace
{
    inline constexpr u32_t kMaxCachedChampionDefs = 32u;

    ChampionDef MakeChampionDef(const ClientData::ChampionModelVisualDefinition& visual)
    {
        ChampionDef def{};
        def.id = visual.champion;
        def.animPrefix = visual.animPrefix;
        def.idleAnimKey = visual.idleAnimation;
        def.runAnimKey = visual.runAnimation;
        def.basicAttackKey = visual.basicAttackAnimation;
        def.basicAttackRange = visual.basicAttackRange;
        def.fbxPath = visual.mesh.resourceRelativePath;
        def.shaderPath = visual.shader.runtimePath;
        def.defaultTexturePath = visual.defaultTexture.resourceRelativePath;

        for (u32_t i = 0u;
            i < kChampionTextureSlotMax && i < ClientData::kChampionModelTextureSlotCount;
            ++i)
        {
            def.texturePath[i] = visual.textureSlots[i].resourceRelativePath;
        }

        def.spawnPosition = {
            visual.spawnPositionX,
            visual.spawnPositionY,
            visual.spawnPositionZ
        };
        def.spawnScale = visual.spawnScale;
        def.displayName = visual.displayName;
        return def;
    }

    const ChampionDef* GetCachedChampionDefs(u32_t& outCount)
    {
        static ChampionDef s_defs[kMaxCachedChampionDefs]{};
        static u32_t s_count = 0u;
        static bool_t s_bBuilt = false;

        if (!s_bBuilt)
        {
            const ClientData::ChampionModelVisualPack& pack =
                ClientData::GetChampionModelVisualPack();
            s_count = pack.modelCount < kMaxCachedChampionDefs
                ? pack.modelCount
                : kMaxCachedChampionDefs;

            for (u32_t i = 0u; i < s_count; ++i)
                s_defs[i] = MakeChampionDef(pack.models[i]);

            s_bBuilt = true;
        }

        outCount = s_count;
        return s_defs;
    }
}

const ChampionDef* FindChampionDef(eChampion champ)
{
    u32_t count = 0u;
    const ChampionDef* pDefs = GetCachedChampionDefs(count);
    for (u32_t i = 0u; i < count; ++i)
    {
        if (pDefs[i].id == champ)
            return &pDefs[i];
    }
    return nullptr;
}

const char* GetChampionDisplayName(eChampion champ)
{
    if (const ClientData::ChampionModelVisualDefinition* pVisual =
        ClientData::FindChampionModelVisualDefinition(champ))
    {
        if (pVisual->displayName)
            return pVisual->displayName;
    }

    switch (champ)
    {
    case eChampion::IRELIA: return "Irelia";
    case eChampion::YASUO: return "Yasuo";
    case eChampion::KALISTA: return "Kalista";
    case eChampion::SYLAS: return "Sylas";
    case eChampion::VIEGO: return "Viego";
    case eChampion::ANNIE: return "Annie";
    case eChampion::ASHE: return "Ashe";
    case eChampion::FIORA: return "Fiora";
    case eChampion::GAREN: return "Garen";
    case eChampion::RIVEN: return "Riven";
    case eChampion::ZED: return "Zed";
    case eChampion::EZREAL: return "Ezreal";
    case eChampion::YONE: return "Yone";
    case eChampion::JAX: return "Jax";
    case eChampion::MASTERYI: return "MasterYi";
    case eChampion::KINDRED: return "Kindred";
    case eChampion::LEESIN: return "LeeSin";
    default: return "(unnamed)";
    }
}

void RegisterAllLegacy()
{
    u32_t count = 0u;
    const ChampionDef* pDefs = GetCachedChampionDefs(count);
    for (u32_t i = 0u; i < count; ++i)
    {
        ChampionDef copy = pDefs[i];
        if (!copy.displayName)
            copy.displayName = GetChampionDisplayName(copy.id);
        CChampionRegistry::Instance().Add(copy.id, copy);
    }
}
