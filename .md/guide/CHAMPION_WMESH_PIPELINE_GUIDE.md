# Champion `.wmesh / .wskel / .wanim` Pipeline Guide

**용도**: 새 챔피언을 추가할 때마다 참조하는 **재사용 절차서**. 1체 추가 ≤ 30 분 목표.
**전제**: Phase B-9 사이클 통과 (가렌 1체로 byte offset 매칭 + 6 챔프 일괄 검증). 본 문서는 **포맷 / 변환 / 검증** 절차만 다루며, BanPick UI / SkillTable / FX / Init 코드는 별도 가이드.

---

## 0. 개요 — 자체 포맷 인프라 현황

| 데이터 | 포맷 | 로더 | 비고 |
|---|---|---|---|
| 메시 지오메트리 | `.wmesh` | `CWMeshLoader` (zero-copy) | bone_count, vertex_stride 76, BoneEntry 포함 |
| 스켈레톤 계층 | `.wskel` | `CWSkelLoader` | rest_transform + GlobalInverseRoot |
| 애니메이션 | `.wanim` | `CWAnimLoader::LoadAsAnimation` | tick 단위 + skel_hash trailer |
| **텍스처** | (FBX 머티리얼 경로) | Assimp postFlags=0 가볍게 | Phase C-0 `.wtex` 진입 시 분리 |

→ 현재 **메시 빅3 자체 포맷, 텍스처만 FBX 의존 (hybrid)**. 챔프 로드 시 Assimp 호출은 텍스처 수집용 30~50ms 만.

---

## 1. 변환 절차 — 1 챔프 / 일괄 모두 동일 패턴

### 1.1 디렉토리 컨벤션
```
Client/Bin/Resource/Texture/Character/<Champ>/
├── <basename>.fbx           ← 입력 (Assimp 가 텍스처 추출용으로 읽음)
├── <basename>.wskel         ← 산출
├── <basename>.wmesh         ← 산출
├── anims/
│   └── <node>_<anim>.wanim  ← 산출 N개 (Assimp anim 순서)
└── *.png                    ← 텍스처 (FBX 머티리얼이 가리킴)
```

`<basename>` = FBX 파일명 (확장자 제외). 런타임 `Model::ReplaceExtToWMesh` 가 FBX 와 같은 basename 의 `.wmesh` 자동 검색.

### 1.2 변환 순서 — 반드시 `skel → mesh --skel → anim --skel` ★

순서 어기면 vertex bone index 가 wskel order 와 어긋나 메시 폭발.

```bat
cd Tools

:: 1. Skeleton 먼저 — bone DFS 인덱스 권위
WintersAssetConverter.exe skel <DIR>\<basename>.fbx -o <DIR>\<basename>.wskel

:: 2. Mesh — wskel 인덱스 기준으로 vertex bone indices pack
WintersAssetConverter.exe mesh <DIR>\<basename>.fbx --skel <DIR>\<basename>.wskel -o <DIR>\<basename>.wmesh

:: 3. Animation — wskel hash trailer 박힘
del /Q <DIR>\anims\*.wanim 2>nul
WintersAssetConverter.exe anim <DIR>\<basename>.fbx --skel <DIR>\<basename>.wskel -o <DIR>\anims
```

### 1.3 일괄 변환

`Tools/convert_all_assets.bat` — 등록된 챔프 모두 위 3 단계 자동 실행.

```bat
cd Tools
convert_all_assets.bat champions    :: 챔프만 (5~10분)
convert_all_assets.bat              :: 챔프 + static mesh 전체
```

신규 챔프 추가 시 `:convert_champ` 호출 1줄 추가:
```bat
call :convert_champ "Annie" "annie.fbx"
```

---

## 2. 산출물 검증 ★ F5 전 필수

### 2.1 헤더 spot check
```bat
WintersAssetConverter.exe info <basename>.wskel    :: [Skel] bones=N hash=0x...
WintersAssetConverter.exe info <basename>.wmesh    :: [Mesh] stride=76, bones=N
WintersAssetConverter.exe info anims\<one>.wanim   :: [Anim] + skel_hash=0x...
```

