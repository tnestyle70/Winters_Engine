#include "GameObject/Projectile/ProjectileVisualCatalog.h"

#include "GameObject/Projectile/ProjectileKind.h"
#include "WintersMath.h"

namespace
{
    constexpr u16_t kStructureProjectileKind = 100;

    constexpr ProjectileVisualDesc kNoProjectileVisual{};

    constexpr ProjectileVisualDesc kYasuoTornadoVisual{
        nullptr, nullptr, "Yasuo.Q.TornadoHit"
    };

    constexpr ProjectileVisualDesc kEzrealBasicAttackVisual{
        "Ezreal.BA.Projectile", "Ezreal.Q.Hit", nullptr
    };

    constexpr ProjectileVisualDesc kEzrealMysticShotVisual{
        "Ezreal.Q.Projectile", "Ezreal.Q.Hit", nullptr
    };

    constexpr ProjectileVisualDesc kEzrealEssenceFluxVisual{
        "Ezreal.W.Projectile", nullptr, nullptr
    };

    constexpr ProjectileVisualDesc kEzrealGlobalBeamVisual{
        "Ezreal.R.Missile", "Ezreal.R.Hit", nullptr, nullptr, nullptr, nullptr,
        WintersMath::kPi
    };

    constexpr ProjectileVisualDesc kEzrealArcaneShiftBoltVisual{
        "Ezreal.E.Projectile", "Ezreal.E.Hit", nullptr
    };

    constexpr ProjectileVisualDesc kKalistaBasicAttackVisual{
        "Kalista.BA.Projectile", nullptr, "Kalista.Rend.Spear", nullptr, nullptr, nullptr,
        WintersMath::kPi, true
    };

    constexpr ProjectileVisualDesc kKalistaPierceVisual{
        "Kalista.Q.Projectile", nullptr, "Kalista.Rend.Spear", nullptr, nullptr, nullptr,
        WintersMath::kPi, true
    };

    constexpr ProjectileVisualDesc kLeeSinQVisual{
        "LeeSin.Q.Projectile", "LeeSin.Q.Hit", "LeeSin.Q.Mark" 
    };

    constexpr ProjectileVisualDesc kZedShurikenVisual{
        "Zed.Q.Projectile", "Zed.Q.Hit", nullptr
    };

    constexpr ProjectileVisualDesc kAsheBasicAttackVisual{
        "Ashe.BA.Arrow", nullptr, "Ashe.BA.Hit", nullptr, nullptr, nullptr,
        -WintersMath::kPi * 0.5f
    };

    constexpr ProjectileVisualDesc kAsheVolleyArrowVisual{
        "Ashe.W.Arrow", "Ashe.W.Hit", nullptr, nullptr, nullptr, nullptr,
        -WintersMath::kPi * 0.5f
    };

    constexpr ProjectileVisualDesc kAsheHawkshotVisual{
        "Ashe.E.Hawkshot", nullptr, nullptr, nullptr, nullptr, nullptr,
        -WintersMath::kPi * 0.5f
    };

    constexpr ProjectileVisualDesc kAsheCrystalArrowVisual{
        "Ashe.R.Arrow", "Ashe.R.Hit", nullptr, nullptr, nullptr, nullptr,
        -WintersMath::kPi * 0.5f
    };

    constexpr ProjectileVisualDesc kStructureProjectileVisual{
        "Turret.Projectile.Red", "Turret.Projectile.Hit.Red", nullptr
    };

    constexpr ProjectileVisualDesc kMinionRangedBlueVisual{
        "Minion.Ranged.Projectile.Blue", nullptr, nullptr
    };

    constexpr ProjectileVisualDesc kMinionRangedRedVisual{
        "Minion.Ranged.Projectile.Red", nullptr, nullptr
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
        case eProjectileKind::EQRing:
            return kNoProjectileVisual;
        case eProjectileKind::Tornado:
            return kYasuoTornadoVisual;
        case eProjectileKind::EzrealBasicAttack:
            return kEzrealBasicAttackVisual;
        case eProjectileKind::MysticShot:
            return kEzrealMysticShotVisual;
        case eProjectileKind::EssenceFlux:
            return kEzrealEssenceFluxVisual;
        case eProjectileKind::GlobalBeam:
            return kEzrealGlobalBeamVisual;
        case eProjectileKind::ArcaneShiftBolt:
            return kEzrealArcaneShiftBoltVisual;
        case eProjectileKind::KalistaBasicAttack:
            return kKalistaBasicAttackVisual;
        case eProjectileKind::KalistaPierce:
            return kKalistaPierceVisual;
        case eProjectileKind::LeeSinQ:
            return kLeeSinQVisual;
        case eProjectileKind::SylasChain:
            return kNoProjectileVisual;
        case eProjectileKind::ZedShuriken:
            return kZedShurikenVisual;
        case eProjectileKind::AsheBasicAttack:
            return kAsheBasicAttackVisual;
        case eProjectileKind::AsheVolleyArrow:
            return kAsheVolleyArrowVisual;
        case eProjectileKind::AsheHawkshot:
            return kAsheHawkshotVisual;
        case eProjectileKind::AsheCrystalArrow:
            return kAsheCrystalArrowVisual;
        case eProjectileKind::MinionRangedBasicBlue:
            return kMinionRangedBlueVisual;
        case eProjectileKind::MinionRangedBasicRed:
            return kMinionRangedRedVisual;
        default:
            return kNoProjectileVisual;
        }
    }

    bool_t IsStructureProjectileKind(u16_t kind)
    {
        return kind == kStructureProjectileKind;
    }
}
