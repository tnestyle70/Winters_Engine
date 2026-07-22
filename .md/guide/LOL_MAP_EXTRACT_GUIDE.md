# LoL 소환사의 협곡 맵 추출 → DX11 엔진 로드 가이드

> **최종 업데이트**: 2026-04-16
> **검증 맵**: base.mapgeo (소환사의 협곡 기본)
> **필요 도구**: Obsidian, lol2gltf v1.0.0+, Blender 5.1, Assimp 6.x
> **적용 엔진**: Winters Engine (DX11, Assimp, DirectXTK)

---

## 전체 흐름

```
[Obsidian]           [lol2gltf]                [Winters Engine]
map11.wad.client  →  base.mapgeo + .bin  →  sr_base.glb (텍스처 임베딩)  →  Assimp 직접 로드 → DX11 렌더링
                     mapgeo2gltf -d MAP       ↑ FBX 변환 불필요!
```

**핵심**: FBX 변환 없이 **glb를 Assimp으로 직접 로드**. glTF 임베딩 텍스처를 `CTexture::CreateFromMemory()`로 처리.

---

## Phase 1: WAD 추출 (Obsidian)

1. Obsidian 실행 → `File → Open`
2. `C:\Riot Games\League of Legends\Game\DATA\FINAL\Maps\Shipping\Map11.wad.client`
3. 전체 `Extract All` → `C:\Users\user\Desktop\LOL_Resource\MAP\`

### 추출 결과 핵심 파일

```
LOL_Resource/MAP/
├── data/maps/mapgeometry/map11/
│   ├── base.mapgeo           (27MB — 기본 SR 지오메트리)
│   └── base.materials.bin    (885KB — 머티리얼 정의)
├── assets/maps/kitpieces/    (텍스처 원본 .tex 파일들)
└── [기타 시즌/이벤트 변형: bloom, arcade, crepe 등]
```

---

## Phase 2: mapgeo → glb 변환 (lol2gltf)

```cmd
"C:\Users\user\Downloads\lol2gltf.exe" mapgeo2gltf ^
  -m "C:\...\MAP\data\maps\mapgeometry\map11\base.mapgeo" ^
  -b "C:\...\MAP\data\maps\mapgeometry\map11\base.materials.bin" ^
  -g "C:\...\MAP\output\sr_base.glb" ^
  -d "C:\...\MAP" ^
  -q Medium ^
  -l Default
