#pragma once

class CScene_InGame;

class CInGameCombatInputBridge final
{
public:
    static void UpdateTargeting(CScene_InGame& scene);
    static void UpdateCombatInput(CScene_InGame& scene, bool& outSkipGroundMove);
};
