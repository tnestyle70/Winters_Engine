# Elden Ring Asset Extraction Survey - 2026-05-25

## 결론

원래 목표였던 asset extract 관점에서 보면 방향은 명확하다.

1. `C:/Users/tnest/Desktop/EldenRing`에는 이미 캐릭터/애니/FX 일부가 FBX/PNG/DDS까지 추출되어 있다.
2. `Downloads`에는 UXM, WitchyBND, Blender, DS Anim Studio, Soulstruct Blender add-on, texconv가 모두 존재한다.
3. Unreal 쪽 실제 프로젝트/임포트 산출물은 현재 확인되지 않았다. `Desktop/UE5`, `Documents/Unreal Projects`, `source/Unreal` 모두 실질 파일이 없다.
4. Winters 변환기는 빌드 가능하고, `.wskel/.wanim/.wmesh` smoke test도 일부 통과했다.
5. 다만 Elden Ring 캐릭터 스켈레톤은 Winters 현재 GPU skinning 한계인 256 bones를 넘는다. 캐릭터 mesh binary 안정화 전에 bone buffer 확장이 필요하다.

따라서 첫 목표는 "전체 Elden 캐릭터 완전 로드"가 아니라 아래 순서가 맞다.

```text
Extract pipeline inventory
-> toolchain script화
-> low-bone/static FX mesh로 Winters binary smoke scene
-> material manifest 정규화
-> 256+ bone skinning 확장
-> Elden character binary load
-> minimal WintersElden client boot
```

## 조사 대상

### 메모장

대상:

```text
C:/Users/tnest/메모장/Elden Ring.txt
```

확인된 핵심 절차:

| 영역 | 메모 내용 요약 |
|---|---|
| BND/DCX | `chr`, `div`, `tex`를 WitchyBND로 풀기 |
| Character mesh | Blender에서 `chr` FLVER import |
| Animation | Game root 설정 후 `div` animation / generated hkx import |
| FBX animation export | Armature, FBX All, Forward `-Z`, Only Deform Bones, Add Leaf Bone off, All Actions on |
| Texture | TPF -> DDS -> PNG, `texconv.exe -ft png -o ./output *.dds` |
| Material mapping | material name과 png name을 출력하고 `mapping.csv` 기반으로 albedo/normal/mask 연결 |
| Material prefix | `C[(id)]_(name)`에서 prefix 추출 후 `_a`, `_n`, `_m` suffix 매칭 |
| FBX textured export | PathMode Copy, embed textures option, Armature/Mesh, animation off |
| Unreal check | FBX와 PNG를 같은 위치에 두고 character/anim import 확인 |
| FX | FXR XML -> JSON 파싱, model/texture 정보 추출 |

### 도구 경로

| 도구 | 경로 | 상태 |
|---|---|---|
| Blender 4.2.18 | `C:/Users/tnest/Downloads/blender-4.2.18-windows-x64/blender-4.2.18-windows-x64/blender.exe` | 사용 가능 |
| WitchyBND release | `C:/Users/tnest/Downloads/WitchyBND-v3.0.0.0-win-x64` | 존재 |
| WitchyBND source | `C:/Users/tnest/Downloads/WitchyBND-main` | 존재 |
| UXM Selective Unpack | `C:/Users/tnest/Downloads/UXM.Selective.Unpack.2.4.2.0` | 존재 |
| DS Anim Studio | `C:/Users/tnest/Downloads/DSAS-V5-RC4.2` | 존재 |
| Soulstruct Blender add-on | `C:/Users/tnest/Downloads/io_soulstruct-2.5.0 (1)` | 존재 |
| texconv | `C:/Users/tnest/Downloads/texconv.exe` | 존재 |
| raw chr dump | `C:/Users/tnest/Downloads/chr` | 존재 |

UXM README 기준: archive unpack 및 loose file 기반 modding용. Elden Ring은 unpacked/modified executable 상태로 online play 불가.

