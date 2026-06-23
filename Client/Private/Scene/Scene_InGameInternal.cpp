#include "Scene/Scene_InGameInternal.h"

#include "GamePlay/ChampionCatalog.h"
#include "GamePlay/ChampionRegistry.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"

#include <Windows.h>
#include <cwchar>

const ChampionDef* FindClientChampionDef(eChampion champion)
{
    const ChampionCatalogEntry* pEntry = CChampionCatalog::Instance().Find(champion);
    if (pEntry && pEntry->pDef)
        return pEntry->pDef;

    const ChampionDef* pDef = CChampionRegistry::Instance().Find(champion);
    if (pDef)
        return pDef;

    return FindChampionDef(champion);
}

f32_t& LocalKalistaPassiveDashDurationSec()
{
    static f32_t s_fDurationSec =
        ChampionGameDataDB::ResolvePassiveDashDurationSec(eChampion::KALISTA);
    return s_fDurationSec;
}

void SetLocalPassiveDashDuration(f32_t duration)
{
    LocalKalistaPassiveDashDurationSec() = (duration < 0.03f) ? 0.03f : duration;
}

f32_t GetLocalPassiveDashDuration()
{
    return LocalKalistaPassiveDashDurationSec();
}

bool_t HasCommandLineToken(const wchar_t* token)
{
    const wchar_t* cmd = GetCommandLineW();
    return cmd != nullptr && token != nullptr && std::wcsstr(cmd, token) != nullptr;
}
