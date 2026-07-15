# Engine Source DNA Guide - Winters x Unreal x Unity

Session - Winters Engine, local Unreal Engine source tree, and Unity project architecture are compared from the lowest runtime pnciple up to editor workflow, scene creation, client/server separation, and a League-of-Legends-style C++/C# production path.
ri
## 0. 읽는 방식

이 문서는 "엔진이 무엇인가?"를 추상적으로 설명하는 문서가 아니다.

목표는 다음 질문에 답하는 것이다.

```text
1. 엔진의 가장 밑바닥은 무엇인가?래스, 빌드 시스템에서 어떻게 보이는가?
3. 에디터는 엔진 위에 어떻게 얹히는가?
4. Scene/Level/Map은 실제로 무엇을 저장하는가?
5. Client, Engine, Server
2. 그 밑바닥이 실제 폴더, 파일, 클는 어디서 분리되어야 하는가?
6. LoL 모작을 만든다면 어떤 파일을 만들고 어떤 순서로 쌓아야 하는가?
7. Winters Engine의 도메인은 Unreal/Unity의 어떤 도메인과 대응되는가?
```

NYPC식으로 말하면 이 문서의 핵심은 다음이다.

```text
Engine DNA = Time + Memory + Object/Entity Lifetime + World State + Asset Database + Render/Sim Loop + Tooling Contract
```

게임 엔진은 "화면을 그리는 라이브러리"가 아니다.

게임 엔진은 다음을 반복 가능하게 만드는 시스템이다.

```text
source asset -> imported asset -> cooked runtime asset
designer edit -> serialized scene -> runtime world
input -> command -> simulation -> replicated state -> presentation
code change -> build/reflection -> editor/runtime object
team change -> validation -> versioned content -> deployable build
```

## 1. 현재 확인한 로컬 증거

### 1.1 Unreal Engine 로컬 소스

확인한 루트:

```text
C:\Users\user\Desktop\UnrealEngine\UnrealEngine
```

확인한 버전:

```json
{
  "MajorVersion": 5,
  "MinorVersion": 7,
  "PatchVersion": 4,
  "CompatibleChangelist": 47537391,
  "BranchName": "UE5"
}
```

중요 폴더:

```text
C:\Users\user\Desktop\UnrealEngine\UnrealEngine
  Engine/
  Samples/
  Templates/
  Setup.bat
  GenerateProjectFiles.bat
  Default.uprojectdirs
```

`Engine/Source`는 실제로 다음 다섯 계층으로 나뉜다.

```text
Engine/Source/Runtime
Engine/Source/Editor
Engine/Source/Developer
Engine/Source/Programs
Engine/Source/ThirdParty
```

이 다섯 계층만 봐도 Unreal의 본질이 보인다.

```text
Runtime    = 게임 실행에 필요한 엔진 모듈
Editor     = 에디터 전용 모듈
Developer  = 개발/빌드/분석/캐시/소스컨트롤 도구 모듈
Programs   = UBT, UAT, ShaderCompileWorker, UnrealInsights 같은 독립 실행 프로그램
ThirdParty = 외부 라이브러리
```

### 1.2 Unreal 핵심 헤더 위치

실제 로컬 코드에서 확인한 핵심 파일:

```text
Engine/Source/Runtime/Core/Public/CoreMinimal.h
Engine/Source/Runtime/CoreUObject/Public/UObject/Object.h
Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h
Engine/Source/Runtime/Engine/Classes/Engine/World.h
Engine/Source/Runtime/Engine/Classes/GameFramework/Actor.h
Engine/Source/Runtime/Engine/Classes/GameFramework/Pawn.h
Engine/Source/Runtime/Engine/Classes/GameFramework/Character.h
Engine/Source/Runtime/Engine/Classes/GameFramework/PlayerController.h
Engine/Source/Runtime/Engine/Classes/GameFramework/GameModeBase.h
Engine/Source/Runtime/Engine/Classes/GameFramework/GameStateBase.h
Engine/Source/Runtime/Engine/Classes/Engine/DataAsset.h
Engine/Source/Runtime/Engine/Classes/Engine/AssetManager.h
Engine/Source/Programs/UnrealBuildTool/UnrealBuildTool.cs
```

확인한 클래스 위치:

```text
Actor.h:        AActor : public UObject
Pawn.h:         APawn : public AActor
Character.h:    ACharacter : public APawn
World.h:        UWorld final : public UObject, public FNetworkNotify
GameModeBase.h: AGameModeBase : public AInfo
GameStateBase.h: AGameStateBase : public AInfo
DataAsset.h:    UDataAsset : public UObject
DataAsset.h:    UPrimaryDataAsset : public UDataAsset
```

이것이 Unreal에서 C++ 게임을 만드는 실제 중심축이다.

### 1.3 Unity 로컬 설치

확인한 설치:

```text
C:\Program Files\Unity\Hub\Editor\2022.3.15f1
C:\Program Files\Unity\Hub\Editor\6000.0.79f1
```

Unity 6000.0.79f1 설치에서 확인한 내부 폴더:

```text
Editor/Data/Managed
Editor/Data/il2cpp
Editor/Data/MonoBleedingEdge
Editor/Data/PlaybackEngines/windowsstandalonesupport
Editor/Data/Resources/PackageManager/BuiltInPackages
Editor/Data/UnityReferenceAssemblies
Editor/Data/Tools
```

Unity는 Unreal처럼 C++ 엔진 소스 전체를 프로젝트 폴더에 두고 수정하는 방식이 아니다.

Unity의 개발자가 직접 다루는 핵심은 보통 다음이다.

```text
Assets/
Packages/
ProjectSettings/
*.cs
*.asmdef
*.unity
*.prefab
*.asset
*.meta
```

즉 Unity의 본질은 "닫힌 네이티브 엔진 + C# 컴포넌트 작성 + 직렬화된 에셋/씬 + 패키지 시스템"에 가깝다.

### 1.4 Winters 로컬 코드 구조

확인한 Winters 루트:

```text
C:\Users\user\Desktop\Winters
```

상위 계층:

```text
Engine/
Client/
Server/
Shared/
Services/
```

핵심 구조:

```text
Engine/
  Public/
  Private/
  Include/
  Bin/

Client/
  Public/
  Private/
  Include/
  Bin/

Server/
  Public/
  Private/
  Include/
  Bin/

Shared/
  GameSim/
  Network/
  Replay/
  Schemas/

Services/
  cmd/
  internal/
  migrations/
  pkg/
  docker-compose.yml
```

Winters의 중요한 증거 파일:

```text
Engine/Public/Editor/ImGuiLayer.h
Client/Public/Scene/Scene_Editor.h
Client/Private/Scene/Scene_Editor.cpp
Shared/GameSim/Core/World/World.h
Shared/GameSim/Core/Ecs/*.h
Shared/GameSim/Components/*.h
Shared/GameSim/Systems/*.h
Shared/GameSim/Definitions/*.h
Shared/GameSim/Champions/*/*GameSim.*
Shared/Schemas/*.fbs
Services/docker-compose.yml
```

Winters의 본질은 Unreal/Unity보다 명시적으로 다음에 있다.

```text
Client Input -> GameCommand -> Server GameSim -> Snapshot/Event/FX Cue -> Client Visual
```

## 2. 엔진의 가장 밑바닥 본질

### 2.1 엔진은 "반복 가능한 상태 변환기"다

가장 밑바닥에서 게임 엔진은 이것이다.

```text
previous state + input + delta time + data = next state + output
```

여기서 output은 다음을 포함한다.

```text
render commands
audio commands
animation pose
physics contacts
network packets
save data
debug events
editor preview state
```

엔진이 커질수록 복잡해지는 이유는 그래픽이 어려워서만이 아니다.

진짜 어려운 것은 다음이다.

```text
상태를 누가 소유하는가?
상태를 언제 바꾸는가?
상태 변경을 누가 관찰하는가?
상태를 에디터와 런타임이 같은 의미로 읽는가?
상태를 네트워크/저장/리플레이/협업에서 재현할 수 있는가?
```

### 2.2 밑바닥부터 쌓는 엔진 계층

가장 낮은 층부터 쌓으면 다음 순서다.

```text
Layer 00. OS / Process / File System
Layer 01. Memory / Allocator / Containers
Layer 02. Time / Frame Loop / Job System
Layer 03. Build System / Module Boundary
Layer 04. Object or Entity Identity
Layer 05. Reflection / Serialization / Schema
Layer 06. Asset Database / Import / Cook / Cache
Layer 07. World / Scene / Level / Map
Layer 08. Component / System / Gameplay Framework
Layer 09. Render / RHI / Animation / Physics / Audio / Input
Layer 10. Gameplay Rules / Data Definitions
Layer 11. Networking / Replication / Prediction / Replay
Layer 12. Editor / Tool / Inspector / Content Browser
Layer 13. Build Farm / Package / Deploy / Backend / Docker
```

