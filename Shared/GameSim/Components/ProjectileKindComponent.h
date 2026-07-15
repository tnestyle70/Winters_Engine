#pragma once

#include <cstdint>

enum class eProjectileKind : uint16_t
{
    Generic = 0,
    Wind = 1,
    Tornado = 2,
    EQRing = 3,
    EzrealBasicAttack = 9,
    MysticShot = 10,
    EssenceFlux = 11,
    GlobalBeam = 12,
    AsheVolleyArrow = 13,
    AsheCrystalArrow = 14,
    ArcaneShiftBolt = 15,
    KalistaBasicAttack = 16,
    KalistaPierce = 17,
    AsheBasicAttack = 18,
    LeeSinQ = 20,
    KindredArrow = 21,
    SylasChain = 22,
    ZedShuriken = 30,
    MinionRangedBasicBlue = 40,
    MinionRangedBasicRed = 41,
    PROJECTILE_END
};
