#pragma once

#include "ECS/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

namespace Engine
{
    enum class UIWorldHealthBarKind : u8_t
    {
        Character = 0,
        Unit = 1,
        Structure = 2,
    };

    struct UIWorldHealthBarDesc
    {
        EntityID Entity = NULL_ENTITY;
        UIWorldHealthBarKind Kind = UIWorldHealthBarKind::Character;
        Vec3 vWorldPos{};
        f32_t fCurrent = 0.f;
        f32_t fMaximum = 0.f;
        f32_t fManaCurrent = 0.f;
        f32_t fManaMaximum = 0.f;
        u8_t iTeam = 255u;
        bool_t bDead = false;
    };
}
