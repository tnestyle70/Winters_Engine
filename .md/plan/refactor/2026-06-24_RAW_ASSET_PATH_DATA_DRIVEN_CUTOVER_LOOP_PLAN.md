Session - raw C++ product asset path를 ClientPublic visual data와 LoLDefinitionPack 생성물로 이관하고 전수 조사 게이트가 0이 될 때까지 반복한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Tools/LoLData/FindRawAssetPaths.ps1

새 파일:

```powershell
param(
    [switch]$FailOnCandidate
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

$SearchRoots = @(
    "Client",
    "Engine",
    "Server",
    "Shared"
)

$RgArgs = @(
    "--glob", "!Client/Bin/**",
    "--glob", "!EngineSDK/**",
    "--glob", "!Engine/External/**",
    "--glob", "!Engine/ThirdPartyLib/**",
    "--glob", "!**/Generated/**",
    "-g", "*.cpp",
    "-g", "*.h",
    "-g", "*.hpp",
    "-g", "*.inl",
    "-n",
    "--fixed-strings",
    "Client/Bin/Resource"
)

Push-Location $Root
try {
    $Lines = @(rg @RgArgs @SearchRoots)
    $Candidates = @()

    foreach ($Line in $Lines) {
        if ($Line -match "^(?<file>[^:]+):(?<line>\d+):(?<text>.*)$") {
            $File = $Matches["file"].Replace("\", "/")
            $Text = $Matches["text"]
            $Candidates += [pscustomobject]@{
                File = $File
                Line = [int]$Matches["line"]
                Text = $Text.Trim()
            }
        }
    }

    $Groups = $Candidates |
        Group-Object File |
        Sort-Object @{ Expression = "Count"; Descending = $true }, @{ Expression = "Name"; Descending = $false }

    Write-Host "[RawAssetPathAudit] candidate files: $($Groups.Count)"
    Write-Host "[RawAssetPathAudit] candidate occurrences: $($Candidates.Count)"

    foreach ($Group in $Groups) {
        Write-Host ("{0,4} {1}" -f $Group.Count, $Group.Name)
    }

    if ($FailOnCandidate -and $Candidates.Count -gt 0) {
        throw "Raw product asset paths remain in hand-written runtime code."
    }
}
finally {
    Pop-Location
}
```

1-2. C:/Users/tnest/Desktop/Winters/Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1

기존 코드:

```powershell
    Invoke-Checked "Legacy ownership audit" {
        powershell -ExecutionPolicy Bypass -File Tools/LoLData/Collect-LoLLegacyDataAudit.ps1
    }
```

아래에 추가:

```powershell
    Invoke-Checked "Raw product asset path audit" {
        powershell -ExecutionPolicy Bypass -File Tools/LoLData/FindRawAssetPaths.ps1
    }
```

1-3. C:/Users/tnest/Desktop/Winters/Data/LoL/ClientPublic/Visual/ObjectVisualDefs.json

새 파일:

```json
{
  "schemaVersion": 1,
  "structures": [
    {
      "key": "structure.nexus.blue",
      "kind": "Structure_Nexus",
      "team": "Blue",
      "mesh": "Texture/Object/Nexus/nexus_textured.wmesh",
      "shader": "Shaders/Mesh3D.hlsl",
      "visibilityStates": [
        {
          "name": "Destroyed",
          "submeshIndex": 0,
          "visibleWhenDestroyed": true
        },
        {
          "name": "Alive",
          "submeshIndex": 1,
          "visibleWhenDestroyed": false
        }
      ]
    },
    {
      "key": "structure.nexus.red",
      "kind": "Structure_Nexus",
      "team": "Red",
      "mesh": "Texture/Object/Nexus/nexus_red_textured.wmesh",
      "shader": "Shaders/Mesh3D.hlsl",
      "visibilityStates": [
        {
          "name": "Destroyed",
          "submeshIndex": 0,
          "visibleWhenDestroyed": true
        },
        {
          "name": "Alive",
          "submeshIndex": 1,
          "visibleWhenDestroyed": false
        }
      ]
    },
    {
      "key": "structure.inhibitor.blue",
      "kind": "Structure_Inhibitor",
      "team": "Blue",
      "mesh": "Texture/Object/Inhibitor/inhibitor_textured.wmesh",
      "shader": "Shaders/Mesh3D.hlsl",
      "visibilityStates": []
    },
    {
      "key": "structure.inhibitor.red",
      "kind": "Structure_Inhibitor",
      "team": "Red",
      "mesh": "Texture/Object/Inhibitor/inhibitor_red_textured.wmesh",
      "shader": "Shaders/Mesh3D.hlsl",
      "visibilityStates": []
    },
    {
      "key": "structure.turret.blue",
      "kind": "Structure_Turret",
      "team": "Blue",
      "mesh": "Texture/Object/Turret/turret_textured.wmesh",
      "shader": "Shaders/Mesh3D.hlsl",
      "visibilityStates": []
    },
    {
      "key": "structure.turret.red",
      "kind": "Structure_Turret",
      "team": "Red",
      "mesh": "Texture/Object/Turret/turret_red_textured.wmesh",
      "shader": "Shaders/Mesh3D.hlsl",
      "visibilityStates": []
    }
  ]
}
```

