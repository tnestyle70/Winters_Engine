# EldenRing Asset Extraction To Winters Binary Pipeline

## 목표

먼저 에셋 파이프라인을 안정화한다.

그 다음 `WintersElden.exe` 런타임 개발을 밀어붙인다.

순서:

```
Original extracted assets
  -> normalized source assets
  -> FBX/PNG/DDS verification
  -> Winters binary conversion
  -> runtime load validation
  -> client gameplay development
```

## 로컬 경로

원본/실험 루트:

```
C:/Users/tnest/Desktop/EldenRing
```

프로젝트 편입 루트:

```
WintersElden/Bin/Resource/
```

권장 편입 구조:

```
WintersElden/Bin/Resource/
├── Character/
│   ├── chr3000/
│   ├── chr3010/
│   └── chr2130/
├── Map/
│   ├── FieldTest/
│   └── DungeonTest/
├── FX/
│   └── sfx/
├── UI/
├── Sound/
└── Bundles/
```

## 사용 도구

| 도구 | 경로 | 용도 |
|---|---|---|
| Blender 4.2.18 | `C:/Users/tnest/Downloads/blender-4.2.18-windows-x64` | FBX import/export, material 적용, animation export |
| WitchyBND | `C:/Users/tnest/Downloads/WitchyBND-v3.0.0.0-win-x64` | BND/TPF 추출 |
| UXM Selective Unpack | `C:/Users/tnest/Downloads/UXM.Selective.Unpack.2.4.2.0` | 게임 데이터 unpack |
| WintersAssetConverter | `Tools/Bin/Debug/WintersAssetConverter.exe` | `.wmesh/.wskel/.wanim` 변환 |
| texconv | 별도 배치 위치 필요 | DDS -> PNG 또는 추후 DDS -> `.wtex` |

## 법적/배포 경계

원본 추출 에셋은 로컬 연구/비공개 시연용이다.

금지:

1. GitHub에 원본 EldenRing 에셋 업로드
2. 공개 빌드에 원본 추출 에셋 포함
3. 에셋 번들 공개 배포

허용 목표:

1. 로컬에서 엔진 파이프라인 검증
2. 면접/비공개 시연 영상 제작
3. 공개용 빌드에서는 자체/대체 에셋으로 교체
4. 포트폴리오에는 엔진 코드, 변환 도구, 구조 문서, 영상 중심으로 공개

## 입력 자산 인벤토리

현재 확인된 파일 유형:

| 확장자 | 수량 | 의미 |
|---|---:|---|
| `.fbx` | 17 | 캐릭터/애니/FX 메시 |
| `.png` | 674 | 캐릭터/FX 텍스처 |
| `.dds` | 4 | 원본 또는 중간 텍스처 |

첫 안정화 후보:

| 후보 | 이유 |
|---|---|
| `chr3010/chr3010.fbx` + `chr3010/anim3010.fbx` | 크기 적당, 텍스처 다수, 첫 캐릭터 검증용 |
| `chr3000/chr3000.fbx` + `chr3000/anim3000.fbx` | 두 번째 캐릭터 검증용 |
| `chrTex2130/chr2130Separated.fbx` | 분리 메시/머티리얼 매핑 검증용 |
| `sfx/s88070Blender.fbx` | FX mesh path 검증용 |

## 전체 파이프라인

```
UXM / WitchyBND
  -> chr/div/tex/sfx unpack
  -> Blender import
  -> material mapping
  -> normalized FBX export
  -> texture normalization
  -> WintersAssetConverter
       skel -> .wskel
       mesh -> .wmesh
       anim -> .wanim
  -> info validation
  -> runtime load
```

## 1단계: 원본 추출

로컬 메모 기준:

1. `chr`, `div`, `tex`를 WitchyBND에 드래그 앤 드랍한다.
2. 텍스처는 TPF를 한 번 더 풀어 DDS를 만든다.
3. DDS는 `texconv`로 PNG 중간 산출물을 만든다.

예시:

```bat
mkdir output
texconv.exe -ft png -o ./output *.dds
```

주의:

