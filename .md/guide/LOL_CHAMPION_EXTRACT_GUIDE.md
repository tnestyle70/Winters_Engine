# LoL 챔피언 리소스 추출 → FBX 변환 가이드

> **최종 업데이트**: 2026-04-24
> **검증 챔피언**: Irelia, Yasuo, Kalista (다른 챔피언도 동일 과정)
> **SCB 파이프라인 누적 검증**: 60개 `.scb` (Irelia 8 + Yasuo 26 + Kalista 26, v2.2 + v3.2 혼합, 100% 변환 성공)
> **필요 도구**: Obsidian, lol2gltf, Blender, (스킬 이펙트용) 커스텀 SCB 파서

---

## 전체 흐름

```
[Obsidian]              [lol2gltf / scb_to_fbx]           [Blender]              [Winters Engine]
Irelia.wad.client  →  .skn/.skl/.anm/.tex  (본체)     →  .glb  →  FBX + 텍스처 ↘
                   →  .scb                (스킬 이펙트) →  (직접 파싱)  →  FBX + 프리뷰 PNG ↗   DX11 렌더링
```

**처리 분기**:
- 본체(스키닝 메시): Phase 1-4 — Obsidian → lol2gltf → Blender
- 스킬 이펙트(정적 메시, `.scb`): Phase 5 — 커스텀 Python 파서 + Blender headless
- UI/파티클 텍스처: Phase 4 — `.tex`/`.dds` → PNG

---

## Phase 1: Obsidian에서 에셋 추출

### 1-1. WAD 파일 열기

1. Obsidian 실행
2. `File → Open` → LoL 설치 경로에서 WAD 파일 열기:
   ```
   C:\Riot Games\League of Legends\Game\DATA\FINAL\Champions\Irelia.wad.client
   ```

### 1-2. 추출 대상 선택

WAD 내부 경로:
```
assets/characters/irelia/skins/base/
```

이 폴더 안에서 **3종류** 파일을 추출:

| 종류 | 확장자 | 설명 |
|------|--------|------|
| 메시 + 스켈레톤 | `.skn`, `.skl` | 3D 모델 + 본 계층구조 |
| 애니메이션 | `.anm` (전부) | 키프레임 데이터 (idle, run, attack 등) |
| 텍스처 | `.tex` | 디퓨즈맵, 칼날 텍스처 등 |

### 1-3. 추출 실행

1. `assets/characters/irelia/skins/base/` 폴더 선택
2. **왼쪽 상단 파일 모양 아이콘 → `Extract All` 클릭** (개별 선택 X, 전체 추출)
3. 저장 경로: `C:\Users\user\Desktop\LOL_Resource\Irelia\`

> **중요**: 반드시 `Extract All`로 전체 추출할 것. 개별 선택 시 animations 폴더 등이 누락될 수 있음.

### 1-4. 추출 결과 확인

```
C:\Users\user\Desktop\LOL_Resource\Irelia\
├── irelia_base.skn                    ← 메시
├── irelia_base.skl                    ← 스켈레톤
├── irelia_base_tx_cm.tex              ← 몸체 텍스처
├── irelia_base_blades_tx_cm.tex       ← 칼날 텍스처
├── irelialoadscreen.tex               ← 로딩 화면 (불필요)
├── animations/                        ← 애니메이션 폴더
│   ├── irelia_idle1.anm
│   ├── irelia_run.anm
│   ├── irelia_attack_01.anm
│   ├── irelia_dance.anm
│   ├── irelia_death.anm
│   └── ... (68개)
└── particles/                         ← 파티클 (불필요)
```

---

## Phase 2: lol2gltf로 glTF 변환

### 2-1. 도구 준비

- [lol2gltf 다운로드](https://github.com/Crauzer/lol2gltf/releases)
- CLI 프로그램이므로 **cmd에서 실행** (더블클릭하면 바로 종료됨)

### 2-2. 머티리얼 이름 확인

`.skn` 파일 내부에 머티리얼 이름이 바이너리로 저장되어 있다.
Python으로 추출 가능:

```cmd
python -c "import re; data=open(r'경로\irelia_base.skn','rb').read(); [print(s.decode()) for s in re.findall(b'[A-Za-z_][A-Za-z0-9_]{3,}', data)[:5]]"
```

이렐리아 결과:
```
Irelia_Skin05_Mat    ← 칼날 머티리얼
Irelia_Base_Mat      ← 몸체 머티리얼
blades               ← 서브메시 이름
```

### 2-3. 변환 명령어

**핵심 1**: `--materials` 수와 `--textures` 수가 **반드시 동일**해야 한다.
**핵심 2**: `--materials`와 `--textures`를 **반드시 포함**해야 Blender에서 텍스처가 보인다. 없이 실행하면 텍스처 없는 회색 모델만 생성됨.

```cmd
"C:\Users\user\Downloads\lol2gltf.exe" skn2gltf ^
  -m "C:\Users\user\Desktop\LOL_Resource\Irelia\irelia_base.skn" ^
  -s "C:\Users\user\Desktop\LOL_Resource\Irelia\irelia_base.skl" ^
  -g "C:\Users\user\Desktop\LOL_Resource\Irelia\irelia_textured.glb" ^
  -a "C:\Users\user\Desktop\LOL_Resource\Irelia\animations" ^
  --materials "Irelia_Base_Mat" "Irelia_Skin05_Mat" ^
  --textures "C:\Users\user\Desktop\LOL_Resource\Irelia\irelia_base_tx_cm.tex" "C:\Users\user\Desktop\LOL_Resource\Irelia\irelia_base_blades_tx_cm.tex"
