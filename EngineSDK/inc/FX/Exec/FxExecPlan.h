#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "FX/Graph/FxGraph.h"

#include <functional>
#include <string>
#include <vector>

class CFxParticlePool;

struct FxExecContext
{
    f32_t dt = 0.f;
    f32_t emitterAge = 0.f;
    u64_t seed = 0;
};

struct FxExecStep
{
    eFxNodeStage stage = eFxNodeStage::Update;
    std::function<void(CFxParticlePool&, const FxExecContext&)> fn;
};

struct CFxExecPlan
{
    std::vector<FxExecStep> spawnSteps;
    std::vector<FxExecStep> initSteps;
    std::vector<FxExecStep> updateSteps;

    eFxRenderType renderType = eFxRenderType::Billboard;
    FxMaterialDesc renderMaterial{};
    wstring_t strTexturePath;
    std::string strModelPath;
    u32_t maxParticles = 256;
};

class CFxGraphCompiler
{
public:
    WINTERS_ENGINE static bool_t Compile(
        const FxEmitterGraph& graph,
        const std::vector<u32_t>& topoOrder,
        CFxExecPlan& outPlan,
        std::string& outError);
};
