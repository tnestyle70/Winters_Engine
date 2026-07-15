#define _CRT_SECURE_NO_WARNINGS
#include "Manager/AmbientProp_Manager.h"

#include "Client/Private/Data/LoLVisualDefinitionPack.h"
#include "WintersPaths.h"
#include "ProfilerAPI.h"

#include <cmath>
#include <cstdio>

namespace
{
    struct MapAmbientPropRecord
    {
        u32_t kind = 0;
        f32_t lolX = 0.f;
        f32_t lolY = 0.f;
        f32_t lolZ = 0.f;
        f32_t lolYaw = 0.f;
        f32_t scale = 1.f;
    };

    constexpr f32_t kLolMap11ToStage = 0.01f * 0.70710678118f;
    constexpr u32_t kFireflyKind = 2u;

    Vec3 ConvertMap11LoLToStage(const MapAmbientPropRecord& record)
    {
        return {
            (record.lolX + record.lolZ) * kLolMap11ToStage,
            record.lolY * 0.01f,
            (record.lolX - record.lolZ) * kLolMap11ToStage
        };
    }

    f32_t ConvertMap11LoLYawToStage(f32_t lolYaw)
    {
        const f32_t rawX = std::sin(lolYaw);
        const f32_t rawZ = std::cos(lolYaw);
        const f32_t stageX = rawX + rawZ;
        const f32_t stageZ = rawX - rawZ;
        return std::atan2(stageX, stageZ);
    }
}

void CAmbientProp_Manager::Spawn(
    const std::function<void(Vec3&)>& projectToSurface)
{
    m_props.clear();

    const ClientData::AmbientPropVisualPack& visualPack =
        ClientData::GetAmbientPropVisualPack();

    wchar_t resolvedPath[MAX_PATH]{};
    if (!visualPack.placement.resourceRelativePath ||
        !WintersResolveContentPath(visualPack.placement.resourceRelativePath, resolvedPath, MAX_PATH))
    {
        return;
    }

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

    for (u32_t i = 0; i < header[2]; ++i)
    {
        MapAmbientPropRecord record{};
        if (fread(&record, sizeof(record), 1, fp) != 1)
            break;

        const ClientData::AmbientPropVisualDefinition* pVisual =
            ClientData::FindAmbientPropVisualDefinition(record.kind);
        if (!pVisual || !pVisual->mesh.resourceRelativePath || !pVisual->shader.runtimePath)
            continue;

        auto pRenderer = std::make_unique<ModelRenderer>();
        if (!pRenderer->Initialize(pVisual->mesh.resourceRelativePath, pVisual->shader.runtimePath))
        {
#if defined(_DEBUG)
            char debugMessage[128]{};
            sprintf_s(debugMessage, "[MapAmbient] init failed kind=%u\n", record.kind);
            OutputDebugStringA(debugMessage);
#endif
            continue;
        }
        if (pVisual->idleAnimation && pVisual->idleAnimation[0])
            pRenderer->PlayAnimationByName(pVisual->idleAnimation, true);

        Vec3 worldPos = ConvertMap11LoLToStage(record);
        if (projectToSurface)
            projectToSurface(worldPos);
        if (record.kind == kFireflyKind)
        {
            worldPos.y += 1.4f;
            pRenderer->SetMaterialOverrideColor({ 0.52f, 0.95f, 0.20f, 1.f }, true);
        }

        Prop prop{};
        prop.pRenderer = std::move(pRenderer);
        prop.transform.SetPosition(worldPos);
        prop.transform.SetRotation({ 0.f, ConvertMap11LoLYawToStage(record.lolYaw), 0.f });
        const f32_t fScale = 0.01f * record.scale;
        prop.transform.SetScale({ fScale, fScale, fScale });
        m_props.push_back(std::move(prop));
    }

    fclose(fp);

#if defined(_DEBUG)
    char debugMessage[128]{};
    sprintf_s(debugMessage, "[MapAmbient] spawned %zu ambient props\n", m_props.size());
    OutputDebugStringA(debugMessage);
#endif
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

u32_t CAmbientProp_Manager::AppendRenderSnapshotMeshes(
    RenderWorldSnapshot& snapshot,
    const Mat4& matViewProjection)
{
    u32_t appendedCount = 0;

    for (Prop& prop : m_props)
    {
        if (!prop.pRenderer)
            continue;

        prop.pRenderer->UpdateTransform(prop.transform.GetWorldMatrix());
        appendedCount += prop.pRenderer->AppendRenderSnapshotMeshesFrustumCulled(
            snapshot,
            matViewProjection);
    }

    WINTERS_PROFILE_COUNT("AmbientProp::RHISnapshotMeshes", appendedCount);
    return appendedCount;
}

void CAmbientProp_Manager::Shutdown()
{
    m_props.clear();
}
