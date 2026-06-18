# EldenRing Pipeline V2 + Winters 런타임 에셋 계약

> 2026-06-11 기준. 코드 검증(엔진 로더/셰이더/LoL 클라이언트 규약 분석) 결과를 바탕으로
> "추출된 에셋을 EldenRingClient에서 바로 쓸 수 있는 형태"의 계약과 전체 파이프라인 v2를 기록한다.
> 02/09/10 문서의 초기 계획 대비 무엇이 바뀌었는지의 비교도 포함한다.

## 1. Winters 런타임 에셋 계약 (코드 근거)

CModel은 cooked 전용 로더다. FBX 폴백은 없다.

| 항목 | 계약 | 근거 |
|---|---|---|
| 파일 컨테이너 | 모든 `.w*`는 16B `WintersFileHeader`(magic `WINT`, version_major=1, flags=WF_NONE만 허용) | `Engine/Public/AssetFormat/Common/WintersFileHeader.h` |
| 스템 규칙 | `<base>.wmesh` 옆에 **같은 스템** `<base>.wskel`, `<base>.wmat` 필수 | `CModel::LoadModel` |
| 본 페어링 | `wmesh.bone_count == wskel.bone_count` + 본 name_hash 배열이 **인덱스 단위로 일치** | `WMeshAndWSkelNamesMatch` |
| 애니 폴더 | wmesh 형제 디렉터리 `anims/` (소문자) 의 `*.wanim` 전부, 사전순 로드 | `CModel::LoadModel` |
| 애니 검증 | `.wanim` 트레일러 skel_hash == wskel 본 name_hash FNV-1a 체인 | `WAnimLoader` |
| 메시 한계 | 로더: bones≤**1024**, submesh≤2048, vertex≤10M, stride 48(static)/76(skinned)만 | `WMeshLoader.cpp`, `WSkelLoader.cpp` |
| 셰이더 본 팔레트 | **1024 — StructuredBuffer SRV(t8)** (2026-06-13: cbuffer 256→512를 거쳐 `StructuredBuffer<SkinBoneMatrix{row_major float4x4}>`로 전환. `Skinned3D.hlsl`/`Skinned3D_PBR.hlsl`/`SSAO/SkinnedNormalOnly.hlsl` + `ModelRenderer.cpp` `BoneMatrixSRVBuffer`(동적 SRV 버퍼, VS t8 바인딩) + `WMeshWriter` 가드 1024. 멀기트 935본·트리가드 934본 스킨드 렌더/애니 확인) | 이 문서의 변경 |
| 텍스처 | `.dds` → DDSTextureLoader, 그 외 → WIC(PNG/JPG/BMP OK, **TGA 불가**) | `Texture.cpp` |
| 텍스처 경로 | `.wmat` MaterialEntry.diffuse_path(wchar 260)가 `WintersResolveContentPath`로 해석 → `Client/Bin/Resource/...` 또는 `Resource/...` 상대형이어야 함. 절대경로 금지 | `WMaterialLoader`, resolver |
| 맵 | **본 없는 .wmesh**는 combined-static-mesh 분기를 타서 머티리얼 정렬 + 드로우콜 병합 | `Model.cpp:817` |
| UI | PNG/DDS + JSON 아틀라스 매니페스트(`hud_atlas_manifest.json` 스키마) → `UIAtlasManifest` + `CUIRenderer::DrawImage` | LoL HUD 경로 |
| 폰트 | TTF/OTF (ImGui AddFontFromFileTTF). Elden `font.gfx`는 사용 불가 → 대체 폰트 | `CFont_Manager` |

### 캐릭터 폴더 표준 (LoL 챔피언 규약 미러)

```text
Client/Bin/Resource/EldenRing/Runtime/Character/<cXXXX>/
├── <cXXXX>.wmesh     # skel-driven 본 테이블 (--skel 필수)
├── <cXXXX>.wskel
├── <cXXXX>.wmat      # diffuse_path = 저장소 상대 textures/ 경로로 재작성
├── anims/*.wanim     # convert-hkx-anim 산출물
└── textures/*.dds    # texbnd(_h 우선) DDS
```

