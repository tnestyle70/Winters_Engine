# EldenRing 전체 에셋 바이너리화 — 4인 팀 2개월 마스터 플랜

> 작성: 2026-06-13. 대상: Codex 및 4인 협업 팀.
> 목표: EldenRing 원본 에셋 **전부**를 Winters 바이너리(`.w*`)로 변환하고, UI·맵·캐릭터·FX·사운드까지
> EldenRingClient에서 **문제 없이 바로 쓸 수 있는 깔끔한 형태**로 정리. 완료될 때까지 자동 루프.
> 선행 문서: `14_PIPELINE_V2_RUNTIME_CONTRACT.md`(런타임 계약), `02/09/10/13`(파이프라인 상세).

---

## 0. 완료 정의 (Definition of Done)

전체 프로젝트는 아래 5개 게이트가 **모두 통과**하면 완료다.

| 게이트 | 기준 | 검증 방법 |
|---|---|---|
| **G1 변환 완전성** | 큐 71,261 witchy-unpack 레코드 중 변환 가능한 전부가 `FullGame/`에 산출, 영구 실패는 사유 분류 | `audit-full-pipeline` errors=0, 미처리 0 |
| **G2 런타임 계약** | 모든 `Runtime/` 에셋이 `14`문서 계약 통과(스템·본페어링·skel hash·상대경로) | `cook-runtime-character` + converter `info` 100% |
| **G3 좌표계/스케일** | 캐릭터·맵·에셋이 엔진 Y-up에서 똑바로, 정상 크기 | 쇼케이스 씬 시각 검증 + 자동 AABB 체크 |
| **G4 텍스처 완전성** | 모든 `.wmat` diffuse 채움률 ≥ 95%, 끊긴 링크 0 | wmat 스캐너 리포트 |
| **G5 UI/폰트** | 메뉴 UI가 DDS+아틀라스 매니페스트로 렌더, 폰트 대체 적용 | UI 쇼케이스 씬 |

**완료까지 루프**의 의미: 각 카테고리는 `드라이버 → 검증 게이트 → 실패분 재처리`를 자동 반복하며,
사람은 게이트 리포트만 보고 막힌 곳(영구 실패 사유)만 손본다.

---

## 1. 현재 상태 스냅샷 (2026-06-13 실측)

### 변환 큐 (witchy-unpack 71,261)
| topDir/bundleKind | 큐 | topDir/bundleKind | 큐 |
|---|---:|---|---:|
| asset/dcx | 26,759 | chr/character-animation-binder | 563 |
| asset/asset-geometry-binder | 12,657 | chr/character-texture-binder | 526 |
| map/texture-binder | 10,233 | script/dcx | 505 |
| map/dcx | 6,876 | param/dcx | 446 |
| map/map-binder | 4,457 | chr/character-binder | 289 |
| parts/parts-binder | 2,992 | chr/character-behavior-binder | 280 |
| map/ivinfo-binder | 1,614 | sfx/effect-binder | 220 |
| map/map-msb | 935 | cutscene/texture-binder | 158 |
| map/navmesh-binder | 695 | msg/message-binder | 113 |
| event/dcx | 593 | map/battle-binder | 109 |

### 현재 cook 진척 (대략)
| 카테고리 | cook / 큐 | % |
|---|---|---:|
| chr 캐릭터 binder | 269 / 289 | 93% |
| map 지오메트리 | 2,201 / 4,457 | 49% |
| sfx 이펙트 | 110 / 220 | 50% |
| asset 지오메트리 | 2,268 / 12,657 | 18% |
| parts | ~600 / 2,992 | 20% |
| **전체 `.wmesh`** | **5,621** | — |
| **전체 `.dds`** | **77,281** | — |