1. 원본 폴더는 수정하지 않는다.
2. 변환 중간 산출물은 `WintersElden/Bin/Resource/_SourceStaging/` 또는 별도 staging 폴더에 둔다.
3. C++ 문자열에 Windows 경로를 넣을 때는 `\U`, `\u`, `\W` 이스케이프 사고를 피하기 위해 forward slash를 쓴다.

## 2단계: Blender 표준화

캐릭터 메시 import:

1. chr 필터로 Blender import
2. armature + mesh 확인
3. material name 출력
4. texture filename 출력
5. CSV mapping으로 material -> texture 연결

Blender material name 출력:

```python
import bpy

for mat in bpy.data.materials:
    print(mat.name)
```

텍스처 파일명 출력:

```python
import os

TEX_DIR = r"C:/path/to/texture/folder"

for f in sorted(os.listdir(TEX_DIR)):
    if f.endswith(".png"):
        print(f)
```

자동/수동 material mapping은 기존 메모의 `mapping.csv` 방식을 유지한다.

## 3단계: FBX export 표준

메시 FBX export:

| 설정 | 값 |
|---|---|
| Path Mode | Copy |
| Embed textures | 필요 시 선택. 런타임 최종은 외부 텍스처 또는 `.wtex` |
| Object Types | Armature, Mesh |
| Apply Scaling | FBX All |
| Forward | `-Z Forward` |
| Apply Unit | off |
| Apply Transform | off |
| Add Leaf Bone | off |
| Only Deform Bones | on |
| Animation | 메시 export에서는 off 권장 |

애니메이션 FBX export:

| 설정 | 값 |
|---|---|
| Object Types | Armature |
| All Actions | on |
| Add Leaf Bone | off |
| Only Deform Bones | on |
| Apply Unit | off |

## 4단계: 프로젝트 편입 규칙

예시 `chr3010`:

```
WintersElden/Bin/Resource/Character/chr3010/
├── chr3010.fbx
├── anim3010.fbx
├── textures/
│   ├── c3010_BD_a.png
│   ├── c3010_BD_n.png
│   └── ...
├── mapping.csv
├── chr3010.wskel
├── chr3010.wmesh
└── anims/
    ├── idle.wanim
    ├── walk.wanim
    └── ...
```

초기에는 FBX material 경로가 텍스처를 찾기 쉽게 `textures/`와 같은 폴더를 유지한다. `.wtex/.wmat` 진입 후에는 material 경로 의존을 제거한다.

## 5단계: Winters binary 변환

캐릭터 변환 순서는 반드시 아래다.

```bat
set CONV=Tools\Bin\Debug\WintersAssetConverter.exe
set DIR=WintersElden\Bin\Resource\Character\chr3010
set FBX=chr3010.fbx
set BASE=chr3010

%CONV% skel "%DIR%\%FBX%" -o "%DIR%\%BASE%.wskel"
%CONV% mesh "%DIR%\%FBX%" --skel "%DIR%\%BASE%.wskel" -o "%DIR%\%BASE%.wmesh"

if not exist "%DIR%\anims" mkdir "%DIR%\anims"
del /Q "%DIR%\anims\*.wanim" 2>nul
%CONV% anim "%DIR%\anim3010.fbx" --skel "%DIR%\%BASE%.wskel" -o "%DIR%\anims"
```

만약 메시 FBX와 애니 FBX의 skeleton hierarchy가 다르면:

1. Blender에서 같은 armature 기준으로 재export한다.
2. 그래도 hash가 다르면 `wskel` 기준 bone name/order remap 도구가 필요하다.

## 6단계: info 검증

필수 검증:

```bat
%CONV% info "%DIR%\chr3010.wskel"
%CONV% info "%DIR%\chr3010.wmesh"
%CONV% info "%DIR%\anims\<one>.wanim"
```

합격 기준:

| 항목 | 기준 |
|---|---|
| `wmesh.vertex_stride` | 76 |
| `wmesh.bone_count` | `wskel.bone_count`와 동일 |
| `wanim.skel_hash` | `wskel.hash`와 동일 |
| `bone_count` | 256 미만 |
| submesh | 1개 이상 |

불합격 시 런타임으로 넘어가지 않는다.

## 7단계: Runtime load 검증

