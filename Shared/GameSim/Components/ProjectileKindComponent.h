#pragma once

#include <cstdint>

enum class eProjectileKind : uint16_t
{
    Generic = 0,
    Wind = 1,
    Tornado = 2,
    EQRing = 3,
    MysticShot = 10,
    EssenceFlux = 11,
    GlobalBeam = 12,
    AsheVolleyArrow = 13,
    AsheCrystalArrow = 14,
    LeeSinQ = 20,
    KindredArrow = 21,
    ZedShuriken = 30,
    PROJECTILE_END
};