### 기술 부채 (반드시 해결)
| # | 문제 | 영향 | 담당 레인 |
|---|---|---|---|
| TD1 | **좌표계 Z-up→Y-up** 보정이 cook에 안 구워짐 → 런타임 RotX(+90) 수동 | 모든 캐릭터·맵 | D(파이프라인) |
| TD2 | **935+본 스킨드 애니 정점 폭발** (bind pose offset ≠ wanim 본 공간) | 멀기트/트리가드 등 보스 | A(캐릭터) |
| TD3 | **멀기트 wmesh 한 축 13.6m** (무기/부속 비정상) | 거구 표시 | A |
| TD4 | **wmat diffuse 누락** (MATBIN 토큰 매칭 실패) | 트리가드 등 흰색 | A |
| TD5 | **UI .gfx(Scaleform) 미렌더** → DDS+아틀라스 경로 미구현 | 전 UI | C |
| TD6 | **AET 에셋 텍스처 binder 미추출** → 맵 일부 흰색 | 맵 타일 | B |
| TD7 | **NavMesh/Collision(hkx 페어) 변환 없음** | 충돌·이동 | B/D |
| TD8 | **TAE 애니 이벤트 → .wanim event 미변환** | 콤보·사운드 트리거 | A |
| TD9 | **이펙트(FXR) → 렌더 가능 형태 미변환** — sfx 220 binder/110 wmesh, FXR raw+매니페스트만 있고 `.wfx` 그래프·런타임 재생 경로 없음 | 전 FX | C |
| TD10 | **EldenRing 오디오가 `Resource/Sound/` 밖** → CSound_Manager가 못 찾음. + 3D positional·TAE 이벤트 SFX 없음 | 사운드 | C |
| TD11 | **UI 아틀라스 스키마 미준수** — `CUIAtlasManifest`는 `textures`+`sprites`(uv 없음, 런타임 계산) 정확 스키마 요구, EldenRing 매니페스트 불일치 + FontRaw 미연결 | UI/폰트 | C |
| TD12 | **3D 콜리전 전무** — 엔진은 2D 비트그리드(CNavGrid 512×512)만. 캡슐/AABB/hitbox/hurtbox·hkx 콜리전 변환기 없음 | 전투·이동 | D(Phase2) |
| TD13 | **시퀀서/컷신 런타임 전무** — 설계(06문서)만, `CSequencePlayer`·`.wseq` 미구현 | 컷신 | D(Phase2) |
| TD14 | **월드 스트리밍/async 로더 전무** — 동기 전체맵 로드, `CWorldPartition`/`CAssetStreaming` 미구현 → 대형 필드 히치 | 월드 | D(Phase2) |
| TD15 | **맵 로더가 단일 monolithic wmesh 가정** — 다중 배치 씬은 새 placement 로더 필요 | 맵 배치 | B |

---

## 2. 4인 역할 분담 (병렬 레인)

각 레인은 **독립 디렉터리 + 독립 드라이버 + 독립 검증 게이트**를 갖는다. 충돌 최소화를 위해
서로 다른 `FullGame/<topDir>`와 `Runtime/<kind>` 서브트리만 쓴다.

### Lane A — 캐릭터 / 애니메이션 / 스킨 (담당 1)
- **대상**: chr 전 binder(character/texture/animation/behavior), `Runtime/Character/`
- **핵심 과제**: TD2(935본 스킨 폭발 근본수정), TD3(거구 정규화), TD4(wmat 텍스처), TD8(TAE 이벤트)
- **DoD**: 모든 chr이 스킨드 wmesh+wskel+wanim 정합 + 애니 폭발 없음 + 텍스처 ≥95%
- **TD2 근본 원인 가설**: `cook-runtime-character`의 skel-driven wmesh `offset_matrix`와
  `convert-hkx-anim` FBX의 bind pose 공간이 다름. → 둘을 **같은 FBX/같은 bind pose**에서 파생시켜야 함.
  검증: bind pose에서 `skinMatrix == identity`인지 (정점이 안 움직이는지) 단위 테스트.

### Lane B — 맵 / 월드 / 배치 (담당 2)
- **대상**: map 전 binder(map-binder/texture/msb/navmesh/ivinfo/battle), asset geometry, `Maps/`
- **핵심 과제**: asset geometry 12,657 + map texture 10,233 완주, TD6(AET 텍스처),
  `build-map-placement` → `.wmap` 바이너리화, MSB Part 전 타일 placement
- **DoD**: Limgrave 전 타일이 텍스처 입은 채로 배치, placement 바이너리 로더 동작
- **참고**: `winters.elden.map_placement.v1` JSON 이미 존재(`build-map-placement`). 이를 `.wmap`로 승격.

