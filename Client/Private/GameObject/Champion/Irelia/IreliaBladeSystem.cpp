#include "GameObject/Champion/Irelia/IreliaBladeSystem.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GameObject/FX/FxBeamComponent.h"
#include "GameObject/FX/FxMeshSystem.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "GameObject/Champion/Irelia/Irelia_Tuning.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxRibbonComponent.h"
#include "GameObject/Champion/Irelia/IreliaFxPresets.h"

#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ProfilerAPI.h"
#include "WintersMath.h"
#include <algorithm>
#include <vector>
#include <cmath>

namespace
{
    constexpr const char* kPathBladeModel = "Client/Bin/Resource/Texture/FX/Irelia/fbx/irelia_base_e_blade.fbx";
    constexpr const wchar_t* kPathBladeTexture = L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_blades_passive_4_texture.png";

    void MarkFxPendingDelete(CWorld& world, EntityID fxID)
    {
        if (fxID == NULL_ENTITY)
            return;

        if (world.HasComponent<FxMeshComponent>(fxID))
            world.GetComponent<FxMeshComponent>(fxID).bPendingDelete = true;
        if (world.HasComponent<FxBillboardComponent>(fxID))
            world.GetComponent<FxBillboardComponent>(fxID).bPendingDelete = true;
        if (world.HasComponent<FxBeamComponent>(fxID))
            world.GetComponent<FxBeamComponent>(fxID).bPendingDelete = true;
        if (world.HasComponent<FxRibbonComponent>(fxID))
            world.GetComponent<FxRibbonComponent>(fxID).bPendingDelete = true;
    }

    void UpdateOrbitFx(
        CWorld& world,
        EntityID fxID,
        const IreliaBladeComponent& blade,
        f32_t fOrbitAngle,
        const Irelia::IreliaTuning& tuning)
    {
        if (fxID == NULL_ENTITY || !world.HasComponent<FxMeshComponent>(fxID))
            return;

        FxMeshComponent& orbitFx = world.GetComponent<FxMeshComponent>(fxID);
        orbitFx.vWorldPos = {
            blade.vWorldPos.x + std::cosf(fOrbitAngle) * tuning.fOrbitRadius,
            blade.vWorldPos.y,
            blade.vWorldPos.z + std::sinf(fOrbitAngle) * tuning.fOrbitRadius
        };
        orbitFx.vRotation = {
            blade.vOrbitBaseRotation.x,
            blade.vOrbitBaseRotation.y + fOrbitAngle,
            blade.vOrbitBaseRotation.z
        };
    }
}

std::unique_ptr<CIreliaBladeSystem> CIreliaBladeSystem::Create()
{
    return std::unique_ptr<CIreliaBladeSystem>(new CIreliaBladeSystem());
}

void CIreliaBladeSystem::Execute(CWorld& world, f32_t fTimeDelta)
{
    WINTERS_PROFILE_SCOPE("IreliaBlade::Execute");

    std::vector<EntityID> vecDelete;
    world.ForEach<IreliaBladeComponent>(
        std::function<void(EntityID, IreliaBladeComponent&)>(
            [&](EntityID entity, IreliaBladeComponent& blade)
            {
                blade.fElapsed += fTimeDelta;
                //이렐리아가 E를 최대 시간 동안 사용하지 않았을 때
                if (blade.fElapsed >= blade.fLifetime)
                {
                    MarkFxPendingDelete(world, blade.fxMeshID);
                    MarkFxPendingDelete(world, blade.orbitFxID1);
                    MarkFxPendingDelete(world, blade.orbitFxID2);
                    if (blade.groundGlowFxID != NULL_ENTITY &&
                        world.HasComponent<FxBillboardComponent>(blade.groundGlowFxID))
                        world.GetComponent<FxBillboardComponent>(blade.groundGlowFxID).bPendingDelete = true;
                    if (blade.groundCoreFxID != NULL_ENTITY &&
                        world.HasComponent<FxBillboardComponent>(blade.groundCoreFxID))
                        world.GetComponent<FxBillboardComponent>(blade.groundCoreFxID).bPendingDelete = true;
                    if (blade.placeCueFxID != NULL_ENTITY &&
                        world.HasComponent<FxBillboardComponent>(blade.placeCueFxID))
                        world.GetComponent<FxBillboardComponent>(blade.placeCueFxID).bPendingDelete = true;
                    for (u32_t i = 0; i < blade.placeCueFxCount; ++i)
                        MarkFxPendingDelete(world, blade.placeCueFxIDs[i]);
                    vecDelete.push_back(entity);
                    return;
                }
                // Orbit the smaller blades around the placed center blade.
                {
                    const Irelia::IreliaTuning& tuning = Irelia::GetTuning();

                    blade.fOrbitAngle += tuning.fOrbitAngularSpeed * fTimeDelta;
                    if (blade.fOrbitAngle > WintersMath::kTwoPi ||
                        blade.fOrbitAngle < -WintersMath::kTwoPi)
                    {
                        blade.fOrbitAngle =
                            std::fmod(blade.fOrbitAngle, WintersMath::kTwoPi);
                    }

                    UpdateOrbitFx(world, blade.orbitFxID1, blade, blade.fOrbitAngle, tuning);
                    UpdateOrbitFx(
                        world,
                        blade.orbitFxID2,
                        blade,
                        blade.fOrbitAngle + WintersMath::kPi,
                        tuning);
                }

                const Irelia::IreliaTuning& t = Irelia::GetTuning();
                if (blade.groundGlowFxID != NULL_ENTITY &&
                    world.HasComponent<FxBillboardComponent>(blade.groundGlowFxID))
                {
                    world.GetComponent<FxBillboardComponent>(blade.groundGlowFxID).fYaw +=
                        t.eGroundSpinSpeed * fTimeDelta;
                }
                if (blade.groundCoreFxID != NULL_ENTITY &&
                    world.HasComponent<FxBillboardComponent>(blade.groundCoreFxID))
                {
                    world.GetComponent<FxBillboardComponent>(blade.groundCoreFxID).fYaw -=
                        t.eGroundSpinSpeed * 0.6f * fTimeDelta;
                }

            }));

    for (EntityID e : vecDelete)
        world.DestroyEntity(e);
}

