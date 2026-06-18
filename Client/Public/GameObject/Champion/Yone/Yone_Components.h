#pragma once

#include "Defines.h"
#include "ECS/Entity.h"
#include "WintersMath.h"

struct YoneStateComponent
{
    bool_t bEActive = false;
    f32_t fETimer = 0.f;
    f32_t fEDurationSec = 5.f;
    Vec3 vOriginalPosition{};
    EntityID soulEntity = NULL_ENTITY;
    EntityID soulGlowEntity = NULL_ENTITY;
    EntityID soulEyeTrailLeftEntity = NULL_ENTITY;
    EntityID soulEyeTrailRightEntity = NULL_ENTITY;
};

enum class eYoneSoulRequestAction : u8_t
{
    Spawn = 0,
    Despawn = 1
};

struct YoneSoulRequestComponent
{
    eYoneSoulRequestAction action = eYoneSoulRequestAction::Spawn;
    f32_t fDurationSec = 5.f;
    Vec3 vSpawnPosition{};
};
