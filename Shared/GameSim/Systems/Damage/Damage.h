#pragma once

#include "Shared/GameSim/Components/DamageRequestComponent.h"

class CWorld;

void ApplyDamage(CWorld& world, EntityID source, eTeam srcTeam,
    EntityID target, f32_t amount);

void EnqueueDamage(CWorld& world, const DamageRequestComponent& req);