EntityID CIreliaBladeSystem::SpawnPlaced(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    const Vec3& vGround, EntityID owner, f32_t fScale, const Vec3& vRotation,
    f32_t fWorldYawSpinSpeed)
{
    // Raise the blade above the ground so the mesh does not sink into the floor.
    const Vec3 vRaisedGround = { vGround.x, vGround.y + 3.f, vGround.z };

    const Irelia::IreliaTuning& t = Irelia::GetTuning();
    const f32_t fMainScale = fScale * t.fPlacedBladeScaleMul;
    const f32_t fOrbitScale = fMainScale * t.fOrbitBladeScaleMul;

    EntityID entity = world.CreateEntity();
    TransformComponent tf{};
    tf.m_LocalPosition = vRaisedGround;
    world.AddComponent<TransformComponent>(entity, tf);

    IreliaBladeComponent blade{};
    blade.vWorldPos = vRaisedGround;
    blade.ownerEntity = owner;
    blade.fLifetime = 3.f;
    world.AddComponent<IreliaBladeComponent>(entity, blade);

    FxMeshComponent fxmesh{};
    fxmesh.vWorldPos = vRaisedGround;
    fxmesh.vScale = { fMainScale, fMainScale, fMainScale };
    fxmesh.vRotation = vRotation;
    fxmesh.fWorldYawSpinSpeed = fWorldYawSpinSpeed;
    fxmesh.modelPath = kPathBladeModel;
    fxmesh.texturePath = kPathBladeTexture;
    // eBladeColor는 EffectTuner의 Irelia Live Tuning에서 조정한다.
    fxmesh.vColor = t.eBladeColor;
    fxmesh.fLifetime = 3.f;
    const EntityID fxID = CFxMeshSystem::Spawn(world, pRenderer, fxmesh);
    if (fxID == NULL_ENTITY)
    {
        world.DestroyEntity(entity);
        MSG_BOX("이렐리아 FX Blade 생성 실패!");
        return NULL_ENTITY;
    }

    FxMeshComponent orbitMesh = fxmesh;
    orbitMesh.vWorldPos = { vRaisedGround.x + t.fOrbitRadius, vRaisedGround.y, vRaisedGround.z };
    orbitMesh.vScale = { fOrbitScale, fOrbitScale, fOrbitScale };
    orbitMesh.fWorldYawSpinSpeed = fWorldYawSpinSpeed * t.fOrbitSpinSpeedMul;

    const EntityID orbitFxID1 = CFxMeshSystem::Spawn(world, pRenderer, orbitMesh);

    FxMeshComponent orbitMesh2 = orbitMesh;
    orbitMesh2.vWorldPos = { vRaisedGround.x - t.fOrbitRadius, vRaisedGround.y, vRaisedGround.z };
    const EntityID orbitFxID2 = CFxMeshSystem::Spawn(world, pRenderer, orbitMesh2);

    IreliaBladeComponent& placedBlade = world.GetComponent<IreliaBladeComponent>(entity);
    placedBlade.fxMeshID = fxID;
    placedBlade.orbitFxID1 = orbitFxID1;
    placedBlade.orbitFxID2 = orbitFxID2;
    placedBlade.vOrbitBaseRotation = vRotation;

    FxCueContext fx{};
    fx.vWorldPos = vRaisedGround;
    fx.vForward = { std::sinf(vRotation.y), 0.f, std::cosf(vRotation.y) };
    fx.pFxMeshRenderer = pRenderer;
    std::vector<EntityID> placeCueFxIds;
    placedBlade.placeCueFxID = CFxCuePlayer::PlayAll(world, "Irelia.E.Place", fx, &placeCueFxIds);
    const u32_t copyCount =
        std::min<u32_t>(static_cast<u32_t>(placeCueFxIds.size()), 12u);
    placedBlade.placeCueFxCount = copyCount;
    for (u32_t i = 0; i < copyCount; ++i)
        placedBlade.placeCueFxIDs[i] = placeCueFxIds[i];

    return entity;
}

bool CIreliaBladeSystem::TriggerReturn(CWorld& world, EntityID bladeEntity)
{
    return false;
}

void CIreliaBladeSystem::ExpireAfter(CWorld& world, EntityID bladeEntity, f32_t fDelay)
{
    if (bladeEntity == NULL_ENTITY || !world.IsAlive(bladeEntity) ||
        !world.HasComponent<IreliaBladeComponent>(bladeEntity))
        return;

    IreliaBladeComponent& blade = world.GetComponent<IreliaBladeComponent>(bladeEntity);
    const f32_t fTargetLifetime = blade.fElapsed + ((fDelay > 0.f) ? fDelay : 0.f);
    if (blade.fLifetime > fTargetLifetime)
        blade.fLifetime = fTargetLifetime;
}
