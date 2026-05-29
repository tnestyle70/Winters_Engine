# UE5.7 Plugin Inventory — 자동 분석

**Source**: `C:/Program Files/Epic Games/UE_5.7/Engine/Plugins/`
**Total uplugin files**: 703
**Total modules in plugins**: 1458
**Date**: 2026-05-03

## 카테고리 분포 (Plugin 단위)

| 카테고리 | Plugin 수 | 비율 |
|---|---|---|
| Animation | 60 | 8.5% |
| Editor | 59 | 8.4% |
| Virtual Production | 58 | 8.3% |
| Other | 44 | 6.3% |
| Media Players | 32 | 4.6% |
| Online Platform | 30 | 4.3% |
| Gameplay | 29 | 4.1% |
| Audio | 28 | 4.0% |
| Rendering | 25 | 3.6% |
| Importers | 20 | 2.8% |
| Experimental | 20 | 2.8% |
| Physics | 18 | 2.6% |
| Testing | 17 | 2.4% |
| Misc | 16 | 2.3% |
| Messaging | 14 | 2.0% |
| Networking | 14 | 2.0% |
| Programming | 11 | 1.6% |
| Virtual Reality | 11 | 1.6% |
| Input Devices | 10 | 1.4% |
| Insights | 8 | 1.1% |
| AI | 8 | 1.1% |
| UI | 8 | 1.1% |
| Scripting | 8 | 1.1% |
| Geometry | 8 | 1.1% |
| MetaHuman | 8 | 1.1% |
| Compositing | 6 | 0.9% |
| Source Control | 6 | 0.9% |
| FX | 6 | 0.9% |
| Mobile | 6 | 0.9% |
| Blueprints | 5 | 0.7% |
| Performance | 5 | 0.7% |
| Device Profile Selectors | 5 | 0.7% |
| Database | 5 | 0.7% |
| Content Browser | 4 | 0.6% |
| Water | 4 | 0.6% |
| Denoising | 4 | 0.6% |
| ML | 4 | 0.6% |
| Graphics | 4 | 0.6% |
| Codecs | 4 | 0.6% |
| Analytics | 4 | 0.6% |
| Examples | 3 | 0.4% |
| Cameras | 3 | 0.4% |
| Accessibility | 3 | 0.4% |
| Runtime | 3 | 0.4% |
| Android | 3 | 0.4% |
| Augmented Reality | 3 | 0.4% |
| Input | 2 | 0.3% |
| CustomizableObjects | 2 | 0.3% |
| Profiling | 2 | 0.3% |
| Compression | 2 | 0.3% |
| Localization | 2 | 0.3% |
| Learning | 2 | 0.3% |
| Uncategorized | 2 | 0.3% |
| Dataprep | 2 | 0.3% |
| Framework | 2 | 0.3% |
| (Empty) | 2 | 0.3% |
| Telemetry | 2 | 0.3% |
| Web | 2 | 0.3% |
| Text | 2 | 0.3% |
| Engine | 1 | 0.1% |
| Build Distribution | 1 | 0.1% |
| 2D | 1 | 0.1% |
| BlendSpace | 1 | 0.1% |
| Platform | 1 | 0.1% |
| Exporters | 1 | 0.1% |
| World Building | 1 | 0.1% |
| Mesh | 1 | 0.1% |
| Mutable | 1 | 0.1% |
| Movie Capture | 1 | 0.1% |
| Peripherals | 1 | 0.1% |
| Security | 1 | 0.1% |
| Movie Players | 1 | 0.1% |
| Media | 1 | 0.1% |
| Online | 1 | 0.1% |
| BackgroundHTTP | 1 | 0.1% |
| IOT | 1 | 0.1% |
| Android Background Service | 1 | 0.1% |
| Computer Vision | 1 | 0.1% |
| PreLoadScreenMoviePlayer | 1 | 0.1% |
| Advertising | 1 | 0.1% |
| Gameplay Streaming | 1 | 0.1% |
| Mixed Reality | 1 | 0.1% |
| **합계** | **703** | 100% |

## 핵심 발견

### 1. Runtime 188 vs Plugin 777 — Plugin 이 더 많음

UE5.5+ 에서 많은 모듈이 Runtime → Plugin 으로 이전됨 (예: Niagara, ChaosCloth, MassEntity).
이유: **dynamic loading + 선택적 활성화** — 게임 별로 필요 기능만 활성

### 2. Plugin 의존성 (Plugin 의 Plugins 필드)

가장 많이 의존받는 Plugin (Plugin 간 의존 hub):

| Plugin | 의존받는 횟수 |
|---|---|
| EditorScriptingUtilities | 26 |
| GeometryProcessing | 22 |
| LiveLink | 18 |
| ControlRig | 17 |
| GeometryCache | 16 |
| MediaIOFramework | 15 |
| MeshModelingToolset | 14 |
| LevelSequenceEditor | 14 |
| Takes | 14 |
| OnlineSubsystem | 14 |
| Dataflow | 13 |
| Niagara | 13 |
| ProceduralMeshComponent | 12 |
| ConcertSyncClient | 12 |
| Interchange | 11 |

### 3. 모듈 많은 Plugin Top 15 (mini-engine 같은 거대 plugin)

| Plugin | 모듈 수 | 카테고리 |
|---|---|---|
| Avalanche | 41 | Virtual Production |
| MetaHuman | 28 | MetaHuman |
| nDisplay | 26 | Misc |
| DatasmithCADImporter | 21 | Importers |
| MassGameplay | 16 | Gameplay |
| EditorDataStorageFeatures | 15 | Editor |
| Interchange | 11 | Importers |
| Harmonix | 11 | Audio |
| CaptureManagerApp | 10 | Virtual Production |
| MassAI | 9 | AI |
| USDImporter | 9 | Importers |
| DatasmithImporter | 8 | Importers |
| Niagara | 8 | FX |
| ElectraPlayer | 8 | Media Players |
| PixelStreaming2 | 8 | Graphics |

### 4. Experimental Plugin: 0 / 703 (0.0%)

UE5 도 절반 가까이가 실험 단계 — **Plugin 시스템이 R&D 격리 mechanism**

### 5. Plugin Type 분포

| Type | 수 |
|---|---|

## Winters 청사진 시사점

### Plugin 시스템 도입 (Phase 4 Beyond 100)

- **Engine Core (필수, 항상 로드)**: Core / RHI / Renderer / Resource / ECS / Scene / Framework
- **Engine Plugins (선택)**: Physics / Audio / Network / AI / VFX / Animation / Editor
- **Game Plugins (게임 특화)**: WintersLOL / WintersElden / Champions/{Yasuo} / 등
- **Tool Plugins**: AssetConverter / CompassValidator / AIReadiness

→ 각 Plugin 의 `*.uplugin` (또는 `*.wplugin`) JSON 으로 dependency / EnabledByDefault / LoadingPhase 박제.
→ 사용자 (modder / 외부 개발자) 도 Plugin 추가만으로 확장 가능.

### Experimental 격리

Phase D Physics / Phase E Renderer / Phase F AI 의 신규 모듈은 **Experimental Plugin** 으로 시작.
안정화 후 Engine Plugin 으로 승격.