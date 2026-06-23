#pragma once

#include "ECS/Entity.h"
#include "GameContext.h"

class CWorld;

void AttachChampionSimComponents(CWorld& world, EntityID entity, eChampion champion);
