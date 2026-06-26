#include "WintersPCH.h"
#include "AI/RLBridge.h"
#include "AI/MCTSPlanner.h"
#include "ECS/World.h"
#include "ECS/Components/AIControlComponents.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/TransformComponent.h"

#include <algorithm>

NS_BEGIN(Engine)

struct CRLBridge::Impl
{
    bool_t bDummy = true;
};

std::unique_ptr<CRLBridge> CRLBridge::Create()
{
    auto pBridge = std::unique_ptr<CRLBridge>(new CRLBridge());
    pBridge->m_pImpl = std::make_unique<Impl>();
    return pBridge;
}

CRLBridge::~CRLBridge() = default;

bool CRLBridge::LoadModel(const std::string&)
{
    m_bLoaded = false;
    return false;
}

void CRLBridge::EncodeState(CWorld& world, EntityID self, std::vector<f32_t>& out) const
{
    out.assign(STATE_DIM, 0.f);
    if (!world.HasComponent<TransformComponent>(self) ||
        !world.HasComponent<HealthComponent>(self))
    {
        return;
    }

    const Vec3 myPos = world.GetComponent<TransformComponent>(self).GetPosition();
    const HealthComponent& hp = world.GetComponent<HealthComponent>(self);

    out[0] = myPos.x / 280.f;
    out[1] = myPos.y;
    out[2] = myPos.z / 280.f;
    out[3] = (hp.fMaximum > 0.f) ? (hp.fCurrent / hp.fMaximum) : 0.f;

    if (world.HasComponent<AIResourceStateComponent>(self))
    {
        const AIResourceStateComponent& resource =
            world.GetComponent<AIResourceStateComponent>(self);
        for (u32_t i = 0; i < 4; ++i)
            out[4 + i] = resource.fCooldowns[i];
        out[8] = (resource.fMaxMana > 0.f)
            ? (resource.fMana / resource.fMaxMana)
            : 0.f;
    }
}

bool CRLBridge::Infer(const std::vector<f32_t>&, std::vector<f32_t>& outLogits)
{
    if (!m_bLoaded)
    {
        outLogits.assign(ACTION_DIM, 0.f);
        return false;
    }

    return false;
}

i32_t CRLBridge::BestAction(const std::vector<f32_t>& logits) const
{
    if (logits.empty())
        return 0;

    auto it = std::max_element(logits.begin(), logits.end());
    return static_cast<i32_t>(std::distance(logits.begin(), it));
}

NS_END