Winters, Unreal, Unity는 같은 문제를 서로 다른 방식으로 푼다.

```text
Winters = 명시적 계층 분리와 서버 권위 GameSim 중심
Unreal  = C++ 엔진 소스 + UObject reflection + Actor framework + 강력한 Editor
Unity   = C# component authoring + serialized assets + Package/AssetDatabase + 빠른 iteration
```

### 2.3 엔진 설계의 가장 중요한 질문

엔진의 가장 본질적인 질문은 "어떤 렌더링 기술을 쓰는가?"가 아니다.

진짜 핵심 질문은 이것이다.

```text
게임의 진실은 어디에 있는가?
```

답은 엔진마다 다르다.

```text
Winters:
  진실 = Shared/GameSim + Server
  Client = 표현, 입력, 약한 예측, 보간, UI, FX

Unreal:
  진실 = Authority를 가진 World/Actor/GameMode/GameState
  보통 서버 권위 Actor replication 모델
  GameMode는 서버에만 존재
  GameState/PlayerState/Actor replicated property가 클라이언트 표현을 만든다

Unity:
  기본 구조에는 진실 위치가 강제되어 있지 않음
  작은 게임은 Client MonoBehaviour가 truth가 되기 쉽다
  MOBA/멀티플레이에서는 별도 server simulation 또는 authoritative headless Unity server를 명시적으로 설계해야 한다
```

## 3. Winters Engine의 본질

### 3.1 Winters DNA

Winters Engine의 DNA를 한 줄로 쓰면 다음이다.

```text
Winters = Server-authoritative deterministic GameSim + generic C++ runtime engine + product-specific client presentation + tool/data/backend pipeline
```

Winters는 Unreal처럼 거대한 통합 에디터를 먼저 만든 엔진이 아니다.

Winters는 현재 다음을 중심으로 성장하고 있다.

```text
1. LoL 모작 클라이언트
2. 서버 권위 GameSim
3. Engine/Client/Server/Shared 경계
4. asset import/cook/runtime resource pipeline
5. ImGui 기반 editor/debug/tool surface
6. Services + docker 기반 backend
```

### 3.2 Winters의 가장 밑바닥

Winters의 밑바닥은 대략 이렇게 구성된다.

```text
WintersEngine.dll
  generic runtime
  render/resource/input/ui/editor primitive

WintersLOL.exe
  product client
  scene/input/camera/UI/presentation

Server.exe
  authoritative game room
  GameCommand ingestion
  GameSim tick
  Snapshot/Event/FX cue emission

Shared/GameSim
  deterministic gameplay truth
  ECS-like components/systems
  champion skill rules
  definition lookup

Shared/Schemas
  FlatBuffers command/snapshot/event contracts

Services
  data/backend/lobby/service layer
  docker-compose for local infrastructure
```

이 구조에서 중요한 점은 Engine이 게임의 규칙을 모른다는 것이다.

Engine은 다음을 제공해야 한다.

```text
frame loop
resource loading
render primitives
UI primitives
editor panels
asset runtime formats
profiling/debug hooks
generic ECS/runtime infrastructure
```

Client는 다음을 제공한다.

```text
input collection
camera and scene setup
snapshot application
animation playback
FX playback
UI state
weak prediction and interpolation
debug overlays
```

Server와 Shared/GameSim은 다음을 제공한다.

```text
movement truth
damage truth
cooldown truth
buff/debuff truth
projectile truth
death/respawn truth
AI decision output as commands
snapshot/event serialization
```

### 3.3 Winters의 Client / Engine / Server 분리

Winters에서 가장 중요한 경계는 이것이다.

```text
Shared/GameSim must not depend on Engine, Client, Renderer, UI, ImGui, or DX types.
Engine must not depend on LoL Client or Server product code.
Server must not depend on Client visuals.
Client may consume Engine and Shared contracts, but must not invent authoritative gameplay truth.
```

표로 보면 더 선명하다.

| 계층 | 소유하는 것 | 소유하면 안 되는 것 |
|---|---|---|
| Engine | 렌더링, 리소스, 런타임 primitive, UI primitive, editor primitive | LoL 전용 챔피언 규칙, 서버 권위 결과 |
| Shared/GameSim | deterministic gameplay truth, component/system, gameplay definitions | DX, ImGui, Client scene, renderer |
| Server | GameCommand 처리, GameSim tick, Snapshot/Event 송신 | Client animation, UI, camera |
| Client | 입력, 예측, 보간, 애니메이션, FX, UI, 시각화 | HP/damage/cooldown의 최종 판정 |
| Services | 계정, 매치, 데이터, API, DB, Docker 기반 인프라 | frame gameplay truth |

### 3.4 Winters에서 Scene은 무엇인가

현재 Winters의 scene은 Unreal의 `.umap`이나 Unity의 `.unity`처럼 완성된 범용 에디터 산출물이라기보다, 코드/데이터/리소스 로딩 경로가 섞인 런타임 장면 구성이다.

확인한 파일:

```text
Client/Public/Scene/Scene_Editor.h
Client/Private/Scene/Scene_Editor.cpp
```

즉 현재 상태는 다음에 가깝다.

```text
runtime scene + ImGui/debug editor + data/resource tooling
```

아직 Unreal Editor처럼 다음을 하나의 제품으로 완전히 통합한 상태는 아니다.

```text
Content Browser
Level Editor
Property Inspector
Asset Importer
Prefab/Blueprint equivalent
Cook pipeline
Play-in-editor
Multi-user editing
```

하지만 Winters가 향하는 editor 방향은 이미 분명하다.

```text
Content catalog
Importer/converter
Material resolver
WFX/FX graph
Sequencer
World partition / streaming cell tool
Validation panels
Runtime-compatible cooked Winters binary assets
```

### 3.5 Winters에서 게임 기능을 추가하는 실제 방식

예를 들어 챔피언 스킬 하나를 추가한다고 하자.

Winters에서 올바른 방향은 다음이다.

```text
1. Shared/GameSim/Definitions
   - ChampionGameplayDef
   - SkillGameplayDef
   - SkillAtomData
   - SkillCommand
   - DataPackManifest

2. Shared/GameSim/Components
   - champion-specific sim component
   - cooldown/mana/action/buff/projectile component

3. Shared/GameSim/Systems
   - command validation
   - movement/damage/projectile/status systems
   - champion hook

4. Shared/Schemas
   - command/snapshot/event schema change if wire contract changes

5. Server
   - GameCommand ingestion
   - GameSim tick integration
   - snapshot/event/FX cue emission

6. Client
   - input binding
   - snapshot apply
   - visual catalog lookup
   - animation/FX/UI playback

7. Engine
   - only if generic runtime/render/resource/tool capability is missing

8. Tools/Data
   - authored JSON or source data
   - cooked immutable packs
   - validators and debug panels
```

중요한 decoy는 이것이다.

```text
Decoy:
  "클라이언트에서 스킬 이펙트가 잘 보이니까 거기에 판정도 넣자."

Correct:
  "클라이언트는 보여준다. 서버/GameSim이 판정한다."
```

### 3.6 Winters의 Unreal/Unity 대응 도메인

| Winters | Unreal 대응 | Unity 대응 | 본질 |
|---|---|---|---|
| Engine/Public, Engine/Private | Engine/Source/Runtime | Unity native runtime + packages | 공용 런타임 기능 |
| Shared/GameSim/Core/World | UWorld 일부 + gameplay sim layer | Scene + custom simulation world | 게임 상태 컨테이너 |
| Shared/GameSim/Components | ActorComponent와는 다르지만 데이터 컴포넌트 역할 | Component/ECS component | 상태 조각 |
| Shared/GameSim/Systems | subsystem/gameplay system | MonoBehaviour systems, DOTS systems | 상태 변환 |
| Shared/GameSim/Definitions | DataAsset/DataTable/GameplayTags | ScriptableObject/JSON/Addressables data | 게임 데이터 |
| Shared/Schemas | replication schema, RPC, NetSerialize | Netcode messages, custom protocol | 네트워크 계약 |
| Client Scene | Level-specific presentation layer | Scene-specific GameObjects | 화면 구성 |
| Engine/Public/Editor/ImGuiLayer | UnrealEd/Slate/Details panel 일부 | EditorWindow/Inspector 일부 | 도구 UI |
| Services/docker-compose.yml | OnlineSubsystem/backend 별도 | UGS/backend 별도 | 라이브 서비스 인프라 |

## 4. Unreal Engine의 본질

### 4.1 Unreal DNA

Unreal의 DNA를 한 줄로 쓰면 다음이다.

