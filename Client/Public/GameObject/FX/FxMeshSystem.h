#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "GameObject/FX/FxMeshComponent.h"
#include <memory>

class CWorld;
class CDynamicCamera;

namespace Engine
{
    class CFxStaticMeshRenderer;
}

class CFxMeshSystem final
{
public:
    ~CFxMeshSystem() = default;

    static std::unique_ptr<CFxMeshSystem> Create(Engine::CFxStaticMeshRenderer* pRenderer);

    void Update(CWorld& world, f32_t fTimeDelta);
    void Render(CWorld& world, const CDynamicCamera* pCamera);

    // 정적 스폰 헬퍼 — Renderer 인자 받아 PreloadMesh 동기 호출 후 Component 추가
    static EntityID Spawn(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer, const FxMeshComponent& tmpl);
    static EntityID SpawnFromAsset(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        const CFxAssetRegistry& registry, FxAssetHandle handle,
        const Vec3& vWorldPos, EntityID attachTo = NULL_ENTITY);

private:
    CFxMeshSystem() = default;

    Engine::CFxStaticMeshRenderer* m_pRenderer = nullptr;   // 비소유 (Scene 또는 GameInstance 가 소유)
};
