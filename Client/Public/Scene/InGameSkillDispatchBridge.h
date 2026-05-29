#pragma once

#include "GameObject/SkillDef.h"

class CScene_InGame;

class CInGameSkillDispatchBridge final
{
public:
    static bool DispatchSkillInput(CScene_InGame& scene, uint8_t slot, u8_t requestedStage = 0);
    static bool BuildCastCommand(CScene_InGame& scene, const SkillDef& def, CastSkillCommand& outCmd);
    static void ApplyLocalPrediction(
        CScene_InGame& scene,
        const CastSkillCommand& cmd,
        const SkillDef& def,
        u8_t skillStage = 1);
    static void RotatePlayerToward(CScene_InGame& scene, eRotateMode mode, const CastSkillCommand& cmd);

private:
    static void SendNetworkSkillCommand(CScene_InGame& scene, u8_t slot,
        const CastSkillCommand& cmd, u8_t skillStage = 1);
};