```

> **주의**: `--textures` 경로는 **절대 경로**로 지정할 것. 상대 경로 시 파일 못 찾을 수 있음.

**머티리얼 수 > 텍스처 수인 경우** (예: 야스오 머티리얼 3개, 텍스처 1개):
같은 텍스처를 반복 지정하여 개수를 맞춘다:

```cmd
--materials "Mat_A" "Mat_B" "Mat_C" ^
--textures "body.tex" "body.tex" "body.tex"
```

| 플래그 | 의미 |
|--------|------|
| `-m` | .skn 파일 (메시) |
| `-s` | .skl 파일 (스켈레톤) |
| `-g` | 출력 .glb 경로 |
| `-a` | 애니메이션 폴더 (.anm 전부 포함) |
| `--materials` | 머티리얼 이름 (skn에서 추출한 순서대로) |
| `--textures` | 텍스처 파일 (materials와 1:1 매칭) |

### 2-4. 텍스처 미적용 시

lol2gltf가 `.tex` 텍스처를 glb에 임베딩하지만, Blender에서 회색으로 보일 수 있다.
→ **Phase 3에서 Blender에서 수동으로 텍스처 연결** (가장 확실한 방법)

### 2-5. Gotchas

- `--materials`와 `--textures` 개수 불일치 시 에러:
  ```
  System.InvalidOperationException: Material name count and Animation path count must be equal
  ```
  → 반드시 개수를 맞출 것

- `--textures` 없이 실행하면 텍스처 없는 glb 생성 (메시+애니메이션만)

---

## Phase 3: Blender에서 텍스처 연결 + FBX 익스포트

### 3-0. 자동화 스크립트 (권장)

Blender를 **headless로 실행**하여 glb 임포트 → 머티리얼별 텍스처 바인딩 → Apply Transform → FBX Export를 한 번에 처리한다. 이후의 3-1 ~ 3-4는 수동 작업용 참고 자료이며, 대부분은 이 스크립트로 충분하다.

**스크립트**: `C:\Users\user\Desktop\LOL_Resource\Character\glb_to_fbx_multi.py`

| 플래그 | 의미 |
|--------|------|
| `<glb>` | lol2gltf 출력 glb 경로 |
| `<fbx>` | 출력 FBX 경로 |
| `<material>=<texture.png>` | 머티리얼 이름과 텍스처의 1:1 매핑 (공백 없이, 여러 개 나열 가능) |

**호출 형태**:

```cmd
"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe" ^
  --background ^
  --python "C:\Users\user\Desktop\LOL_Resource\Character\glb_to_fbx_multi.py" ^
  -- <glb> <fbx> <mat1>=<png1> [<mat2>=<png2> ...]
```

**예시 — Jax (머티리얼 4개, 텍스처 3개 공유)**:

```cmd
"...blender.exe" --background --python "...\glb_to_fbx_multi.py" -- ^
  "C:\...\Jax\jax_textured.glb" ^
  "C:\...\Jax\jax.fbx" ^
  "Body=C:\...\Jax\jax_base_body_tx_cm.png" ^
  "Weapon=C:\...\Jax\jax_base_weapon_tx_cm.png" ^
  "Bluefish=C:\...\Jax\jax_base_fish_tx_cm.png" ^
  "Goldfish=C:\...\Jax\jax_base_fish_tx_cm.png"
```

**예시 — MasterYi (머티리얼 1개)**:

```cmd
"...blender.exe" --background --python "...\glb_to_fbx_multi.py" -- ^
  "C:\...\MasterYi\masteryi_textured.glb" ^
  "C:\...\MasterYi\masteryi.fbx" ^
  "MasterYi_Base_MD_MasterYi=C:\...\MasterYi\masteryi_2013_tx_cm.png"
