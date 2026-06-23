#include "FX/Exec/FxExecPlan.h"

#include "FX/Exec/FxParticlePool.h"
#include "FX/Graph/FxGraphValidator.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace
{
    const FxGraphNode* FindNode(const FxEmitterGraph& graph, u32_t id)
    {
        for (const FxGraphNode& node : graph.nodes)
        {
            if (node.id == id)
                return &node;
        }

        return nullptr;
    }

    const FxGraphParamValue* FindParam(const FxGraphNode& node, const char* name)
    {
        const auto it = node.params.find(name);
        return it != node.params.end() ? &it->second : nullptr;
    }

    bool_t ReadU32Param(const FxGraphNode& node, const char* name, u32_t& outValue)
    {
        const FxGraphParamValue* value = FindParam(node, name);
        if (!value)
            return false;

        if (const u32_t* pUInt = std::get_if<u32_t>(value))
        {
            outValue = *pUInt;
            return true;
        }

        if (const i32_t* pInt = std::get_if<i32_t>(value))
        {
            if (*pInt < 0)
                return false;

            outValue = static_cast<u32_t>(*pInt);
            return true;
        }

        if (const f32_t* pFloat = std::get_if<f32_t>(value))
        {
            if (*pFloat < 0.f)
                return false;

            outValue = static_cast<u32_t>(*pFloat);
            return true;
        }

        return false;
    }

    bool_t ReadF32Param(const FxGraphNode& node, const char* name, f32_t& outValue)
    {
        const FxGraphParamValue* value = FindParam(node, name);
        if (!value)
            return false;

        if (const f32_t* pFloat = std::get_if<f32_t>(value))
        {
            outValue = *pFloat;
            return true;
        }

        if (const i32_t* pInt = std::get_if<i32_t>(value))
        {
            outValue = static_cast<f32_t>(*pInt);
            return true;
        }

        if (const u32_t* pUInt = std::get_if<u32_t>(value))
        {
            outValue = static_cast<f32_t>(*pUInt);
            return true;
        }

        return false;
    }

    bool_t ReadVec3Param(const FxGraphNode& node, const char* name, Vec3& outValue)
    {
        const FxGraphParamValue* value = FindParam(node, name);
        if (!value)
            return false;

        const Vec3* pVec = std::get_if<Vec3>(value);
        if (!pVec)
            return false;

        outValue = *pVec;
        return true;
    }

    bool_t ReadVec4Param(const FxGraphNode& node, const char* name, Vec4& outValue)
    {
        const FxGraphParamValue* value = FindParam(node, name);
        if (!value)
            return false;

        const Vec4* pVec = std::get_if<Vec4>(value);
        if (!pVec)
            return false;

        outValue = *pVec;
        return true;
    }

    bool_t ReadStringParam(const FxGraphNode& node, const char* name, std::string& outValue)
    {
        const FxGraphParamValue* value = FindParam(node, name);
        if (!value)
            return false;

        const std::string* pString = std::get_if<std::string>(value);
        if (!pString)
            return false;

        outValue = *pString;
        return true;
    }

    wstring_t WidenAscii(const std::string& value)
    {
        return wstring_t(value.begin(), value.end());
    }

    void AppendStep(CFxExecPlan& plan, eFxNodeStage stage, FxExecStep step)
    {
        step.stage = stage;

        switch (stage)
        {
        case eFxNodeStage::Spawn:
            plan.spawnSteps.push_back(std::move(step));
            break;
        case eFxNodeStage::Init:
            plan.initSteps.push_back(std::move(step));
            break;
        case eFxNodeStage::Update:
            plan.updateSteps.push_back(std::move(step));
            break;
        default:
            break;
        }
    }

    void AppendCompiledNodeStep(const FxGraphNode& node, CFxExecPlan& plan)
    {
        FxExecStep step{};

        switch (node.type)
        {
        case eFxNodeType::SpawnBurst:
        {
            u32_t count = 1;
            ReadU32Param(node, "count", count);
            count = std::clamp(count, 1u, 1000000u);
            step.fn = [count](CFxParticlePool& pool, const FxExecContext&)
            {
                pool.Allocate(count);
            };
            AppendStep(plan, eFxNodeStage::Spawn, std::move(step));
            break;
        }
        case eFxNodeType::SpawnRate:
        {
            f32_t rate = 0.f;
            ReadF32Param(node, "rate", rate);
            rate = std::max(rate, 0.f);
            step.fn = [rate](CFxParticlePool& pool, const FxExecContext& ctx)
            {
                const u32_t count = static_cast<u32_t>(std::max(0.f, rate * ctx.dt));
                if (count > 0)
                    pool.Allocate(count);
            };
            AppendStep(plan, eFxNodeStage::Spawn, std::move(step));
            break;
        }
        case eFxNodeType::InitVelocity:
        {
            Vec3 velocity{ 0.f, 0.f, 0.f };
            ReadVec3Param(node, "velocity", velocity);

            f32_t speed = 0.f;
            if (ReadF32Param(node, "speed", speed) && velocity.x == 0.f && velocity.y == 0.f && velocity.z == 0.f)
                velocity = Vec3{ 0.f, speed, 0.f };

            step.fn = [velocity](CFxParticlePool& pool, const FxExecContext&)
            {
                const u32_t alive = pool.AliveCount();
                for (u32_t i = 0; i < alive; ++i)
                    pool.velocity[i] = velocity;
            };
            AppendStep(plan, eFxNodeStage::Init, std::move(step));
            break;
        }
        case eFxNodeType::InitLifetime:
        {
            f32_t minLifetime = 1.f;
            f32_t maxLifetime = 1.f;
            ReadF32Param(node, "min", minLifetime);
            ReadF32Param(node, "max", maxLifetime);
            if (maxLifetime < minLifetime)
                maxLifetime = minLifetime;
            const f32_t lifetime = std::max(0.001f, (minLifetime + maxLifetime) * 0.5f);
            step.fn = [lifetime](CFxParticlePool& pool, const FxExecContext&)
            {
                const u32_t alive = pool.AliveCount();
                for (u32_t i = 0; i < alive; ++i)
                    pool.lifetime[i] = lifetime;
            };
            AppendStep(plan, eFxNodeStage::Init, std::move(step));
            break;
        }
        case eFxNodeType::InitColor:
        {
            Vec4 color{ 1.f, 1.f, 1.f, 1.f };
            ReadVec4Param(node, "color", color);
            step.fn = [color](CFxParticlePool& pool, const FxExecContext&)
            {
                const u32_t alive = pool.AliveCount();
                for (u32_t i = 0; i < alive; ++i)
                    pool.color[i] = color;
            };
            AppendStep(plan, eFxNodeStage::Init, std::move(step));
            break;
        }
        case eFxNodeType::Age:
        {
            step.fn = [](CFxParticlePool& pool, const FxExecContext& ctx)
            {
                const u32_t alive = pool.AliveCount();
                for (u32_t i = 0; i < alive; ++i)
                    pool.age[i] += ctx.dt;
                pool.KillExpired();
            };
            AppendStep(plan, eFxNodeStage::Update, std::move(step));
            break;
        }
        case eFxNodeType::Gravity:
        {
            f32_t gravity = -9.8f;
            ReadF32Param(node, "g", gravity);
            step.fn = [gravity](CFxParticlePool& pool, const FxExecContext& ctx)
            {
                const u32_t alive = pool.AliveCount();
                for (u32_t i = 0; i < alive; ++i)
                {
                    pool.velocity[i].y += gravity * ctx.dt;
                    pool.position[i] += pool.velocity[i] * ctx.dt;
                }
            };
            AppendStep(plan, eFxNodeStage::Update, std::move(step));
            break;
        }
        case eFxNodeType::Drag:
        {
            f32_t drag = 0.f;
            ReadF32Param(node, "k", drag);
            drag = std::max(drag, 0.f);
            step.fn = [drag](CFxParticlePool& pool, const FxExecContext& ctx)
            {
                const f32_t scale = std::max(0.f, 1.f - drag * ctx.dt);
                const u32_t alive = pool.AliveCount();
                for (u32_t i = 0; i < alive; ++i)
                    pool.velocity[i] = pool.velocity[i] * scale;
            };
            AppendStep(plan, eFxNodeStage::Update, std::move(step));
            break;
        }
        default:
            AppendStep(plan, FxStageFromNodeType(node.type), std::move(step));
            break;
        }
    }

    void ApplyRenderNodeDefaults(const FxGraphNode& node, CFxExecPlan& plan)
    {
        switch (node.type)
        {
        case eFxNodeType::MeshRenderer:
            plan.renderType = eFxRenderType::MeshParticle;
            break;
        case eFxNodeType::RibbonRenderer:
            plan.renderType = eFxRenderType::Ribbon;
            break;
        default:
            plan.renderType = eFxRenderType::Billboard;
            break;
        }

        Vec4 tint{};
        if (ReadVec4Param(node, "color", tint) || ReadVec4Param(node, "tint", tint))
            plan.renderMaterial.vTint = tint;

        u32_t styleMode = 0;
        if (ReadU32Param(node, "styleMode", styleMode) || ReadU32Param(node, "style_mode", styleMode))
            plan.renderMaterial.iStyleMode = styleMode;

        std::string path;
        if (ReadStringParam(node, "texture", path) || ReadStringParam(node, "material", path))
            plan.strTexturePath = WidenAscii(path);

        if (ReadStringParam(node, "model", path))
            plan.strModelPath = path;
    }

    std::string FormatValidationErrors(const FxValidationResult& validation)
    {
        std::ostringstream oss;
        for (size_t i = 0; i < validation.issues.size(); ++i)
        {
            if (i > 0)
                oss << "; ";
            oss << validation.issues[i].message;
            if (validation.issues[i].nodeId != 0)
                oss << " node=" << validation.issues[i].nodeId;
        }
        return oss.str();
    }
}