```text
Unreal = C++ source engine + UObject reflection + package/asset system + Actor gameplay framework + integrated editor
```

Unreal의 본질은 "C++로 만든 객체 세계를 에디터가 이해할 수 있게 만든 것"이다.

Unreal에서 C++ 클래스는 단순 C++ 클래스가 아니다.

```cpp
UCLASS()
class AMyActor : public AActor
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MoveSpeed = 600.0f;

    UFUNCTION(Server, Reliable)
    void ServerCastSkill();
};
```

이 코드는 세 가지 세계에 동시에 걸린다.

```text
C++ compiler world
Unreal reflection world
Editor/Blueprint/serialization world
```

그 중간을 이어주는 것이 다음이다.

```text
UnrealHeaderTool
UnrealBuildTool
generated headers
UClass / UProperty / UFunction metadata
```

### 4.2 Unreal 소스 폴더의 의미

로컬 Unreal `Engine/Source`는 다음처럼 읽어야 한다.

```text
Runtime
  실제 게임과 packaged build에 들어갈 수 있는 엔진 기능

Editor
  Unreal Editor 안에서만 쓰이는 기능
  LevelEditor, ContentBrowser, PropertyEditor, UnrealEd 등

Developer
  개발 중 필요한 도구성 모듈
  AssetTools, DerivedDataCache, SourceControl, TargetPlatform 등

Programs
  독립 실행 도구
  UnrealBuildTool, AutomationTool, ShaderCompileWorker, UnrealInsights, UnrealPak 등

ThirdParty
  외부 라이브러리
```

이 구조가 Unreal의 Client/Engine/Server 분리보다 더 밑에 있는 "모듈 분리"다.

Unreal에서 게임 프로젝트는 이 거대한 엔진 소스 위에 자기 모듈을 추가한다.

```text
MyGame/
  MyGame.uproject
  Source/
    MyGame/
      MyGame.Build.cs
      MyGame.cpp
      MyGame.h
    MyGame.Target.cs
    MyGameEditor.Target.cs
    MyGameServer.Target.cs
  Content/
  Config/
```

### 4.3 Unreal Runtime 계층

로컬 Runtime 폴더에서 확인한 주요 모듈:

```text
Core
CoreUObject
Engine
ApplicationCore
InputCore
RenderCore
Renderer
RHI
RHICore
D3D12RHI
VulkanRHI
AssetRegistry
Projects
PakFile
Json
JsonUtilities
Slate
SlateCore
UMG
AnimationCore
AnimGraphRuntime
AIModule
NavigationSystem
GameplayTags
GameplayTasks
MassEntity
Net
Networking
NetworkReplayStreaming
LevelSequence
MovieScene
PhysicsCore
AudioMixer
Landscape
Foliage
```

Unreal Runtime의 바닥을 순서대로 보면 다음이다.

```text
Core
  primitive types, containers, logging, platform abstraction, memory, delegates

CoreUObject
  UObject, UClass, reflection, garbage collection, object construction, serialization

Engine
  UWorld, AActor, Components, GameFramework, Level, AssetManager, GameInstance

Renderer/RHI
  rendering abstraction, GPU command path, platform graphics backend

Gameplay/runtime feature modules
  AI, Navigation, Animation, UMG, Audio, Networking, GameplayTags, GameplayTasks
```

### 4.4 UObject가 Unreal의 바닥인 이유

Unreal에서 `UObject`는 단순한 base class가 아니다.

`UObject`는 다음을 묶는다.

```text
identity
name
outer/package ownership
reflection metadata
serialization
garbage collection
editor visibility
Blueprint exposure
asset reference
network-aware object reference 기반
```

로컬 코드 기준:

```text
Engine/Source/Runtime/CoreUObject/Public/UObject/Object.h
Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h
```

`UObjectGlobals.h`에는 `NewObject`와 `StaticConstructObject_Internal` 계열이 있다.

즉 Unreal에서 객체 생성은 단순 `new`가 아니라 다음 경로를 탄다.

```text
UClass metadata
Object initializer
Outer/package
Flags
Reflection registration
GC tracking
Serialization readiness
```

이것이 Unreal C++이 일반 C++과 다르게 느껴지는 가장 큰 이유다.

### 4.5 UWorld / Actor / Component

Unreal에서 실제 게임 월드의 중심은 `UWorld`다.

로컬 코드:

```text
Engine/Source/Runtime/Engine/Classes/Engine/World.h
```

확인한 선언:

```text
UWorld final : public UObject, public FNetworkNotify
```

`UWorld`는 다음을 붙잡는다.

```text
levels
actors
world settings
physics scene
timer manager
net driver
game instance relation
streaming
partition
play/editor world context
```

Actor는 월드 안에 놓이거나 spawn되는 gameplay object다.

로컬 코드:

```text
Engine/Source/Runtime/Engine/Classes/GameFramework/Actor.h
```

확인한 선언:

```text
AActor : public UObject
```

Actor는 다음의 컨테이너다.

```text
Transform
Components
Tick
Replication
Construction script
BeginPlay/EndPlay lifecycle
Editor placement
```

Pawn과 Character는 Actor 위에 쌓인 gameplay convention이다.

```text
APawn : public AActor
ACharacter : public APawn
```

MOBA에서 champion을 만들 때 선택지는 둘이다.

```text
Option A. ACharacter 기반
  CharacterMovement, capsule, skeletal mesh, animation integration이 쉬움
  대신 LoL식 deterministic/server-authoritative movement와 충돌할 수 있음

Option B. APawn 또는 AActor 기반 custom movement
  LoL식 click-to-move, server simulation, interpolation을 명시적으로 설계하기 좋음
  대신 animation/movement 편의 기능을 직접 더 만들어야 함
```

LoL 모작이라면 보통 `APawn` 기반 custom movement 또는 `ACharacter`를 쓰더라도 movement authority를 강하게 통제하는 방식을 추천한다.

### 4.6 Unreal Gameplay Framework의 본질

Unreal의 Gameplay Framework는 대략 이 구조다.

```text
UGameInstance
  process/session-level persistent game object

UWorld
  active world/level runtime state

AGameModeBase
  server-only game rule owner

AGameStateBase
  replicated game state visible to clients

APlayerController
  human player's control endpoint
  server + owning client에 존재

APlayerState
  replicated per-player state

APawn / ACharacter
  possessed controllable actor

AActor / UActorComponent
  placeable/spawnable object and behavior pieces
```

서버 권위 관점에서 중요한 규칙:

```text
GameMode = server only
GameState = server and clients, replicated
PlayerController = server and owning client
Pawn/Actor = replicated if configured
Client UI = local presentation
```

Winters와 비교하면 다음이다.

```text
Winters Server GameSim truth
  ~= Unreal server-authoritative GameMode/GameState/Actor state

Winters Client Visual
  ~= Unreal client-side replicated actor presentation + UMG/Niagara/Animation

Winters Shared/Schemas
  ~= Unreal reflected property/RPC/NetSerialize + replicated actor references
```

단, 큰 차이가 있다.

Unreal은 replication이 Actor/UObject framework에 깊게 들어가 있다.

Winters는 FlatBuffers schema와 GameSim snapshot/event를 더 명시적으로 관리한다.

### 4.7 Unreal Editor의 실제 구성

로컬 `Engine/Source/Editor`에서 확인한 주요 모듈:

```text
UnrealEd
LevelEditor
ContentBrowser
ContentBrowserData
SceneOutliner
PropertyEditor
GraphEditor
Kismet
KismetCompiler
BlueprintGraph
MaterialEditor
StaticMeshEditor
SkeletalMeshEditor
AnimationEditor
AnimationBlueprintEditor
BehaviorTreeEditor
DataTableEditor
CurveTableEditor
Sequencer
UMGEditor
WorldPartitionEditor
LandscapeEditor
FoliageEdit
ProjectSettingsViewer
EditorSubsystem
EditorWidgets
ToolMenusEditor
SourceControlWindows
UndoHistoryEditor
```

이것은 Unreal Editor가 단일 거대 창이 아니라, 수많은 editor module이 합쳐진 애플리케이션이라는 뜻이다.

Editor의 본질:

```text
Runtime object model을 사람이 편집할 수 있게 만든 도구 계층
```

Editor는 Runtime보다 위에 있다.

```text
Editor depends on Runtime.
Runtime must not depend on Editor.
```

이 원칙은 Winters에도 그대로 적용된다.

```text
Winters Editor/Tool can depend on Engine runtime contracts.
Engine runtime should not require editor-only product code to run.
```

### 4.8 Unreal Developer / Programs 계층

로컬 `Engine/Source/Developer`에서 확인한 주요 모듈:

