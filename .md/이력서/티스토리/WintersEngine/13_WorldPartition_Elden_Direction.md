# Winters Engine 해부기 13 - World Partition과 Elden 방향

World Partition의 본질은 "큰 맵을 불러온다"가 아니다.

Winters에서 World Partition의 본질은 다음이다.

> streaming source를 기준으로 world cell의 상태를 전환하고, 필요한 asset과 visible instance를 runtime에 공급하는 구조

## 문제 정의

LoL형 MOBA는 제한된 arena map을 기준으로 한다.

하지만 Elden형 Action RPG는 다르다.

- 넓은 공간
- streaming boundary
- world cell
- skinned character
- animation state
- lock-on camera
- boss arena
- action combat
- asset loading/unloading

이 요구사항을 LoL Client에 억지로 붙이면 두 문제가 생긴다.

1. LoL Client가 Elden 예외를 품는다.
2. Elden Client가 LoL renderer/runtime을 복사한다.

둘 다 피해야 한다.

## Winters의 접근

Winters는 Elden을 LoL 코드의 확장이 아니라 두 번째 Product Client로 본다.

즉 Elden은 다음을 검증하기 위한 방향이다.

- WintersEngine.dll이 LoL 전용이 아닌가?
- RHI renderer가 product-independent하게 재사용 가능한가?
- Asset Pipeline이 Action RPG 리소스까지 감당 가능한가?
- World Partition/Streaming이 arena map 이후의 구조를 지탱할 수 있는가?

## 코드 근거

관련 파일:

- `EngineSDK/inc/World/WorldPartitionSystem.h`
- `EngineSDK/inc/World/AssetStreamingSystem.h`
- `EngineSDK/inc/World/WorldPartitionTypes.h`
- `Tools/EldenAssetPipeline`

핵심 API:

```cpp
class WINTERS_ENGINE CWorldPartitionSystem final
{
public:
    bool_t LoadWorld(const std::string& strWorldJsonPath);
    void Unload();

    void SetSource(u32_t uSourceId, const StreamingSourceComponent& src);
    void RemoveSource(u32_t uSourceId);

    void Update(f32_t fDeltaTime);
    void CollectVisibleInstances(std::vector<VisibleInstance>& out) const;
};
```

이 API는 world를 한 번에 전부 로드하는 구조가 아니라, source와 cell state를 기준으로 runtime에 필요한 instance를 수집하는 방향이다.

## Cell state가 중요한 이유

큰 월드에서는 모든 것을 항상 visible 상태로 둘 수 없다.

cell은 대략 다음 상태를 가질 수 있다.

- unloaded
- queued
- loaded hidden
- visible

이 상태 전환은 asset streaming, visibility, memory budget과 연결된다.

관련 debug stat도 중요하다.

```cpp
struct WorldPartitionDebugStats
{
    CellStateCounts stateCounts;
    u32_t uCellCount = 0u;
    u32_t uTotalTransitions = 0u;
    u32_t uMissingRequiredAssets = 0u;
    u32_t uVisibleInstances = 0u;
};
```

이런 통계는 World Partition을 감으로 튜닝하지 않게 해준다.

## Elden을 어떻게 설명해야 하나

Elden 방향은 아직 완성 게임으로 과장하면 안 된다.

대신 이렇게 설명해야 한다.

> Winters가 LoL 전용 엔진이 되지 않도록 Action RPG 요구사항을 두 번째 제품 클라이언트로 설정했고, 이를 World Partition, Asset Streaming, skinned animation, lock-on camera, action combat slice로 분해해 검증하고 있다.

이 설명은 현재 진행 방향과 엔진 설계 의도를 솔직하게 보여준다.

## 이 글을 이력서 문장으로 압축하면

> LoL arena 구조에 갇히지 않도록 Elden형 Action RPG 요구사항을 World Partition, Asset Streaming, skinned animation, lock-on camera slice로 분해해 엔진 확장 방향을 설계했습니다.

## 다음 글

다음 글에서는 대형 리팩터링과 data migration을 어떻게 검증 루프로 닫는지 설명한다.