### Lane C — UI / 폰트 / 사운드 / FX 에셋 (담당 3)
- **대상**: menu(UI), font, `UI/`, `Sound/`, sfx FX 에셋, 전 카테고리 텍스처 정규화
- **핵심 과제**: TD5/TD11(UI 아틀라스), TD9(FXR→`.wfx`), TD10(사운드 배치), 폰트 TTF 대체, `.wtex`(BC 압축)
- **DoD**: 메인메뉴/HUD 아틀라스 렌더 + 폰트 표시 + FX 메시 재생 + 사운드 자동 발견
- **UI 아틀라스 정확 스키마**(필수 준수):
  ```json
  { "textures": { "<id>": { "path": "Resource/UI/...png", "width": 1024, "height": 1024 } },
    "sprites":  { "<id>": { "texture": "<id>", "x": 0, "y": 0, "w": 64, "h": 64 } } }
  ```
  uv 필드 없음(런타임 계산), 텍스처≥1 AND 스프라이트≥1 아니면 `LoadFromJson` 실패. 경로는 exe-상대.
  매니페스트는 `UI_Manager.cpp`의 kPath 상수 위치(`Resource/UI/*.json`) 또는 새 상수+`LoadFromJson`/`ForEachTexture` 호출 추가.
- **사운드**: EldenRing 오디오를 `Client/Bin/Resource/Sound/<카테고리>/`에 배치(FMOD `.wav`/`.ogg`, forward-slash 키 자동, 매니페스트 없음). 현재 EldenRing 오디오는 이 루트 밖이라 미발견.
- **FX 에셋**: sfx mesh는 본 없는 wmesh + 텍스처(이미 WP_A_7051 검증). FXR은 `parse-fxr`/`resolve-fxr` 매니페스트 → `.wfx` 그래프 또는 mesh+texture 바인딩으로 변환해 `FxStaticMeshRenderer`/`CBFxParams`(vTint/vUVRect/vUVScroll/fAlphaClip/fErodeThreshold) 경로로 재생.
- **참고**: MenuTex DDS 34,570장 추출됨. UI 텍스처는 PNG로(SRV `Load_TextureSRV`는 LoL에서 PNG). 폰트는 `CFont_Manager::AddFont(tag,path,size)` + `FindUIFont`로 연결(FontRaw 미연결 상태).

### Lane D — 인프라 / 파이프라인 / 검증 / 런타임 시스템 (담당 4, 리드)
- **대상**: `elden_pipeline.py`, `run_24h_driver.py`, converter, 검증 게이트, CI + Phase2 런타임 시스템 설계
- **핵심 과제**: TD1(좌표계 cook에 굽기), TD7(NavMesh/Collision 큐 확장), 드라이버 안정화,
  `audit-full-pipeline` 게이트 자동화, 디스크 관리 + (Phase2) TD12~TD14 설계
- **DoD**: 사람 개입 없이 G1~G5 리포트 자동 갱신 + 실패분 자동 재시도 + Phase2 시스템 설계서
- **TD1 구현**: `cook-runtime-character`와 FLVER→FBX 단계에서 **변환 시점에 Y-up으로 굽기**
  (Blender export `axis_forward='-Z', axis_up='Y'` 또는 converter에서 RotX(+90) 정점 적용).
  → 런타임 RotX 불필요 (TD1 해결 시 A/B/C 전부 단순화).
- **Phase2 런타임 시스템**(에셋 변환 완료 후, 설계만 W7~8): TD12 3D 콜리전(캡슐+AABB hurtbox/hitbox ECS, hkx→Winters 콜리전 변환 or navgrid carve), TD13 `CSequencePlayer`+`.wseq`, TD14 `CWorldPartition`+`CAssetStreaming`(async IO, GPU upload budget). 06/04/07 설계 문서 기반.
  - **재사용 가능**: `CNavGrid`+`CPathfinder`(flat field-test 슬라이스), `CSound_Manager`(FMOD), `.navgrid` 바이너리(WNVG v1).
  - **신규 필요**: 3D 콜리전 볼륨/overlap, 시퀀서 트랙(Camera/Anim/Fx/Audio/Event/Visibility/TimeDilation), per-cell `.wnav`/`.wcell` async 로딩.

---

## 3. 8주 마일스톤

| 주차 | Lane A 캐릭터 | Lane B 맵 | Lane C UI | Lane D 인프라 |
|---|---|---|---|---|
| **W1** | TD2 근본원인 단위테스트 + 수정 설계 | asset geometry 배치 cook 재개 | gfx 구조 분석 + 아틀라스 스키마 | **TD1 좌표계 cook에 굽기** (최우선) |
| **W2** | TD2 수정 → 보스급 스킨 애니 정상 | map texture 10,233 cook | DDS 아틀라스 자동 생성기 | NavMesh/Collision 큐 확장(TD7) |
| **W3** | 전 chr 재cook(Y-up) + 텍스처 ≥95% | asset geometry 완주 + AET 텍스처 | UIAtlasManifest 로더 연결 | audit 게이트 자동화 |
| **W4** | TD8 TAE 이벤트 → wanim | `.wmap` placement 바이너리화 | 메인메뉴 렌더 | 드라이버 무인 24h 안정 |
| **W5** | 전 chr 애니 셋 완비(idle/move/attack) | Limgrave 전 타일 텍스처 완성 | HUD 아틀라스 | 디스크/번들 관리 |
| **W6** | 캐릭터 쇼케이스 G2/G3/G4 통과 | 맵 쇼케이스 G3 통과 | UI G5 통과 | G1 변환 완전성 통과 |
| **W7** | 잔여 보스/특수 캐릭터 | 잔여 맵 영역(Limgrave 외) | 폰트/로컬라이즈 | 전체 audit 재실행 |
| **W8** | 통합 검증·버그픽스 | 통합 검증 | 통합 검증 | **G1~G5 전체 통과 + 패키징** |