WitchyBND README 기준: DCX, BND3/4, FFXBND, TPF, FXR1/3, MATBIN, MTD, PARAM 등 FromSoftware 포맷 unpack/repack/serialize 지원. UXM으로 game archive를 먼저 풀어야 한다.

Soulstruct 쪽 확인: Elden Ring FLVER는 texture name을 거의 직접 들고 있지 않고, material/texture 정보가 MATBIN으로 deferred되는 구조다. 즉 완전 자동화를 하려면 suffix 매칭만으로 끝내지 말고 MATBIN/FXR 파서가 필요하다.

DS Anim Studio config 확인:

| 항목 | 값 |
|---|---|
| FLVER import | `ConvertFromZUp: true`, `SceneScale: 1.0`, `KeepOriginalDummyPoly: true` |
| Anim FBX import | `RootMotionNodeName: root`, `SampleToFramerate: 60.0`, root motion 관련 옵션 enabled |
| Game root | 현재 config에는 고정 저장값 없음 |

## Unreal 폴더 조사

조사한 위치:

```text
C:/Users/tnest/Desktop/UE5
C:/Users/tnest/Documents/Unreal Projects
C:/Users/tnest/source/Unreal
C:/Users/tnest/Desktop
```

결과:

| 항목 | 결과 |
|---|---|
| `.uproject` | Elden 관련 파일 없음 |
| `.uasset` / `.umap` | Elden 관련 파일 없음 |
| Unreal material import 산출물 | 없음 |
| 참고 문서 | `.markdown/엔진/Winters&Unreal/*`만 존재 |

따라서 지금은 Unreal 폴더에서 가져올 material/import 메타데이터가 없다. 현재 source of truth는 `Desktop/EldenRing`의 FBX/PNG/DDS와 `Downloads/chr`의 raw unpack 결과다.

## 추출 산출물 인벤토리

대상:

```text
C:/Users/tnest/Desktop/EldenRing
```

원본 조사 시점 확장자 수:

| 확장자 | 개수 |
|---|---:|
| `.png` | 674 |
| `.fbx` | 17 |
| `.dds` | 4 |

디렉터리별 크기:

| 경로 | 파일 수 | 크기 |
|---|---:|---:|
| `chr3000` | 41 | 41.5 MB |
| `chr3010` | 68 | 88.7 MB |
| `chrTex2130` | 119 | 160.2 MB |
| `chrTex2130.fbm` | 4 | 6.3 MB |
| `sfx` | 458 | 131.9 MB |

### FBX 목록

| FBX | 크기 | Blender import 요약 |
|---|---:|---|
| `Character1.fbx` | 353.3 MB | `c2050`, mesh 2, armature 1, bones 1062, materials 38, images 2, actions 68 |
| `anim2130_1.fbx` | 37.0 MB | mesh 0, bones 714, actions 9 |
| `chr2130.fbx` | 6.9 MB | mesh 1, bones 714, materials 32, images 0 |
| `chrTex2130.fbx` | 13.2 MB | mesh 1, bones 714, materials 31, image paths 4 |
| `chrTex2130/chr2130Separated.fbx` | 38.2 MB | mesh 5, bones 714, materials 31, material images 31/31, image paths 38 |
| `chrTex2130/chrTex2130_1.fbx` | 35.6 MB | mesh 1, bones 714, materials 31, material images 31/31, image paths 38 |
| `chr3000/chr3000.fbx` | 8.6 MB | mesh 1, bones 368, materials 49, material images 49/49, image paths 39 |
| `chr3000/anim3000.fbx` | 26.6 MB | mesh 0, bones 368, actions 15 |
| `chr3010/chr3010.fbx` | 17.4 MB | mesh 1, bones 354, materials 39, material images 36/39, image paths 32 |
| `chr3010/anim3010.fbx` | 44.9 MB | mesh 1, bones 354, materials 39, images 0, actions 21 in Blender |
| `sfx/WP_A_7051.fbx` | 0.1 MB | mesh 1, bones 36, materials 6, images 0 |
| `sfx/s88070Blender.fbx` | tiny | mesh 1 sphere, no materials |
| `MeshEffect.fbx`, `sfx/EffectModeling.fbx`, `sfx/MeshEffect.fbx`, `sfx/S88070Mesh.fbx`, `sfx/s88070.fbx` | tiny | 대부분 empty/helper FBX |

