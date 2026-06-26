#pragma once

#include "Engine_Defines.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"

struct VisionSourceComponent
{
    f32_t  sightRange = 12.f;
    bool_t bTrueSight = false;
    bool_t bFlying = false;
    f32_t  sightRangeInConcealment = 0.f;
};

struct VisionConeComponent
{
    Vec3 forward{ 0.f, 0.f, 1.f };
    f32_t halfAngleCos = 0.8660254f;
};

struct VisibilityComponent
{
    u8_t teamVisibilityMask = 0;
    bool_t bInConcealment = false;
    EntityID concealmentId = NULL_ENTITY;
};

struct ConcealmentVolumeComponent
{
    Vec3 center{};
    f32_t radius = 4.f;
    u32_t volumeId = 0;
};

struct VisionSensorComponent
{
    f32_t remainingDuration = 90.f;
    u8_t ownerTeam = 0u;
    bool_t bControlSensor = false;
};

struct LocalPlayerVisionTag {};