`WintersElden.exe`에서 첫 캐릭터 로드 시 기대 로그:

```
[CModel] .wmesh+.wskel fast-path: ...chr3010.wmesh
[CModel] wskel loaded: bones=N hash=0x...
[CModel] Loaded N wanim files
[CModel] Loaded from .wmesh: meshes=M textures=K
```

시각 검증:

| 항목 | 기준 |
|---|---|
| bind pose | idle 재생 시 탈출 |
| skinning | 폭발/뒤틀림 없음 |
| scale | 액션 RPG 캐릭터 기준 1.7~2.2m 근처 |
| orientation | forward 방향 통일 |
| material | 최소 albedo visible |
| normal | Phase PBR 전에는 optional |

## 8단계: 텍스처 `.wtex` 전환

초기:

```
FBX material path -> PNG load
```

목표:

```
PNG/DDS -> .wtex
material mapping.csv/json -> .wmat
runtime -> .wmat references .wtex
```

권장 `.wtex` 정책:

| 텍스처 | 포맷 | sRGB | mip |
|---|---|---|---|
| albedo `_a` | BC7 | yes | yes |
| normal `_n` | BC5 | no | yes |
| mask `_m`, roughness, metallic | BC4/BC7 | no | yes |
| emissive `_em` | BC7 | yes 또는 HDR 정책 | yes |
| UI | BC7 또는 RGBA8 | yes | 필요 시 |

## 9단계: 맵/UI/FX 전체 추출 계획

캐릭터 다음 순서:

| 순서 | 대상 | 산출 |
|---|---|---|
| 1 | Character | `.wmesh/.wskel/.wanim/.wtex/.wmat` |
| 2 | FX mesh/texture | `.wmesh/.wtex/.wfx` |
| 3 | UI | `.wtex`, UI atlas metadata |
| 4 | Map static mesh | `.wmesh/.wtex/.wmat` |
| 5 | Map partition metadata | `.wmap` |
| 6 | Audio | FMOD event or loose sound key |
| 7 | Bundle | `.winters` |

## 10단계: 자동화 도구 목표

`Tools/EldenAssetPipeline/` 추가 후보:

```
Tools/EldenAssetPipeline/
├── README.md
├── elden_asset_manifest.json
├── convert_elden_character.bat
├── convert_elden_all.bat
├── blender/
│   ├── apply_material_mapping.py
│   ├── export_character_fbx.py
│   └── validate_fbx_scene.py
└── scripts/
    ├── inventory_elden_assets.ps1
    ├── make_mapping_template.py
    └── validate_winters_binary.py
```

Manifest 예시:

```json
{
  "characters": [
    {
      "id": "chr3010",
      "meshFbx": "WintersElden/Bin/Resource/Character/chr3010/chr3010.fbx",
      "animFbx": "WintersElden/Bin/Resource/Character/chr3010/anim3010.fbx",
      "textureDir": "WintersElden/Bin/Resource/Character/chr3010/textures",
      "scale": 1.0,
      "forward": "-Z"
    }
  ]
}
```

## 안정화 게이트

아래 게이트가 모두 통과하기 전에는 대량 콘텐츠 작업으로 넘어가지 않는다.

| Gate | 기준 |
|---|---|
| A0 Inventory | 원본 에셋 목록 자동 생성 |
| A1 Character | `chr3010` 변환 + 런타임 표시 |
| A2 Animation | idle/run/attack/dodge 4종 재생 |
| A3 Texture | albedo/normal/mask 매핑 확인 |
| A4 FX | sfx mesh 1개 + texture 1개 표시 |
| A5 Map | static map chunk 1개 표시 |
| A6 Binary | FBX 없이 `.w*` 우선 로드 |
| A7 Package | 최소 `.winters` bundle 로드 |

## 작업 철학

에셋을 많이 뜯는 것보다 중요한 것은 "한 개를 끝까지 통과시키는 파이프라인"이다.

첫 캐릭터 하나가 아래 경로를 완주하면, 이후 캐릭터/맵/UI/FX는 배치 작업으로 확장할 수 있다.

```
extract -> normalize -> convert -> validate -> load -> render -> animate
```
