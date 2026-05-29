# Ch3. World Partition / Streaming / Open World

> Winters 현재: `Client/Public/Map/`은 stage 단위 단일 맵, `SpatialIndex.h`는 2D grid (LoL 5v5용).
> 목표: 엘든링/GTA6 스케일의 cell streaming + HLOD + DataLayer.
> 레퍼런스: `UnrealEngine/Engine/Source/Runtime/Engine/Public/WorldPartition/WorldPartition.h`, `Classes/Components/WorldPartitionStreamingSourceComponent.h`.

---

## 1. 기초 원리 — 왜 단일 ULevel이 한계인가

옛날 UE4 / 옛날 LoL 시절 월드:
- **하나의 큰 .umap (ULevel)**에 모든 actor를 박는다
- 에디터를 열면 모든 actor가 메모리에 올라온다
- 1km × 1km 넘으면 에디터가 죽고, 협업이 깨진다 (한 명이 .umap 들고 있으면 다른 사람 못 만짐)

이걸 풀려고 UE4는 **Sublevel Streaming**을 썼다 — 사람이 직접 큰 맵을 grid로 쪼개고 sublevel로 등록. 단점:
- cell 경계를 사람이 손으로 잡음 (월드 디자인이 grid에 종속됨)
- HLOD를 따로 빌드
- actor가 어느 sublevel에 속하는지 사람이 지정

UE5 **World Partition**의 해결:
- **actor 좌표 → 자동 cell 분류** (사람 손 안 탐)
- 각 actor가 **별도 파일**(External Actor) → 충돌 없는 협업
- HLOD **자동 빌드**
- **DataLayer**로 gameplay layer 토글 (낮/밤, 챕터별 활성화)
- **StreamingSource**가 어디서든 등록 가능 (플레이어, 카메라, AI agent, 네트워크 viewpoint)

핵심 원칙:
1. **actor = 데이터**. cell membership은 좌표로 자동 결정
2. **streaming source = 관심 지점**. cell은 source 근처만 메모리에 둠
3. **HLOD = LOD 계층의 cell 단위**. 멀리 있는 cell은 prebaked merged mesh
4. **DataLayer = 토글 가능한 actor set**. 같은 cell에서 챕터 진행에 따라 actor on/off

---

## 2. 핵심 — UE5 World Partition 실코드

### 2.1 UWorldPartition

`Source/Runtime/Engine/Public/WorldPartition/WorldPartition.h:62~78`:

```cpp
enum class EWorldPartitionInitState
{
    Uninitialized,
    Initializing,
    Initialized,
    Uninitializing
};

UENUM()
enum class EWorldPartitionServerStreamingMode : uint8
{
    ProjectDefault = 0 UMETA(ToolTip = "Use project default (wp.Runtime.EnableServerStreaming)"),
    Disabled = 1       UMETA(ToolTip = "Server streaming is disabled"),
    Enabled = 2        UMETA(ToolTip = "Server streaming is enabled"),
    EnabledInPIE = 3   UMETA(ToolTip = "Server streaming is only enabled in PIE"),
};
```

**관전 포인트**: server streaming도 1급 시민이다. 서버가 모든 actor를 메모리에 둘 필요 없다. 플레이어 근처만 메모리 → 같은 머신에서 더 큰 월드 가능.

### 2.2 StreamingSourceComponent

`Source/Runtime/Engine/Classes/Components/WorldPartitionStreamingSourceComponent.h:15~70`:

```cpp
UCLASS(Meta = (BlueprintSpawnableComponent))
class UWorldPartitionStreamingSourceComponent : public UActorComponent,
                                                public IWorldPartitionStreamingSourceProvider
{
    GENERATED_UCLASS_BODY()

    UFUNCTION(BlueprintCallable, Category = "Streaming")
    void EnableStreamingSource()  { bStreamingSourceEnabled = true; }

    UFUNCTION(BlueprintCallable, Category = "Streaming")
    void DisableStreamingSource() { bStreamingSourceEnabled = false; }

    // 핵심: actor → 'streaming source' 변환
    ENGINE_API virtual bool GetStreamingSource(FWorldPartitionStreamingSource& OutStreamingSource) const override;

    UFUNCTION(BlueprintCallable, Category = "Streaming")
    ENGINE_API bool IsStreamingCompleted() const;

    // Streaming source가 영향 미칠 grid 지정 (생략 시 전체)
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Streaming")
    TArray<FName> TargetGrids;
};
```

