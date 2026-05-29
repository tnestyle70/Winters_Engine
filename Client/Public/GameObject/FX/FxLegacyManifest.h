#pragma once

#include "Defines.h"

#include <vector>

namespace LegacyFx
{
    struct FxLegacyManifestEntry
    {
        const char* pszId = nullptr;
        const char* pszChampion = nullptr;
        const char* pszSourceFunction = nullptr;
        const char* pszSourceFile = nullptr;
        const char* pszRenderTypes = nullptr;
        const tchar_t* pszAssetPath = nullptr;
        const tchar_t* pszMaterialInstancePath = nullptr;
        const char* pszMigrationState = nullptr;
    };

    const std::vector<FxLegacyManifestEntry>& GetSeedManifestEntries();
}

