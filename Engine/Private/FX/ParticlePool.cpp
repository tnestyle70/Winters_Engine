#include "FX/ParticlePool.h"

void CParticlePool::Initialize(u32_t uMaxParticles)
{
    m_uCapacity = uMaxParticles;
    m_uActiveCount = 0;

    m_vecPos.resize(m_uCapacity);
    m_vecVelocity.resize(m_uCapacity);
    m_vecColor.resize(m_uCapacity);
    m_vecLifetime.resize(m_uCapacity);
    m_vecAge.resize(m_uCapacity);
    m_vecSize.resize(m_uCapacity);
    m_vecUV.resize(m_uCapacity);

    for (std::vector<u32_t>& column : m_arrCustomInt)
        column.resize(m_uCapacity);
    for (std::vector<Vec4>& column : m_arrCustomVec4)
        column.resize(m_uCapacity);

    for (u32_t i = 0; i < m_uCapacity; ++i)
        ResetParticle(i);
}

void CParticlePool::Reset()
{
    m_uActiveCount = 0;
}

u32_t CParticlePool::Spawn()
{
    if (m_uActiveCount >= m_uCapacity)
        return FX_INVALID_PARTICLE;

    const u32_t index = m_uActiveCount++;
    ResetParticle(index);
    return index;
}

bool_t CParticlePool::KillSwapBack(u32_t uIndex)
{
    if (uIndex >= m_uActiveCount)
        return false;

    const u32_t last = m_uActiveCount - 1;
    if (uIndex != last)
        MoveParticle(uIndex, last);

    --m_uActiveCount;
    ResetParticle(last);
    return true;
}

Vec3* CParticlePool::GetPosColumn()
{
    return m_vecPos.data();
}

Vec3* CParticlePool::GetVelocityColumn()
{
    return m_vecVelocity.data();
}

Vec4* CParticlePool::GetColorColumn()
{
    return m_vecColor.data();
}

f32_t* CParticlePool::GetLifetimeColumn()
{
    return m_vecLifetime.data();
}

f32_t* CParticlePool::GetAgeColumn()
{
    return m_vecAge.data();
}

f32_t* CParticlePool::GetSizeColumn()
{
    return m_vecSize.data();
}

Vec2* CParticlePool::GetUVColumn()
{
    return m_vecUV.data();
}

u32_t* CParticlePool::GetCustomIntColumn(u32_t uSlot)
{
    if (uSlot >= m_arrCustomInt.size())
        return nullptr;
    return m_arrCustomInt[uSlot].data();
}

Vec4* CParticlePool::GetCustomVec4Column(u32_t uSlot)
{
    if (uSlot >= m_arrCustomVec4.size())
        return nullptr;
    return m_arrCustomVec4[uSlot].data();
}

const Vec3* CParticlePool::GetPosColumn() const
{
    return m_vecPos.data();
}

const Vec3* CParticlePool::GetVelocityColumn() const
{
    return m_vecVelocity.data();
}

const Vec4* CParticlePool::GetColorColumn() const
{
    return m_vecColor.data();
}

const f32_t* CParticlePool::GetLifetimeColumn() const
{
    return m_vecLifetime.data();
}

const f32_t* CParticlePool::GetAgeColumn() const
{
    return m_vecAge.data();
}

const f32_t* CParticlePool::GetSizeColumn() const
{
    return m_vecSize.data();
}

const Vec2* CParticlePool::GetUVColumn() const
{
    return m_vecUV.data();
}

const u32_t* CParticlePool::GetCustomIntColumn(u32_t uSlot) const
{
    if (uSlot >= m_arrCustomInt.size())
        return nullptr;
    return m_arrCustomInt[uSlot].data();
}

const Vec4* CParticlePool::GetCustomVec4Column(u32_t uSlot) const
{
    if (uSlot >= m_arrCustomVec4.size())
        return nullptr;
    return m_arrCustomVec4[uSlot].data();
}

u32_t CParticlePool::GetActiveCount() const
{
    return m_uActiveCount;
}

u32_t CParticlePool::GetCapacity() const
{
    return m_uCapacity;
}

bool_t CParticlePool::IsInitialized() const
{
    return m_uCapacity > 0;
}

void CParticlePool::ResetParticle(u32_t uIndex)
{
    if (uIndex >= m_uCapacity)
        return;

    m_vecPos[uIndex] = { 0.f, 0.f, 0.f };
    m_vecVelocity[uIndex] = { 0.f, 0.f, 0.f };
    m_vecColor[uIndex] = { 1.f, 1.f, 1.f, 1.f };
    m_vecLifetime[uIndex] = 0.f;
    m_vecAge[uIndex] = 0.f;
    m_vecSize[uIndex] = 1.f;
    m_vecUV[uIndex] = { 0.f, 0.f };

    for (std::vector<u32_t>& column : m_arrCustomInt)
        column[uIndex] = 0;
    for (std::vector<Vec4>& column : m_arrCustomVec4)
        column[uIndex] = { 0.f, 0.f, 0.f, 0.f };
}

void CParticlePool::MoveParticle(u32_t uDst, u32_t uSrc)
{
    m_vecPos[uDst] = m_vecPos[uSrc];
    m_vecVelocity[uDst] = m_vecVelocity[uSrc];
    m_vecColor[uDst] = m_vecColor[uSrc];
    m_vecLifetime[uDst] = m_vecLifetime[uSrc];
    m_vecAge[uDst] = m_vecAge[uSrc];
    m_vecSize[uDst] = m_vecSize[uSrc];
    m_vecUV[uDst] = m_vecUV[uSrc];

    for (std::vector<u32_t>& column : m_arrCustomInt)
        column[uDst] = column[uSrc];
    for (std::vector<Vec4>& column : m_arrCustomVec4)
        column[uDst] = column[uSrc];
}