```text
AssetTools
DerivedDataCache
DesktopPlatform
HotReload
SourceCodeAccess
SourceControl
TargetPlatform
MeshBuilder
MeshUtilities
MaterialBaking
TextureBuild
TextureCompressor
ShaderCompilerCommon
MessageLog
OutputLog
TraceInsights
AutomationController
SessionFrontend
UncontrolledChangelists
UnsavedAssetsTracker
```

로컬 `Engine/Source/Programs`에서 확인한 주요 프로그램:

```text
UnrealBuildTool
AutomationTool
ShaderCompileWorker
UnrealInsights
UnrealPak
UnrealFrontend
CrashReportClient
DerivedDataBuildWorker
TextureBuildWorker
UnrealMultiUserServer
UnrealTraceServer
UnrealVersionSelector
LiveCodingConsole
```

즉 Unreal의 생산성은 Editor UI만의 결과가 아니다.

다음이 함께 작동한다.

```text
UBT   = C++ module/target build graph
UHT   = reflection metadata generation
UAT   = automation/build/package/cook pipeline
DDC   = derived asset cache
ShaderCompileWorker = shader compilation offload
UnrealPak/IoStore = packaged content
UnrealInsights/Trace = performance analysis
```

Winters에서 앞으로 필요한 도구들도 결국 이 범주로 나뉜다.

```text
Build graph
Reflection/schema generation
Asset cook/cache
Shader/material compile
Packager
Profiler/trace viewer
Validation dashboard
```

### 4.9 Unreal Plugins

로컬 `Engine/Plugins`에서 확인한 중요한 플러그인 영역:

```text
Runtime/GameplayAbilities
Runtime/GameplayTags related modules
Runtime/ReplicationGraph
Runtime/NetworkPrediction
Runtime/CommonUI
Runtime/MassEntity
FX/Niagara
FX/NiagaraFluids
EnhancedInput
PCG
Online
Animation
AI
```

특히 LoL 모작과 관련 깊은 것:

```text
GameplayAbilities
  skill, attribute, effect, cooldown, gameplay tags

GameplayTags
  state/skill/status/effect identity

GameplayTasks
  async gameplay execution primitive

ReplicationGraph
  actor replication scaling

NetworkPrediction
  movement/prediction architecture reference

Niagara
  VFX graph and runtime

CommonUI / UMG
  HUD and menu

EnhancedInput
  input mapping

AIModule / NavigationSystem
  bot, minion, pathing
```

## 5. Unreal에서 C++로 LoL 모작을 만드는 과정

### 5.1 프로젝트 생성 후 파일 구조

Unreal에서 C++ 프로젝트를 만들면 기본적으로 이런 구조가 된다.

```text
WintersMOBA/
  WintersMOBA.uproject
  Config/
    DefaultEngine.ini
    DefaultGame.ini
    DefaultInput.ini
  Content/
    Maps/
    Characters/
    Abilities/
    UI/
    FX/
    Data/
  Source/
    WintersMOBA/
      WintersMOBA.Build.cs
      WintersMOBA.h
      WintersMOBA.cpp
      Private/
      Public/
    WintersMOBA.Target.cs
    WintersMOBAEditor.Target.cs
    WintersMOBAServer.Target.cs
```

LoL 모작이라면 처음부터 Server target을 둬야 한다.

```csharp
// Source/WintersMOBAServer.Target.cs
using UnrealBuildTool;
using System.Collections.Generic;

public class WintersMOBAServerTarget : TargetRules
{
    public WintersMOBAServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        ExtraModuleNames.Add("WintersMOBA");
    }
}
```

### 5.2 Build.cs 설계

MOBA 기본 module dependency 예시:

```csharp
// Source/WintersMOBA/WintersMOBA.Build.cs
using UnrealBuildTool;

public class WintersMOBA : ModuleRules
{
    public WintersMOBA(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "GameplayTags",
            "GameplayTasks",
            "GameplayAbilities",
            "AIModule",
            "NavigationSystem",
            "UMG"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Slate",
            "SlateCore"
        });
    }
}
```

핵심은 모듈 의존성이다.

```text
Gameplay module이 Editor module에 의존하면 안 된다.
Server build가 UMG/Slate/Editor-only path를 직접 요구하면 안 된다.
Editor-only 도구는 별도 WintersMOBAEditor module로 뺀다.
```

### 5.3 최소 클래스 세트

LoL 모작의 Unreal C++ 최소 클래스는 다음이다.

```text
Core game flow
  AMobaGameMode
  AMobaGameState
  AMobaPlayerController
  AMobaPlayerState
  UMobaGameInstance

World actors
  AMobaChampion
  AMobaMinion
  AMobaTurret
  AMobaNexus
  AMobaProjectile
  AMobaNeutralCamp
  AMobaVisionWard

Components
  UMobaAbilitySystemComponent
  UMobaHealthComponent
  UMobaManaComponent
  UMobaTeamComponent
  UMobaVisionComponent
  UMobaClickMoveComponent
  UMobaReplicationComponent

Data
  UChampionDataAsset
  UAbilityDataAsset
  UItemDataAsset
  UMinionWaveDataAsset
  UMapRuleDataAsset

UI
  UMobaHudWidget
  UMobaMinimapWidget
  UMobaSkillBarWidget

AI
  AMobaMinionAIController
  AMobaJungleAIController
  BehaviorTree / Blackboard assets
```

### 5.4 Champion DataAsset 예시

```cpp
// Source/WintersMOBA/Public/Data/ChampionDataAsset.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ChampionDataAsset.generated.h"

UCLASS(BlueprintType)
class WINTERSMOBA_API UChampionDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Champion")
    FName ChampionId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Champion")
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
    float BaseHealth = 600.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
    float BaseMana = 300.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
    float MoveSpeed = 550.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags")
    FGameplayTagContainer ChampionTags;
};
```

Unreal에서 DataAsset은 Unity의 ScriptableObject와 대응된다.

Winters에서는 `Shared/GameSim/Definitions`와 cooked definition pack에 대응된다.

### 5.5 Champion Actor 예시

```cpp
// Source/WintersMOBA/Public/Actors/MobaChampion.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MobaChampion.generated.h"

class UChampionDataAsset;

UCLASS()
class WINTERSMOBA_API AMobaChampion : public APawn
{
    GENERATED_BODY()

public:
    AMobaChampion();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Champion")
    TObjectPtr<UChampionDataAsset> ChampionData;

    UPROPERTY(ReplicatedUsing=OnRep_Health, BlueprintReadOnly, Category="State")
    float Health = 1.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="State")
    FVector_NetQuantize10 ServerMoveTarget;

    UFUNCTION(Server, Reliable)
    void ServerIssueMove(FVector_NetQuantize10 TargetLocation);

    UFUNCTION(Server, Reliable)
    void ServerCastAbility(int32 AbilitySlot, FVector_NetQuantize10 TargetLocation);

protected:
    UFUNCTION()
    void OnRep_Health();
};
```

중요한 점:

```text
ServerIssueMove = client input을 server command로 보냄
ServerCastAbility = skill command 요청
Health = server가 소유하고 replicated
OnRep_Health = client presentation update
```

이 구조는 Winters의 흐름과 거의 같다.

```text
Client input -> Server RPC -> server authority -> replicated state -> client visual
```

차이는 Unreal은 RPC/replication을 Actor framework가 제공하고, Winters는 command/snapshot schema를 직접 가진다는 점이다.

### 5.6 GameMode / GameState

MOBA에서 GameMode는 서버 전용 rule owner다.

```cpp
// Source/WintersMOBA/Public/Game/MobaGameMode.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MobaGameMode.generated.h"

UCLASS()
class WINTERSMOBA_API AMobaGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    virtual void StartPlay() override;

protected:
    void SpawnTeams();
    void StartMinionWaveTimer();
    void ResolveMatchEnd();
};
```

GameState는 클라이언트가 볼 수 있는 match state다.

```cpp
// Source/WintersMOBA/Public/Game/MobaGameState.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "MobaGameState.generated.h"

UCLASS()
class WINTERSMOBA_API AMobaGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    UPROPERTY(Replicated, BlueprintReadOnly)
    int32 BlueTeamKills = 0;

    UPROPERTY(Replicated, BlueprintReadOnly)
    int32 RedTeamKills = 0;

    UPROPERTY(Replicated, BlueprintReadOnly)
    float MatchTimeSeconds = 0.0f;
};
```

설계 규칙:

```text
GameMode에 있는 것은 client가 직접 볼 수 없다.
Client가 봐야 하는 match state는 GameState/PlayerState/replicated Actor로 보낸다.
```

이것은 Winters의 Server -> Snapshot/Event -> Client Visual 구조와 동일한 철학이다.

### 5.7 Unreal Editor에서 Scene 만드는 과정

LoL 모작 기준 Unreal Editor 작업 순서:

