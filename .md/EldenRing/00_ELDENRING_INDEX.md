# Winters EldenRing Plan Index

> 목적: `WintersEngine.dll` 하나로 `WintersLOL.exe`와 `WintersElden.exe`를 분리 구동하고, EldenRing 계열 액션 RPG 클라이언트를 통해 엔진 확장성, 에셋 파이프라인, 월드 스트리밍, 서버 권위 멀티플레이, 에디터 툴링을 포트폴리오급으로 증명한다.

## 핵심 결론

`Client.vcxproj` 하나에 LoL과 EldenRing을 같이 넣지 않는다.

```
WintersEngine.dll
├── WintersLOL.exe      // MOBA client
└── WintersElden.exe    // Action RPG client
```

공통 엔진은 `Engine/`과 `EngineSDK/`가 제공하고, 게임별 씬/카메라/입력/전투/월드/레이드/에셋은 클라이언트 프로젝트 단위로 분리한다.

## 문서 세트

| 문서 | 역할 |
|---|---|
| `01_CLIENT_SPLIT_ENGINE_BOUNDARY.md` | 두 클라이언트 분리, 엔진 DLL 경계, vcxproj/Output 구조 |
| `02_ASSET_EXTRACTION_TO_WINTERS_BINARY_PIPELINE.md` | EldenRing 원본 에셋 추출부터 `.w*` 변환까지 전체 파이프라인 |
| `03_ELDEN_CLIENT_RUNTIME_ARCHITECTURE.md` | `WintersElden.exe` 런타임 구조, Scene/Camera/Combat/Raid 모듈 |
| `04_WORLD_PARTITIONING_STREAMING.md` | World Partitioning, DataLayer, HLOD, streaming source 설계 |
| `05_NETWORK_PVP_COOP_RAID_SERVER_AUTH.md` | TCP/UDP/IOCP, PvP/Co-op/Raid, 서버 권위 시뮬레이션 |
| `06_FX_GRAPH_SEQUENCER_EDITOR.md` | FX 노드 그래프, Sequencer, Editor 툴링 구조 |
| `07_ASSET_LOADER_AND_STREAMING_RUNTIME.md` | 비동기 Asset Loader, GPU upload queue, streaming budget |
| `08_SESSION_ROADMAP.md` | 실제 세션 단위 진행 순서와 완료 기준 |
| `09_ASSET_EXTRACTION_SURVEY_2026_05_25.md` | 2026-05-25 asset extract 전수 조사, FBX/texture inventory, Winters converter smoke test |
| `10_ASSET_PIPELINE_TOOLING.md` | MATBIN/FXR parser, Blender 자동 material mapping, Winters binary 준비 도구 |

## 현재 로컬 입력 자산

원본/실험 에셋 루트:

```
C:/Users/tnest/Desktop/EldenRing
```

확인된 주요 입력:

| 경로 | 용도 |
|---|---|
| `chr3000/chr3000.fbx` | 캐릭터 메시 후보 |
| `chr3000/anim3000.fbx` | 캐릭터 애니 후보 |
| `chr3010/chr3010.fbx` | 첫 로드 후보 1 |
| `chr3010/anim3010.fbx` | 첫 애니 검증 후보 |
| `chrTex2130/chr2130Separated.fbx` | 분리 메시/텍스처 검증 후보 |
| `sfx/*.fbx`, `sfx/*.png` | FX 메시/텍스처 후보 |

사용 가능한 로컬 도구:

| 도구 | 경로 |
|---|---|
| Blender 4.2.18 | `C:/Users/tnest/Downloads/blender-4.2.18-windows-x64` |
| WitchyBND release | `C:/Users/tnest/Downloads/WitchyBND-v3.0.0.0-win-x64` |
| WitchyBND source | `C:/Users/tnest/Downloads/WitchyBND-main` |
| UXM Selective Unpack | `C:/Users/tnest/Downloads/UXM.Selective.Unpack.2.4.2.0` |

## 대원칙

1. 원본 추출 에셋은 로컬/비공개 검증용으로만 둔다.
2. `WintersElden/Bin/Resource`에는 정리된 입력 FBX/PNG/DDS와 변환 산출물을 둔다.
3. 런타임은 최종적으로 FBX/PNG 직접 로딩이 아니라 Winters binary를 우선 로드한다.
4. 공개 포트폴리오에는 엔진 코드, 파이프라인 코드, 구조 문서, 시연 영상 중심으로 보여준다.
5. 공개 배포 빌드는 대체 가능 에셋 또는 자체 제작 에셋으로 갈아끼울 수 있게 경계를 유지한다.

## Winters Binary 목표 포맷

| 포맷 | 역할 | 우선순위 |
|---|---|---|
| `.wmesh` | 정적/스키닝 메시 | P0 |
| `.wskel` | 스켈레톤 계층 | P0 |
| `.wanim` | 애니메이션 클립 | P0 |
| `.wtex` | BC 압축 텍스처, mip 포함 | P1 |
| `.wmat` | 머티리얼 파라미터/텍스처 바인딩 | P1 |
| `.wmap` | 월드 셀/배치/Nav/streaming metadata | P2 |
| `.wfx` | FX 노드 그래프 | P2 |
| `.wseq` | Sequencer 컷신/트랙 | P3 |
| `.winters` | 번들/패키징 | P3 |

## 첫 세로 슬라이스

첫 목표는 "엘든링 전체 복각"이 아니다. 아래 한 줄을 먼저 증명한다.

```
WintersElden.exe boots with WintersEngine.dll,
loads an Elden-style skinned character through Winters binary,
plays idle/run/attack/dodge,
and runs in a small partitioned test field.
```

합격 기준:

| 항목 | 기준 |
|---|---|
| 클라이언트 분리 | `WintersLOL.exe`, `WintersElden.exe`가 같은 엔진 DLL 사용 |
| 에셋 변환 | 첫 캐릭터 `.wmesh/.wskel/.wanim` info 검증 통과 |
| 렌더링 | bind pose 탈출, skinning 폭발 없음 |
| 카메라 | 3인칭 spring arm + lock-on |
| 전투 | 이동, 회피, 약공격, 피격 window |
| 로딩 | sync full load가 아니라 streaming 계획에 맞는 로더 경계 확보 |

## 기존 문서 링크

| 주제 | 기존 문서 |
|---|---|
| 엔진/클라이언트 경계 | `.md/architecture/WINTERS_GAMEPLAY_ARCHITECTURE.md` |
| 엔진 컨벤션 | `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md` |
| 챔피언 `.wmesh/.wskel/.wanim` 절차 | `.md/guide/CHAMPION_WMESH_PIPELINE_GUIDE.md` |
| Winters format 전체 계획 | `.md/plan/WintersFormat/00_WINTERS_FORMAT_INDEX.md` |
| Network system | `.md/plan/engine/NETWORK_SYSTEM.md` |
| Effect Tool | `.md/plan/EffectTool/01_ARCHITECTURE.md` |
