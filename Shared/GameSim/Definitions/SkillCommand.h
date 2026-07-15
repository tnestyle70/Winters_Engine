#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "SkillTypes.h"
#include "WintersMath.h"
#include "WintersTypes.h"

struct CastSkillCommand
{
    u8_t     slot = 0;
    u8_t     resolvedTargetMode = 0;
    u16_t    _pad = 0;
    EntityID targetEntityId = NULL_ENTITY;
    Vec3     groundPos{ 0.f, 0.f, 0.f };
    Vec3     direction{ 0.f, 0.f, 0.f };
};