```

**스크립트 내부 동작**:

1. `bpy.ops.import_scene.gltf`로 glb 임포트 (애니메이션 액션 전부 포함)
2. 각 머티리얼의 기존 `TEX_IMAGE` 노드의 `image` 속성을 지정한 PNG로 교체
   - Blender 5.x의 glTF 임포트는 `Emission + Transparent BSDF + Mix Shader` 구조를 사용하므로 Principled BSDF가 **없다**. TEX_IMAGE 노드 이미지만 바꾸면 끝.
   - 머티리얼 이름은 **대소문자 무시** + Blender의 `.001` 충돌 접미사 자동 제거
3. `transform_apply`로 모든 오브젝트에 Location/Rotation/Scale 베이킹
4. `export_scene.fbx` 호출 — `bake_space_transform=True`, `add_leaf_bones=False`, `use_armature_deform_only=True`, 모든 액션 bake, Simplify=0.0

**PNG 경로는 Phase 4의 산출물을 쓴다** — `.tex` 원본은 Blender가 직접 못 읽으므로 PNG 변환이 선행되어야 한다.

**매핑 규칙 (Jax 사례)**:
- `.skn`에서 추출한 서브메시/머티리얼 이름 (`Body`, `Weapon`, `Bluefish`, `Goldfish`) → 각 본체 텍스처 파일
- Bluefish/Goldfish처럼 **같은 텍스처를 공유하는 경우** 같은 PNG 경로를 중복 지정하면 된다.

**Gotcha**:
- 스크립트는 머티리얼의 `TEX_IMAGE` 노드가 **이미 존재**한다고 가정 (lol2gltf가 만들어 둠). 없으면 `[WARN] no TEX_IMAGE node on <mat>`이 출력되며 해당 머티리얼은 교체되지 않는다 — lol2gltf에 `--textures`를 빠뜨린 glb를 넘긴 경우이므로 Phase 2로 돌아가서 텍스처 포함하여 재생성할 것.
- FBX 크기: 애니메이션이 많을수록 커진다 (Jax 53개 → 64MB, MasterYi 26개 → 20MB). 정상.

### 3-1. glb 임포트

1. Blender 실행 → 기본 큐브 삭제
2. `File → Import → glTF 2.0 (.glb/.gltf)` → `irelia_textured.glb` 선택

### 3-2. 텍스처 수동 연결 (필수)

glb에 텍스처가 임베딩되어도 Blender Material에 자동 연결 안 되는 경우가 있다.

**머티리얼 목록** (Properties → Material 탭):
- `Irelia_Base_Mat` → 몸체
- `Irelia_Skin05_Mat` → 몸체 (같은 텍스처)
- `blades` → 칼날

**텍스처 연결 방법**:

1. 메시 선택 → Properties → Material 탭
2. 머티리얼 하나 선택 (예: `Irelia_Base_Mat`)
3. `Surface → Base Color` 옆의 노란 점 클릭 → `Image Texture` 선택
4. `Open` → `.tex`에서 변환한 텍스처 또는 원본 `.tex` 파일 선택
5. 적용할 텍스처 매칭:

| 머티리얼 | 텍스처 파일 |
|----------|------------|
| `Irelia_Base_Mat` | `irelia_base_tx_cm.tex` (몸체) |
| `Irelia_Skin05_Mat` | `irelia_base_tx_cm.tex` (몸체, 동일) |
| `blades` | `irelia_base_blades_tx_cm.tex` (칼날) |

6. Viewport Shading → `Material Preview` (상단 바 구 아이콘)로 전환하여 텍스처 확인

> **참고**: Blender는 `.tex` 파일을 직접 읽지 못할 수 있다. 그 경우 lol2gltf가 glb 내부에 임베딩한 이미지를 사용하거나, `.tex` → `.png` 변환이 필요하다.

### 3-3. FBX 익스포트 설정

`File → Export → FBX (.fbx)` → 우측 패널:

```
Include:
  Object Types: ☑ Armature, ☑ Mesh

Transform:
  Scale: 1.0
  Apply Scalings: "FBX All"
  Forward: "-Z Forward"          ← DX11 왼손 좌표계
  Up: "Y Up"

Armature:
  ☑ Add Leaf Bones → 끄기       ← 불필요한 말단 본 방지
  ☑ Only Deform Bones → 켜기

Bake Animation:
  ☑ Bake Animation → 켜기
  ☑ All Actions → 켜기          ← 모든 애니메이션 클립 포함
  ☑ NLA Strips → 끄기
  Simplify: 0.0                  ← 키프레임 손실 방지