1-4. C:/Users/tnest/Desktop/Winters/Client/Private/Data/LoLVisualDefinitionPack.h

기존 코드:

```cpp
#include "Shared/GameSim/Definitions/DefinitionIds.h"
#include "WintersTypes.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Definitions/MapDataFormats.h"
```

기존 코드:

```cpp
    struct ChampionVisualDefinition
    {
        DefinitionKey key = kInvalidDefinitionKey;
        eChampion legacyChampion = eChampion::END;
        f32_t modelYawOffsetRadians = 0.f;
        SkillVisualDefinition skills[kVisualSkillSlotCount] = {};
    };

    const ChampionVisualDefinition* FindChampionVisualDefinition(eChampion champion);
    f32_t ResolveChampionModelYawOffset(eChampion champion);
```

아래로 교체:

```cpp
    inline constexpr u8_t kVisualSubmeshStateCount = 4u;

    struct VisualAssetPathRef
    {
        const char* resourceRelativePath = nullptr;
    };

    struct VisualShaderPathRef
    {
        const wchar_t* runtimePath = nullptr;
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

    struct ChampionVisualDefinition
    {
        DefinitionKey key = kInvalidDefinitionKey;
        eChampion legacyChampion = eChampion::END;
        f32_t modelYawOffsetRadians = 0.f;
        SkillVisualDefinition skills[kVisualSkillSlotCount] = {};
    };

    const ChampionVisualDefinition* FindChampionVisualDefinition(eChampion champion);
    f32_t ResolveChampionModelYawOffset(eChampion champion);
    const StructureVisualDefinition* FindStructureVisualDefinition(Winters::Map::eObjectKind kind, eTeam team);
```

1-5. C:/Users/tnest/Desktop/Winters/Tools/LoLData/Build-LoLDefinitionPack.py

`emit_client_visual_cpp`, `client_visual_json`, `outputs`가 `ObjectVisualDefs.json`을 읽고 `StructureVisualDefinition` 생성 코드를 내보내도록 수정한다.

CONFIRM_NEEDED:
- `Winters::Map::eObjectKind` enum 이름과 `None` 기본값이 현재 public include에서 그대로 접근 가능한지 확인한다.
- Python generator에서 `ObjectVisualDefs.json`을 어떤 source hash에 포함할지 결정한다.
- `resourceRelativePath`를 런타임에서 `Client/Bin/Resource`와 결합하는 기존 resolver API가 있는지 확인한다.

1-6. C:/Users/tnest/Desktop/Winters/Client/Private/Manager/Structure_Manager.cpp

삭제할 코드:

```cpp
    constexpr const char* PATH_NEXUS_BLUE  = "Client/Bin/Resource/Texture/Object/Nexus/nexus_textured.wmesh";
    constexpr const char* PATH_NEXUS_RED   = "Client/Bin/Resource/Texture/Object/Nexus/nexus_red_textured.wmesh";
    constexpr const char* PATH_INHIB_BLUE  = "Client/Bin/Resource/Texture/Object/Inhibitor/inhibitor_textured.wmesh";
    constexpr const char* PATH_INHIB_RED   = "Client/Bin/Resource/Texture/Object/Inhibitor/inhibitor_red_textured.wmesh";
    constexpr const char* PATH_TURRET_BLUE = "Client/Bin/Resource/Texture/Object/Turret/turret_textured.wmesh";
    constexpr const char* PATH_TURRET_RED  = "Client/Bin/Resource/Texture/Object/Turret/turret_red_textured.wmesh";
```

삭제할 범위:
`const char* CStructure_Manager::ResolveModelPath(Winters::Map::eObjectKind kind, eTeam team)` 함수 전체를 삭제한다.

CONFIRM_NEEDED:
- 삭제 후 `Spawn_FromEntry`에서 `ClientData::FindStructureVisualDefinition(kind, team)`을 호출하고, `VisualAssetPathRef`를 런타임 full path로 변환한 뒤 `ModelRenderer::Initialize`에 전달한다.
- `BuildNexusVisibilityMask`는 hardcoded submesh index 대신 `StructureVisualDefinition::submeshStates`를 순회하도록 바꾼다.
- Blue/Red Nexus 모두 동일한 visibility-state data를 타야 하며, Render path와 RHI snapshot path는 같은 helper를 호출해야 한다.

