#pragma once

// 2026-07-09 데이터 경계 이동: ChampionDef는 클라이언트 비주얼 정의(애님 키/메시/텍스처 경로)라
// Shared/GameSim/Definitions에서 Client/Public/GameObject로 이동했다 (WINTERS_DATA_ARCHITECTURE.md).
// 게임플레이 truth 스탯은 ChampionGameplayDef(ServerPrivate pack)와 replicated StatComponent가 소유한다.

#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <cstdint>

inline constexpr uint32_t kChampionTextureSlotMax = 8;

struct ChampionDef
{
    eChampion   id = eChampion::END;
    const char* animPrefix = "";
    const char* idleAnimKey = "Idle1";
    const char* runAnimKey = "run";
    const char* basicAttackKey = "attack_01";
    f32_t basicAttackRange = 6.f;

    const char* fbxPath = nullptr;
    // LoL champion assets are diffuse-only by default; PBR remains opt-in.
    const wchar_t* shaderPath = L"Shaders/Mesh3D.hlsl";
    const wchar_t* defaultTexturePath = nullptr;
    const wchar_t* texturePath[kChampionTextureSlotMax] = {};
    Vec3 spawnPosition = { 0.f, 1.f, 0.f };
    f32_t spawnScale = 0.01f;
    const char* displayName = nullptr;
};

const char* GetChampionDisplayName(eChampion champ);
const ChampionDef* FindChampionDef(eChampion champ);
void RegisterAllLegacy();