### 2.2 통과 기준 (셋 다 만족 필수)

| 검증 | 합격 |
|---|---|
| `wmesh.stride` | == 76 (Skinned3D IL 계약) |
| `wmesh.bone_count` | == `wskel.bone_count` |
| `wanim.skel_hash` | == `wskel.hash` |

하나라도 어긋나면 [Model.cpp](../../../Engine/Private/Resource/Model.cpp) 가드가 **Assimp fallback** 으로 전환 → fast-path 의미 없음. 즉시 진단:
- `bone_count` 미스매치 → 변환 순서 (skel → mesh --skel → anim) 위반
- `stride != 76` → WMeshFormat::VertexSkinned POD 변경됨 (CLAUDE.md byte offset 매칭 gotcha 위반)
- `skel_hash` 미스매치 → wskel 만 재생성하고 wanim 재생성 안 함 (반드시 묶어서)

---

## 3. F5 런타임 검증

### 3.1 기대 로그 (챔프 진입 시)
```
[CModel] .wmesh+.wskel fast-path: ...<champ>.wmesh
[CModel] wskel loaded: bones=N hash=0x...
[CModel] Loaded N wanim files
[CModel] Loaded from .wmesh: meshes=M textures=K
```

### 3.2 금지 로그 (즉시 진단)

| 로그 | 원인 |
|---|---|
| `[CModel] wmesh has bones but wskel missing — Assimp fallback` | wskel 파일 없음 |
| `[CModel] wskel/wmesh mismatch — Assimp fallback` | bone_count 불일치 (변환 순서 위반) |
| `[CModel] .wmesh build failed` | wmesh CMesh 생성 실패 (드물게) |
| **fast-path 로그 정상인데 화면 안 보임** | **byte offset 미스매치 1순위 의심** (CLAUDE.md gotcha 참조) |

### 3.3 시각/입력 체크리스트

| 항목 | 합격 |
|---|---|
| 모델 표시 | bind pose 탈출, 정상 위치 |
| Idle/Run | 무한 재생, 우클릭 이동 시 run |
| BA | 적 호버 + 우클릭 시 attack 애니 |
| Q/W/E/R | 챔프별 SkillTable 정의대로 |
| 카메라 follow | 선택 챔프 추적 |
| **Skinning 폭발 0** | 메시 사라짐/폭발/뒤틀림 0 |

---

## 4. 자주 발생하는 사고 (Gotcha 박제)

### G-1. 변환 순서 위반
`mesh` 를 `--skel` 없이 먼저 만들면 vertex bone indices = `aiMesh::mBones` 의 메시-로컬 인덱스. 런타임 wskel 의 DFS 인덱스와 어긋나 메시 폭발. **반드시 `skel → mesh --skel → anim --skel`**.

### G-2. tangent.w handedness 손실
현재 `VertexSkinned::tan[3]` 은 float3 (handedness 없음). PBR 진입 시 normal map 처리하려면 별도 채널 추가. 지금은 unlit 이라 무관.

### G-3. wanim 시간 단위 = tick (sec 아님)
`AnimMetaHeader::duration_ticks` + `ticks_per_second` + key time_ticks 모두 Assimp tick. CAnimator 가 `dt × TicksPerSecond` 으로 누적하므로 정합. SkillDef.castFrame 도 같은 단위.

### G-4. mTicksPerSecond=0 케이스
일부 FBX 는 `mTicksPerSecond` 가 0 → Writer 에서 24.0 fallback (`WAnimWriter.cpp` 박제). 사용자 코드 X.

### G-5. bone_count >= 256 거부
GPU 셰이더 [Skinned3D.hlsl:13](../../../Shaders/Skinned3D.hlsl:13) `g_BoneMatrices[256]` 한계. Writer 가 `bone_count >= 256` 즉시 거부. 챔프 평균 70~150 본이라 실 발생 0이지만 가드 유지.