**관전 포인트**:
- 어떤 actor든 이 컴포넌트 붙이면 streaming source가 됨 (플레이어/카메라/AI/네트 viewpoint)
- `TargetGrids`로 grid 다중화 가능. 예: "Foliage" grid는 가까이만, "Quest" grid는 멀리까지.

### 2.3 데이터 흐름

```text
[Editor cook 단계]
  각 actor.uasset (External Actor) → 좌표 → grid cell 분류
  HLODBuilder가 cell마다 merged mesh / proxy 생성
  RuntimeHash (현재 UE5는 HashSet 기반)에 cell descriptor 박음

[Runtime]
  매 tick:
    streamingSources = (플레이어들 + 카메라 + AI viewpoint).Get()
    for each cell c:
      desired_state = compute(distance(c, sources), c.priority, datalayer.active)
      if desired_state != current_state:
        Load/Unload async (CookPackageContext + streaming policy)
    HLOD swap: 가까운 cell은 full actor, 먼 cell은 HLOD proxy

[네트워크]
  서버도 같은 streaming source 사용 (server streaming mode)
  플레이어 근처만 actor 활성 → replication graph가 그 안에서만 priority
```

### 2.4 ActorDescContainer

각 cell이 들고 있는 것은 **full actor가 아니라 descriptor**(이름, 좌표, bounds, class). Load 시점에 descriptor → 실제 actor instance.

```cpp
class UWorldPartitionActorDescInstance
{
    FName ActorName;
    FBox  Bounds;
    FSoftObjectPath ActorClass;
    TArray<FName> DataLayers;
    // ... 실제 component / property는 load 후에만 메모리에
};
```

→ **메모리 1 entity = ~수십 bytes**까지 축약. 10만 actor 월드도 descriptor만이면 MB 단위.

---

## 3. 심화

### 3.1 HLOD (Hierarchical LOD)

먼 cell들은 묶어서 하나의 merged mesh + atlas texture로 베이크.

```text
HLOD Layer 0: 원본 actor들
HLOD Layer 1: 1km cell 4개를 묶어 1 merged actor + atlas
HLOD Layer 2: HLOD Layer 1 묶음 4개를 또 묶음 + 더 큰 atlas
...
```

엘든링/GTA6의 "지평선에 보이는 산"은 HLOD layer N의 imposter.

UE5: `Source/Runtime/Engine/Public/WorldPartition/HLOD/HLODLayer.h`.

### 3.2 DataLayer

같은 cell에서 actor 집합을 layer로 묶음. Runtime toggle.

예: GTA6
- `DataLayer.Day` / `DataLayer.Night`
- `DataLayer.PreHeist` / `DataLayer.PostHeist`
- `DataLayer.Online` / `DataLayer.SinglePlayer`

cell 로드 시 active datalayer에 속한 actor만 spawn.

### 3.3 Streaming Generation (cook 단계)

cook 시 cell 분류 + HLOD 빌드 + dependency 검증.

UE5: `WorldPartitionStreamingGeneration.h` — actor reference graph를 분석해 cell 간 의존성 그래프 생성. 한 actor가 다른 cell의 actor를 참조하면 두 cell이 묶여서 같이 로드되어야 함.

### 3.4 Server-side Streaming

`EWorldPartitionServerStreamingMode::Enabled`로 켜면 서버도 cell streaming.
- 100인 PvP / MMO 필드 보스 / 던전 인스턴스에 필수
- 단점: replication graph가 cell-aware해야 함 (Ch7과 묶임)

### 3.5 Level Instance

같은 cell pattern을 재사용. 로아 던전이 N개 인스턴스 = 같은 LevelInstance N개 spawn.

### 3.6 Async IO

cell 로드는 IO bound. UE5는 `IoStore`(고성능 archive) + IO thread pool 사용. 디스크 → memory → GPU upload를 pipeline.

