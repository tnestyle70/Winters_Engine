#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"

#include <array>
#include <vector>

constexpr u32_t FX_INVALID_PARTICLE = 0xFFFFFFFFu;

class CParticlePool final
{
public:
    CParticlePool() = default;
    ~CParticlePool() = default;

    CParticlePool(const CParticlePool&) = delete;
    CParticlePool& operator=(const CParticlePool&) = delete;

    WINTERS_ENGINE void Initialize(u32_t uMaxParticles);
    WINTERS_ENGINE void Reset();

    WINTERS_ENGINE u32_t Spawn();
    WINTERS_ENGINE bool_t KillSwapBack(u32_t uIndex);

    WINTERS_ENGINE Vec3* GetPosColumn();
    WINTERS_ENGINE Vec3* GetVelocityColumn();
    WINTERS_ENGINE Vec4* GetColorColumn();
    WINTERS_ENGINE f32_t* GetLifetimeColumn();
    WINTERS_ENGINE f32_t* GetAgeColumn();
    WINTERS_ENGINE f32_t* GetSizeColumn();
    WINTERS_ENGINE Vec2* GetUVColumn();
    WINTERS_ENGINE u32_t* GetCustomIntColumn(u32_t uSlot);
    WINTERS_ENGINE Vec4* GetCustomVec4Column(u32_t uSlot);

    WINTERS_ENGINE const Vec3* GetPosColumn() const;
    WINTERS_ENGINE const Vec3* GetVelocityColumn() const;
    WINTERS_ENGINE const Vec4* GetColorColumn() const;
    WINTERS_ENGINE const f32_t* GetLifetimeColumn() const;
    WINTERS_ENGINE const f32_t* GetAgeColumn() const;
    WINTERS_ENGINE const f32_t* GetSizeColumn() const;
    WINTERS_ENGINE const Vec2* GetUVColumn() const;
    WINTERS_ENGINE const u32_t* GetCustomIntColumn(u32_t uSlot) const;
    WINTERS_ENGINE const Vec4* GetCustomVec4Column(u32_t uSlot) const;

    WINTERS_ENGINE u32_t GetActiveCount() const;
    WINTERS_ENGINE u32_t GetCapacity() const;
    WINTERS_ENGINE bool_t IsInitialized() const;

private:
    void ResetParticle(u32_t uIndex);
    void MoveParticle(u32_t uDst, u32_t uSrc);

    u32_t m_uActiveCount = 0;
    u32_t m_uCapacity = 0;

    std::vector<Vec3> m_vecPos;
    std::vector<Vec3> m_vecVelocity;
    std::vector<Vec4> m_vecColor;
    std::vector<f32_t> m_vecLifetime;
    std::vector<f32_t> m_vecAge;
    std::vector<f32_t> m_vecSize;
    std::vector<Vec2> m_vecUV;
    std::array<std::vector<u32_t>, 4> m_arrCustomInt;
    std::array<std::vector<Vec4>, 8> m_arrCustomVec4;
};
