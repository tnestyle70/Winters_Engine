#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersMath.h"
#include "ECS/ISystem.h"
#include "ECS/Entity.h"
#include "ECS/Components/GameplayComponents.h"
#include <memory>
#include <vector>

class CJobSystem;
class CWorld;

NS_BEGIN(Engine)

class WINTERS_ENGINE CMinionAISystem final : public ISystem
{
public:
    ~CMinionAISystem() override = default;

    static std::unique_ptr<CMinionAISystem> Create()
    {
        return std::unique_ptr<CMinionAISystem>(new CMinionAISystem());
    }

    uint32_t    GetPhase() const override { return 2; }   // Transform(0) -> Spatial(1) -> MinionAI(2)
    const char* GetName()  const override { return "MinionAISystem"; }
    void        DescribeAccess(CSystemAccessBuilder& builder) const override;
    void        Execute(CWorld& world, f32_t fTimeDelta) override;

    void Set_JobSystem(CJobSystem* pJS);

private:
    struct MinionDecision
    {
        EntityID self = NULL_ENTITY;
        EntityID target = NULL_ENTITY;
        MinionStateComponent::State desiredState = MinionStateComponent::Idle;
        Vec3 navTarget{};
        bool_t bSetNavTarget = false;
        bool_t bStopMovement = false;
        bool_t bStartAttack = false;
        f32_t cooldownAfterTick = 0.f;
        f32_t targetScanCooldownAfterTick = 0.f;
    };

    struct DamageEvent
    {
        EntityID source = NULL_ENTITY;
        EntityID target = NULL_ENTITY;
        f32_t amount = 0.f;
        bool_t bKill = true;
    };

    CMinionAISystem() = default;

    void     Ensure_SlotBuffers();
    void     DecisionPass(CWorld& world, EntityID id, f32_t dt);
    void     ApplyPass(CWorld& world, f32_t dt);
    void     Push_Decision(const MinionDecision& dec);
    void     QueueDamage(EntityID source, EntityID target, f32_t amount, bool_t bKill);
    void     Apply_Damage(CWorld& world, const DamageEvent& evt);
    EntityID FindClosestEnemy(CWorld& world, EntityID self,
        const Vec3& myPos, uint8_t myTeamRaw, f32_t searchRange);

    CJobSystem* m_pJobSystem = nullptr;
    std::vector<std::vector<MinionDecision>> m_vecDecisionsPerSlot;
    std::vector<std::vector<DamageEvent>>    m_vecDamagesPerSlot;
};

NS_END
