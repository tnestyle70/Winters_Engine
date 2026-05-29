#pragma once

#include "Defines.h"
#include "FX/FxAsset.h"
#include "GameObject/FX/FxLegacyManifest.h"

#include <vector>

namespace LegacyFx
{
    bool SaveManifest(const wstring_t& strPath,
        const std::vector<FxLegacyManifestEntry>& entries);
    bool SaveSeedManifest(const wstring_t& strPath);
    bool SaveAssetAsWfx(const wstring_t& strPath, const FxAsset& asset);
}