---

## 4. "완료까지 루프" 자동화 구조

```text
[각 레인 무인 드라이버]  run_<lane>_driver.py
   while 미완료 레코드 > 0 and 디스크 OK and not STOP:
       다음 슬라이스(40레코드) cook
       status JSON 기록
       연속 전부실패 N회 → 사유 분류 후 해당 슬라이스 skip-list 등록
   → audit-full-pipeline 게이트 리포트 갱신

[검증 게이트]  audit-full-pipeline (D 레인이 매일 cron)
   G1 변환완전성 / G2 계약 / G3 좌표·스케일 / G4 텍스처 / G5 UI
   → 게이트별 red/green + 막힌 레코드 top-N + 사유

[사람 개입]  red 게이트의 "사유 분류된 영구 실패"만 수동 처리
   (예: 빈 FLVER=설계상 정상, MATBIN 토큰 누락=토큰 정규화 보강)
```

핵심 원칙:
1. **멱등** — 모든 단계 `--resume`, 같은 입력 = 같은 출력. 중단/재시작 안전.
2. **사유 분류** — 실패를 "재시도 가능 / 영구(설계상 정상) / 버그"로 나눠 무한루프 방지.
3. **디스크 가드** — 13GiB 플로어, 번들 정리 자동, 원본 아카이브는 Git 제외.
4. **게이트 우선** — 진행률(%)이 아니라 **게이트 통과 여부**로 완료 판단.

---

## 5. 카테고리별 완료 기준 (DoD 상세)

| 카테고리 | 완료 기준 |
|---|---|
| 캐릭터 | wmesh+wskel+wmat+anims/ 계약 통과, 스킨 애니 폭발 없음, 텍스처 ≥95%, Y-up 정상 |
| 맵 지오메트리 | 본 없는 wmesh, 머티리얼 정렬 드로우콜 병합, AET 텍스처 입힘 |
| 맵 배치 | MSB Part → `.wmap` placement, 전 타일 좌표/회전/스케일 정확 |
| UI | DDS + 아틀라스 매니페스트(`hud_atlas_manifest.json` 스키마), 폰트 대체 |
| FX | sfx mesh = 본 없는 wmesh + 텍스처, FXR→`.wfx` 또는 mesh+texture 바인딩, `FxStaticMeshRenderer` 재생 |
| UI | `textures`+`sprites` 스키마(uv 없음), PNG SRV, kPath 매니페스트 + 폰트 `AddFont(tag,path,size)` |
| 사운드 | `Resource/Sound/<cat>/`에 FMOD `.wav`/`.ogg` 배치, forward-slash 키 자동 발견 (매니페스트 없음) |
| 텍스트/데이터 | FMG/PARAM `Raw/` 보존 (런타임 파싱은 별도) |
| 콜리전(Phase2) | hkx 콜리전 → 3D 볼륨(캡슐/AABB) 또는 navgrid carve, hitbox/hurtbox 타임라인 ECS |
| 시퀀서(Phase2) | `.wseq` JSON + `CSequencePlayer` 트랙(Camera/Anim/Fx/Audio/Event/Visibility/TimeDilation) |
| 월드/스트리밍(Phase2) | `.wmap`/`.wcell`/`.wnav` 바이너리 + `CWorldPartition`/`CAssetStreaming` async |

---

## 5.5 2단계 실행 구조 (에셋 우선, 런타임 시스템 차순)

사용자 1차 목표는 **"모든 에셋 바이너리화 + 깔끔하게 사용 가능"**이다. 따라서:

- **Phase 1 (W1~W6, 에셋 변환·정리)**: 전 카테고리 `.w*` 변환 + UI/사운드/FX 에셋 정리 +
  좌표계(TD1)·스킨(TD2)·텍스처(TD4)·UI(TD11)·사운드 배치(TD10) 부채 해결. → **에셋이 깔끔하게 준비됨**.