```

저장 경로: `C:\Users\user\Desktop\LOL_Resource\Irelia\irelia.fbx`

### 3-4. 텍스처 별도 관리

FBX에는 텍스처가 임베딩되지 않으므로, 엔진에서 로드할 텍스처를 별도 관리:

```
Winters/Resources/Models/Irelia/
├── irelia.fbx
├── irelia_base_tx_cm.dds (또는 .png)
└── irelia_base_blades_tx_cm.dds (또는 .png)
```

---

## Phase 4: 스킬/UI 이펙트 텍스처 PNG 변환

엔진(DX11)에서 UI 아이콘/파티클 텍스처를 로드하려면 `.tex`/`.dds` → `.png` 변환이 필요하다.
Obsidian이 PNG 직접 추출을 지원하지 않으므로 **TEX → DDS → PNG 2단계 파이프라인**을 사용한다.

### 4-1. 필요 도구

| 도구 | 역할 | 경로 |
|---|---|---|
| `tex2dds.py` | LoL TEX → DDS (자체 파서, 의존성 없음) | `C:\Users\user\Desktop\LOL_Tools\scripts\tex2dds.py` |
| `texconv.exe` | DirectXTex의 DDS → PNG 변환기 | `C:\Users\user\Desktop\LOL_Tools\texconv.exe` |
| `irelia_tex_to_png.py` | 래퍼 스크립트 (병렬 처리 + 배치) | `C:\Users\user\Desktop\LOL_Tools\scripts\irelia_tex_to_png.py` |

> **왜 texconv 단독으로 안 되나?**
> `.tex`는 Riot 자체 컨테이너 포맷(12바이트 헤더 + BC1/BC3/BGRA8 픽셀 데이터, 최대 mip이 파일 **끝**에 저장됨). texconv는 표준 DDS만 읽으므로 먼저 `tex2dds`로 풀어야 한다.

### 4-2. 파일 종류별 처리

| 확장자 | 정체 | 처리 |
|---|---|---|
| `.tex` | LoL 자체 포맷 | `tex2dds` → `.dds` → `texconv` → `.png` |
| `.dds` | 표준 DirectDraw Surface | `texconv` → `.png` (1단계) |
| `.scb` | 파티클/스킬 이펙트 지오메트리 (정적 메시) | **Phase 5 참조** — FBX + PNG 렌더 파이프라인 별도 |
| `.anm` / `.skn` / `.skl` | 애니메이션/메시/스켈레톤 | Phase 2 참조 |

### 4-3. 실행 (한 방에 전체 변환)

**다른 챔피언에 적용할 때는 스크립트 상단의 `ROOT` 한 줄만 수정:**

```python
# irelia_tex_to_png.py
ROOT = r"C:\Users\user\Desktop\LOL_Resource\Character\{챔피언명}"
```

실행:
```cmd
python "C:\Users\user\Desktop\LOL_Tools\scripts\irelia_tex_to_png.py"
```

### 4-4. 출력 구조

PNG는 **원본 `.tex`/`.dds` 파일 옆에 같은 이름으로 생성**된다:

```
LOL_Resource\Character\Irelia\
├── irelia_base_tx_cm.tex           ← 원본
├── irelia_base_tx_cm.dds           ← 중간 산출물 (재사용 가능)
├── irelia_base_tx_cm.png           ← 최종 ✅
├── icons2d\
│   ├── irelia_q.dds                ← 원본 (DDS)
│   └── irelia_q.png                ← 최종 ✅
├── particles\
│   ├── irelia_base_q_trail.tex     ← 원본
│   ├── irelia_base_q_trail.dds     ← 중간
│   └── irelia_base_q_trail.png     ← 최종 ✅
└── _conversion_logs\
    ├── tex_fails.log               ← TEX 변환 실패 목록
    └── png_fails.log               ← PNG 변환 실패 목록 (false positive 가능)
