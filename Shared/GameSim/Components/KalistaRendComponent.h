#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersTypes.h"

#include <type_traits>

// 칼리스타 평타/Q 적중으로 대상에 박힌 창(Rend 스택)의 서버 권위 추적.
// E(Rend) 시전 시 기본 + 창당 데미지로 환산 후 소모된다.
struct KalistaRendStackComponent
{
    EntityID sourceEntity = NULL_ENTITY;  // 창을 박은 칼리스타
    u8_t stackCount = 0u;
    f32_t fRemainingSec = 0.f;            // 스택 유지 시간 (히트마다 갱신)
};

static_assert(std::is_trivially_copyable_v<KalistaRendStackComponent>);