---

## 4. Winters 매핑

### 4.1 현재 한계

`Client/Public/Map/` 단일 LoL 맵. `Engine/Public/ECS/SpatialIndex.h`는 2D grid (LoL fog-of-war용).

→ 100m × 100m까지는 OK. 그 이상은 unstreaming.

### 4.2 Ch3 추가 헤더 (제안)

```cpp
// Engine/Public/World/WorldPartition.h
namespace winters
{
class WINTERS_ENGINE CWorldPartition
{
public:
    void Initialize(const WorldPartitionDesc& desc);
    void Tick(const std::vector<WorldStreamingSource>& sources);

    void RegisterStreamingSource(EntityID owner, const WorldStreamingSource& src);
    void UnregisterStreamingSource(EntityID owner);

    bool IsStreamingCompleted() const;

private:
    CRuntimeHashSet m_cells;     // grid + bvh hybrid
    CHLODManager    m_hlod;
    CDataLayerMgr   m_dataLayers;
};

struct WorldStreamingSource
{
    Vec3      position;
    f32_t     loadingRangeMeters;
    eGridName targetGrid;        // "Default" / "Foliage" / "Quest"
    f32_t     priority;
};
}
```

```cpp
// Shared/GameSim/Components/StreamingSourceComponent.h
struct StreamingSourceComponent  // ECS component
{
    f32_t     loadingRange = 100.f;
    bool_t    enabled      = true;
    eGridMask grids        = eGridMask::Default;
};
```

### 4.3 Bot AI / GameSim과의 관계 (불변식 재확인)

- **Bot AI의 viewpoint도 streaming source**가 될 수 있다. 멀리 있는 봇은 detail actor 없이 동작 (lazy actor).
- 그러나 Bot AI는 **streaming 상태를 보고 행동을 바꾸지 않는다**. server simulation이 cell-aware하면 봇의 GameCommand 후보(타겟)도 그 cell 안에서만 유효.
- `S10_BotAIStage1`은 단일 lane이라 cell 1개로 축퇴. Ch3은 그 뒤(라인 → 정글 → 다중 맵) 단계.

### 4.4 단계별

```text
Ch3-Stage1  WorldPartition cell grid (2D) + descriptor
Ch3-Stage2  StreamingSource registration (player/camera)
Ch3-Stage3  async load/unload + IO thread pool
Ch3-Stage4  HLOD layer 1 (cell merged proxy)
Ch3-Stage5  DataLayer toggle (시간대/이벤트/스토리)
Ch3-Stage6  Server-side streaming (replication graph 연동, Ch7과 묶임)
Ch3-Stage7  LevelInstance (재사용 가능한 sub-world)
Ch3-Stage8  Streaming Generation (cook 시 cell 분류, actor ref graph)
```

### 4.5 게임별 적용

| 게임 | 필요 Stage |
|------|-----------|
| LoL (현재) | Stage 0 (cell 1개로 축퇴) — 사실상 안 써도 됨 |
| 로스트아크 던전 | Stage 1, 2, 7 (LevelInstance로 던전 인스턴싱) |
| 엘든링 | Stage 1~5 + HLOD layer 2~3 |
| GTA6 | Stage 1~8 전부 + Server streaming + Multi-region |

---

## 5. 검증 명령

```powershell
# 비주얼 디버그
.\Client\Bin\Debug-DX12\WintersGame.exe --wp-debug --wp-show-cells

# 기대 로그
# [WP] partition initialized: 256 cells (8x8x4), 24 datalayers
# [WP] streaming source registered: Player_0 @ (123, 0, 456) range=100m
# [WP] cell (3,2) state: Unloaded → Loading → Loaded (84ms)
# [WP] HLOD swap: cell (7,5) Full → HLOD1 (distance=520m)
```

---

## 6. 다음 챕터로

Ch3 Stage 3까지 가야 **Ch4 Animation**의 motion DB(`Ch4-MotionMatching`)가 메모리 budget 안에 들어온다. motion DB는 GB 단위 — streaming 없이는 메모리 폭주.
