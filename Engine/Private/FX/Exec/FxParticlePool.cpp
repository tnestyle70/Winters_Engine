#include "FX/Exec/FxParticlePool.h"

CFxParticlePool::CFxParticlePool(u32_t capacity)
{
    Reset(capacity);
}

void CFxParticlePool::Reset(u32_t capacity)
{
    m_capacity = capacity;
    m_alive = 0;

    position.resize(m_capacity);
    velocity.resize(m_capacity);
    color.resize(m_capacity);
    size.resize(m_capacity);
    age.resize(m_capacity);
    lifetime.resize(m_capacity);

    for (u32_t i = 0; i < m_capacity; ++i)
        ResetParticle(i);
}

u32_t CFxParticlePool::Allocate(u32_t count)
{
    if (count == 0 || m_alive >= m_capacity)
        return m_alive;

    const u32_t start = m_alive;
    const u32_t available = m_capacity - m_alive;
    const u32_t allocateCount = count < available ? count : available;

    for (u32_t i = 0; i < allocateCount; ++i)
        ResetParticle(start + i);

    m_alive += allocateCount;
    return start;
}

void CFxParticlePool::KillExpired()
{
    u32_t i = 0;
    while (i < m_alive)
    {
        if (age[i] >= lifetime[i])
        {
            const u32_t last = m_alive - 1;
            if (i != last)
                MoveParticle(i, last);

            --m_alive;
            ResetParticle(last);
            continue;
        }

        ++i;
    }
}

void CFxParticlePool::ResetParticle(u32_t index)
{
    if (index >= m_capacity)
        return;

    position[index] = { 0.f, 0.f, 0.f };
    velocity[index] = { 0.f, 0.f, 0.f };
    color[index] = { 1.f, 1.f, 1.f, 1.f };
    size[index] = 1.f;
    age[index] = 0.f;
    lifetime[index] = 1.f;
}

void CFxParticlePool::MoveParticle(u32_t dst, u32_t src)
{
    position[dst] = position[src];
    velocity[dst] = velocity[src];
    color[dst] = color[src];
    size[dst] = size[src];
    age[dst] = age[src];
    lifetime[dst] = lifetime[src];
}
