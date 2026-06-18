Session - Elden Ring PBR material resolver and renderer path

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/EldenRingClient/Public/EldenRingMaterialResolver.h

새 파일:

```cpp
CONFIRM_NEEDED - .wmat 포맷, WintersAssetConverter material output, Elden Ring texture directory naming을 함께 확인한 뒤 전체 파일 본문을 작성한다.

책임:
- *_a albedo, *_n normal, *_m metallic, *_r roughness, *_em emissive suffix를 Winters material channel로 매칭한다.
- wmat에 들어온 material/submesh 이름과 주변 texture 후보를 score 기반으로 묶는다.
- 결과를 material_bindings.json으로 저장해서 editor가 재스캔 없이 재사용한다.
```

1-2. C:/Users/tnest/Desktop/Winters/EldenRingClient/Private/EldenRingMaterialResolver.cpp

새 파일:

```cpp
CONFIRM_NEEDED - EldenRingMaterialResolver.h 확정 후 전체 구현 본문을 작성한다.
구현 기준:
- exact match: same stem + suffix.
- relaxed match: material id, FLVER texture stem, folder-local texture.
- missing channel은 default texture 또는 scalar fallback으로 둔다.
- ambiguous match는 자동 적용하지 않고 ImGui에서 선택하게 한다.
```

1-3. C:/Users/tnest/Desktop/Winters/Engine/Public/Renderer/FxStaticMeshRenderer.h

삭제할 코드/범위:

```cpp
이번 세션에서는 CFxStaticMeshRenderer를 PBR renderer로 확장하지 않는다.
현재 renderer는 FX/static smoke용 diffuse/erode path이므로, 첫 material resolver 검증 뒤 별도 RHI static PBR renderer로 분리한다.
```

1-4. C:/Users/tnest/Desktop/Winters/Engine/Public/Renderer/RHIStaticPBRMeshRenderer.h

새 파일:

```cpp
CONFIRM_NEEDED - RHI texture binding 개수, normal/tangent vertex layout, existing PBR shader contract 확인 후 전체 파일 본문을 작성한다.

책임:
- DX12 RHI 전용 static PBR draw path를 제공한다.
- WMesh + material bindings를 입력으로 받는다.
- albedo, normal, metallic, roughness, emissive texture를 bind group으로 묶는다.
- GGX/PBR path는 기존 DX11 ModelRenderer PBR와 visual parity를 목표로 하되, 첫 세션은 "텍스처 채널이 올바르게 붙는지"까지만 본다.
```

1-5. C:/Users/tnest/Desktop/Winters/Engine/Private/Renderer/RHIStaticPBRMeshRenderer.cpp

새 파일:

```cpp
CONFIRM_NEEDED - RHIStaticPBRMeshRenderer.h 확정 후 전체 구현 본문을 작성한다.
구현 기준:
- CRHIFxMeshResourceCache의 WMesh loading 코드를 그대로 복사하지 말고 공유 가능한 ResourceCache 추출 여부를 먼저 판단한다.
- single directional light + camera position + material textures로 시작한다.
- missing normal은 flat normal, missing roughness/metallic은 scalar fallback을 쓴다.
```

1-6. C:/Users/tnest/Desktop/Winters/EldenRingClient/Private/EldenAssetProbeScene.cpp

기존 코드:

```cpp
    m_wstrSmokeDiffusePath.clear();
```

아래로 교체:

```cpp
    m_wstrSmokeDiffusePath.clear();
    CONFIRM_NEEDED - MaterialResolver가 생성한 material_bindings.json을 읽은 뒤,
    smoke mesh의 diffuse path를 resolved albedo로 교체한다.
    PBR renderer가 들어오기 전까지는 albedo만 CFxStaticMeshRenderer smoke에 연결한다.
```

2. 검증

2-1. 사전 확인

```powershell
Get-ChildItem "Client/Bin/Resource/EldenRing" -Recurse -Filter "*_a.png" | Measure-Object
Get-ChildItem "Client/Bin/Resource/EldenRing" -Recurse -Filter "*_n.png" | Measure-Object
Get-ChildItem "Client/Bin/Resource/EldenRing" -Recurse -Filter "*_m.png" | Measure-Object
Get-ChildItem "Client/Bin/Resource/EldenRing" -Recurse -Filter "*_r.png" | Measure-Object
Get-ChildItem "Client/Bin/Resource/EldenRing" -Recurse -Filter "*_em.png" | Measure-Object
```

2-2. 검증 명령

```powershell
msbuild Winters.sln /t:EldenRingClient /p:Configuration=Debug-DX12 /p:Platform=x64
```

2-3. 런타임 확인

```text
1. Material Resolver panel에서 selected asset의 albedo/normal/metallic/roughness/emissive 후보가 보인다.
2. exact/relaxed/ambiguous/missing 상태가 구분된다.
3. CFxStaticMeshRenderer smoke는 resolved albedo만 먼저 적용한다.
4. PBR renderer 전환 후 normal map 방향, roughness/metallic 값, emissive 강도를 개별 토글로 검증한다.
```

2-4. 다음 세션 게이트

```text
material matching이 안정화되기 전에는 World Partition 전체 렌더를 PBR로 바꾸지 않는다.
먼저 선택한 AEG 1개, Erdtree candidate 1개, character static 1개에서 texture channel이 맞는지 본다.
```