1-7. C:/Users/tnest/Desktop/Winters/Client/Public/GameObject/ChampionVisualData.h

`ChampionModelVisualData`의 `fbxPath`, `defaultTexturePath`, `texturePath`를 raw path pointer에서 `VisualAssetPathRef` 또는 동일한 의미의 manifest key field로 이관한다.

CONFIRM_NEEDED:
- 기존 champion registration 파일들이 `ChampionVisualData`를 값으로 조립하는지, table lookup 결과를 참조하는지 확인한다.
- migration 중에는 legacy registration wrapper를 유지하되, 새 raw path가 추가되면 `FindRawAssetPaths.ps1 -FailOnCandidate`가 실패해야 한다.

1-8. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/ChampionTable.cpp

`Client/Bin/Resource`를 포함하는 champion model, texture, portrait, loading-screen 경로를 `Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json` 또는 별도 `ChampionAssetVisualDefs.json`로 이동한다.

CONFIRM_NEEDED:
- `ChampionVisualDefs.json`이 timing data와 asset data를 함께 소유할지, `ChampionAssetVisualDefs.json`로 분리할지 결정한다. 한 pack으로 생성되는 구조만 유지하고 runtime reader를 둘로 만들지 않는다.

1-9. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/Champion/*/*FxPresets.cpp

FX mesh, texture, wfx, animation path는 `ReplicatedCueId` 또는 `VisualCueKey`에서 시작해 ClientPublic visual pack으로 resolve한다.

CONFIRM_NEEDED:
- `FxLegacyAssetDumper`, `WfxEffectToolPanel`, `EffectTuner`가 authoring/export 도구인지 normal F5 runtime path인지 분류한다.
- runtime champion hook은 resolved cue handle만 받아야 하며, gameplay 결과를 새로 만들면 안 된다.

1-10. C:/Users/tnest/Desktop/Winters/Client/Private/Manager/Jungle_Manager.cpp

정글 몬스터 model/texture/FX path를 `ClientPublic/Visual/ObjectVisualDefs.json`의 `jungleCamps` 또는 기존 spawn-object pack의 visual sibling으로 이동한다.

CONFIRM_NEEDED:
- ServerPrivate `SpawnObjectGameplayDefs.json`의 jungle gameplay subKind와 visual key를 어떤 stable key로 매칭할지 확인한다.

1-11. C:/Users/tnest/Desktop/Winters/Client/Private/Manager/Minion_Manager.cpp

미니언 model/texture/phase asset path를 `ClientPublic/Visual/ObjectVisualDefs.json`의 `minions` 또는 기존 spawn-object pack의 visual sibling으로 이동한다.

CONFIRM_NEEDED:
- `MinionVisualPlaybackState`가 runtime state만 담도록 유지하고, asset 선택 정보는 generated definition으로 이동한다.

1-12. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/LobbyRosterHelpers.cpp

로비/선택/로딩 화면용 portrait/loading/card asset path를 champion visual data로 이동한다.

CONFIRM_NEEDED:
- UI-only image는 Engine UI atlas manifest로 보낼지, LoL ClientPublic champion visual data에 둘지 소유권을 확정한다.

1-13. C:/Users/tnest/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

Engine 소유 UI fallback/resource-root 경로만 남기고, LoL 제품 이미지 경로가 있으면 ClientPublic visual data 또는 UI atlas manifest로 이동한다.

CONFIRM_NEEDED:
- Engine generic default asset과 LoL product asset을 한 검색어로 같이 제거하지 않는다.

2. 검증

반복 1회는 아래 순서로만 완료 처리한다.

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --check
powershell -ExecutionPolicy Bypass -File Tools/LoLData/FindRawAssetPaths.ps1
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug
git diff --check
```

raw path audit가 실패하면 같은 반복에서 새 파일을 더 만들지 말고, 가장 occurrence가 많은 owner 하나만 골라 data migration을 끝낸 뒤 다시 검증한다.

Client runtime smoke에서 확인한다.
- Blue/Red Nexus가 alive/destroyed submesh를 같은 data rule로 렌더한다.
- Structure, minion, jungle, champion, FX가 normal F5 flow에서 사라지지 않는다.
- server log만으로 visual 성공을 판정하지 않는다.

완료 기준:
- `Client/Bin/Resource` 문자열이 손으로 작성한 Client product runtime code에서 0개다.
- 예외는 resource root resolver, Engine generic fallback, validation/tool script, generated output뿐이다.
- `Data/LoL/ClientPublic/Visual`은 visual/presentation 값만 갖고, ServerPrivate gameplay 값과 섞이지 않는다.
- Shared/GameSim, Server, Engine public API에 LoL Client visual path가 새로 올라오지 않는다.
