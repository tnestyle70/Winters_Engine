#pragma once
// AmbientProp_Manager.h — map11 앰비언트 프롭(새/오리) owner.
// Stage 2: CScene_InGame이 직접 소유하던 m_AmbientProps(게임플레이 무관 장식)를
// owner로 이관(CMinion_Manager 싱글톤 패턴). presentation 전용 — gameplay truth 아님.
// 설계: .md/plan/refactor/17_INGAME_SCENE_STAGE2_OWNER_DESIGN.md
#include "Defines.h"
#include "Renderer/ModelRenderer.h"
#include "Core/CTransform.h"

#include <functional>
#include <memory>
#include <vector>

class CAmbientProp_Manager final
{
    DECLARE_SINGLETON(CAmbientProp_Manager)
    CAmbientProp_Manager() = default;

public:
    ~CAmbientProp_Manager() = default;

    // .wamb 로드 → 새/오리 렌더러 생성. 좌표는 LoL 공간이라 mapWorld/mapYaw로 변환하고,
    // 맵 표면 투영은 projectToSurface 콜백으로 위임받는다(MapNav/scene 결합 회피).
    void Spawn(const Mat4& mapWorld, f32_t mapYaw,
        const std::function<void(Vec3&)>& projectToSurface);
    void Tick(f32_t dt);
    void Render(const Mat4& matViewProjection, const Vec3& cameraWorld);
    void Shutdown();

private:
    struct Prop
    {
        std::unique_ptr<ModelRenderer> pRenderer;
        CTransform transform;
    };
    std::vector<Prop> m_props{};
};
