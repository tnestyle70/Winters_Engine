# UE5.7 Runtime Module Inventory — 자동 카테고리 분류

**Source**: `C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/`
**Total**: 188 modules
**Date**: 2026-05-03
**Categorized via**: regex pattern matching (20 rules, top-down priority)

## 카테고리 분포

| 카테고리 | 모듈 수 | 비율 |
|---|---|---|
| Core | 8 | 4.3% |
| Rendering | 10 | 5.3% |
| Audio | 16 | 8.5% |
| Animation | 5 | 2.7% |
| AI | 6 | 3.2% |
| Physics | 4 | 2.1% |
| Network | 7 | 3.7% |
| UI | 7 | 3.7% |
| Geometry | 5 | 2.7% |
| VFX_Particle | 1 | 0.5% |
| Asset | 7 | 3.7% |
| Movie_Cinema | 6 | 3.2% |
| XR | 2 | 1.1% |
| Platform_OS | 6 | 3.2% |
| Input | 1 | 0.5% |
| Build_Tools | 3 | 1.6% |
| Tests | 3 | 1.6% |
| Streaming | 2 | 1.1% |
| Misc | 89 | 47.3% |
| **합계** | **188** | 100% |

## 카테고리별 모듈 list

### Core (8)

- Core · CoreOnline · CorePreciseFP · CoreUObject
- Engine · EngineSettings · InstallBundleManager · PreLoadScreen

### Rendering (10)

- D3D12RHI · NonRealtimeAudioRenderer · OpenGLDrv · RHI
- RHICore · RenderCore · Renderer · SlateNullRenderer
- SlateRHIRenderer · VulkanRHI

### Audio (16)

- AdpcmAudioDecoder · AudioAnalyzer · AudioCaptureCore · AudioCaptureImplementations
- AudioDeviceEnumeration · AudioExtensions · AudioLink · AudioMixer
- AudioMixerCore · AudioPlatformConfiguration · AudioPlatformSupport · BinkAudioDecoder
- OpusAudioDecoder · RadAudioCodec · SignalProcessing · VorbisAudioDecoder

### Animation (5)

- AnimGraphRuntime · AnimationCore · MovieScene · MovieSceneCapture
- MovieSceneTracks

### AI (6)

- AIModule · GameplayTags · GameplayTasks · MassEntity
- NavigationSystem · Navmesh

### Physics (4)

- ClothingSystemRuntimeCommon · ClothingSystemRuntimeInterface · ClothingSystemRuntimeNv · PhysicsCore

### Network (7)

- Net · NetworkFile · NetworkFileSystem · NetworkReplayStreaming
- Networking · Online · Sockets

### UI (7)

- ApplicationCore · InputCore · MediaAssets · Slate
- SlateCore · UMG · WidgetCarousel

### Geometry (5)

- GeometryCore · GeometryFramework · MeshDescription · SkeletalMeshDescription
- StaticMeshDescription

### VFX_Particle (1)

- VectorVM

### Asset (7)

- AssetRegistry · BuildSettings · Json · JsonUtilities
- PakFile · Serialization · XmlParser

### Movie_Cinema (6)

- CinematicCamera · LevelSequence · MediaUtils · MoviePlayer
- MoviePlayerProxy · TimeManagement

### XR (2)

- EyeTracker · HeadMountedDisplay

### Platform_OS (6)

- Android · Apple · IOS · Linux
- Unix · Windows

### Input (1)

- InputDevice

### Build_Tools (3)

- EngineMessages · Projects · TraceLog

### Tests (3)

- AutomationMessages · AutomationTest · AutomationWorker

### Streaming (2)

- StreamingFile · StreamingPauseRendering

### Misc (89)

