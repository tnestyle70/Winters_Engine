#pragma once

#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <vector>

NS_BEGIN(Engine)

class CMapSurfaceSampler;
class CNavGrid;

struct MapWalkableBakeDesc
{
    f32_t playableBaseY = 0.5f;
    f32_t playableHeightBand = 1.25f;
    f32_t minNormalY = 0.60f;
    f32_t maxStepHeight = 0.65f;
    f32_t maxWorldY = 20.0f;
    i32_t agentRadiusCells = 1;
};

class WINTERS_ENGINE CMapWalkableBaker final
{
public:
    static bool_t BakeIntoNavGrid(
        const CMapSurfaceSampler& surface,
        CNavGrid& navGrid,
        const std::vector<Vec3>& playableSeeds,
        const MapWalkableBakeDesc& desc = {});

private:
    CMapWalkableBaker() = delete;
};

NS_END