권장 첫 캐릭터 후보:

| 후보 | 장점 | 단점 |
|---|---|---|
| `chr3010` | material path가 비교적 잘 살아 있고 anim도 20개 변환 가능 | 354~357 bones라 현재 skinned mesh binary 불가 |
| `chr3000` | material 49/49 이미지 연결 | 368 bones |
| `chr2130Separated` | mesh 분리와 material image 연결이 좋음 | 714 bones, 매우 복잡 |
| `Character1` | action 68개 대량 포함 | 1062 bones, 353 MB, 첫 타겟 부적합 |

### Texture suffix 분포

| suffix | 개수 | 1차 의미 |
|---|---:|---|
| `_a` | 338 | Albedo/BaseColor 후보 |
| `_n` | 205 | Normal 후보 |
| `_m` | 86 | Mask/metallic/material packed 후보 |
| `_em` | 25 | Emissive 후보 |
| `_1m` | 14 | 추가 mask 후보 |
| `_r` | 8 | Roughness/reflection/ramp 계열 후보 |
| `_d` | 1 | Depth/detail 후보 |
| `_3m` | 1 | 추가 mask 후보 |

초기 Winters 렌더는 albedo만 연결해도 시각 확인 가능하다. 하지만 포트폴리오급 material pipeline은 `_n`, `_m`, `_em`, `_r`, `_1m/_3m`까지 `.wmat`에 기록해야 한다.

### PNG 해상도 분포

| 해상도 | 개수 |
|---|---:|
| 256x256 | 152 |
| 512x512 | 118 |
| 128x128 | 69 |
| 1024x1024 | 43 |
| 256x512 | 38 |
| 2048x2048 | 27 |
| 128x256 | 26 |
| 256x128 | 24 |
| 256x1024 | 21 |
| 128x512 | 20 |

큰 PNG 예시:

| 파일 | 해상도 | 크기 |
|---|---:|---:|
| `chrTex2130/c2130_fabric1_n.png` | 2048x2048 | 8.3 MB |
| `sfx/s31301_a.png` | 2048x2048 | 5.8 MB |
| `sfx/s31021_a.png` | 2048x2048 | 5.4 MB |
| `sfx/s31051_a.png` | 2048x2048 | 5.2 MB |

### raw chr dump

대상:

```text
C:/Users/tnest/Downloads/chr
```

확장자 분포:

| 확장자 | 개수 |
|---|---:|
| `.dcx` | 1658 |
| `.HKX` | 106 |
| `.dds` | 52 |
| `.xml` | 10 |
| `.txt` | 5 |
| `.flver` | 3 |
| `.tpf` | 2 |
| `.tae` | 1 |
| `.clm2` | 1 |

`c2130` raw unpack 확인:

| 경로 | 의미 |
|---|---|
| `c2130-chrbnd-dcx/c2130.flver` | character mesh 원본 |
| `c2130-chrbnd-dcx/c2130.hkx` | skeleton/physics 계열 |
| `c2130-anibnd-dcx-wanibnd/.../hkx` | base animation |
| `c2130_div00-anibnd-dcx-wanibnd/.../hkx_div00_compendium` | div animation |
| `c2130_div01-anibnd-dcx-wanibnd/.../hkx_div01_compendium` | div animation |
| `c2130_h-texbnd-dcx/c2130_h-tpf/*.dds` | texture 원본 |
| `_witchy-*.xml` | WitchyBND manifest |

