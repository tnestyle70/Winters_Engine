#include "GameObject/Projectile/ProjectileVisualCatalog.h"

#include "GameObject/Projectile/ProjectileKind.h"

namespace
{
    constexpr u16_t kStructureProjectileKind = 100;

    constexpr const wchar_t* kGenericProjectileTexture =
        L"Texture/FX/Kalista/common_glowring_blue.png";
    constexpr const wchar_t* kGenericProjectileHitTexture =
        L"Texture/FX/Kalista/common_fire-sphere32.png";
    constexpr const wchar_t* kEzrealQHitTexture =
        L"Texture/Character/Ezreal/particles/ezreal_base_q_hit_spark.png";

    constexpr ProjectileVisualDesc kGenericProjectileVisual{
        nullptr, nullptr, nullptr,
        kGenericProjectileTexture, kGenericProjectileHitTexture,
        0.8f, 0.8f, 1.4f, 1.4f,
        true, true
    };

    constexpr ProjectileVisualDesc kNoSpawnGenericHitVisual{
        nullptr, nullptr, nullptr,
        nullptr, kGenericProjectileHitTexture,
        0.8f, 0.8f, 1.4f, 1.4f,
        false, true
    };

    constexpr ProjectileVisualDesc kEzrealMysticShotVisual{
        nullptr, "Ezreal.Q.Hit", nullptr,
        nullptr, kEzrealQHitTexture,
        0.8f, 0.8f, 1.4f, 1.4f,
        false, true
    };

    constexpr ProjectileVisualDesc kLeeSinQVisual{
        "LeeSin.Q.Projectile", "LeeSin.Q.Hit", "LeeSin.Q.Mark",
        kGenericProjectileTexture, kGenericProjectileHitTexture,
        0.8f, 0.8f, 1.4f, 1.4f,
        true, true
    };

    constexpr ProjectileVisualDesc kSylasChainVisual{
        nullptr, nullptr, nullptr,
        nullptr, nullptr,
        0.8f, 0.8f, 1.4f, 1.4f,
        false, false
    };

    constexpr ProjectileVisualDesc kZedShurikenVisual{
        "Zed.Q.Projectile", "Zed.Q.Hit", nullptr,
        kGenericProjectileTexture, kGenericProjectileHitTexture,
        0.8f, 0.8f, 1.4f, 1.4f,
        true, true
    };

    constexpr ProjectileVisualDesc kAsheVolleyArrowVisual{
        "Ashe.W.Arrow", "Ashe.W.Hit", nullptr,
        kGenericProjectileTexture, kGenericProjectileHitTexture,
        0.8f, 0.8f, 1.4f, 1.4f,
        true, true
    };

    constexpr ProjectileVisualDesc kAsheCrystalArrowVisual{
        "Ashe.R.Arrow", "Ashe.R.Hit", nullptr,
        kGenericProjectileTexture, kGenericProjectileHitTexture,
        1.1f, 1.1f, 1.8f, 1.8f,
        true, true
    };

    constexpr ProjectileVisualDesc kStructureProjectileVisual{
        "Turret.Projectile.Red", "Turret.Projectile.Hit.Red", nullptr,
        nullptr, nullptr,
        0.8f, 0.8f, 1.4f, 1.4f,
        false, false
    };

    constexpr ProjectileVisualDesc kMinionRangedBlueVisual{
        "Minion.Ranged.Projectile.Blue", nullptr, nullptr,
        nullptr, nullptr,
        0.42f, 0.42f, 0.62f, 0.62f,
        false, false
    };

    constexpr ProjectileVisualDesc kMinionRangedRedVisual{
        "Minion.Ranged.Projectile.Red", nullptr, nullptr,
        nullptr, nullptr,
        0.42f, 0.42f, 0.62f, 0.62f,
        false, false
    };
}

namespace ProjectileVisualCatalog
{
    const ProjectileVisualDesc& Resolve(u16_t kind)
    {
        if (IsStructureProjectileKind(kind))
            return kStructureProjectileVisual;

        switch (static_cast<eProjectileKind>(kind))
        {
        case eProjectileKind::Wind:
        case eProjectileKind::Tornado:
        case eProjectileKind::EQRing:
            return kNoSpawnGenericHitVisual;
        case eProjectileKind::MysticShot:
            return kEzrealMysticShotVisual;
        case eProjectileKind::LeeSinQ:
            return kLeeSinQVisual;
        case eProjectileKind::SylasChain:
            return kSylasChainVisual;
        case eProjectileKind::ZedShuriken:
            return kZedShurikenVisual;
        case eProjectileKind::AsheVolleyArrow:
            return kAsheVolleyArrowVisual;
        case eProjectileKind::AsheCrystalArrow:
            return kAsheCrystalArrowVisual;
        case eProjectileKind::MinionRangedBasicBlue:
            return kMinionRangedBlueVisual;
        case eProjectileKind::MinionRangedBasicRed:
            return kMinionRangedRedVisual;
        default:
            return kGenericProjectileVisual;
        }
    }

    bool_t IsStructureProjectileKind(u16_t kind)
    {
        return kind == kStructureProjectileKind;
    }
}
