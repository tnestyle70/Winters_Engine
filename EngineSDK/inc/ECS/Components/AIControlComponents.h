#pragma once

#include "ECS/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <vector>

enum class eAIIntentKind : u8_t
{
    None = 0,
    Move,
    Attack,
    CastSkill,
    Recall,
};

struct AIIntent
{
    eAIIntentKind eKind = eAIIntentKind::None;
    u8_t iSlot = 0u;
    Vec3 vTargetPos{};
    EntityID TargetEntity = NULL_ENTITY;
};

struct AIIntentQueueComponent
{
    std::vector<AIIntent> Intents;
    bool_t bActive = false;

    void Push(const AIIntent& Intent)
    {
        Intents.push_back(Intent);
        bActive = true;
    }
};

struct AIResourceStateComponent
{
    f32_t fMana = 0.f;
    f32_t fMaxMana = 0.f;
    f32_t fCooldowns[4]{};
};