## Winters 변환기 검증

변환기 위치:

```text
C:/Users/tnest/Desktop/Winters/Tools/WintersAssetConverter/WintersAssetConverter.vcxproj
```

직접 vcxproj 빌드는 `$(SolutionDir)`가 비정상 해석되어 Assimp lib path가 틀어질 수 있다. 아래처럼 `SolutionDir`를 명시하면 성공한다.

```bat
"C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/amd64/MSBuild.exe" ^
  "C:/Users/tnest/Desktop/Winters/Tools/WintersAssetConverter/WintersAssetConverter.vcxproj" ^
  /p:Configuration=Debug /p:Platform=x64 ^
  /p:SolutionDir="C:/Users/tnest/Desktop/Winters/" /m
```

빌드 산출물:

```text
C:/Users/tnest/Desktop/Winters/Tools/Bin/Debug/WintersAssetConverter.exe
```

지원 명령:

| 명령 | 역할 |
|---|---|
| `skel <fbx> -o <out.wskel>` | skeleton 추출 |
| `mesh <fbx> --skel <wskel> -o <out.wmesh>` | mesh 추출 |
| `anim <fbx> --skel <wskel> -o <out_dir>` | animation clip 추출 |
| `info <wmesh/wskel/wanim>` | Winters binary 검사 |

### smoke test 결과

테스트 출력 위치:

```text
C:/Users/tnest/Desktop/EldenRing/_winters_probe
```

`chr3010`:

| 단계 | 결과 |
|---|---|
| `skel chr3010.fbx` | 성공, bones 357, hash `0xf0baf909b5455f46` |
| `info chr3010.wskel` | 성공 |
| `mesh chr3010.fbx --skel chr3010.wskel` | 실패 |
| 실패 이유 | `WMeshWriter`가 `skel bone_count >= 256`을 거절 |
| `anim anim3010.fbx --skel chr3010.wskel` | 성공, 20/20 `.wanim` |
| sample `.wanim info` | channels 355, duration 112 ticks, tick rate 24, keys 24192, same skel hash |

`sfx/WP_A_7051`:

| 단계 | 결과 |
|---|---|
| `skel WP_A_7051.fbx` | 성공, bones 39 |
| `mesh WP_A_7051.fbx --skel WP_A_7051.wskel` | 성공 |
| `info WP_A_7051.wmesh` | submeshes 6, bones 39, vertices 1485, indices 3420, stride 76 |

따라서 첫 Winters binary 렌더 smoke test는 `WP_A_7051` 같은 low-bone FX/weapon mesh로 잡는 것이 안전하다.

## 중대 기술 이슈

### 1. Elden character는 256 bone 제한을 초과한다

현재 shader:

```hlsl
row_major matrix g_BoneMatrices[256];
```

현재 writer guard:

```cpp
if (pSkelNameToIdx->size() >= 256)
    return false;
```

Elden 추출물:

| Asset | Blender bones | Winters `.wskel` bones |
|---|---:|---:|
| `chr3010` | 354 | 357 |
| `chr3000` | 368 | not tested, expected > 256 |
| `chr2130` | 714 | not tested, expected > 256 |
| `Character1/c2050` | 1062 | not tested, expected > 256 |

해결책 후보:

| 후보 | 설명 | 판단 |
|---|---|---|
| StructuredBuffer bone matrices | VS에서 `StructuredBuffer<float4x4>` 또는 3x4 matrix buffer로 bone palette를 SRV 바인딩 | 가장 직접적 |
| Per-submesh local palette | submesh별 사용 bone을 256 이하로 remap하고 shader에는 local palette만 업로드 | 성능 좋음, 포맷 변경 필요 |
| CPU skinning fallback | CPU에서 skinned vertex 업데이트 | 구현은 빠르지만 포트폴리오/성능 관점에서 후순위 |
| Skeleton prune | 사용하지 않는 helper/root/dummy bone 제거 | 보조 최적화, 단독 해결 불확실 |