### G-6. WAnimChannel 의 `bone_name_hash` → name 역참조
`CSkeleton::FindBoneIndex(string)` 가 string 으로만 찾음. WAnimLoader 가 `ResolveBoneByHash` 헬퍼로 hash→name 변환 후 `BoneChannel::strBoneName` 에 채워 push. 사용자 코드 X.

### G-7. ★ byte offset 매칭 (가장 위험)
`.wmesh::VertexSkinned` POD 의 필드 순서/크기 ↔ 셰이더 IL `D3D11_INPUT_ELEMENT_DESC::AlignedByteOffset` 이 **byte 단위로** 일치해야 함. 임의 추가 필드/패딩 넣으면 GPU 가 다음 필드 한 byte 씩 밀어 잘못 읽음 → 정점 NaN/0 → 메시 **소리 없이 사라짐** (애니/Transform 정상 로그라 진단 어려움).

확정 layout (76B):
| 필드 | offset | 크기 | semantic |
|---|---|---|---|
| pos[3] | 0 | 12 | POSITION |
| nrm[3] | 12 | 12 | NORMAL |
| uv[2] | 24 | 8 | TEXCOORD |
| tan[3] | 32 | 12 | TANGENT (float3) |
| indices[4] | 44 | 16 | BLENDINDICES (uint32×4) |
| weights[4] | 60 | 16 | BLENDWEIGHT |

신규 vertex 포맷 추가 시 **IL 부터 read off → POD 설계** 순서.

---

## 5. 신규 챔프 추가 — 30 분 워크플로

```
1. (5분)  LoL 추출 → Client/Bin/Resource/Texture/Character/<Champ>/<basename>.fbx + 텍스처 PNG
2. (1분)  convert_all_assets.bat 에 :convert_champ 1줄 추가
3. (~5분) 변환 실행 (anim 수에 비례, 챔프 평균 ~10초)
4. (3분)  info × 3 검증 (wskel/wmesh/wanim) — 통과 기준 §2.2
5. (5분)  Scene_InGame.cpp 에 m_<Champ> + Init/LoadMeshTexture/Transform 추가
6. (3분)  ChampionTable.cpp + SkillTable.cpp 행 추가
7. (3분)  BanPick 버튼 (선택) 또는 NPC 로 Scene 등록
8. (5분)  F5 검증 (§3) + 시각/입력 체크리스트
```

5~7 단계는 별도 가이드 (`.md/plan/Champion/03_GAREN_PHASE_B8_PIPELINE.md` v2 의 6 레이어 패턴) 참조.

---

## 6. 다음 사이클

`.wtex` (BC7 + 밉맵, Phase C-0) 진입 시 본 가이드의 §0 텍스처 행이 "FBX 의존" → "`.wtex` 직접" 으로 갱신. 그때 Assimp `ReadFile` 호출 자체 제거 가능 → 챔프 로드 < 5ms 목표 달성.

---

## 7. 참조

- 실제 코드: `Engine/Public/AssetFormat/{Mesh,Anim,Common}/`, `Engine/Private/Resource/Model.cpp`
- 변환 도구: `Engine/Private/Tools/AssetConverter/main.cpp`, `Tools/convert_all_assets.bat`
- 셰이더 IL: [Shaders/Skinned3D.hlsl:13](../../../Shaders/Skinned3D.hlsl:13)
- 1회성 사이클 산출물: `.md/plan/Champion/04_GAREN_WSKEL_WANIM_VERIFICATION.md` (v4 spec) + `04b_5CHAMP_BATCH_CONVERSION.md` (M7 회귀 점검)
- Format spec: `.md/plan/WintersFormat/04_STAGE3_WANIM_WSKEL.md`
- CLAUDE.md gotchas — `.wmesh skinned 정점 레이아웃 = 런타임 Skinned3D IL byte offset 매칭 필수` 항목

---

## 한 줄

**`skel → mesh --skel → anim` 순서 + stride 76 + skel_hash 매칭 + byte offset 절대 변경 금지. 신규 챔프 30분.**
