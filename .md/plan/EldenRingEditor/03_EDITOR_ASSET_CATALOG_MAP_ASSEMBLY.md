Session - Editor asset catalog and Limgrave map assembly panel

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/EldenRingClient/Public/EldenRingAssetCatalog.h

새 파일:

```cpp
CONFIRM_NEEDED - 전체 파일 본문은 다음 코드 작성 세션에서 JSON 파서 선택을 확인한 뒤 확정한다.
확인할 것:
1. EldenRingClient가 Network/Backend/json.hpp를 포함해도 되는지.
2. 포함하지 않는다면 manifests 전용 최소 JSON cursor를 EldenRingClient 내부에 둘지.
3. Client/Bin/Resource/EldenRing/Manifests/limgrave_static_assets.json, limgrave_characters_static.json의 cooked 배열 필드명을 그대로 계약으로 삼을지.

책임:
- static asset manifest와 character static manifest를 읽는다.
- id, category/folder, ok, source_flver, fbx, wmesh, wmat, wmesh_bytes를 구조화한다.
- 첫 smoke에는 ok=true이고 wmesh가 존재하는 항목만 노출한다.
- c2130/c3251 같은 고본수 character는 catalog에는 보이되 "skeletal blocked" 플래그를 붙인다.
```

1-2. C:/Users/tnest/Desktop/Winters/EldenRingClient/Private/EldenRingAssetCatalog.cpp

새 파일:

```cpp
CONFIRM_NEEDED - EldenRingAssetCatalog.h의 JSON 파서 결정 후 전체 구현 본문을 작성한다.
구현 기준:
- WintersResolveContentPath를 사용해서 Client/Bin/Resource 기준 상대경로와 절대경로를 모두 허용한다.
- 파일 존재 여부는 std::filesystem::exists로 검증한다.
- Load 실패 사유는 OutputDebugStringA와 ImGui 패널에서 볼 수 있게 lastError 문자열로 보존한다.
- manifest의 wmesh path가 Resource 밖을 가리키면 catalog entry는 비활성화한다.
```

1-3. C:/Users/tnest/Desktop/Winters/EldenRingClient/Public/EldenRingMapAssembly.h

새 파일:

```cpp
CONFIRM_NEEDED - map_assembly.json의 현재 필드는 part/model id 중심이고 exact transform은 notes에 "fuller MSB parser pass"로 남아 있다.
전체 파일 본문은 m60_42_36_00 map_assembly.json 전체 구조를 확인한 뒤 작성한다.

책임:
- schema, mapId, area, sourceMsbXml, notes를 읽는다.
- mapPieces, collisions, enemyIds, assetIds를 읽는다.
- transform이 없는 항목은 SpawnPreview 상태가 아니라 CatalogReference 상태로 둔다.
- World Partition 세션이 쓸 seed cell key를 mapId에서 추출한다.
```

1-4. C:/Users/tnest/Desktop/Winters/EldenRingClient/Private/EldenRingMapAssembly.cpp

새 파일:

```cpp
CONFIRM_NEEDED - EldenRingMapAssembly.h와 동일하게 map_assembly.json 전체 구조 확인 후 전체 구현 본문을 작성한다.
구현 기준:
- m60_42_36_00 같은 mapId를 area=60, blockX=42, blockY=36, variant=00으로 파싱한다.
- assetIds/enemyIds와 AssetCatalog entry를 join할 수 있도록 id lookup 함수를 제공한다.
- exact transform이 없을 때는 editor viewport에 배치하지 않고 "MSB transform parser required"로 표시한다.
```

1-5. C:/Users/tnest/Desktop/Winters/EldenRingClient/Public/EldenRingProbeScene.h

기존 코드:

```cpp
	std::unique_ptr<Engine::CFxStaticMeshRenderer> m_pStaticMeshRenderer;
	std::string m_strSmokeMeshPath;
	std::wstring m_wstrSmokeDiffusePath;
	bool m_bSmokeMeshReady = false;
```

아래에 추가:

```cpp
	std::unique_ptr<CEldenRingAssetCatalog> m_pAssetCatalog;
	std::unique_ptr<CEldenRingMapAssembly> m_pMapAssembly;
	int m_iSelectedAssetIndex = -1;
	int m_iSelectedMapRefIndex = -1;
	bool m_bShowOnlyRenderableAssets = true;
```

1-6. C:/Users/tnest/Desktop/Winters/EldenRingClient/Private/EldenAssetProbeScene.cpp

기존 코드:

```cpp
bool CEldenRingAssetProbeScene::OnEnter()
{
    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();
```

아래에 추가:

```cpp
    m_pAssetCatalog = CEldenRingAssetCatalog::Create();
    if (m_pAssetCatalog)
    {
        m_pAssetCatalog->LoadStaticAssets(
            "Client/Bin/Resource/EldenRing/Manifests/limgrave_static_assets.json");
        m_pAssetCatalog->LoadCharacterStatics(
            "Client/Bin/Resource/EldenRing/Manifests/limgrave_characters_static.json");
    }

    m_pMapAssembly = CEldenRingMapAssembly::Create();
    if (m_pMapAssembly)
    {
        m_pMapAssembly->Load(
            "Client/Bin/Resource/EldenRing/Maps/Limgrave/m60_42_36_00/map_assembly.json");
    }
```

기존 코드:

```cpp
void CEldenRingAssetProbeScene::OnImGui()
{
    ImGui::Begin("Elden Ring Asset Probe");
    ImGui::TextUnformatted("DX12 cube + static WMesh smoke");
```

아래에 추가:

```cpp
    CONFIRM_NEEDED - AssetCatalog와 MapAssembly 타입 확정 후 아래 UI를 실제 코드로 작성한다.
    UI 구성:
    - Assets tab: category/id/wmesh_bytes/ok/skeletalBlocked columns.
    - Map tab: mapPieces, collisions, enemyIds, assetIds summary.
    - Selection: 선택한 asset의 wmesh를 smoke mesh path로 교체하고 PreloadMesh 재시도.
    - Transform 없음: map assembly row에 "reference only" 상태 표시.
```

2. 검증

2-1. 사전 확인

```powershell
Test-Path "Client/Bin/Resource/EldenRing/Manifests/limgrave_static_assets.json"
Test-Path "Client/Bin/Resource/EldenRing/Manifests/limgrave_characters_static.json"
Test-Path "Client/Bin/Resource/EldenRing/Maps/Limgrave/m60_42_36_00/map_assembly.json"
```

2-2. 검증 명령

```powershell
msbuild Winters.sln /t:EldenRingClient /p:Configuration=Debug-DX12 /p:Platform=x64
```

2-3. 런타임 확인

```text
1. ImGui Assets tab에서 LimgraveAEG 63개 이상, Erdtree candidate 11개 이상이 식별된다.
2. Characters tab에서 c2050, c2270, c3200, c3210, c3251, c4200 등이 보인다.
3. Map tab에서 m60_42_36_00의 MapPiece 4, Enemy 64, Asset 687 요약이 보인다.
4. transform이 없는 map part는 배치되지 않고 reference-only로 남는다.
```

2-4. 다음 세션 게이트

```text
Catalog에서 선택한 정적 AEG 하나를 DX12 viewport에 표시할 수 있어야 World Partition streaming으로 넘어간다.
Map 전체 배치는 exact transform MSB parser가 들어온 뒤 진행한다.
```