권장: 먼저 StructuredBuffer로 1024+ bones를 통과시키고, 이후 per-submesh palette를 최적화로 추가한다.

### 2. Material은 "보이는 것"과 "정확한 것"이 다르다

현재 `CModel::LoadTextures`는 Assimp diffuse texture만 로드한다. 즉 albedo 시각화는 가능하지만 normal/mask/emissive는 아직 엔진 material로 살아나지 않는다.

필요한 정식 단계:

```text
FBX material name
-> extract C[id]_material token
-> resolve texture set from MATBIN if available
-> fallback suffix match: _a/_n/_m/_em/_r/_1m/_3m
-> write material_manifest.json
-> later .wmat + .wtex
```

### 3. FX는 FBX만으로는 부족하다

`sfx` 폴더에는 PNG가 452개 있고, 일부 mesh FBX가 있지만 material image path가 비어 있는 파일이 있다. 특히 `P[WP_A_7050]_Glow` 같은 material은 FXR/FFXBND 쪽 연결 정보가 필요하다.

FX 최종 파이프라인:

```text
FFXBND/FXR
-> WitchyBND XML
-> FXR parser JSON
-> mesh id / texture id / material token resolve
-> .wfx graph
-> .wmesh/.wtex binding
```

## 완전 추출 파이프라인 설계

### Canonical pipeline

```text
1. UXM Selective Unpack
   chr / map / asset / sfx / material 관련 원본 확보

2. WitchyBND unpack
   .dcx / .chrbnd / .anibnd / .texbnd / .ffxbnd / .matbinbnd / .fxr

3. Raw decode
   FLVER -> Blender/Soulstruct import
   HKX/ANIBND -> Blender/Soulstruct or DSAS import
   TPF/DDS -> PNG preview + DDS retain
   MATBIN -> material sampler manifest
   FXR -> FX graph manifest

4. Normalized authoring assets
   normalized FBX
   texture folder
   material_manifest.json
   animation_manifest.json
   source_manifest.json

5. Winters binary conversion
   .wskel
   .wmesh
   .wanim
   .wtex later
   .wmat later
   .wfx later

6. Validation
   Blender import summary
   converter info
   texture existence
   material coverage
   bone count / shader compatibility
   first viewport smoke render

7. Mini WintersElden project
   boot
   load one asset
   orbit/dynamic camera
   material preview
   animation preview after 256+ bone fix
```

### 권장 디렉터리

Raw/copyright asset은 Git에 넣지 않는다.

```text
C:/Users/tnest/Desktop/EldenRing/
  raw/              # UXM/Witchy 원본 unpack mirror
  staging/          # FLVER/HKX/TPF/MATBIN/FXR 중간 산출
  normalized/       # Blender export FBX + PNG/DDS + manifest
  winters/          # .wmesh/.wskel/.wanim/.wtex/.wmat/.wfx
  _winters_probe/   # smoke test output, 임시
```

Winters repo에는 코드/문서/스크립트만 둔다.

```text
Winters/
  Tools/EldenAssetPipeline/
    inspect_fbx.py
    build_material_manifest.py
    convert_character.bat
    convert_fx.bat
    validate_asset_manifest.py
  .md/EldenRing/
    pipeline docs
```

### Manifest 최소 스키마

```json
{
  "assetId": "chr3010",
  "source": {
    "meshFbx": "C:/Users/tnest/Desktop/EldenRing/chr3010/chr3010.fbx",
    "animFbx": "C:/Users/tnest/Desktop/EldenRing/chr3010/anim3010.fbx",
    "rawBnd": []
  },
  "mesh": {
    "meshCount": 1,
    "boneCount": 357,
    "materials": 39
  },
  "materials": [
    {
      "name": "C[c3010]_BD",
      "albedo": "c3010_BD_a.png",
      "normal": "c3010_BD_n.png",
      "mask": "c3010_BD_m.png"
    }
  ],
  "animations": [
    {
      "name": "a002_009451",
      "frames": [1, 113]
    }
  ],
  "validation": {
    "fbxImportsInBlender": true,
    "wintersSkel": true,
    "wintersMesh": false,
    "blocker": "bone_count >= 256"
  }
}
```