```text
1. C++ 프로젝트 생성
2. C++ base classes 추가
3. Compile
4. Editor에서 Blueprint subclass 생성
5. Content/Data에 DataAsset 생성
6. Content/Maps에 SummonerRift.umap 생성
7. terrain/static mesh 배치
8. lane spline 또는 waypoint 배치
9. turret/nexus/inhibitor actor 배치
10. minion spawner actor 배치
11. NavMeshBoundsVolume 배치
12. PlayerStart 또는 team spawn point 배치
13. World Settings에서 GameMode 지정
14. Project Settings에서 GameInstance/Input/Maps 설정
15. PIE로 listen/dedicated server 테스트
16. packaged client/server build 테스트
```

Unreal의 `.umap`은 단순 텍스트 scene 파일이 아니다.

본질은 다음이다.

```text
serialized package containing level/world object graph
```

그 안에는 placed actors, component properties, references to assets, world settings, streaming data 등이 들어간다.

### 5.8 Unreal에서 실제 코드 편집과 Editor 반영

코드 수정 흐름:

```text
1. Header/CPP 수정
2. UCLASS/UPROPERTY/UFUNCTION이 바뀌면 UHT 대상 변경
3. Visual Studio/Rider에서 build
4. Editor는 reflection metadata와 compiled module을 읽음
5. Blueprint/Details panel에 property/function 노출
6. Blueprint subclass나 placed Actor에 값 저장
7. 저장된 값은 .uasset/.umap package에 직렬화
```

주의:

```text
Live Coding/Hot Reload는 iteration 도구다.
UObject layout, UPROPERTY 변경, constructor default, Blueprint-exposed shape가 크게 바뀌면 Editor 재시작이 안전하다.
```

### 5.9 Unreal에서 Client / Engine / Server 분리

Unreal에서는 "Engine", "Game", "Server"가 다음처럼 나뉜다.

```text
Engine source
  Core, CoreUObject, Engine, Renderer, Editor, Programs...

Game project module
  WintersMOBA runtime gameplay code

Editor module
  WintersMOBAEditor editor-only tools

Target
  WintersMOBA         = game/client target
  WintersMOBAEditor   = editor target
  WintersMOBAServer   = dedicated server target
```

서버 빌드에서 피해야 할 것:

```text
Editor module dependency
UMG-only presentation assumptions
client-only Niagara triggering as gameplay truth
camera/HUD code inside authoritative rules
```

MOBA에서 추천하는 Unreal 분리:

```text
Source/WintersMOBA
  Runtime shared gameplay classes
  Actor state
  replicated components
  server-authoritative command execution

Source/WintersMOBAEditor
  custom asset validators
  map lane editor
  champion data editor
  batch importers

Content/
  cooked/serialized assets
  maps, blueprints, data assets, materials, animations, Niagara systems

Dedicated Server target
  match simulation
  replicated actor state
  no editor dependency
```

### 5.10 Unreal LoL 모작 구현 순서

가장 안전한 순서:

```text
Stage 01. Empty C++ project + dedicated server target
Stage 02. AMobaGameMode / AMobaGameState / AMobaPlayerController
Stage 03. AMobaChampion with replicated position/health
Stage 04. right-click move command via Server RPC
Stage 05. top-down camera and local input
Stage 06. simple lane map .umap
Stage 07. turret/nexus/minion actors
Stage 08. minion wave spawn and AI path
Stage 09. damage pipeline
Stage 10. ability command and cooldown
Stage 11. DataAssets for champions/skills/items
Stage 12. UMG HUD/minimap/skill bar
Stage 13. Niagara FX triggered by replicated gameplay event
Stage 14. animation montage/state machine
Stage 15. replication scaling with relevancy/ReplicationGraph if needed
Stage 16. packaged client/server smoke
```

Decoy:

```text
"먼저 화려한 챔피언 스킬과 Niagara부터 만든다."
```

Correct:

```text
"먼저 server authority + command + replicated state + simple visual까지 관통시킨다."
```

## 6. Unity Engine의 본질

### 6.1 Unity DNA

Unity의 DNA를 한 줄로 쓰면 다음이다.

```text
Unity = closed native engine runtime + C# script components + serialized GameObject scenes/prefabs + AssetDatabase/package workflow
```

Unity는 Unreal과 다르게 개발자가 일반적으로 엔진 C++ 소스를 수정하지 않는다.

개발자는 다음을 만든다.

```text
C# scripts
GameObjects
Components
Prefabs
Scenes
ScriptableObjects
Packages
EditorWindows
Build settings
Addressables/AssetBundles
```

Unity의 강점은 빠른 iteration이다.

```text
script 작성 -> GameObject에 attach -> Inspector에서 값 조정 -> Scene 저장 -> Play
```

### 6.2 Unity 설치 폴더의 의미

로컬 Unity 6000.0.79f1 기준 확인한 폴더:

```text
Editor/Data/Managed
  UnityEngine.dll, UnityEditor.dll 등 C# API assembly 계층

Editor/Data/il2cpp
  IL2CPP backend

Editor/Data/MonoBleedingEdge
  Mono runtime/toolchain

Editor/Data/PlaybackEngines/windowsstandalonesupport
  platform-specific player build support

Editor/Data/Resources/PackageManager/BuiltInPackages
  built-in packages
  com.unity.render-pipelines.universal
  com.unity.render-pipelines.high-definition
  com.unity.visualeffectgraph
  com.unity.shadergraph
  com.unity.ugui
  com.unity.textmeshpro
  com.unity.modules.animation
  com.unity.modules.physics
  com.unity.modules.particlesystem
```

이 구조는 Unity의 본질을 보여준다.

```text
Editor executable/native engine
  + managed C# API
  + package manager
  + platform playback engine
  + script compilation
  + asset import database
```

### 6.3 Unity 프로젝트 파일 구조

일반적인 MOBA 프로젝트 구조:

```text
WintersMobaUnity/
  Assets/
    Scenes/
      MainMenu.unity
      MobaMap.unity
    Scripts/
      Runtime/
        Game/
        Champions/
        Abilities/
        Networking/
        UI/
        AI/
      Editor/
        DataValidators/
        MapTools/
    Prefabs/
      Champions/
      Minions/
      Turrets/
      Projectiles/
      UI/
    ScriptableObjects/
      Champions/
      Abilities/
      Items/
      MinionWaves/
    Materials/
    Animations/
    VFX/
    AddressableAssetsData/
  Packages/
    manifest.json
    packages-lock.json
  ProjectSettings/
    ProjectSettings.asset
    EditorBuildSettings.asset
    InputManager.asset or InputSystem settings
    GraphicsSettings.asset
    QualitySettings.asset
  Library/
    generated asset database and imported artifacts
  UserSettings/
```

Git에 보통 넣는 것:

```text
Assets/
Packages/
ProjectSettings/
*.meta
```

Git에 보통 넣지 않는 것:

```text
Library/
Temp/
Obj/
Logs/
Build/
UserSettings/ 일부
```

### 6.4 .meta와 AssetDatabase의 본질

Unity에서 `.meta` 파일은 매우 중요하다.

본질:

```text
asset path가 아니라 GUID가 asset identity다.
```

예:

```text
Assets/Prefabs/Champions/Irelia.prefab
Assets/Prefabs/Champions/Irelia.prefab.meta
```

`.meta`에는 GUID와 import settings가 들어간다.

Scene, Prefab, ScriptableObject는 이 GUID로 서로를 참조한다.

그래서 협업에서 가장 중요한 규칙:

```text
.meta 파일을 반드시 함께 커밋한다.
asset을 OS 탐색기에서 막 옮기기보다 Unity Editor 안에서 이동한다.
Library는 재생성 산출물이므로 커밋하지 않는다.
```

Unreal의 `.uasset` package reference와 비슷하지만, Unity는 source asset 옆의 `.meta`가 눈에 보이는 방식이다.

### 6.5 Unity Scene / GameObject / Component

Unity의 scene은 `.unity` 파일이다.

본질:

```text
serialized hierarchy of GameObjects and Components
```

GameObject는 이름/Transform/활성 상태를 가진 컨테이너다.

실제 행동은 Component가 가진다.

```text
GameObject
  Transform
  MeshRenderer
  Animator
  Collider
  Rigidbody
  ChampionController : MonoBehaviour
  HealthView : MonoBehaviour
```

Unity 공식 구조상 `MonoBehaviour`는 GameObject에 붙는 script component다.

즉 Unity에서 gameplay code의 기본 단위는 다음이다.

```text
class MyBehaviour : MonoBehaviour
```

Unreal의 Actor/Component와 대응하면 다음이다.

```text
Unreal AActor      ~= Unity GameObject + Transform + Components bundle
Unreal Component   ~= Unity Component / MonoBehaviour
Unreal UDataAsset  ~= Unity ScriptableObject asset
Unreal .umap       ~= Unity .unity scene
Unreal Blueprint   ~= Unity Prefab + MonoBehaviour + serialized fields 일부 조합
```

