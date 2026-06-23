#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"

#include <vector>

class CFxParticlePool
{
public:
    WINTERS_ENGINE explicit CFxParticlePool(u32_t capacity = 0);

    WINTERS_ENGINE void Reset(u32_t capacity);
    WINTERS_ENGINE u32_t Allocate(u32_t count);
    WINTERS_ENGINE void KillExpired();

    u32_t AliveCount() const { return m_alive; }
    u32_t Capacity() const { return m_capacity; }

    std::vector<Vec3> position;
    std::vector<Vec3> velocity;
    std::vector<Vec4> color;
    std::vector<f32_t> size;
    std::vector<f32_t> age;
    std::vector<f32_t> lifetime;

private:
    void ResetParticle(u32_t index);
    void MoveParticle(u32_t dst, u32_t src);

    u32_t m_capacity = 0;
    u32_t m_alive = 0;
};
