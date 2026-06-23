#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "FX/FxAsset.h"

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

enum class eFxNodeStage : u8_t
{
    Spawn,
    Init,
    Update,
    Render,
};

enum class eFxNodeType : u8_t
{
    SpawnBurst,
    SpawnRate,
    InitPosition,
    InitVelocity,
    InitLifetime,
    InitColor,
    Age,
    Gravity,
    Drag,
    SizeOverLife,
    ColorOverLife,
    BillboardRenderer,
    MeshRenderer,
    RibbonRenderer,
};

enum class eFxParamScope : u8_t
{
    User,
    System,
    Emitter,
    Particle,
    Engine,
};

enum class eFxGraphParamType : u8_t
{
    Float,
    Int,
    UInt,
    Bool,
    Vec3,
    Vec4,
    String,
};

using FxGraphParamValue = std::variant<f32_t, i32_t, u32_t, bool_t, Vec3, Vec4, std::string>;

struct FxGraphCurveKey
{
    f32_t fTime = 0.f;
    Vec4 vValue = { 0.f, 0.f, 0.f, 0.f };
};

struct FxGraphNode
{
    u32_t id = 0;
    eFxNodeType type = eFxNodeType::SpawnBurst;
    f32_t x = 0.f;
    f32_t y = 0.f;
    std::unordered_map<std::string, FxGraphParamValue> params;
    std::vector<FxGraphCurveKey> curve;
};

struct FxGraphEdge
{
    u32_t from = 0;
    u32_t to = 0;
};

struct FxGraphUserParam
{
    std::string name;
    eFxParamScope scope = eFxParamScope::User;
    eFxGraphParamType type = eFxGraphParamType::Float;
    FxGraphParamValue value = f32_t{ 0.f };
};

using FxGraphParam = FxGraphUserParam;

struct FxEmitterGraph
{
    std::string strName;
    eFxRenderType renderType = eFxRenderType::Billboard;
    std::vector<FxGraphNode> nodes;
    std::vector<FxGraphEdge> edges;
};

struct CFxGraph
{
    std::vector<FxGraphUserParam> userParams;
    std::vector<FxEmitterGraph> emitterGraphs;

    WINTERS_ENGINE static bool_t LoadFromJson(
        const std::string& strPath,
        CFxGraph& outGraph,
        std::string* pOutError = nullptr);

    WINTERS_ENGINE bool_t SaveToJson(
        const std::string& strPath,
        std::string* pOutError = nullptr) const;

    const FxGraphNode* FindNode(const FxEmitterGraph& emitter, u32_t id) const
    {
        for (const FxGraphNode& node : emitter.nodes)
        {
            if (node.id == id)
                return &node;
        }

        return nullptr;
    }
};

inline bool_t FxIsRenderNode(eFxNodeType type)
{
    return type == eFxNodeType::BillboardRenderer ||
        type == eFxNodeType::MeshRenderer ||
        type == eFxNodeType::RibbonRenderer;
}

inline eFxNodeStage FxStageFromNodeType(eFxNodeType type)
{
    switch (type)
    {
    case eFxNodeType::SpawnBurst:
    case eFxNodeType::SpawnRate:
        return eFxNodeStage::Spawn;
    case eFxNodeType::InitPosition:
    case eFxNodeType::InitVelocity:
    case eFxNodeType::InitLifetime:
    case eFxNodeType::InitColor:
        return eFxNodeStage::Init;
    case eFxNodeType::BillboardRenderer:
    case eFxNodeType::MeshRenderer:
    case eFxNodeType::RibbonRenderer:
        return eFxNodeStage::Render;
    default:
        return eFxNodeStage::Update;
    }
}
