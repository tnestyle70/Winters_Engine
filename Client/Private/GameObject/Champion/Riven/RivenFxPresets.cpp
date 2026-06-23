#include "GameObject/Champion/Riven/RivenFxPresets.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "Client/Private/Data/LoLVisualDefinitionPack.h"

namespace
{
    constexpr const char* kCueQSlashA = "Riven.Q.SlashA";
    constexpr const char* kCueQSlashB = "Riven.Q.SlashB";
    constexpr const char* kCueQSlashC = "Riven.Q.SlashC";
    constexpr const char* kCueWNova = "Riven.W.Nova";
    constexpr const char* kCueEShield = "Riven.E.Shield";
    constexpr const char* kCueRActivate = "Riven.R.Activate";
    constexpr const char* kCueRWindSlash = "Riven.R.WindSlash";

    struct RivenCueAnchor
    {
        Vec3 vWorldPos{ 0.f, 0.f, 0.f };
        Vec3 vForward{ 0.f, 0.f, 1.f };
    };

    RivenCueAnchor ResolveCueAnchor(CWorld& world, EntityID owner)
    {
        RivenCueAnchor anchor{};
        if (owner != NULL_ENTITY && world.HasComponent<TransformComponent>(owner))
        {
            const TransformComponent& tf = world.GetComponent<TransformComponent>(owner);
            anchor.vWorldPos = tf.GetPosition();
            const f32_t yaw =
                tf.GetRotation().y - ClientData::ResolveChampionModelYawOffset(eChampion::RIVEN);
            anchor.vForward = WintersMath::DirectionFromYawXZ(yaw);
        }

        return anchor;
    }

    const char* ResolveQSlashCue(uint8_t stackIndex)
    {
        if (stackIndex >= 2)
            return kCueQSlashC;
        if (stackIndex == 1)
            return kCueQSlashB;
        return kCueQSlashA;
    }

    void PlayRivenCue(CWorld& world, EntityID owner, const char* pszCueName,
        bool_t bAttachToOwner, Engine::CFxStaticMeshRenderer* pRenderer,
        f32_t fForwardVelocity = 0.f)
    {
        if (owner == NULL_ENTITY || !pszCueName)
            return;

        const RivenCueAnchor anchor = ResolveCueAnchor(world, owner);

        FxCueContext cue{};
        cue.vWorldPos = anchor.vWorldPos;
        cue.vForward = anchor.vForward;
        cue.attachTo = bAttachToOwner ? owner : NULL_ENTITY;
        cue.pFxMeshRenderer = pRenderer;
        if (fForwardVelocity != 0.f)
        {
            cue.vVelocity = {
                anchor.vForward.x * fForwardVelocity,
                0.f,
                anchor.vForward.z * fForwardVelocity
            };
            cue.bOverrideVelocity = true;
        }

        CFxCuePlayer::PlayAll(world, pszCueName, cue, nullptr);
    }
}

void RivenFx::SpawnQSlash(CWorld& world, EntityID owner, uint8_t stackIndex,
    f32_t fLifetime, Engine::CFxStaticMeshRenderer* pRenderer)
{
    (void)fLifetime;
    PlayRivenCue(world, owner, ResolveQSlashCue(stackIndex), false, pRenderer);
}

void RivenFx::SpawnWNova(CWorld& world, EntityID owner, f32_t fLifetime,
    Engine::CFxStaticMeshRenderer* pRenderer)
{
    (void)fLifetime;
    PlayRivenCue(world, owner, kCueWNova, true, pRenderer);
}

void RivenFx::SpawnEShield(CWorld& world, EntityID owner, f32_t fDuration,
    Engine::CFxStaticMeshRenderer* pRenderer)
{
    (void)fDuration;
    PlayRivenCue(world, owner, kCueEShield, true, pRenderer);
}

void RivenFx::SpawnRActivate(CWorld& world, EntityID owner, f32_t fLifetime,
    Engine::CFxStaticMeshRenderer* pRenderer)
{
    (void)fLifetime;
    PlayRivenCue(world, owner, kCueRActivate, true, pRenderer);
}

void RivenFx::SpawnRWindSlash(CWorld& world, EntityID owner, f32_t fLifetime,
    Engine::CFxStaticMeshRenderer* pRenderer)
{
    (void)fLifetime;
    PlayRivenCue(world, owner, kCueRWindSlash, false, pRenderer, 10.5f);
}