- AVEncoder · AVIWriter · AdvancedWidgets · Advertising
- Analytics · AppFramework · AugmentedReality · AutoRTFM
- BlueprintRuntime · CEF3Utils · CUDA · Cbor
- ClientPilot · ColorManagement · CookOnTheFly · CrashReportCore
- Datasmith · DeveloperSettings · Experimental · ExternalRPCRegistry
- FieldNotification · Foliage · FriendsAndChat · GameMenuBuilder
- GameplayDebugger · GameplayMediaEncoder · HardwareSurvey · IESFile
- IPC · ImageCore · ImageWrapper · ImageWriteQueue
- Instrumentation · InteractiveToolsFramework · Interchange · Landscape
- Launch · LiveLinkAnimationCore · LiveLinkInterface · LiveLinkMessageBusFramework
- MRMesh · MaterialShaderQualitySettings · MathCore · Media
- MeshConversion · MeshConversionEngineTypes · MeshUtilitiesCommon · Messaging
- MessagingCommon · MessagingRpc · NNE · NullDrv
- NullInstallBundleManager · OodleDataCompression · OpenColorIOWrapper · Overlay
- PacketHandlers · PerfCounters · PlatformThirdPartyHelpers · Portal
- PropertyPath · RSA · RawMesh · RewindDebuggerRuntimeInterface
- RuntimeAssetCache · SandboxFile · SessionMessages · SessionServices
- Solaris · SoundFieldRendering · StateStream · StorageServerClient
- StorageServerClientDebug · StudioTelemetry · SymsLib · SynthBenchmark
- TextureUtilitiesCommon · Toolbox · TypedElementFramework · TypedElementRuntime
- UEJpegComp · UELibrary · UEWavComp · UniversalObjectLocator
- UnrealGame · VerseCompiler · VirtualProduction · WebBrowser
- WebBrowserTexture

## Winters 와의 매핑 — 시사점

### 현재 Winters Engine 필터 13개 → UE5 카테고리 매핑

| Winters | UE5 매칭 카테고리 | UE5 모듈 수 | Winters 미래 모듈 수 (예상) |
|---|---|---|---|
| 00. Manager (DX11/RHI) | Rendering (RHI 부분) | ~10 | 5-8 |
| 01. Core | Core | ~12 | 8-10 |
| 02. Structure (Framework) | Core (Engine/Framework) | ~5 | 3-4 |
| 03. Renderer | Rendering | ~50 | 10-15 |
| 04. Editor | (UE5 는 Editor/ 별도 영역 — 143 모듈) | — | 5-8 |
| 05. ECS | (UE5 미사용 — Actor/Component) | — | 5-8 |
| 06. Resource | Asset | ~15 | 8-10 |
| 07. Physics | Physics | ~10 | 8-12 |
| 08. Audio | Audio | ~25 | 5-8 |
| 09. Network | Network | ~12 | 8-12 |
| 10. JobSystem | (UE5 는 Core 안 TaskGraph) | — | 3-5 |
| 11. Scene | (UE5 는 World/Level) | — | 3-5 |
| 12. Collision | Physics 의 일부 | — | 통합 |
| 13. AI | AI | ~10 | 8-12 |
| **합계 (현 Winters)** | | | **~80-115 모듈** |

→ UE5 의 188 모듈 (Runtime only) 와 견주려면 Winters 도 80-115 모듈 규모 필요.
  현 13 → 100 정도가 LoL 모작 + 엘든링 모작 + 자체 엔진 디자인의 자연 분기점.

### UE5 가 가진 카테고리 중 Winters 미흡한 영역

- **Movie_Cinema** (UE5 ~15 모듈) — Sequencer / LevelSequence — 컷씬 + replay
- **XR** (UE5 ~8 모듈) — VR/AR 미래 대비
- **Streaming** (UE5 ~5 모듈) — WorldPartition, 오픈월드 (엘든링 모작 필수)
- **Geometry** (UE5 ~10 모듈) — 절차적 메시, 디코페이션
- **Build_Tools** (UE5 ~10 모듈) — 빌드 시스템 자체 도구화 (UBT 같은 것)
- **Localization** (UE5 ~5 모듈) — 다국어 / iCU

### 분류 한계 (Misc 카테고리)

Misc 에 잡힌 모듈들 — 추가 분류 필요:
- AVEncoder
- AVIWriter
- AdvancedWidgets
- Advertising
- Analytics
- AppFramework
- AugmentedReality
- AutoRTFM
- BlueprintRuntime
- CEF3Utils
- CUDA
- Cbor
- ClientPilot
- ColorManagement
- CookOnTheFly
- CrashReportCore
- Datasmith
- DeveloperSettings
- Experimental
- ExternalRPCRegistry
- FieldNotification
- Foliage
- FriendsAndChat
- GameMenuBuilder
- GameplayDebugger
- GameplayMediaEncoder
- HardwareSurvey
- IESFile
- IPC
- ImageCore