```

| 플래그 | 의미 |
|--------|------|
| `-m` | .mapgeo 파일 (필수) |
| `-b` | .materials.bin 파일 (필수) |
| `-g` | 출력 .glb 경로 (필수) |
| `-d` | MAP 폴더 루트 — **텍스처를 glb에 임베딩** |
| `-q Medium` | 텍스처 2x 축소 (Low=4x, 생략=원본) |
| `-l Default` | 레이어 그루핑 유지 |

**결과**: `sr_base.glb` (~322MB, 1268 머티리얼, 160 텍스처 임베딩, 1723 메시)

---

## Phase 3: 엔진 로드 (FBX 변환 불필요)

### 왜 FBX를 안 쓰는가

| 방식 | 문제 |
|------|------|
| glb → Blender → FBX | Blender FBX 익스포터가 glTF PBR 머티리얼의 **텍스처 경로를 FBX에 기록하지 못함** → 엔진 자동 매핑 불가 |
| **glb → Assimp 직접** | Assimp 6.x가 glTF 2.0 + 임베딩 텍스처 완전 지원 → **자동 매핑** |

### 엔진 코드 변경 (3파일)

**1. CTexture::CreateFromMemory() 추가** (`Texture.h/cpp`)
- `DirectX::CreateWICTextureFromMemory()` / `CreateDDSTextureFromMemory()` 사용
- DDS 매직 체크 (`"DDS "`) → WIC 폴백

**2. CModel::LoadTextures() 수정** (`Model.cpp`)
- `aiMaterial::GetTexture()` 반환값이 `*0`, `*1` 형태면 임베딩 텍스처
- `pScene->GetEmbeddedTexture(path)` → `aiTexture` 획득
- `mHeight == 0` → compressed (PNG) → `CreateFromMemory(pcData, mWidth)`
- 기존 파일 경로 로직은 그대로 유지 (FBX 호환)

**3. CGameApp에서 로드**
```cpp
m_Map.Init("C:/.../MAP/output/sr_base.glb", L"Shaders/Mesh3D.hlsl");
m_MapTransform.SetScale(0.01f);
```

---

## Phase 4: Z-Fighting 해결 (Layer 필터링)

### 문제

glb 내부에 **Layer1~8 (646 메시)**이 Direct Children (1077 메시) 위에 동일 평면으로 겹침:
- `Layer1~7`: VertexDeform (풀/나무 흔들림 — 전용 셰이더 필요)
- `Layer2,8`: Fire/Earth element (엘리멘탈 드래곤 맵 변형)

LoL 엔진은 이걸 커스텀 셰이더+블렌딩으로 처리하지만, 우리 포워드 렌더러에서는 Z-fighting 발생.

### 해결

`CModel::ProcessNode()`에서 `"Layer"` 이름 노드를 스킵:

```cpp
string nodeName(pNode->mName.C_Str());
if (nodeName.size() >= 5 && nodeName.substr(0, 5) == "Layer")
{
    OutputDebugStringA(("[CModel] SKIP overlay node: " + nodeName + "\n").c_str());
    return;
}
```

**결과**: Z-fighting 완전 제거, 메시 1723 → 1077 (37% 감소)

### 맵 내부 레이어 구조 (참고)

```
sr_base.glb 노드 계층:
├── Direct Children (1077) ← 이것만 렌더링
│   ├── grnd_terrain (16) — 메인 지형 바닥
│   ├── chunk_atlas (88) — 아틀라스 텍스처 청크
│   ├── periph (47) — 외곽 절벽/벽
│   ├── lambert (93) — 기본 머티리얼 오브젝트
│   └── other (833) — 벽, 소품, 건물
│
├── Layer1 (74) — VertexDeform (풀 흔들림) ← SKIP
├── Layer2 (80) — Fire element + VertexDeform ← SKIP
├── Layer3 (141) — Earth element + VertexDeform ← SKIP
├── Layer4~7 (각 74~75) — VertexDeform ← SKIP
└── Layer8 (54) — Fire element + VertexDeform ← SKIP
```

### 2026-07-15: 진짜 bush mesh 복구 경로

- `S003`의 단일 PNG billboard와 windgrass crossed-card는 기술적으로 로드됐지만, 평면 실루엣과 불투명 atlas 때문에 시각적으로 실패했다. 이 경로는 `S004`에서 퇴출됐으며 정상 F5의 대체 경로로 다시 쓰지 않는다.
- 별도 bush FBX가 필요한 것이 아니다. `base.mapgeo`에서 만든 `sr_base_flip.glb`의 Direct Children에 `VertexDeform` foliage mesh가 803개 들어 있고, `base.materials.bin`은 이 재질을 `ASSETS/Maps/KitPieces/SRX/textures/SRU_Brush.tex`에 연결한다.
- map cook은 `--pretransform`으로 노드 배치를 정점에 bake하고, `VertexDeform_inst` diffuse를 런타임 `sru_brush.png`로 remap해야 한다. 둘 중 하나라도 빠지면 부쉬가 원점에 겹치거나 기본 흰 텍스처로 렌더된다.
- `sru_brush.png` 자체는 저채도 detail/alpha diffuse다. `base.materials.bin`의 `USE_GRASS_TINT_MAP`와 shipping `map11.bin`의 `GrassTint_SRX.tex`를 함께 복구해야 원본 위치별 foliage 색이 나온다. raw/unlit diffuse 출력이나 SSAO 제거는 색 복구 방법이 아니다.
- 시각 맵 `sr_base_flip.wmesh`에는 foliage를 포함하되, 높이 샘플러용 `sr_base_flip_surface.wmesh`에서는 같은 재질을 제외한다. 잎 삼각형이 지면 높이/보행 판정에 들어가는 회귀를 막기 위해서다.
- 재생성은 `Tools/convert_all_assets.ps1 -Mode maps`, 구조와 GrassTint asset 검증은 `py -3 Tools/audit_map11_foliage.py`를 사용한다. 기존 `base.mapgeo`/`base.materials.bin`/GLB가 존재하므로, 이 검증이 실패하기 전에는 Obsidian 재추출이나 FBX 우회를 먼저 하지 않는다.

---

## Gotchas

| # | 문제 | 원인 | 해결 |
|---|------|------|------|
| 1 | FBX에 텍스처 경로 0개 | Blender FBX 익스포터가 glTF PBR 텍스처를 FBX에 기록 못함 | **glb를 Assimp으로 직접 로드** (FBX 변환하지 않기) |
| 2 | Z-fighting (텍스처 자글거림) | Layer1~8 오버레이가 지형과 동일 평면에 겹침 | `ProcessNode()`에서 `"Layer"` 노드 스킵 |
| 3 | Near/Far로 Z-fighting 안 풀림 | 코플라나(완전 동일 Z) 문제 — depth 정밀도와 무관 | Layer 필터링이 정답 (Near/Far는 부동소수 오차에만 유효) |
| 4 | 동일 DepthBias로 안 풀림 | 같은 RS로 같은 bias면 두 면의 상대 깊이 차이 불변 | 비대칭 bias 또는 Layer 필터링 필요 |
| 5 | tex2png vs Blender 추출 다름 | Pillow DXT 디코더 ≠ Blender 디코더 (max diff 87) | **Blender 추출 또는 glb 직접 로드** (glb 임베딩이 원본) |
| 6 | glb 로딩 수~십 초 | 322MB + 160 텍스처 디코딩 | 정상. `-q Low`로 재생성하면 축소 |
| 7 | GPU 메모리 ~2.5GB | 160 × 2048×2048 RGBA | 정상. 부족 시 `-q Low` 사용 |
| 8 | **맵 좌우 반전 — 도구 옵션 무력화** | `lol2gltf.exe mapgeo2gltf` 의 `-x, --flipX` 가 `(Default: true)` 로 X 축 자동 반전. **`-x false` / `--flipX false` / `--flipX=false` / `-x:false` 4가지 형식 전부 무시됨** (1.0.0+d36a532 CLI parser 버그, md5 비교로 확정). 도구 단에서 X flip 끌 방법 없음 | **코드에서 우회**: 맵 transform 에 `m_MapTransform.SetScale({-0.01f, 0.01f, 0.01f})` X 미러. `Mesh3D` 파이프라인이 `D3D11_CULL_NONE` 이라 winding 뒤집혀도 면 안 사라짐. 챔피언 fbx 는 lol2gltf 안 거치니 영향 0. 현재 적용 위치: [Client/Private/Scene/Scene_InGame.cpp:39](../../Client/Private/Scene/Scene_InGame.cpp), [Client/Private/Scene/Scene_Editor.cpp:22](../../Client/Private/Scene/Scene_Editor.cpp) |
| 9 | **변환 후 결과 검증 안 하면 시간 낭비** | lol2gltf 처럼 옵션이 silent 하게 무시되는 도구는 명령 성공 = 결과 정상이 아님. exit 0 + 파일 생성됐는데도 옵션 효과 0 | **변환 직후 즉시 md5 비교** (옵션 켜고 끈 결과 비교) 또는 정점 분포 분석. 검증 없이 다음 단계 (빌드/실행) 진행하면 "도구 옵션 형식 잘못" vs "도구 자체 버그" 구분 못 함. 2026-04-19 lol2gltf X-flip 사고가 그 예 (4번 변환 후 md5 비교로 발견) |
| 10 | 부쉬/LevelProp이 원점이나 우물 근처에 겹침 | WMesh writer가 GLB 노드 transform을 버리고 mesh-local 정점만 저장 | 맵 cook에 `--pretransform`을 사용하고 audit의 foliage X/Z span 검사를 통과시킨다. 스킨 모델에는 적용하지 않는다. |
| 11 | foliage가 흰색으로 렌더됨 | GLB PBR material에 빠진 `base.materials.bin`의 `VertexDeform → SRU_Brush.tex` 연결이 WMat에 전달되지 않음 | map 전용 `--material-remap`을 사용하고 사용 중인 empty diffuse submesh가 0인지 audit한다. |
| 12 | 부쉬 복구 뒤 챔피언 지면 높이/보행이 튐 | 시각 foliage 삼각형까지 MapSurfaceSampler가 지면으로 rasterize | 시각 WMesh와 foliage 제외 surface WMesh를 분리하고 `baseMapSurface`가 후자를 가리키게 한다. |
| 13 | genuine foliage mesh인데도 부쉬가 회색임 | cook이 `SRU_Brush` diffuse만 남기고 `USE_GRASS_TINT_MAP` / `GrassTint_SRX` 위치 색상 단계를 버림 | exact `VertexDeform_inst` 803개에만 map-space GrassTint를 적용한 뒤 기존 stylized lighting/AO를 계속 사용한다. |

---

## 향후 확장

- **엘리멘탈 맵 변형**: Layer2(Fire), Layer3(Earth) 활성화 + 전용 셰이더/블렌딩
- **VertexDeform**: Layer1,4~7 활성화 + 정점 셰이더에서 풀 흔들림 구현
- **base_srx.mapgeo** (88MB): SRX 시즌 맵 — 동일 파이프라인으로 로드 가능
- **ARAM (Map12)**: `map12/base.mapgeo` — 동일 과정
