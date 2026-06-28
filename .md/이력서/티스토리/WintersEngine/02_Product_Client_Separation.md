# Winters Engine 해부기 2 - Product Client Separation

Winters Engine의 첫 번째 핵심은 "하나의 엔진 위에 여러 게임 클라이언트를 올릴 수 있어야 한다"는 것이다.

이 말은 단순히 실행 파일을 여러 개 만든다는 뜻이 아니다. LoL형 MOBA와 Elden형 Action RPG처럼 장르가 다른 게임이 같은 엔진 런타임, 렌더링, 리소스, 툴 체인을 공유하되, 게임별 입력/카메라/전투/presentation은 분리되어야 한다는 뜻이다.

## 문제 정의

처음 하나의 게임만 만들 때는 Client와 Engine의 경계가 흐려져도 큰 문제가 없어 보인다.

Scene에서 renderer를 직접 만지고, UI에서 게임 manager를 직접 조회하고, resource path를 코드에 박아도 화면은 나온다. DX11 device accessor를 여기저기 전달해도 당장은 편하다.

하지만 두 번째 제품 클라이언트가 등장하면 문제가 바로 드러난다.

예를 들어 LoL Client에 맞춰 만든 renderer와 scene 구조 위에 Elden형 Action RPG를 붙인다고 생각해보면 선택지는 두 가지다.

1. LoL 코드를 복사해 Elden Client를 만든다.
2. LoL 코드에 Elden 분기를 계속 추가한다.

두 방식 모두 장기적으로 위험하다.

복사는 renderer, asset cache, UI, debug, editor가 두 갈래로 나뉘는 문제를 만든다. 분기는 하나의 Client가 모든 장르의 예외를 품는 문제를 만든다.

## 핵심 질문

Winters는 이 질문에서 출발한다.

> LoL과 Elden이 달라야 하는 부분은 무엇이고, 반드시 공유해야 하는 부분은 무엇인가?

이 질문에 답하려면 기능 목록이 아니라 책임 경계를 먼저 나누어야 한다.

## Winters의 접근

Winters에서는 Engine과 Product Client를 분리한다.

Engine이 소유하는 것:

- frame loop
- platform/window/input primitive
- RHI
- renderer
- resource/cache
- ECS primitive
- FX runtime
- world partition/streaming
- editor/runtime common service

Product Client가 소유하는 것:

- scene 구성
- input mapping
- camera behavior
- UI state
- network snapshot 적용
- animation/FX presentation
- LoL/Elden별 gameplay bridge

핵심은 Engine이 LoL이나 Elden을 직접 include하지 않는다는 점이다. Engine은 generic API와 runtime service를 제공한다. Product Client는 자신의 world state를 Engine이 이해할 수 있는 데이터로 변환해 넘긴다.

## 구조로 보면

```text
WintersEngine.dll
├─ WintersLOL.exe
│  ├─ MOBA input
│  ├─ MOBA camera
│  ├─ champion presentation
│  └─ snapshot visual bridge
│
└─ WintersElden.exe
   ├─ action RPG input
   ├─ lock-on camera
   ├─ skinned character slice
   └─ world streaming slice
```

이 구조에서 Engine은 "LoL 챔피언"이나 "Elden 보스"를 몰라도 된다. 대신 mesh, material, transform, render item, resource handle, streaming cell 같은 generic concept를 다룬다.

## 실제 코드 근거

관련 경로:

- `EngineSDK/inc/WintersEngine.h`
- `EngineSDK/inc/GameInstance.h`
- `EngineSDK/inc/Renderer/RenderWorldSnapshot.h`
- `EngineSDK/inc/World/WorldPartitionSystem.h`
- `.md/architecture/WINTERS_CODEBASE_COMPASS.md`

`RenderWorldSnapshot`은 Product Client 분리의 좋은 예시다.

Client는 자기 게임 월드를 renderer가 이해할 수 있는 snapshot으로 변환한다.

```cpp
struct RenderWorldSnapshot
{
    RenderViewDesc view{};
    std::vector<RenderMeshItem> meshes{};
    std::vector<RenderFxItem> fx{};
    std::vector<RenderDebugItem> debug{};
};
```

Renderer는 이 snapshot이 LoL에서 왔는지 Elden에서 왔는지 몰라도 된다.

## 왜 면접에서 중요한가

이 도메인은 단순히 "게임 두 개를 만들 예정입니다"가 아니다.

면접관이 보고 싶은 것은 "두 번째 제품이 등장했을 때 기존 구조가 왜 무너지는지 알고 있는가"다. Product Client Separation은 그 문제를 미리 정의하고 경계로 푼 증거가 된다.

## 이 글을 이력서 문장으로 압축하면

> LoL형 MOBA와 Elden형 Action RPG를 같은 C++ 런타임 위에 분리해 올리기 위해 Engine/Product Client 경계를 설계하고, 게임별 scene/input/presentation과 공용 renderer/resource/runtime을 분리했습니다.

## 다음 글

다음 글에서는 Product Client를 지탱하는 공용 실행 기반, Engine Runtime의 역할을 정리한다.