완전히 같지는 않다.

Unreal은 Actor class 자체가 gameplay framework와 replication에 강하게 연결되어 있다.

Unity는 GameObject/Component가 더 자유롭고, multiplayer authority는 직접 설계해야 한다.

### 6.6 Unity ScriptableObject

Unity에서 게임 데이터를 에셋화하는 기본 도구는 ScriptableObject다.

MOBA champion data 예시:

```csharp
// Assets/Scripts/Runtime/Data/ChampionData.cs
using UnityEngine;

[CreateAssetMenu(menuName = "WintersMOBA/Champion Data")]
public sealed class ChampionData : ScriptableObject
{
    public string championId;
    public string displayName;
    public float baseHealth = 600f;
    public float baseMana = 300f;
    public float moveSpeed = 550f;
    public AbilityData[] abilities;
}
```

이것은 Unreal의 `UPrimaryDataAsset`과 대응된다.

Winters에서는 `Shared/GameSim/Definitions` + cooked definition pack과 대응된다.

### 6.7 Unity MonoBehaviour Champion 예시

```csharp
// Assets/Scripts/Runtime/Champions/ChampionController.cs
using UnityEngine;

public sealed class ChampionController : MonoBehaviour
{
    [SerializeField] private ChampionData championData;
    [SerializeField] private Animator animator;

    private Vector3 serverPosition;
    private float health;

    public string ChampionId => championData != null ? championData.championId : string.Empty;

    private void Awake()
    {
        health = championData != null ? championData.baseHealth : 1f;
    }

    private void Update()
    {
        transform.position = Vector3.Lerp(transform.position, serverPosition, Time.deltaTime * 12f);
    }

    public void ApplyServerSnapshot(Vector3 position, float newHealth)
    {
        serverPosition = position;
        health = newHealth;
    }
}
```

주의:

이 코드만으로는 server authority가 아니다.

이것은 client presentation code다.

MOBA에서는 실제 damage/move/cooldown 판정이 server simulation에 있어야 한다.

### 6.8 Unity Editor 확장

Unity에서 editor tool은 보통 다음 위치에 둔다.

```text
Assets/Scripts/Editor/
Packages/com.company.tool/Editor/
```

EditorWindow 예시:

```csharp
// Assets/Scripts/Editor/MobaDataValidatorWindow.cs
using UnityEditor;
using UnityEngine;

public sealed class MobaDataValidatorWindow : EditorWindow
{
    [MenuItem("WintersMOBA/Data Validator")]
    private static void Open()
    {
        GetWindow<MobaDataValidatorWindow>("MOBA Validator");
    }

    private void OnGUI()
    {
        if (GUILayout.Button("Validate Champion Data"))
        {
            ValidateChampionData();
        }
    }

    private static void ValidateChampionData()
    {
        string[] guids = AssetDatabase.FindAssets("t:ChampionData");
        foreach (string guid in guids)
        {
            string path = AssetDatabase.GUIDToAssetPath(guid);
            ChampionData data = AssetDatabase.LoadAssetAtPath<ChampionData>(path);
            if (data == null || string.IsNullOrWhiteSpace(data.championId))
            {
                Debug.LogError($"Invalid ChampionData: {path}");
            }
        }
    }
}
```

이것은 Unreal의 custom editor module이나 DataAsset validator와 대응된다.

Winters에서는 ImGui tool panel, data validator, converter/cooker와 대응된다.

### 6.9 Unity에서 Scene 만드는 과정

LoL 모작 기준 Unity Editor 작업 순서:

```text
1. Unity project 생성
2. URP 또는 HDRP 선택
3. Assets/Scenes/MobaMap.unity 생성
4. terrain/mesh/map prefab 배치
5. lane waypoint GameObject 또는 spline 배치
6. turret/nexus/minion spawner prefab 배치
7. champion prefab 생성
8. ChampionController, Animator, Collider, selection ring 연결
9. ChampionData ScriptableObject 생성
10. AbilityData ScriptableObject 생성
11. UI Canvas/HUD/minimap prefab 구성
12. VFX Graph 또는 ParticleSystem prefab 연결
13. Network/session bootstrap 배치
14. Play Mode에서 local smoke
15. headless/server build 또는 external server와 연동
```

Unity의 scene 제작은 Unreal보다 자유롭고 빠르다.

하지만 규모가 커지면 다음 문제가 생긴다.

```text
scene에 gameplay truth가 흩어진다
prefab override가 꼬인다
ScriptableObject가 mutable runtime state처럼 오용된다
client-only MonoBehaviour가 서버 판정을 대신하게 된다
```

MOBA에서는 반드시 다음을 분리해야 한다.

```text
Data asset = immutable definition
Runtime component = current state
Server simulation = authority
Client MonoBehaviour = presentation/input
```

### 6.10 Unity에서 Client / Engine / Server 분리

Unity 기본 구조는 client/server 분리를 강제하지 않는다.

그래서 LoL 모작에서는 명시적으로 이렇게 나누는 것이 좋다.

```text
Assets/Scripts/Runtime/Client
  camera
  input
  presentation
  UI
  VFX
  animation

Assets/Scripts/Runtime/Shared
  command structs
  deterministic math where possible
  data IDs
  message schemas

Assets/Scripts/Runtime/Server
  only if using headless Unity server
  authoritative match loop
  command validation
  snapshot generation

ExternalServer/
  if using separate .NET/Go/C++ server
  authoritative GameSim
```

강력 추천 구조:

```text
Client Unity
  input -> command packet
  receives snapshot/event
  visual interpolation

Authoritative Server
  command validation
  movement/damage/cooldown
  snapshot/event generation

Shared Protocol
  IDs, commands, snapshots, events
```

이 구조는 Winters와 거의 동일하다.

```text
Winters Shared/Schemas ~= Unity Shared Protocol
Winters Server/GameSim ~= Unity authoritative server
Winters Client Visual  ~= Unity client presentation scene
```

## 7. 세 엔진의 같은 개념 대응표

| 본질 | Winters | Unreal | Unity |
|---|---|---|---|
| 프로세스 | WintersLOL.exe, Server.exe, WintersEngine.dll | Editor/Game/Server targets | Editor/Player/Headless build |
| 빌드 단위 | vcxproj/lib/dll/module 경계 | Module + Target + UBT | asmdef + packages + player build |
| 객체 정체성 | EntityHandle, DefinitionKey, NetEntityId | UObject, FName, UClass, package path | GameObject instance, GUID, fileID |
| 월드 | Shared/GameSim World, Client scene | UWorld, Level | Scene |
| 배치 객체 | Client scene object, GameSim entity | AActor | GameObject |
| 컴포넌트 | GameSim component, Engine component | UActorComponent | Component/MonoBehaviour |
| 데이터 정의 | Shared/GameSim/Definitions, cooked pack | UDataAsset, DataTable, GameplayTags | ScriptableObject, JSON, Addressables |
| 에셋 | .wmesh/.wmat/.wfx 등 Winters runtime format | .uasset/.umap, cooked pak/io store | source asset + .meta + Library artifact |
| 에디터 | ImGui panels, Scene_Editor, planned tool suite | UnrealEd, LevelEditor, ContentBrowser | Unity Editor, Inspector, EditorWindow |
| FX | WFX / client visual cues | Niagara | VFX Graph / ParticleSystem |
| 애니메이션 | Client animation playback from state/cues | AnimBlueprint, Montage, Control Rig | Animator, AnimationClip, Timeline |
| 네트워크 | FlatBuffers command/snapshot/event | Actor replication, RPC, NetDriver | Netcode/custom transport/messages |
| 서버 권위 | 명시적 핵심 구조 | Gameplay Framework로 지원 | 직접 설계 필요 |
| 백엔드 | Services/docker-compose | OnlineSubsystem/external backend | UGS/external backend |

## 8. Editor의 본질

### 8.1 Editor는 "게임을 실행하는 도구"가 아니다

Editor의 본질은 다음이다.

```text
runtime state를 만들 source data를 사람이 안전하게 편집하게 하는 시스템
```

Editor가 하는 일:

```text
asset import
metadata generation
dependency tracking
scene serialization
property editing
preview
validation
cook/build
source control integration
collaboration conflict reduction
```

Unreal Editor:

```text
UObject/Actor/package ecosystem을 직접 편집
```

Unity Editor:

```text
GameObject/Component/asset GUID ecosystem을 편집
```

Winters Editor:

```text
Winters runtime resource/data contract를 편집하고 검증하는 방향으로 성장 중
```

### 8.2 Editor state와 runtime state