생성 명령: `cook-runtime-character` (아래 3절).

### EldenRingClient 접속 조건

1. `WintersResolveContentPath`가 exe 위치에서 상위로 걸어 올라가며 `Client\Bin\Resource` 루트를 찾으므로 **추가 연결 작업 없이** `Resource/EldenRing/...` 경로가 해석된다 (junction 불필요).
2. 단 EldenRingClient 기본 RHI가 DX12라 ModelRenderer/ResourceCache 부트스트랩이 빠진다 → 모델 작업은 `--rhi=dx11` 또는 `main.cpp` 기본값 변경 필요.
3. 첫 렌더 스모크는 `ModelRenderer::Initialize("<wmesh path>", L"Shaders/Mesh3D.hlsl")` + `Update(dt)` + `Render(ctx)` 패턴(LoL과 동일)으로 새 IScene 하나면 된다.

## 2. 파이프라인 V2 전체 흐름

```text
[1] index-game-root          UXM Game 루트 전수 색인 -> 86,285 레코드 큐 (bundleKind 분류)
[2] run-full-pipeline        레코드 단위 멱등 cook (--resume/--continue-on-error/--clean-unpack)
      witchy unpack -> collect 분류 -> Texture/Material/Effect/AnimationRaw/Raw 복사
      -> TPF->DDS -> FLVER->FBX(Blender+Soulstruct) -> skel/mesh/anim/wmat -> PNG->DDS
      산출: FullGame/<topDir>/<bundleKind>/<recordId>/{Model,Texture,Material,Effect,AnimationRaw,Animation,Raw}
[3] convert-hkx-anim         anibnd HKX -> DivBinder -> Soulstruct -> FBX -> .wanim (skel hash 검증)
[4] cook-runtime-character   FullGame -> Runtime/Character/<id>/ 계약 레이아웃 재요리
                             (skel-driven wmesh 재변환 + anims/ + textures/ + .wmat 경로 재작성)
[5] build-resource-catalog / audit-full-pipeline   검증·카탈로그
[6] run_24h_driver.py        2~5를 제외한 [2]를 5레인 병렬로 무인 가동 (STOP 파일/디스크/마감 가드)
```

서브커맨드 15종: parse-matbin, parse-fxr, resolve-fxr, build-bindings, build-resource-catalog,
sync-ui-menu, sync-source-bundles, convert-textures-dds, build-editor-map-seeds, index-game-root,
run-full-pipeline, audit-full-pipeline, retry-missing-wmesh, convert-hkx-anim, **cook-runtime-character**.

## 3. 신구 파이프라인 비교

| 항목 | 구(02/09/10 문서 시점) | V2 (현재) |
|---|---|---|
| 입력 범위 | 손으로 뽑은 FBX 17개 + PNG 674 (`Desktop/EldenRing`) | UXM 루트 전수 색인 86,285 레코드 |
| FLVER→FBX | Blender GUI 수작업 | Soulstruct 헤드리스 배치 (`--factory-startup`) |
| 텍스처 | TPF→DDS→PNG 수작업 (texconv 배치) | run-full이 TPF→DDS 자동, PNG→DDS 역방향도 자동 |
| 캐릭터 본 한계 | 256 (chr 전부 불가) | 로더/셰이더/작성기 512 (Elden 일반 캐릭터 대부분 통과; >512 보스급만 잔여) |
| 애니메이션 | 불가(HKX 막힘) — anim3010 수동 FBX만 | convert-hkx-anim 자동 체인 + skel hash 자동 검증 |
| 머티리얼 | mapping.csv 수동 | MATBIN raw 수집 + 토큰/접미사 자동 리졸브(.wmat 재작성) |
| 재개성 | 없음 | 레코드 단위 status JSON + --resume, 24h 무인 드라이버 |
| 검증 | 수동 info | audit-full-pipeline (wmat 사이드카, info, DDS 매직, 본 한계) + cook 검증 |
| 런타임 적용 | 미정 | cook-runtime-character가 엔진 계약 레이아웃 생성 |

