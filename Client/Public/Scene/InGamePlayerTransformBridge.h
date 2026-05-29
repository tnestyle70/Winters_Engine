#pragma once

#include "WintersTypes.h"
#include "WintersMath.h"

class CScene_InGame;

class CInGamePlayerTransformBridge final
{
public:
    static bool HasPlayerTransform(const CScene_InGame& scene);
    static Vec3 GetPlayerPosition(const CScene_InGame& scene);
    static void SetPlayerPosition(CScene_InGame& scene, const Vec3& v);
    static f32_t GetPlayerYaw(const CScene_InGame& scene);
    // Client visual/cache yaw writer. Stores the nearest equivalent yaw to current visual yaw.
    static void SetPlayerYaw(CScene_InGame& scene, f32_t yaw);
    static Vec3 GetPlayerForward(const CScene_InGame& scene);
    static void SyncFromECS(CScene_InGame& scene);
    static void SyncToECS(CScene_InGame& scene);
};