세 엔진 모두 editor state와 runtime state를 섞으면 망가진다.

구분:

```text
Editor state
  selection
  gizmo
  inspector expanded state
  unsaved property changes
  preview world
  import settings
  validation messages

Runtime state
  entity position
  HP
  cooldown
  animation time
  particle instances
  replicated state
  physics contacts
```

Unreal은 editor world와 PIE world를 구분한다.

Unity도 edit mode scene과 play mode runtime instance를 구분한다.

Winters도 tool/editor state와 actual GameSim truth를 분리해야 한다.

Decoy:

```text
"에디터에서 보이는 값이 곧 게임 진실이다."
```

Correct:

```text
"에디터 값은 runtime truth를 만들기 위한 source/cooked data다."
```

## 9. Asset Store / Marketplace / 협업 업데이트의 본질

### 9.1 Marketplace의 본질

Asset Store와 Marketplace의 본질은 "파일 판매"가 아니다.

본질은 다음이다.

```text
versioned content package + dependency metadata + engine compatibility + import/update workflow
```

Unreal Marketplace/Fab 계열 패키지는 보통 다음을 제공한다.

```text
.uasset
.umap
materials
meshes
animations
Blueprints
plugins
Config
Content folder layout
engine version compatibility
```

Unity Asset Store/UPM 패키지는 보통 다음을 제공한다.

```text
Assets under project
Packages/com.company.package
Runtime/
Editor/
Samples~
package.json
asmdef
prefabs
scenes
scriptable objects
```

Winters에서 대응되는 구조는 다음이다.

```text
source asset bundle
import recipe
Winters binary cooked output
manifest
validator report
runtime compatibility version
tool/editor integration
```

### 9.2 업데이트의 본질

업데이트는 단순 overwrite가 아니다.

업데이트에서 필요한 것:

```text
identity stability
dependency graph
migration
validation
conflict detection
rollback
version compatibility
```

Unity에서는 `.meta` GUID가 깨지면 참조가 깨진다.

Unreal에서는 package path/name, redirector, asset registry, cooked dependency가 중요하다.

Winters에서는 `DefinitionKey`, manifest, cooked binary version, schema compatibility가 중요하다.

협업 관점에서 중요한 규칙:

```text
1. asset identity를 안정화한다.
2. import setting도 version control에 포함한다.
3. generated/cached artifact와 source artifact를 구분한다.
4. validator를 CI나 pre-submit에 연결한다.
5. runtime에서 읽는 format은 검증된 immutable output이어야 한다.
```

## 10. 최적화의 본질

### 10.1 최적화는 "빠르게 하는 것"이 아니다

최적화의 본질:

```text
측정 가능한 병목을 제거하면서 결과의 의미를 보존하는 것
```

절대 순서:

```text
1. budget 정의
2. counter/scope 정의
3. capture
4. bottleneck 분류
5. 중복 경로 제거
6. data layout 개선
7. batch/cache/stream
8. quality tradeoff
9. regression guard
```

Winters gotcha와 직접 연결되는 규칙:

```text
normal F5 runtime에서 roster/map/minion/champion/snapshot/UI/FX를 숨겨서 숫자를 좋게 만들면 최적화가 아니다.
```

### 10.2 Niagara 최적화의 본질

Niagara 최적화의 본질:

```text
particle count, simulation stage, renderer count, material cost, overdraw, GPU/CPU sim boundary를 제어하는 것
```

Unreal Niagara에서 봐야 할 것:

```text
emitter count
spawn rate
particle lifetime
CPU sim vs GPU sim
collision query
data interface cost
material overdraw
translucency sorting
bounds
LODs / scalability
pooling
Niagara Debugger / Insights / stat Niagara
```

Winters WFX가 배워야 할 본질:

```text
FX graph는 보기 좋은 노드 편집기가 아니라, runtime budget을 예측 가능한 데이터로 만드는 도구여야 한다.
```

### 10.3 Unity VFX/Particle 최적화의 본질

Unity에서는 대응 도구가 다음이다.

```text
ParticleSystem
Visual Effect Graph
Profiler
Frame Debugger
Rendering Debugger
SRP Batcher
Addressables Analyze
```

최적화 질문은 Unreal과 같다.

```text
몇 개를 spawn하는가?
얼마나 오래 사는가?
CPU에서 도는가 GPU에서 도는가?
material pass가 몇 개인가?
transparent overdraw가 큰가?
camera 밖에서도 update하는가?
pooling하는가?
LOD/scalability가 있는가?
```

### 10.4 애니메이션 최적화의 본질

애니메이션 최적화의 본질:

```text
bone evaluation, skinning, blend graph, notify, IK, animation tick frequency, visibility, LOD를 제어하는 것
```

Unreal:

```text
AnimBlueprint
Montage
Animation Budget Allocator
Update Rate Optimization
LOD
Significance Manager
Anim Insights
Control Rig cost control
```

Unity:

```text
Animator Controller
AnimationClip compression
Avatar masks
Culling Mode
LODGroup
Optimize Game Objects
Animation Rigging cost control
Profiler Timeline
```

Winters:

```text
server snapshot/action state는 truth
client animation graph/playback는 presentation
animation state replication은 필요한 state만 보낸다
pose 전체를 매 tick truth처럼 보내지 않는다
```

## 11. LoL 모작을 세 엔진에서 만든다면

### 11.1 Winters로 만든다면

Winters에서 LoL 모작은 이미 구조상 가장 직접적이다.

흐름:

```text
Input
  Client input system

Command
  Shared/Schemas/Command.fbs
  Server command ingestion

Simulation
  Shared/GameSim/Core/World
  Components
  Systems
  Champion GameSim

Replication
  Shared/Schemas/Snapshot.fbs
  Shared/Schemas/Event.fbs
  ReplicatedEventSerializer

Presentation
  Client scene
  animation/FX/UI

Tools
  data definitions
  WFX
  editor panels
  validators

Backend
  Services/docker-compose.yml
```

가장 중요한 완성 기준:

```text
client-only smoke가 아니라 client-server authoritative smoke가 돌아야 한다.
```

### 11.2 Unreal로 만든다면

Unreal에서는 다음 경로가 가장 자연스럽다.

```text
UObject/Actor framework
  AMobaChampion
  AMobaMinion
  AMobaTurret

Server authority
  GameMode
  Server RPC
  replicated Actor state

Data
  PrimaryDataAsset
  DataTable
  GameplayTags
  GameplayAbilities

Visual
  AnimBlueprint
  Niagara
  UMG

Editor
  DataAsset editor
  map editor
  validation commandlets
```

강한 추천:

```text
처음부터 dedicated server target을 만든다.
처음부터 GameMode/GameState/PlayerState/Controller 권한 관계를 맞춘다.
처음부터 스킬 판정을 server RPC 이후 server state change로 만든다.
```

### 11.3 Unity로 만든다면

Unity에서는 다음 경로를 추천한다.

```text
Client Unity Project
  visual scene
  prefabs
  input
  camera
  UI
  VFX

Shared C# Assembly
  command/message structs
  deterministic math helpers
  definition IDs

Authoritative Server
  headless Unity or separate .NET/Go/C++ sim
  command validation
  snapshot/event output

Data
  ScriptableObject for authoring
  exported immutable runtime definitions for server
```

주의:

```text
Unity ScriptableObject를 server runtime mutable state처럼 쓰면 안 된다.
MonoBehaviour Update에서 HP/damage/cooldown truth를 결정하면 MOBA 구조가 약해진다.
```

## 12. Winters가 Unreal/Unity에서 배울 것

### 12.1 Unreal에서 배울 것

Winters가 Unreal에서 배울 핵심:

```text
1. Runtime / Editor / Developer / Programs 모듈 분리
2. build target 개념
3. reflection/metadata 기반 property editing
4. asset registry와 dependency graph
5. Content Browser
6. Details panel
7. cook/cache/build toolchain
8. Gameplay Debugger / Insights 같은 관측 도구
9. Plugin 구조
10. server target과 client/editor target의 엄격한 분리
```

하지만 그대로 따라 하면 안 되는 것:

```text
UObject 같은 대형 object model을 지금 Winters에 억지로 복제
Unreal Editor 전체 규모를 한 번에 구현
Client visual code를 GameSim truth와 섞기
```

### 12.2 Unity에서 배울 것

Winters가 Unity에서 배울 핵심:

```text
1. 빠른 iteration
2. Inspector 기반 데이터 편집
3. Prefab-like reusable object composition
4. ScriptableObject-like data authoring
5. .meta/GUID 기반 참조 안정성 개념
6. Package layout: Runtime / Editor / Tests / Samples
7. EditorWindow로 빠르게 tool surface 만들기
8. Addressables식 runtime asset group 관리
9. validation/search tooling
10. 작은 팀도 쓰기 쉬운 UX
```

