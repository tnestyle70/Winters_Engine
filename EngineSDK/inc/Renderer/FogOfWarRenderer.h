#pragma once

#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "RHI/IRHIDevice.h"

#include <memory>

#pragma warning(push)
#pragma warning(disable: 4251)

class WINTERS_ENGINE CFogOfWarRenderer
{
public:
    static std::unique_ptr<CFogOfWarRenderer> Create(IRHIDevice* pDevice, u32_t dim);
    ~CFogOfWarRenderer();

    CFogOfWarRenderer(const CFogOfWarRenderer&) = delete;
    CFogOfWarRenderer& operator=(const CFogOfWarRenderer&) = delete;

    void UpdateTexture(const u8_t* pData, u32_t dim);
    void RenderWorldOverlay(IRHIDevice* pDevice, const Mat4& matViewProj,
        const Vec2& vWorldAtUv00,
        const Vec2& vWorldAtUv10,
        const Vec2& vWorldAtUv01,
        f32_t fYOffset);
    void* GetNativeSRV() const;
    void* GetMinimapOverlaySRV() const;
private:
    CFogOfWarRenderer() = default;

    struct Impl;
    std::unique_ptr<Impl> m_pImpl{};
};

#pragma warning(pop)
