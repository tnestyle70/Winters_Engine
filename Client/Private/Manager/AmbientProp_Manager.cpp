#define _CRT_SECURE_NO_WARNINGS
#include "Manager/AmbientProp_Manager.h"

#include "WintersPaths.h"
#include "Dev/SmokeLog.h"

#include <DirectXMath.h>
#include <cstdio>

namespace
{
    constexpr wchar_t kMapAmbientPropBinPath[] =
        L"Client/Bin/Resource/Texture/MAP/Map11_Rebuild/cooked/map11_ambient_props.wamb";

    struct MapAmbientPropRecord
    {
        u32_t kind = 0;
        f32_t lolX = 0.f;
        f32_t lolY = 0.f;
        f32_t lolZ = 0.f;
        f32_t lolYaw = 0.f;
        f32_t scale = 1.f;
    };

    struct MapAmbientAssetDesc
    {
        const char* pMeshPath;
        const char* pIdleAnim;
    };

    constexpr MapAmbientAssetDesc kMapAmbientAssets[] = {
        { "Client/Bin/Resource/Texture/MAP/Map11_Rebuild/cooked/ambient/sru_bird/sru_bird.wmesh",
          "sru_bird_idle_tree1" },
        { "Client/Bin/Resource/Texture/MAP/Map11_Rebuild/cooked/ambient/sru_duck/sru_duck.wmesh",
          "sru_duck_idle1" },
    };
}

void CAmbientProp_Manager::Spawn(const Mat4& mapWorld, f32_t mapYaw,
    const std::function<void(Vec3&)>& projectToSurface)
{
    m_props.clear();

    wchar_t resolvedPath[MAX_PATH]{};
    if (!WintersResolveContentPath(kMapAmbientPropBinPath, resolvedPath, MAX_PATH))
        return;

    FILE* fp = nullptr;
    if (_wfopen_s(&fp, resolvedPath, L"rb") != 0 || !fp)
        return;

    constexpr u32_t kAmbientPropMagic = 0x424D4157u; // 'WAMB'
    u32_t header[4]{};
    if (fread(header, sizeof(header), 1, fp) != 1 ||
        header[0] != kAmbientPropMagic ||
        header[1] != 1u ||
        header[2] == 0u ||
        header[2] > 1024u)
    {
        fclose(fp);
        return;
    }

    const DirectX::XMMATRIX xmMapWorld =
        DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&mapWorld.m));

    constexpr u32_t kAssetCount =
        static_cast<u32_t>(sizeof(kMapAmbientAssets) / sizeof(kMapAmbientAssets[0]));

    for (u32_t i = 0; i < header[2]; ++i)
    {
        MapAmbientPropRecord record{};
        if (fread(&record, sizeof(record), 1, fp) != 1)
            break;
        if (record.kind >= kAssetCount)
            continue;

        const MapAmbientAssetDesc& asset = kMapAmbientAssets[record.kind];
        auto pRenderer = std::make_unique<ModelRenderer>();
        if (!pRenderer->Initialize(asset.pMeshPath, L"Shaders/Mesh3D.hlsl"))
        {
            Winters::DevSmoke::Log("[MapAmbient] init failed kind=%u\n", record.kind);
            continue;
        }
        pRenderer->PlayAnimationByName(asset.pIdleAnim, true);

        const DirectX::XMFLOAT3 lolPos{ record.lolX, record.lolY, record.lolZ };
        const DirectX::XMVECTOR vWorld = DirectX::XMVector3TransformCoord(
            DirectX::XMLoadFloat3(&lolPos), xmMapWorld);
        Vec3 worldPos{
            DirectX::XMVectorGetX(vWorld),
            DirectX::XMVectorGetY(vWorld),
            DirectX::XMVectorGetZ(vWorld)
        };
        if (projectToSurface)
            projectToSurface(worldPos);

        Prop prop{};
        prop.pRenderer = std::move(pRenderer);
        prop.transform.SetPosition(worldPos);
        // X-flip 맵이라 LoL yaw는 부호가 반전된 채 맵 회전에 더해진다.
        prop.transform.SetRotation({ 0.f, mapYaw - record.lolYaw, 0.f });
        const f32_t fScale = 0.01f * record.scale;
        prop.transform.SetScale({ fScale, fScale, fScale });
        m_props.push_back(std::move(prop));
    }

    fclose(fp);

    Winters::DevSmoke::Log(
        "[MapAmbient] spawned %zu ambient props\n", m_props.size());
}

void CAmbientProp_Manager::Tick(f32_t dt)
{
    for (Prop& prop : m_props)
    {
        if (prop.pRenderer)
            prop.pRenderer->Update(dt);
    }
}

void CAmbientProp_Manager::Render(const Mat4& matViewProjection, const Vec3& cameraWorld)
{
    for (Prop& prop : m_props)
    {
        if (!prop.pRenderer)
            continue;
        prop.pRenderer->UpdateCamera(matViewProjection, cameraWorld);
        prop.pRenderer->UpdateTransform(prop.transform.GetWorldMatrix());
        prop.pRenderer->RenderFrustumCulled(matViewProjection);
    }
}

void CAmbientProp_Manager::Shutdown()
{
    m_props.clear();
}