bool_t CFxGraphCompiler::Compile(
    const FxEmitterGraph& graph,
    const std::vector<u32_t>& topoOrder,
    CFxExecPlan& outPlan,
    std::string& outError)
{
    outPlan = CFxExecPlan{};
    outError.clear();

    FxValidationResult validation = CFxGraphValidator::Validate(graph);
    if (!validation.bValid)
    {
        outError = FormatValidationErrors(validation);
        if (outError.empty())
            outError = "fx_graph_validation_failed";
        return false;
    }

    const std::vector<u32_t>& order = topoOrder.empty() ? validation.topoOrder : topoOrder;
    outPlan.renderType = graph.renderType;

    bool_t bHasRenderNode = false;
    for (const u32_t nodeId : order)
    {
        const FxGraphNode* pNode = FindNode(graph, nodeId);
        if (!pNode)
        {
            outError = "topo_node_missing";
            return false;
        }

        u32_t maxParticles = 0;
        if (ReadU32Param(*pNode, "maxParticles", maxParticles) ||
            ReadU32Param(*pNode, "max_particles", maxParticles))
        {
            outPlan.maxParticles = std::clamp(maxParticles, 1u, 1000000u);
        }

        if (pNode->type == eFxNodeType::SpawnBurst)
        {
            u32_t count = 1;
            ReadU32Param(*pNode, "count", count);
            outPlan.maxParticles = std::max(outPlan.maxParticles, std::clamp(count, 1u, 1000000u));
        }

        const eFxNodeStage stage = FxStageFromNodeType(pNode->type);
        if (stage == eFxNodeStage::Render)
        {
            bHasRenderNode = true;
            ApplyRenderNodeDefaults(*pNode, outPlan);
        }
        else
        {
            AppendCompiledNodeStep(*pNode, outPlan);
        }
    }

    if (!bHasRenderNode && !graph.nodes.empty())
    {
        outError = "missing_render_node";
        return false;
    }

    outPlan.maxParticles = std::max(outPlan.maxParticles, 1u);
    return true;
}
