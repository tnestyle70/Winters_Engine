# Winters Engine 해부기 11 - FX, UI, Debug Observability

FX와 Debug의 본질은 "보기 좋게 만든다"가 아니다.

Winters에서 이 도메인의 본질은 다음이다.

> 복잡한 gameplay/render/network 문제를 관측 가능한 상태로 만들어 추측 대신 증거로 고치게 하는 것

## 문제 정의

게임 개발에서는 "뭔가 이상하다"가 자주 발생한다.

예를 들어 이런 상황이다.

- 스킬이 맞은 것 같은데 피해가 들어가지 않는다.
- AI가 왜 갑자기 뒤로 빠졌는지 모르겠다.
- FX가 두 번 재생된다.
- Client에서는 맞은 것처럼 보이는데 Server에서는 빗나갔다.
- visual timing과 gameplay action lock이 어긋난다.
- pathfinding이 어느 지점에서 막히는지 보이지 않는다.

이런 문제를 화면만 보고 고치면 감으로 튜닝하게 된다.

그래서 Winters에서는 Debug와 FX를 단순 부가 기능이 아니라 관측성 도메인으로 본다.

## Winters의 접근

Winters는 다음 계층으로 관측성을 만든다.

- Debug overlay
- AI debug panel
- Debug draw
- bounded trace
- server/client debug output
- FX cue
- FX graph

관련 파일:

- `EngineSDK/inc/FX/Graph/FxGraph.h`
- `EngineSDK/inc/FX/Exec/FxExecPlan.h`
- `EngineSDK/inc/Renderer/RHIFxSpriteRenderer.h`
- `Client/Private/UI/AIDebugPanel.cpp`
- `Client/Private/UI/DebugDrawSystem.cpp`
- `Shared/GameSim/Components/ChampionAIComponent.h`

## FX Graph

FX Graph는 effect를 코드에만 묶지 않고 데이터 구조로 다루기 위한 기반이다.

핵심 타입:

```cpp
struct CFxGraph
{
    std::vector<FxGraphUserParam> userParams;
    std::vector<FxEmitterGraph> emitterGraphs;

    static bool_t LoadFromJson(
        const std::string& strPath,
        CFxGraph& outGraph,
        std::string* pOutError = nullptr);
};
```

FX node는 spawn, init, update, render 단계로 분류된다.

```cpp
enum class eFxNodeStage : u8_t
{
    Spawn,
    Init,
    Update,
    Render,
};
```

이 구조는 나중에 editor와 연결하기 좋다. FX를 C++ 코드에만 박아두면 designer workflow가 막히지만, graph 구조를 두면 authoring과 runtime execution을 나눌 수 있다.

## AI Debug

AI는 결과만 보면 의도를 알기 어렵다.

따라서 AI debug는 다음 질문에 답해야 한다.

- 어떤 target을 봤는가?
- 어떤 action을 선택하려 했는가?
- 왜 skill을 쓰지 못했는가?
- range 문제인가, cooldown 문제인가, action lock 문제인가?
- forced debug command가 적용되었는가?

이런 정보를 UI panel이나 debug draw로 볼 수 있어야 AI 튜닝이 가능하다.

## Server Authority와 FX

FX는 visual이지만 gameplay result와 분리해서 생각하면 안 된다.

스킬이 맞았을 때 FX가 재생되어야 한다면, Client가 독자적으로 맞았다고 판단해서 재생하면 위험하다. Server result가 event나 cue로 내려오고, Client visual path에서 한 번 재생되어야 한다.

그래야 다음 문제가 줄어든다.

- hit FX 중복 재생
- Client-local false positive
- skill miss인데 hit FX 발생
- replay/debug 불일치

## 면접에서 말할 포인트

Debug UI나 FX Graph를 "편의 기능"처럼 말하면 약하다.

더 좋은 설명은 이것이다.

> AI, skill, FX, network는 눈에 보이는 결과와 실제 server truth가 다를 수 있기 때문에, debug overlay와 trace, server-driven FX cue로 관측 가능한 파이프라인을 만들었다.

## 이 글을 이력서 문장으로 압축하면

> AI/스킬/FX/네트워크 문제를 재현 가능하게 분석하기 위해 debug overlay, bounded trace, server-driven FX cue, graph 기반 FX 파이프라인을 구축했습니다.

## 다음 글

다음 글에서는 모델, 텍스처, 애니메이션, 맵 리소스를 runtime binary format으로 정리하는 Asset Pipeline을 설명한다.

