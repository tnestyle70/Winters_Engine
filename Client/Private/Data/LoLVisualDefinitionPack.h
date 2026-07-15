#pragma once

#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Definitions/DefinitionIds.h"
#include "Shared/GameSim/Definitions/MapDataFormats.h"
#include "WintersTypes.h"

namespace ClientData
{
    inline constexpr u8_t kVisualSkillSlotCount = 5u;
    inline constexpr u8_t kVisualSkillStageCount = 2u;
    inline constexpr u8_t kVisualSubmeshStateCount = 8u;   // S035: 포탑 alive/destroyed 7상태 수용

    inline constexpr u8_t kVisualTextureOverrideCount = 4u;
    inline constexpr u8_t kChampionModelTextureSlotCount = 8u;

    struct VisualAssetPathRef
    {
        const char* resourceRelativePath = nullptr;
    };

    struct VisualShaderPathRef
    {
        const wchar_t* runtimePath = nullptr;
    };

    struct VisualTexturePathRef
    {
        const wchar_t* resourceRelativePath = nullptr;
    };

    struct SkillVisualStageDef
    {
        f32_t animationPlaybackSpeed = 1.f;
        f32_t castFrame = 0.f;
        f32_t recoveryFrame = 0.f;
    };

    struct SkillVisualDefinition
    {
        u8_t stageCount = 1u;
        ReplicatedCueId replicatedCueId = 0u;
        SkillVisualStageDef stages[kVisualSkillStageCount] = {};
    };

    struct ChampionVisualDefinition
    {
        DefinitionKey key = kInvalidDefinitionKey;
        eChampion legacyChampion = eChampion::END;
        f32_t modelYawOffsetRadians = 0.f;
        SkillVisualDefinition skills[kVisualSkillSlotCount] = {};
    };

    struct ChampionModelVisualDefinition
    {
        DefinitionKey key = kInvalidDefinitionKey;
        eChampion champion = eChampion::END;
        const char* displayName = nullptr;
        const char* animPrefix = "";
        const char* idleAnimation = "Idle1";
        const char* runAnimation = "run";
        const char* basicAttackAnimation = "attack_01";
        f32_t basicAttackRange = 6.f;
        VisualAssetPathRef mesh{};
        VisualShaderPathRef shader{};
        VisualTexturePathRef defaultTexture{};
        VisualTexturePathRef textureSlots[kChampionModelTextureSlotCount] = {};
        f32_t spawnPositionX = 0.f;
        f32_t spawnPositionY = 1.f;
        f32_t spawnPositionZ = 0.f;
        f32_t spawnScale = 0.01f;
    };

    struct ChampionModelVisualPack
    {
        const ChampionModelVisualDefinition* models = nullptr;
        u32_t modelCount = 0u;
    };

    struct ChampionUiVisualDefinition
    {
        DefinitionKey key = kInvalidDefinitionKey;
        eChampion champion = eChampion::END;
        VisualTexturePathRef loadscreen{};
        VisualTexturePathRef portrait{};
    };

    struct StructureVisualSubmeshStateDef
    {
        u32_t submeshIndex = 0u;
        bool_t bVisibleWhenDestroyed = false;
    };

    struct StructureVisualDefinition
    {
        DefinitionKey key = kInvalidDefinitionKey;
        Winters::Map::eObjectKind kind = Winters::Map::eObjectKind::Structure_Nexus;
        eTeam team = eTeam::TEAM_END;
        VisualAssetPathRef mesh{};
        VisualShaderPathRef shader{};
        u8_t submeshStateCount = 0u;
        StructureVisualSubmeshStateDef submeshStates[kVisualSubmeshStateCount] = {};
    };

    struct VisualTextureOverrideDef
    {
        u32_t meshIndex = 0u;
        const wchar_t* resourceRelativePath = nullptr;
    };

    struct JungleVisualDefinition
    {
        DefinitionKey key = kInvalidDefinitionKey;
        u32_t subKind = 0u;
        VisualAssetPathRef mesh{};
        VisualShaderPathRef shader{};
        f32_t visualScaleMultiplier = 1.f;
        u8_t textureOverrideCount = 0u;
        VisualTextureOverrideDef textureOverrides[kVisualTextureOverrideCount] = {};
    };

    struct MinionVisualDefinition
    {
        DefinitionKey key = kInvalidDefinitionKey;
        u32_t type = 0u;
        u32_t team = 0u;
        VisualAssetPathRef mesh{};
        VisualShaderPathRef shader{};
        VisualTexturePathRef textureAllMeshes{};
    };

    struct AmbientPropVisualDefinition
    {
        DefinitionKey key = kInvalidDefinitionKey;
        u32_t kind = 0u;
        VisualAssetPathRef mesh{};
        VisualShaderPathRef shader{};
        const char* idleAnimation = nullptr;
    };

    struct AmbientPropVisualPack
    {
        VisualTexturePathRef placement{};
        const AmbientPropVisualDefinition* props = nullptr;
        u32_t propCount = 0u;
    };

    struct MapRuntimeVisualDefinition
    {
        VisualAssetPathRef baseMapMesh{};
        VisualTexturePathRef baseMapSurface{};
        VisualAssetPathRef fullLayerMapMesh{};
        VisualTexturePathRef fullLayerMapSurface{};
        VisualTexturePathRef brushVolumeCsv{};
        VisualTexturePathRef brushVolumeBinary{};
        VisualTexturePathRef attackRangeTexture{};
    };

    struct FxMeshPreloadVisualDefinition
    {
        DefinitionKey key = kInvalidDefinitionKey;
        VisualAssetPathRef mesh{};
        VisualTexturePathRef texture{};
    };

    struct FxMeshPreloadVisualPack
    {
        const FxMeshPreloadVisualDefinition* entries = nullptr;
        u32_t entryCount = 0u;
    };

    const ChampionVisualDefinition* FindChampionVisualDefinition(eChampion champion);
    f32_t ResolveChampionModelYawOffset(eChampion champion);
    const ChampionModelVisualPack& GetChampionModelVisualPack();
    const ChampionModelVisualDefinition* FindChampionModelVisualDefinition(eChampion champion);
    const ChampionUiVisualDefinition* FindChampionUiVisualDefinition(eChampion champion);
    const StructureVisualDefinition* FindStructureVisualDefinition(Winters::Map::eObjectKind kind, eTeam team);
    const JungleVisualDefinition* FindJungleVisualDefinition(u32_t subKind);
    const MinionVisualDefinition* FindMinionVisualDefinition(u32_t type, u32_t team);
    const AmbientPropVisualPack& GetAmbientPropVisualPack();
    const AmbientPropVisualDefinition* FindAmbientPropVisualDefinition(u32_t kind);
    const MapRuntimeVisualDefinition& GetMapRuntimeVisualDefinition();
    const FxMeshPreloadVisualPack& GetFxMeshPreloadVisualPack();
}