## 세션 진행안

### Session A - pipeline 재고와 스크립트화

완료한 것:

| 작업 | 결과 |
|---|---|
| 도구 위치 확인 | 완료 |
| 메모장 절차 확인 | 완료 |
| Unreal 폴더 확인 | 실질 산출물 없음 |
| FBX Blender import 전수 요약 | 완료 |
| texture suffix/dimension 조사 | 완료 |
| WintersAssetConverter build | 성공 |
| converter smoke test | partial success |

다음 작업:

1. `Tools/EldenAssetPipeline/inspect_fbx.py` 작성
2. `Tools/EldenAssetPipeline/build_material_manifest.py` 작성
3. `chr3010`, `chr3000`, `chr2130Separated`, `sfx/WP_A_7051` manifest 자동 생성
4. `_winters_probe` 결과를 정리하고 재실행 가능한 bat로 고정

### Session B - 첫 화면에 띄우기

첫 화면 목표는 캐릭터가 아니라 `WP_A_7051` low-bone mesh가 좋다.

합격 기준:

| 항목 | 기준 |
|---|---|
| `WintersAssetConverter` | 재빌드 없이 실행 가능 |
| `.wmesh/.wskel` | `info` 통과 |
| mini scene | `WP_A_7051` 표시 |
| camera | orbit/free camera로 관찰 |
| material | 최소 albedo 또는 debug color 표시 |

### Session C - Elden character를 위한 engine 확장

필수 선행:

1. Skinned shader bone upload를 256 고정 cbuffer에서 확장
2. `WMeshWriter`의 256 guard 제거 또는 새 경로 추가
3. `WMeshFormat`에 large skeleton/bone buffer 정책 기록
4. `CModel` runtime validation 변경
5. `chr3010.wmesh` 변환 성공
6. `chr3010` bind pose 표시
7. `anim3010` 한 clip 재생

### Session D - material/FX 완성도 올리기

1. MATBIN 기반 material resolver
2. suffix fallback resolver
3. `.wmat` 초안
4. normal/mask/emissive shader path
5. FXR JSON -> `.wfx` 초안

## 지금 기준 최우선 TODO

1. `Tools/EldenAssetPipeline` 생성: inspect/manifest/convert/validate 스크립트
2. `WP_A_7051`을 첫 binary render target으로 삼아 mini scene을 띄우기
3. `chr3010`은 `.wskel/.wanim`만 계속 검증하고, `.wmesh`는 256+ bone fix 이후 진행
4. material은 현재 FBX diffuse path에 기대는 임시 경로와 `.wmat` 정식 경로를 분리
5. Unreal folder 의존 제거: 현재 확인된 Unreal 산출물이 없으므로 Winters pipeline 자체를 source of truth로 둔다

## 포트폴리오 관점 기록

공개 포트폴리오에는 raw Elden Ring asset을 커밋하거나 배포하지 않는다. 보여줄 것은 다음이다.

| 공개 가능 중심 | 비공개 유지 |
|---|---|
| extractor pipeline 구조 | 원본 game asset |
| manifest/schema/code | 추출된 FBX/PNG/DDS 자체 |
| 자체 binary format 변환기 | 상용 asset 재배포 |
| engine rendering/network/editor 구현 | 원본 리소스 팩 |
| 짧은 로컬 시연 영상/스크린샷 | asset archive |

포트폴리오 메시지는 "상용 엔진 없이 FromSoftware 계열 복잡 asset pipeline을 분석하고, 자체 DX11 engine binary/runtime으로 흡수하는 구조를 설계/구현했다"가 가장 세다.
