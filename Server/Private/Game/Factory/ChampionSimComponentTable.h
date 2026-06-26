#pragma once

#include "ECS/Entity.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"

class CWorld;

void AttachChampionSimComponents(CWorld& world, EntityID entity, eChampion champion);