하지만 그대로 따라 하면 안 되는 것:

```text
scene/prefab에 gameplay truth 흩뿌리기
MonoBehaviour Update식 권위 로직을 Server GameSim에 들여오기
source asset과 runtime cooked asset 경계 흐리기
```

## 13. Winters Editor를 실제로 키우는 방향

현재 Winters는 Unreal Editor 같은 완제품 통합 에디터 단계가 아니다.

그래서 올바른 방향은 "한 번에 Unreal Editor 복제"가 아니다.

단계는 이렇게 가야 한다.

```text
Stage 01. Runtime inspector
  entity/component/state viewer
  snapshot/event viewer
  command log

Stage 02. Asset catalog
  resource browser
  mesh/material/texture/effect preview
  missing reference detection

Stage 03. Data definition editor
  champion/skill/item/map data editing
  validation
  diff-friendly source output

Stage 04. Cooker/validator
  JSON/source asset -> Winters binary
  manifest
  schema version check
  runtime load smoke

Stage 05. Scene/map editor
  placement
  nav/pathing zones
  spawn points
  lane waypoints
  collision/vision debug

Stage 06. WFX editor
  effect graph
  cue binding
  budget preview
  pooling/scalability settings

Stage 07. Animation/sequence editor
  action state preview
  animation event/cue preview
  server event to client playback mapping

Stage 08. Collaboration layer
  lock/check-out or merge strategy
  validation report
  asset dependency graph

Stage 09. Build/package panel
  client/server/resource build
  docker service status
  smoke test runner

Stage 10. Performance lab
  frame scope
  draw/particle/animation counters
  capture comparison
```

이 10개가 Unreal 10개, Unity 10개, Winters 10개보다 더 좋은 포트폴리오 기준이다.

이유:

```text
도구 개수보다 중요한 것은 하나의 runtime truth를 관통하는 완성도다.
```

## 14. 포트폴리오 방향 검토

질문:

```text
Unreal 툴 10개, Unity 툴 10개, Winters 툴 10개 만들면 포트폴리오용으로 완성인가?
```

답:

```text
개수만으로는 완성이 아니다.
하지만 세 엔진의 같은 본질을 다른 방식으로 구현했다는 증거가 있으면 매우 강한 포트폴리오가 된다.
```

좋은 기준:

```text
Bad Portfolio:
  Unreal EditorWindow 10개
  Unity EditorWindow 10개
  Winters ImGui panel 10개
  각각은 버튼 몇 개와 로그 출력뿐

Strong Portfolio:
  같은 문제를 세 엔진에서 푼다.
  예: Champion Data Validator
      Unreal DataAsset validator
      Unity ScriptableObject validator
      Winters GameSim definition validator
  그리고 실제 runtime/cook/build/CI와 연결된다.
```

추천 포트폴리오 묶음:

```text
Vertical A. Champion Data Pipeline
  Unreal: UPrimaryDataAsset + validator + GameplayTags
  Unity: ScriptableObject + EditorWindow validator
  Winters: Definition pack + schema/cook validator

Vertical B. Map / Lane / Spawn Tool
  Unreal: .umap actor placement + lane spline editor
  Unity: scene gizmo + prefab spawn tool
  Winters: map data editor + GameSim spawn validation

Vertical C. FX Cue Pipeline
  Unreal: Niagara cue binding and budget report
  Unity: VFX/Particle prefab cue binding
  Winters: WFX cue binding + server event playback check

Vertical D. Client/Server Replay Debugger
  Unreal: replicated actor event view
  Unity: snapshot/event playback view
  Winters: FlatBuffers snapshot/event replay inspector
```

이렇게 하면 도구 수는 자연스럽게 10개 이상이 된다.

하지만 면접/포트폴리오에서 말할 핵심은 "10개 만들었다"가 아니라 이것이다.

```text
I understand the engine boundary:
  authoring data
  cooked data
  runtime state
  server authority
  client presentation
  editor validation
  build/deploy pipeline
```

## 15. 가장 중요한 결론

### 15.1 세 엔진의 본질 한 줄

```text
Winters:
  서버 권위 GameSim과 직접 소유한 엔진/툴/백엔드 경계를 통해, LoL식 runtime truth를 명시적으로 만드는 엔진.

Unreal:
  C++ source engine, UObject reflection, Actor framework, package system, integrated editor가 결합된 대형 production engine.

Unity:
  닫힌 native runtime 위에 C# component authoring, serialized scene/prefab, AssetDatabase/package workflow를 얹은 빠른 iteration engine.
```

### 15.2 가장 밑바닥의 공통 원리

공통 원리:

```text
Object identity must be stable.
Data must be separated from runtime mutable state.
Editor state must not become gameplay truth.
Client presentation must not become server authority.
Assets must pass through import/cook/validation before runtime.
Optimization must be measured against a budget.
```

### 15.3 Winters의 가장 중요한 방향

Winters가 계속 지켜야 할 핵심:

```text
Client Input -> GameCommand -> Server GameSim -> Snapshot/Event/FX Cue -> Client Visual
```

Winters가 Unreal/Unity처럼 성장하려면 필요한 것:

```text
1. Data editor
2. Asset catalog
3. Cook/validation pipeline
4. Scene/map editor
5. WFX editor
6. Animation/cue preview
7. Snapshot/event replay debugger
8. Performance profiler
9. Build/package/deploy panel
10. Collaboration/versioning workflow
```

이것이 단순히 "툴 10개"가 아니라 Winters Engine의 본질을 위로 쌓는 길이다.

## 16. 참고한 공식 문서와 로컬 근거

공식 문서:

- Unreal Modules: https://dev.epicgames.com/documentation/en-us/unreal-engine/modules
- Unreal Programming Basics: https://dev.epicgames.com/documentation/en-us/unreal-engine/programming-basics
- Unreal Networking Overview: https://dev.epicgames.com/documentation/en-us/unreal-engine/networking-overview
- Unreal Gameplay Ability System: https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-ability-system
- Unity MonoBehaviour: https://docs.unity3d.com/ScriptReference/MonoBehaviour.html
- Unity ScriptableObject: https://docs.unity3d.com/6000.2/Documentation/Manual/class-ScriptableObject.html
- Unity Components: https://docs.unity3d.com/6000.0/Documentation/Manual/Components.html
- Unity Packages: https://docs.unity3d.com/6000.0/Documentation/Manual/Packages.html
- Unity Asset Database metadata: https://docs.unity3d.com/6000.0/Documentation/Manual/asset-database-contents.html
- Unity runtime asset management: https://docs.unity3d.com/6000.0/Documentation/Manual/assets-managing-runtime.html

로컬 근거:

```text
C:\Users\user\Desktop\UnrealEngine\UnrealEngine\Engine\Build\Build.version
C:\Users\user\Desktop\UnrealEngine\UnrealEngine\Engine\Source\Runtime
C:\Users\user\Desktop\UnrealEngine\UnrealEngine\Engine\Source\Editor
C:\Users\user\Desktop\UnrealEngine\UnrealEngine\Engine\Source\Developer
C:\Users\user\Desktop\UnrealEngine\UnrealEngine\Engine\Source\Programs
C:\Users\user\Desktop\UnrealEngine\UnrealEngine\Engine\Plugins
C:\Program Files\Unity\Hub\Editor\6000.0.79f1\Editor\Data
C:\Users\user\Desktop\Winters\Engine
C:\Users\user\Desktop\Winters\Client
C:\Users\user\Desktop\Winters\Server
C:\Users\user\Desktop\Winters\Shared\GameSim
C:\Users\user\Desktop\Winters\Shared\Schemas
C:\Users\user\Desktop\Winters\Services\docker-compose.yml
```

## 17. Verification / Handoff

이 문서는 코드 변경이 아니라 architecture/source-reading guide다.

검증 기준:

```text
1. 로컬 UnrealEngine 소스 루트와 버전이 문서에 반영되어야 한다.
2. Runtime/Editor/Developer/Programs/Plugins 계층이 분리되어 설명되어야 한다.
3. Unreal LoL 모작 C++ 파일 구조와 최소 클래스 세트가 있어야 한다.
4. Unity는 로컬 설치 구조와 프로젝트 구조를 분리해서 설명해야 한다.
5. Winters는 Client/Engine/Server/Shared/GameSim/Services 경계가 깨지지 않게 설명되어야 한다.
6. Scene creation, editor state, asset/cook/update, optimization 본질이 포함되어야 한다.
```

다음 작업으로 이어간다면 추천 순서:

```text
1. Unreal LoL C++ skeleton project plan
2. Unity LoL C# skeleton project plan
3. Winters Editor 10-tool vertical slice plan
4. 세 엔진 공통 Champion Data Pipeline 포트폴리오 계획
```
