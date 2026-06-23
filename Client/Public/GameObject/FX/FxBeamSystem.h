#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "GameObject/FX/FxBeamComponent.h"
#include "GameObject/FX/FxRibbonComponent.h"
#include "FX/FxAsset.h"
#include "RHI/IRHIDevice.h"
#include "Renderer/RHIFxSpriteRenderer.h"
#include "Renderer/FxShaderConstants.h"
#include <memory>
#include <string>
#include <unordered_map>

class CWorld;
class CPlaneRenderer;
class CBlendStateCache;
class CDynamicCamera;

namespace Engine
{
    class CTexture;
}

class CFxBeamSystem final
{
public:
    ~CFxBeamSystem() = default;

    static std::unique_ptr<CFxBeamSystem> Create(
        IRHIDevice* pDevice,
        CBlendStateCache* pBlendCache);

    void Update(CWorld& world, f32_t fTimeDelta);
    void Render(CWorld& world, const CDynamicCamera* pCamera);

    static EntityID Spawn(CWorld& world, const FxBeamComponent& tmpl);
    static EntityID Spawn(CWorld& world, const FxRibbonComponent& tmpl);
    static EntityID SpawnFromAsset(CWorld& world, const FxAsset& asset,
        const Vec3& vWorldPos, EntityID attachTo = NULL_ENTITY);
    static EntityID SpawnFromAsset(CWorld& world, const CFxAssetRegistry& registry,
        FxAssetHandle handle, const Vec3& vWorldPos,
        EntityID attachTo = NULL_ENTITY);

    void Shutdown();

private:
    CFxBeamSystem() = default;

    Engine::CTexture* GetOrLoadTexture(const wchar_t* wszPath);
    RHITextureHandle GetOrLoadRHITexture(const wchar_t* wszPath);
    bool_t DrawSegment(
        const Vec3& vStart,
        const Vec3& vEnd,
        f32_t fWidth,
        const wchar_t* wszTexturePath,
        eBlendPreset blendMode,
        eFxDepthMode depthMode,
        const CBFxParams& fxParams,
        const Mat4& matVP,
        bool_t bUsePlaneBatch);

    std::unique_ptr<CPlaneRenderer> m_pPlane = { nullptr };
    std::unique_ptr<CRHIFxSpriteRenderer> m_pRHISprite = { nullptr };
    std::unordered_map<std::wstring,
        std::unique_ptr<Engine::CTexture>> m_TextureCache = {};
    std::unordered_map<std::wstring, RHITextureHandle> m_RHITextureCache = {};
    IRHIDevice* m_pDevice = { nullptr };
    CBlendStateCache* m_pBlendCache = { nullptr };
};