## 4. 내부 정보 커버리지 (전체 데이터 클래스)

| 데이터 | 캡처 형태 | 상태 |
|---|---|---|
| FLVER 메시 | FBX + .wmesh/.wskel/.wmat | ✅ 자동 |
| TPF 텍스처 | DDS (chr/sfx/menu/map 타일) | ✅ 자동 |
| MATBIN 머티리얼 | raw `.matbin` (`Material/`) + parse-matbin XML 매니페스트 경로 | ✅ raw / 파라미터→.wmat 멀티슬롯은 미래 |
| FXR 이펙트 | raw `.fxr`(`Effect/`) + parse-fxr/resolve-fxr 매니페스트 | ✅ raw+매니페스트 / `.wfx` 미래 |
| MSB 맵 레이아웃 | WitchyBND XML 전체(`Raw/`) — Part별 Position/Rotation/Scale 포함 | ✅ raw / placement JSON 생성기 다음 단계 |
| HKX 스켈레톤/애니 | `AnimationRaw/` raw + (.wanim 변환 체인 가동) | ✅ |
| TAE 애니 이벤트 | `AnimationRaw/` raw | ⚠️ 이벤트→.wanim event 변환 미구현 |
| FMG 텍스트 | msg `Raw/` (payload-only 폴백) | ✅ raw |
| PARAM 게임데이터 | param `Raw/` | ✅ raw |
| 스크립트(lua/hks)/이벤트(emevd) | `Raw/` | ✅ raw |
| NavMesh (nvmhktbnd) | 수집만 (havok) | ⚠️ 변환 없음 |
| 맵 충돌 (hkxbhd/bdt 페어) | **큐 제외** (paired-data) | ❌ 큐 확장 필요 |
| regulation.bin | **큐 제외** | ❌ |
| 사운드(wem/bnk)/무비(bk2) | reference-only (게임 루트 참조) | 설계상 제외 |
| UI .gfx (Scaleform) | raw 복사 (MenuRaw/FontRaw) | Winters에 Scaleform 없음 → DDS+아틀라스 경로 사용 |
| UI 텍스처 | MenuTex DDS 34,570장 | ✅ |
| AET(asset 텍스처 binder) | 미추출 | ❌ 맵 타일 텍스처 갭 — 다음 단계 |

## 5. 알려진 함정 (이번 작업에서 확정)

1. Blender **armature-only FBX는 Assimp가 거부** → 애니 FBX는 메시 포함 export.
2. **WitchyBND는 콘솔 핸들 없으면 즉사**(PromptPlus) → 파이프라인은 `CREATE_NO_WINDOW`로 실행.
3. WitchyBND 산출 경로가 MAX_PATH 초과 가능(shader/material binder) → 복사는 `\\?\`, 삭제는 `rd /s /q "\\?\..."` 폴백(`remove_tree`).
4. 기존 FullGame chr wmesh는 256 가드 시절 **skel 없이 standalone 변환**된 것이라 wskel과 본 수 불일치 → 반드시 `cook-runtime-character`로 재요리해야 런타임 페어링 통과.
5. 시드/어셈블리 JSON은 UTF-8 BOM → 파싱은 `utf-8-sig`.
6. 식생/플레이어 베이스 등 일부 FLVER는 드로우 가능 지오메트리가 없어 영구 실패가 정상(텍스처/FXR은 수집됨).

## 6. 다음 단계

1. >512본(보스급: c2130=717, c2050=1062+) — `StructuredBuffer<float4x4>` SRV 스키닝 경로.
2. 맵: MSB Part XML → `winters.elden.map_placement.v1` JSON 생성기 + EldenRingClient placement 로더 + AET 텍스처 추출.
3. `.wmat` 멀티슬롯(normal/mask/emissive) 확장 + MATBIN 파라미터 반영.
4. TAE → `.wanim` 이벤트.
5. UI: 화면별 아틀라스 매니페스트 JSON 저작(스키마는 LoL `hud_atlas_manifest.json` 재사용).
6. 맵 충돌 페어 아카이브/regulation.bin 큐 확장.
