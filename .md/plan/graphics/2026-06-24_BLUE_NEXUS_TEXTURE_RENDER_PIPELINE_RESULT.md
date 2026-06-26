Session - 2026-06-24 Blue Nexus texture break root-cause and render fix report.

1. 문제 요약

- 증상: Blue team Nexus가 살아있는 상태인데도 표면 텍스처가 깨지거나 파괴 상태처럼 보인다.
- 실제 원인: Nexus `.wmesh` 안에는 alive submesh와 destroyed submesh가 같이 들어있고, 기존 구조물 렌더 경로는 Nexus 상태를 보지 않고 모든 submesh를 렌더했다.
- 결론: 텍스처 파일 누락이 아니라, alive 상태에서 destroyed submesh/material까지 같이 그리는 visibility 문제다.

2. 실제 런타임 렌더링 프로세스

2-1. 구조물 모델 경로 선택

- 위치: `Client/Private/Manager/Structure_Manager.cpp`
- Blue Nexus 경로:

```cpp
PATH_NEXUS_BLUE = "Client/Bin/Resource/Texture/Object/Nexus/nexus_textured.wmesh"
```

- Red Nexus 경로:

```cpp
PATH_NEXUS_RED = "Client/Bin/Resource/Texture/Object/Nexus/nexus_red_textured.wmesh"
```

- `CStructure_Manager::Spawn_FromEntry`가 `ResolveModelPath`로 경로를 고르고 `ModelRenderer::Initialize`를 호출한다.

2-2. ModelRenderer / CModel 로딩

- `ModelRenderer::Initialize`
  - `CEngineApp::GetResourceCache().LoadModel(...)`로 공유 `CModel`을 로드한다.
- `CModel::LoadModel`
  - 입력 경로를 `.wmesh` 기준으로 정규화한다.
  - `CWMeshLoader::Load`로 `.wmesh`를 읽는다.
  - bone data가 있으면 `.wskel`을 읽는다.
  - `LoadCookedTextures`로 `.wmat`를 읽고 material index별 texture를 만든다.

2-3. `.wmesh` 원자 단위 내용

Blue Nexus:

```text
fileHeader: WINT version=1.0 contentSize=1760700
meshHeader: WMSH submeshes=2 bones=47 flags=31 stride=76 vertices=22206 indices=33408 indexStride=2 hasBounds=1
submesh[0]: material=0 vertices=13106 indices=19854 hash=2711526882648264812
submesh[1]: material=1 vertices=9100  indices=13554 hash=16682049985361464237
```

Red Nexus도 같은 submesh/material 구조를 가진다.

2-4. `.wmat` 원자 단위 내용

Blue Nexus:

```text
material[0] name=Destroyed
diffuse=Client/Bin/Resource/Texture/Object/Nexus/nexus_destroyed_tx_cm.png
exists=true

material[1] name=SRUAP_OrderNexus_Mat
diffuse=Client/Bin/Resource/Texture/Object/Nexus/nexus_tx_cm.png
exists=true
```

Red Nexus:

```text
material[0] name=Destroyed
diffuse=Client/Bin/Resource/Texture/Object/Nexus/nexus_destroyed_tx_cm_red.png
exists=true

material[1] name=SRUAP_OrderNexus_Mat
diffuse=Client/Bin/Resource/Texture/Object/Nexus/nexus_tx_cm_red.png
exists=true
```

2-5. 텍스처 바인딩 과정

- `CModel::LoadCookedTextures`
  - `.wmat` entry를 순회한다.
  - `entry.material_index` 위치에 `CTexture::Create(...)` 결과를 저장한다.
  - RHI scene path용 `RHI_CreateTextureFromFile(...)`도 같은 diffuse path로 만든다.
- `CModel::ResolveMaterialTexture`
  - submesh index -> submesh material index -> material texture로 resolve한다.
- `CModel::RenderWithMask`
  - visible submesh만 순회한다.
  - submesh material texture를 PS slot 0에 bind한다.
  - mesh index range를 draw한다.

3. 버그 위치

3-1. 직접 위치

- `Client/Private/Manager/Structure_Manager.cpp`
- 기존 경로는 Nexus를 `Render()` 또는 snapshot append all-visible 경로로 처리했다.
- `Render()`는 내부에서 `MakeAllVisibleMask()`를 사용하므로 submesh 0과 1을 모두 그린다.

3-2. 왜 Blue Nexus에서 눈에 띄었나

- Blue Nexus alive texture는 `nexus_tx_cm.png`이고 destroyed texture는 `nexus_destroyed_tx_cm.png`다.
- 기존 렌더는 살아있는 상태에서도 `Destroyed` material이 달린 submesh 0을 같이 그렸다.
- 그래서 정상 alive surface 위/아래에 파괴 상태 geometry/material이 섞여 보일 수 있다.
- Red Nexus도 동일한 구조를 갖고 있으므로, 이번 수정은 Blue-only 특례가 아니라 양 팀 Nexus 모두에 적용했다.

4. 수정 내용

4-1. 상태별 Nexus submesh mask 추가

- 위치: `Client/Private/Manager/Structure_Manager.cpp`
- 추가 함수:

```cpp
BuildNexusVisibilityMask(const StructureComponent& structure)
```

- 규칙:

```text
alive      : submesh[0] Destroyed = hidden, submesh[1] Alive = visible
destroyed : submesh[0] Destroyed = visible, submesh[1] Alive = hidden
```

4-2. DX11 immediate render path 반영

- Nexus 렌더 시 `Render()` 대신 `RenderWithVisibility(mask)`를 호출한다.
- 다른 구조물은 기존 `RenderFrustumCulled(...)` 경로를 유지한다.

4-3. RHI Scene snapshot path 반영

- Nexus snapshot append 시 `AppendRenderSnapshotMeshes(snapshot, mask)`를 호출한다.
- 다른 구조물은 기존 `AppendRenderSnapshotMeshesFrustumCulled(...)` 경로를 유지한다.

5. 검증

5-1. 코드 위생

```powershell
git diff --check
```

- 통과.
- 줄끝 정규화 경고만 표시됨.

5-2. Client Debug 빌드

```powershell
& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' .\Client\Include\Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

- 통과.
- 산출물: `Client/Bin/Debug/WintersGame.exe`
- 기존 DLL interface 계열 경고는 남아 있음.

6. 남은 런타임 확인

- 실제 인게임에서 Blue Nexus alive 상태가 `nexus_tx_cm.png` 기반으로 정상 표시되는지 확인한다.
- Nexus HP가 0 이하로 내려가는 테스트가 가능하면 destroyed submesh가 반대로 표시되는지 확인한다.
- RHI scene renderer 경로를 켠 상태에서도 같은 mask가 적용되는지 확인한다.
