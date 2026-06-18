#pragma once

#include "IScene.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <memory>
#include <string>
#include <vector>

class ModelRenderer;

struct EldenCharacterPlacement
{
    std::string strLabel;
    std::string strModel;
    std::string strWmesh;
    Vec3 vPosition{};
    Vec3 vRotationDeg{};
    // FromSoft character meshes need this local-axis correction before MSB/world rotation.
    Vec3 vAxisFixDeg{ 90.f, 0.f, 0.f };
    Vec3 vScale{ 1.f, 1.f, 1.f };
    bool bAnimated = true;
    ModelRenderer* pRenderer = nullptr;
};

// Limgrave showcase: every cooked map tile placement under Maps/Limgrave/*/
// plus a character lineup (uniform display height, cycling through each
// model's animations) and the giant tree AEG099_720 as the Erdtree stand-in.
class CEldenLimgraveShowcaseScene final : public IScene
{
public:
    static std::unique_ptr<CEldenLimgraveShowcaseScene> Create();
    ~CEldenLimgraveShowcaseScene() override;

    bool OnEnter() override;
    void OnExit() override;
    void OnUpdate(f32_t deltaTime) override;
    void OnRender() override;
    void OnImGui() override;

private:
    CEldenLimgraveShowcaseScene() = default;

    struct ShowcaseInstance
    {
        std::unique_ptr<ModelRenderer> pRenderer;
        bool bCycleAnims = false;
        u32_t iAnimCount = 0;
        u32_t iAnimIndex = 0;
        f32_t fAnimTimer = 0.f;
    };

    ModelRenderer* SpawnInstance(const std::string& strWmeshPath,
                                 const Mat4& matWorld,
                                 bool bCycleAnims);
    Mat4 BuildCharacterWorldMatrix(const EldenCharacterPlacement& placement) const;
    void ApplyCharacterTransform(EldenCharacterPlacement& placement);
    bool SaveCharacterPlacements() const;
    void FrameLineup();
    void FrameErdtree();
    void SpawnPlacements();
    void SpawnCharacterPlacements();
    void SpawnErdtree();

    std::vector<ShowcaseInstance> m_Instances;
    std::vector<EldenCharacterPlacement> m_CharacterPlacements;
    u32_t m_iPlacedCount = 0;
    u32_t m_iFailedCount = 0;
    u32_t m_iAnimatedCount = 0;
    i32_t m_iSelectedCharacter = 0;
    bool m_bErdtreeLoaded = false;

    void UpdateFreeCamera(f32_t deltaTime);

    // Character focus centered on the MSB-derived showcase row.
    Vec3 m_vLineupCenter{ -16.138f, 104.800f, -120.100f };
    f32_t m_fOrbitAngle = -1.5707963f;
    f32_t m_fOrbitRadius = 28.f;
    f32_t m_fOrbitHeight = 8.f;
    f32_t m_fAspect = 16.f / 9.f;

    // Auto orbit is the safe startup view; F2 switches into WASD free camera.
    // Mouse look is available in free camera; F1 releases the cursor.
    bool m_bFreeCam = false;
    bool m_bFreeCamToggleArmed = false;
    bool m_bFreeCamToggleWasDown = false;
    bool m_bRotating = false;
    bool m_bMouseLook = true;    // FPS mouse-look on by default; F1 toggles off
    bool m_bMouseLookInit = false;
    bool m_bF1WasDown = false;
    Vec3 m_vCamPos{ -16.138f, 112.0f, -148.0f };
    Vec3 m_vErdtreePos{ 78.862f, 100.800f, 9.900f };
    f32_t m_fYaw = 0.f;     // radians, 0 = +Z
    f32_t m_fPitch = -0.04f;
    long m_iLastCursorX = 0;
    long m_iLastCursorY = 0;

    static constexpr f32_t kCharacterDisplayHeight = 3.0f;
    static constexpr f32_t kLineupSpacing = 5.0f;
    static constexpr f32_t kAnimCycleSeconds = 6.0f;  // 클립당 재생 시간(전체 순환)
};
