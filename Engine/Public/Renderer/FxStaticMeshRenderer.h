#pragma once

#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "RHI/IRHIDevice.h"
#include "FX/FxDepthMode.h"
#include <memory>
#include <string>

class DX11Shader;
class DX11Pipeline;
class CBlendStateCache;

NS_BEGIN(Engine)

class CModel;
class CTexture;

struct FxMeshDrawParams
{
    Mat4 matWorld;
    const wchar_t* pTexturePath = nullptr;
    const wchar_t* pErodeTexturePath = nullptr;
    Vec4 vTint{ 1.f, 1.f, 1.f, 1.f };
    Vec4 vUVRect{ 0.f, 0.f, 1.f, 1.f };
    Vec2 vUVScroll{ 0.f, 0.f };
    f32_t fAlphaClip = 0.05f;
    f32_t fErodeThreshold = 0.f;
    Vec4 vStyleColorA{ 1.f, 1.f, 1.f, 1.f };
    Vec4 vStyleColorB{ 0.f, 0.f, 0.f, 1.f };
    Vec4 vRimColor{ 1.f, 1.f, 1.f, 0.f };
    Vec4 vStyleParams{ 0.f, 3.f, 0.f, 0.5f };
    Vec4 vTimeParams{ 0.f, 0.f, 0.f, 0.f };
    Vec4 vMagicScrollA{ 0.f, 0.5f, 0.1f, 0.05f };
    Vec4 vMagicShape{ 2.5f, 0.06f, 1.0f, 0.035f };
    Vec4 vMagicCore{ 2.0f, 1.0f, 2.0f, 0.f };
    u32_t iBlendPreset = 1;
    eFxDepthMode depthMode = eFxDepthMode::DepthTestWriteOn;
    bool bDepthWrite = true;
};

class WINTERS_ENGINE CFxStaticMeshRenderer final
{
public:
    struct Impl;

    ~CFxStaticMeshRenderer();

    static std::unique_ptr<CFxStaticMeshRenderer> Create(
        IRHIDevice* pDevice,
        DX11Shader* pMeshShader,
        DX11Pipeline* pMeshPipeline,
        DX11Shader* pFxMeshShader,
        DX11Pipeline* pFxMeshPipeline,
        CBlendStateCache* pBlendCache);

    bool PreloadMesh(const std::string& strFbxPath, const std::wstring& strTexturePath);
    bool PreloadMesh(const std::string& strFbxPath,
        const std::wstring& strTexturePath,
        const std::wstring& strErodeTexturePath);
    // Loading barriers must not report ready after substituting the white
    // fallback for an explicitly requested diffuse texture.
    bool PreloadMeshStrict(
        const std::string& strFbxPath,
        const std::wstring& strTexturePath);
    void BeginFrame(const Mat4& matViewProj,
        const Vec3& vCameraWorld = Vec3{ 0.f, 0.f, 0.f });
    void DrawMesh(const char* pFbxPath, const FxMeshDrawParams& params);
    void DrawMesh(const char* pFbxPath, const Mat4& matWorld);
    void EndFrame();
    void Shutdown();

private:
    CFxStaticMeshRenderer() = default;

    bool PreloadMeshInternal(
        const std::string& strFbxPath,
        const std::wstring& strTexturePath,
        const std::wstring& strErodeTexturePath,
        bool_t bRequireDiffuseTexture);

    Impl* m_pImpl = nullptr;
};

NS_END