- **Phase 2 (W7~W8 설계 + 이후 구현, 런타임 시스템)**: 3D 콜리전(TD12)·시퀀서(TD13)·
  월드 스트리밍(TD14)은 **에셋이 준비된 뒤** 그 에셋을 소비하는 런타임. 2달 내엔 **설계서 + flat field-test 슬라이스**까지,
  전면 구현은 다음 단계. 콜리전/네비는 `CNavGrid`+`CPathfinder`로 평지 슬라이스 우선 동작.

게이트 G1~G5는 Phase 1으로 달성 가능(에셋 완전성·계약·좌표·텍스처·UI). 콜리전/시퀀서/스트리밍은
"있으면 좋은" Phase 2이며 완료 정의의 필수는 아니다(에셋 자체는 그것 없이도 깔끔히 사용 가능).

## 5.6 LoL 추출 파이프라인 규약 미러 (참고)

EldenRing 에셋이 "LoL처럼 깔끔"하려면 기존 LoL 챔피언이 따른 규약을 그대로 미러한다:

| 항목 | LoL 규약 | EldenRing 적용 |
|---|---|---|
| 캐릭터 폴더 | `Texture/Character/<Champ>/`에 `<base>.wmesh/.wskel/.wmat` + `anims/*.wanim` + textures | `Runtime/Character/<cXXXX>/` 동일 구조 (이미 적용) |
| 추출 스크립트 | 챔피언 폴더에 `glb_to_fbx.py`·`extract_submesh.py`·`_rename_anim.py` + `_conversion_logs/` | EldenRing은 `elden_pipeline.py` 서브커맨드로 통합(스크립트 산재 대신 도구화) |
| 애니 키워드 | `PlayAnimationByName("idle"/"run"/"attack")` — 클립명에 키워드 포함 | wanim 파일명에 의미 키워드 부여 권장(현재 `a000_xxxxxx` 원본명) |
| 등록 | `ChampionTable` 정적 테이블이 wmesh 경로 직접 지정 | EldenRing 캐릭터도 정적 테이블(또는 카탈로그 JSON)로 등록 |
| 정리 원칙 | cooked 런타임 셋과 추출 스크립트/로그 **분리** (스크립트는 Tools/) | raw/staging는 저장소 밖, Runtime/만 깔끔하게 |

**중요**: LoL 리소스 트리에 `'LeeSin" && cp ...'` 같은 손상 디렉터리·산재 스크립트가 섞여 있음 →
EldenRing은 **cooked 런타임 셋(`Runtime/`)과 도구/로그를 엄격히 분리**해 같은 실수 반복 금지.

## 6. 리스크 / 제약

1. **저작권**: 원본 추출 에셋은 로컬 비공개. Git에 원본 금지(코드·스크립트·문서만). `02`문서 배포 경계 준수.
2. **디스크**: 전체 변환 시 수십 GB. 레인별 디스크 가드 + 중간 번들 정리 필수.
3. **도구 의존**: WitchyBND(콘솔 핸들 필수 `CREATE_NO_WINDOW`), Blender 4.2.18 + Soulstruct 2.5, texconv. 함정 `14`문서 5절 준수.
4. **빌드 충돌**: 4인이 Engine/Client 동시 수정 시 충돌. Lane D가 엔진 변경 리드, 나머지는 씬/도구/데이터.
5. **MAX_PATH**: WitchyBND 산출 260자 초과 → `\\?\` 확장경로, `rd /s /q` 삭제.

---

## 7. 산출물 (2달 후)

```text
Client/Bin/Resource/EldenRing/
├── FullGame/        # 전 카테고리 cook (.wmesh/.wskel/.wanim/.wmat + Raw)
├── Runtime/         # 런타임 계약 레이아웃 (Character/ Map/ UI/ FX/)
│   ├── Character/<cXXXX>/   # 계약 통과 + 애니 + 텍스처
│   ├── Maps/<area>/<tile>/  # .wmap placement + 타일 텍스처
│   └── UI/                  # DDS + 아틀라스 매니페스트
├── Manifests/       # 카탈로그·감사·게이트 리포트
└── (원본 raw/staging는 저장소 밖)
```

검증: `EldenRingClient.exe --rhi=dx11`로 캐릭터/맵/UI가 좌표·스케일·텍스처·애니 정상 표시 + 자유 카메라 탐색.