```

### 4-5. 검증 (이렐리아 기준)

```
TEX 원본 102개     →  DDS 변환 102개 (100% 성공)
DDS 원본 8개       →  PNG 직접 변환
최종 PNG 108개     (소요 시간 ≈ 1.7초)
```

### 4-6. 스크립트 내부 동작

1. **파일 수집**: `ROOT` 전체를 walk, `.tex`/`.dds` 수집
2. **TEX → DDS (병렬 8프로세스)**: `tex2dds.convert()` 호출
   - TEX 헤더 파싱 (4바이트 magic + width/height + format + mip flag)
   - BC 포맷은 mip chain 중 **마지막 블록이 최대 해상도**임에 유의
   - DDS 128바이트 표준 헤더 붙여 재포장
3. **DDS → PNG (texconv 배치)**: 디렉토리별로 묶어 `texconv -ft PNG -o ...` 호출
   - 배치 크기 80 (윈도우 cmd 명령줄 길이 제한 회피)

### 4-7. 트러블슈팅

| 문제 | 원인 | 해결 |
|---|---|---|
| `Not a TEX: magic=b'DDS '` | 이미 DDS 파일 | 스크립트가 자동으로 DDS 복사 처리 |
| `Unsupported format: N` | ETC1/ETC2 (모바일용) 포맷 | 데스크탑 챔피언엔 거의 없음. 발생 시 `tex2dds.py`의 `FMT` 딕셔너리 확장 |
| `png_fails.log`에 본체 텍스처가 찍힘 | false positive (texconv stdout에 "FAILED" 문자열 포함) | 실제 PNG는 생성됨, `ls`로 직접 확인 |
| 명령줄 길이 초과 | 한 폴더에 파일이 200개 이상 | `texconv_batch`의 `batch_size`를 40으로 낮춤 |

### 4-8. 엔진 연동 시 주의

- PNG는 알파 채널이 유지됨 (BC3/BGRA8의 경우). UI 아이콘/파티클은 알파 필수.
- DX11 로더에서 `DirectXTex::LoadFromWICFile` 또는 `stb_image`로 로드 가능.
- 파티클 텍스처는 보통 **Additive 블렌딩**으로 사용 (R_ground_pulse 등). 알파 프리멀티플라이 적용 여부는 이펙트마다 확인 필요.

---

## Phase 5: 스킬 이펙트 메시(.scb) → FBX + 프리뷰 PNG

챔피언 본체와 달리, **스킬 이펙트에 사용되는 정적 메시는 `.scb` 포맷** (Riot `r3d2Mesh`)으로 저장된다.
`particles/` 폴더 안의 `*_q_*.scb`, `*_w_*.scb`, `*_e_*.scb`, `*_r_*.scb` 같은 파일들이 여기에 해당.

**핵심 포인트 — 왜 lol2gltf로 안 되나?**
`lol2gltf.exe`는 `skn2gltf`(스키닝 메시)와 `mapgeo2gltf`(맵 지오메트리)만 지원한다.
`.scb`(Static Object Binary)는 별도 포맷이므로 **전용 파서**가 필요하다.

### 5-1. 필요 도구

| 도구 | 역할 | 경로 |
|---|---|---|
| `scb_to_fbx_render.py` | Blender headless — SCB 파싱 → Blender 메시 구축 → FBX 익스포트 → PNG 프리뷰 렌더 (1파일) | `C:\Users\user\Desktop\LOL_Tools\scripts\scb_to_fbx_render.py` |
| `run_scb_batch.py` | 배치 래퍼 — 특정 스킬/패턴에 매칭되는 모든 `.scb`에 대해 위 스크립트를 반복 실행 | `C:\Users\user\Desktop\LOL_Tools\scripts\run_scb_batch.py` |
| `scb_inspect.py` | (디버깅) SCB 헤더 분석 + 페이로드 레이아웃 검증 | `C:\Users\user\Desktop\LOL_Tools\scripts\scb_inspect.py` |

### 5-2. SCB 바이너리 레이아웃 (역분석 결과)

이렐리아/야스오 `particles/*.scb` (34개)로 검증한 v3.2 + v2.2 공통 구조:

```
+------+------+----------------------------------------+
| ofs  | size | field                                  |
+------+------+----------------------------------------+
|   0  |   8  | magic "r3d2Mesh"                       |
|   8  |   4  | version (u16 major, u16 minor)         |
|  12  | 128  | name (null-padded, 때로 원본 경로 포함)|
| 140  |   4  | vertexCount (u32)                      |
| 144  |   4  | faceCount   (u32)                      |
| 148  |   4  | objectFlags (u32)  — 색상 유무와 무관  |
| 152  |  24  | boundingBox (6 × f32: min xyz, max xyz)|
| 176  |  12  | centralPoint (3 × f32)  [v3.2 이상]    |
| 188  |  ..  | vertices[vcount] (3 × f32)             |
|      |  ..  | vertexColors[vcount] (4 × u8)  ← 선택  |
|      | 0-16 | pre_face_pad — 버전별 상이 (아래 참조) |
|      |  ..  | faces[fcount] (100바이트/면)           |
|      | 0-?  | tail_trailer — v2.2 일부 파일에 존재   |
+------+------+----------------------------------------+
```

**Face 레이아웃 (100바이트 고정, v2.2 + v3.2 공통)**:
```
+  0: u32[3]  vertexIndices
+ 12: char[64] material name  (leading null-padding 있을 수 있음 → regex로 ASCII 추출)
+ 76: f32[6]  uvs  (u0, u1, u2, v0, v1, v2)
```

**버전별 차이점 요약**:

| 구분 | v3.2 | v2.2 |
|---|---|---|
| `centralPoint` | 헤더에 있음 (12B) | 헤더에 있음 (12B) — 값은 유효 |
| `boundingBox` | 정상 | 일부 파일에서 쓰레기 값 (무시해도 됨, Blender에서 재계산) |
| `pre_face_pad` | 4바이트 (보통) | 0바이트 (보통) |
| `tail_trailer` | 0-4바이트 | **일부 파일에 수 KB 트레일러 존재** (예: 야스오 windwall_top 2592B) |
| `name` 필드 | 대개 빈 문자열 | 원본 .mesh.dae 파일 경로가 들어있기도 함 |

**핵심 Gotcha**:
1. `objectFlags` 비트로는 vertex color 유무를 **판별 불가** — 파일 크기 산술로 감지.
2. v2.2 파일 중 **파일 끝에 트레일러**가 있는 경우가 있음 → `face_start = len(data) - fcount*100`
   (끝-기준 역산) 공식이 어긋난다. 반드시 **앞-기준 (vertex_end + 패딩) 후보도 같이** 시도해야 함.
3. 파서는 여러 face_start 후보를 시도하고, 첫 face의 `indices`가 모두 `< vcount`인 후보를 채택
   (파서 구현 참고 — `valid_face_start` 함수). 이 방식으로 v2.2와 v3.2 모두 커버 가능.

### 5-3. 단일 파일 변환

```cmd
"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe" ^
  --background ^
  --python "C:\Users\user\Desktop\LOL_Tools\scripts\scb_to_fbx_render.py" ^
  -- <input.scb> <out.fbx> <out.png> [texture.png]
```

| 인자 | 의미 |
|------|------|
| `<input.scb>` | 변환할 SCB 파일 (절대 경로) |
| `<out.fbx>` | 출력 FBX 경로 — 부모 디렉토리 없으면 자동 생성 |
| `<out.png>` | 출력 PNG 렌더 경로 |
| `[texture.png]` | 선택 — 머티리얼의 Base Color에 바인딩할 텍스처 (생략 시 무지 그레이) |

**예시**:
```cmd
"...blender.exe" --background --python "...\scb_to_fbx_render.py" -- ^
  "C:\...\Irelia\particles\irelia_base_e_blade.scb" ^
  "C:\...\Irelia\particles\fbx\irelia_base_e_blade.fbx" ^
  "C:\...\Irelia\particles\render\irelia_base_e_blade.png" ^
  "C:\...\Irelia\particles\irelia_base_e_blade_swirl.png"
```

**스크립트 내부 동작**:

1. `parse_scb(path)` — SCB 바이너리를 파싱하여 `{vertices, faces, face_materials, face_uvs, has_colors}` 반환
2. Blender scene 리셋 후 `mesh.from_pydata(verts, [], faces)`로 메시 구축
3. 머티리얼 이름 단위로 슬롯 생성, 페이스별 `material_index` 할당
4. UV 레이어 생성 — **V축 플립** 처리 (`v → 1-v`) — Blender/DCC 관례에 맞춤
5. `transform_apply`로 위치/회전/스케일 베이킹 → FBX로 익스포트
   (`-Z Forward, Y Up, bake_space_transform=True, add_leaf_bones=False`)
6. **Orthographic 카메라** + 두 개 Sun 광원으로 3/4 뷰 프리뷰 PNG 렌더 (1024×1024, RGBA)

### 5-4. 배치 변환 (특정 스킬 전체)

`run_scb_batch.py` 상단의 경로 + 글롭 패턴 수정 후 실행:

```python
# run_scb_batch.py
PARTICLES = r"C:\Users\user\Desktop\LOL_Resource\Character\Irelia\particles"
FBX_DIR   = os.path.join(PARTICLES, "fbx")
RENDER_DIR = os.path.join(PARTICLES, "render")

def find_targets():
    patterns = [
        os.path.join(PARTICLES, "irelia_base_e_*.scb"),          # E 스킬 메시
        os.path.join(PARTICLES, "irelia_base_temp_e_*.scb"),     # E 인디케이터
    ]
    ...
```

실행:
```cmd
python "C:\Users\user\Desktop\LOL_Tools\scripts\run_scb_batch.py"
```

**자동 텍스처 매칭**: 배치 스크립트는 `<scb명>.png`가 `particles/` 폴더에 있으면 자동으로 텍스처로 바인딩한다.
(예: `irelia_base_e_blade.scb` ↔ `irelia_base_e_blade.png`)

### 5-5. 출력 구조

```
particles/
├── irelia_base_e_blade.scb            ← 원본
├── irelia_base_e_blade.png            ← (있으면) 텍스처
├── fbx/
│   └── irelia_base_e_blade.fbx        ← 최종 FBX ✅
└── render/
    └── irelia_base_e_blade.png        ← 프리뷰 PNG ✅ (1024×1024)
```

### 5-6. 검증 (이렐리아 E 기준)

```
입력: 8개 SCB (총 208KB)
├── irelia_base_e_beam.scb          (5.5KB, 35v/48f)
├── irelia_base_e_blade.scb         (24KB, 116v/228f)
├── irelia_base_e_blade_01/02/03.scb (24KB × 3)
├── irelia_base_e_cast.scb          (11KB, 72v/102f)
├── irelia_base_e_mis_mesh.scb      (24KB, 116v/228f)
└── irelia_base_temp_e_tar_blade_indicator_typhoon.scb (70KB, 414v/640f)

출력: FBX 8개 (17-35KB) + PNG 8개 (225-360KB)  — 총 소요 ≈ 20초 (파일당 약 2.5초)
```

### 5-7. 트러블슈팅

| 문제 | 원인 | 해결 |
|---|---|---|
| `Not a SCB: magic=b'...'` | magic이 `r3d2Mesh`가 아님 (버전 불일치, 다른 포맷) | 파일 확인. v3.2 이외 버전은 파서 확장 필요 |
| `Unexpected layout: remaining=...` | 색상 유무 감지 실패 (header/trailer가 예상과 다름) | `scb_inspect.py`로 원본 분석 후 `parse_scb`의 `gap` 판정 조건 조정 |
| `OverflowError: Python int too large` at `mesh.from_pydata` | face_start 잘못 계산 → indices에 쓰레기 바이트 들어감. 대부분 **v2.2 트레일러**가 원인 (야스오 `windwall_top_mesh.scb`는 2592B 트레일러 보유) | 파서가 `[vertex_end+0, +4, +colors, ...]` 순서로 유효 face_start 후보를 탐색 (indices < vcount 검증). 최신 `scb_to_fbx_render.py`에 반영됨 |
| v2.2 파일 변환 실패 / 머티리얼이 `''`(빈 문자열) | v2.2는 material 필드의 leading null padding이 없거나 다름 | regex `[A-Za-z_][A-Za-z0-9_\-]+`로 ASCII 이름 추출 (파서 반영됨) |
| `mesh.validate()`가 polys를 **전부 제거** (`verts=N polys=0 validate_removed=True`) → 렌더 결과 빈 화면 | face_start 후보 중 첫 face만 우연히 valid인 false-positive로 오프셋이 4바이트 빗나감 (칼리스타 `base_r_charged_sphere.scb`가 대표 사례) | face_start 검증을 **전체 face 반복**으로 수행 (`valid_face_start`가 모든 face의 indices < vcount 확인). 100% 통과하는 후보를 우선 채택, 없으면 최고점 후보 fallback |
| Blender **`EXCEPTION_ACCESS_VIOLATION`** 크래시 (rc=11, crash.txt 생성) | SCB 일부 파일에 벌린 vertex 좌표 포함 (±1e38 이나 1e23 같은 값). 칼리스타 `link_beam_core.scb`의 `v[247]` 사례 | 파서가 `abs > 1e6` 또는 NaN/Inf인 좌표를 **자동으로 원점(0,0,0)으로 클램프**하고 경고 출력 (`[scb] WARN: N corrupt vertex coords clamped`). 이 vertex를 참조하는 face는 degenerate(0넓이)가 되지만 크래시는 방지 |
| 렌더 엔진 크래시 재발 | 특정 메시 토폴로지가 EEVEE_NEXT와 충돌 | `SCB_RENDER_ENGINE=BLENDER_EEVEE` 또는 `BLENDER_WORKBENCH` 환경변수로 엔진 변경. 스크립트가 자동 폴백 순서 `[요청→WORKBENCH→EEVEE_NEXT→EEVEE]`를 시도하지만, native crash는 Python에서 못 잡으므로 배치 단위로 환경변수 바꿔 재실행 |
| 머티리얼이 `\x25` 같은 단일 비인쇄 문자로 추출 | material 필드 leading null 스킵 실패 | 파서가 `re.search(rb'[A-Za-z_][A-Za-z0-9_\-]+', raw_mat)`로 ASCII 이름만 추출함 — 최신 스크립트인지 확인 |
| 렌더 PNG가 완전 공백 (투명/흰색) | 카메라 **clip_end**(기본 100) 초과 — LoL 유닛은 메쉬 하나가 400+ 유닛 쉽게 넘김 | `cam_data.clip_end = max_dim * 20.0` 설정 (스크립트 반영됨) |
| 렌더에 얇은 메시가 edge-on으로 잘려 보임 | Perspective 카메라가 평면 메시(예: 빔)에 취약 | Orthographic 카메라 사용 — 스크립트 기본값 |
| FBX는 나오는데 PNG만 비어있음 | 백페이스 컬링 + 얇은 메시 | `mat.use_backface_culling = False` (스크립트 반영됨) + Sun 광원 두 개 (앞/뒤) |
| `bpy.context.collection` `NoneType` 에러 | headless에서 마스터 컬렉션 참조 방식 | `bpy.context.scene.collection` 사용 (스크립트 반영됨) |

### 5-8. 다른 챔피언/다른 스킬 적용

챔피언명과 스킬만 바꾸면 그대로 재사용:

```python
# run_scb_batch.py
PARTICLES = r"C:\...\Character\{챔피언명}\particles"

def find_targets():
    patterns = [
        os.path.join(PARTICLES, "{챔피언명}_base_q_*.scb"),   # Q 스킬
        os.path.join(PARTICLES, "{챔피언명}_base_w_*.scb"),   # W 스킬
        # ...
    ]
```

LoL의 스킬 넘버링 관례:
- `spell1` / `_q_` → Q (첫 번째 스킬)
- `spell2` / `_w_` → W
- `spell3` / `_e_` → E
- `spell4` / `_r_` → R (궁극기)
- `_p_` → 패시브
- `_ba_` → Basic Attack (평타)

### 5-9. 엔진 연동 시 주의

- SCB는 **정적 메시**(본/애니메이션 없음) — 스키닝된 캐릭터 본체(`.skn`→Phase 2)와 별도 렌더 경로
- 파티클 이펙트용이므로 보통 **Additive 블렌딩 + 알파 텍스처**와 조합됨
- 좌표계는 본체와 동일 (Y-up, 엔진 로드 시 `-Z Forward, Y Up` FBX 설정 그대로)
- LOD 없음 — 모든 유저/뷰어에 동일 버텍스 수

---

## Phase 6: 다른 챔피언 적용

동일한 과정으로 모든 챔피언에 적용 가능:

```
1. Obsidian → {챔피언}.wad.client → skins/base/ 추출
2. python → .skn에서 머티리얼 이름 추출
3. lol2gltf → --materials + --textures 매칭하여 .glb 생성
4. irelia_tex_to_png.py → .tex → .png 변환 (ROOT만 수정)
5. glb_to_fbx_multi.py → Blender headless로 텍스처 바인딩 + FBX Export (본체)
6. run_scb_batch.py → 스킬 이펙트 .scb → FBX + 프리뷰 PNG (Phase 5)
```

4, 5, 6번은 CLI로 완전 자동화 가능하므로, 실제 수동 개입이 필요한 구간은 1~3번뿐이다.

챔피언별 차이점:
- 머티리얼 수: 1~4개 (챔피언마다 다름)
- 텍스처 수: 1~4개 (몸체/무기/이펙트 등)
- 애니메이션 수: 30~100+ 개
- 스킬 `.scb` 메쉬 수: 스킬당 1~10개 (블레이드, 빔, 인디케이터, 시전 이펙트 등)

---

## 트러블슈팅

| 문제 | 원인 | 해결 |
|------|------|------|
| lol2gltf 더블클릭 시 바로 종료 | CLI 프로그램 | cmd에서 실행 |
| `Material name count must be equal` | --materials와 --textures 개수 불일치 | skn에서 머티리얼 이름 확인 후 개수 맞추기 |
| Blender에서 회색으로 보임 | Viewport Shading이 Solid 모드 | Material Preview로 변경 |
| 텍스처 안 보임 | Material에 Image Texture 미연결 | 수동으로 Base Color에 텍스처 연결 |
| `.tex` 파일 Blender에서 못 열음 | LoL 자체 포맷 | glb 임베딩 이미지 사용 또는 별도 변환 |
| 엔진에서 모델이 카메라를 삼킴 / 회전 시 카메라 뒤로 넘어감 | FBX Export 시 Apply Transform 미적용. 노드 오프셋/피벗이 정점에 반영 안 됨 | Blender에서 `Ctrl+A → All Transforms` 적용 후 Export 시 `Apply Transform` 체크 |
| 카메라 뒤로 빼도 모델 내부에서 벗어나지 못함 | 모델 스케일이 너무 크거나 원점(피벗)이 모델 중심이 아님 | `SetScale(0.01f)` 등으로 축소 + 카메라를 충분히 뒤로 배치 |
| S키 누르면 모델이 늘어나는 것처럼 보임 | 카메라가 메시 내부에 있는 상태에서 이동 시 원근 왜곡 | Apply Transform 적용된 FBX 사용 + AABB 로그로 실제 모델 크기 확인 |

---

## Gotchas (핵심 주의사항)

### 🔴 FBX Export 시 반드시 Apply Transform

Blender에서 FBX Export 전에 **반드시** 다음 수행:

1. **모든 오브젝트 선택** (`A`키)
2. `Ctrl+A` → **All Transforms** (위치/회전/스케일을 메시에 베이킹)
3. Export 설정에서 **Apply Transform** 체크

안 하면 FBX 계층 구조의 루트 노드에 거대한 transform이 남아서,
엔진 로더(`ProcessNode`)에서 `aiNode::mTransformation`을 적용하지 않으면
모델이 원점에서 크게 벗어나거나, 회전 시 피벗이 어긋나는 문제 발생.

> **대안**: 엔진 로더에서 `aiProcess_PreTransformVertices` 플래그 사용 시
> 노드 계층을 자동으로 정점에 베이킹. 단, 이 경우 본/애니메이션 정보가 소실됨.
> 스키닝이 필요한 캐릭터에는 사용 불가 → **Blender에서 Apply가 정답.**
