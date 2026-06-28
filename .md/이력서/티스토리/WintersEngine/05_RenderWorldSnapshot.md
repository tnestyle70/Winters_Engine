# Winters Engine 해부기 5 - RenderWorldSnapshot과 공용 Scene Renderer

RenderWorldSnapshot의 본질은 renderer와 game world 사이의 계약이다.

> Client는 이번 프레임에 무엇을 그릴지 snapshot으로 만들고, Engine renderer는 그 snapshot을 backend에 맞게 그린다.

## 문제 정의

게임 클라이언트가 renderer object를 직접 만지기 시작하면 scene logic과 render submission이 엉킨다.

예를 들어 LoL Client가 champion, minion, map, projectile, FX, debug draw를 직접 renderer 호출로 제출한다고 생각해보자. 처음에는 빠르다. 하지만 Elden Client가 등장하면 같은 작업을 다시 해야 한다.

더 큰 문제는 renderer가 게임의 의미를 알기 시작한다는 것이다.

Renderer는 이 mesh가 챔피언인지, 보스인지, 포탑인지 몰라도 된다. Renderer가 알아야 하는 것은 mesh, material, texture, transform, camera, depth, tint 같은 render data다.

## 핵심 질문

Winters에서는 이 질문을 던진다.

> Product Client는 무엇을 그릴지 결정하고, Engine Renderer는 어떻게 그릴지만 담당하게 만들 수 있을까?

이 질문의 답이 `RenderWorldSnapshot`이다.

## 코드 구조

관련 파일:

- `EngineSDK/inc/Renderer/RenderWorldSnapshot.h`
- `EngineSDK/inc/Renderer/RHISceneRenderer.h`
- `EngineSDK/inc/Renderer/RHIMeshResource.h`
- `EngineSDK/inc/Renderer/RHIMaterialResource.h`

핵심 타입:

```cpp
struct RenderWorldSnapshot
{
    RenderViewDesc view{};
    std::vector<RenderMeshItem> meshes{};
    std::vector<RenderFxItem> fx{};
    std::vector<RenderDebugItem> debug{};

    void Clear()
    {
        meshes.clear();
        fx.clear();
        debug.clear();
    }
};
```

여기서 `RenderMeshItem`은 world matrix, mesh slice, texture handle, sampler, tint, depth write 같은 renderer가 필요한 데이터만 가진다.

Product Client는 자기 world를 이 구조로 번역한다.

## 구조의 장점

이 방식의 장점은 명확하다.

1. LoL과 Elden이 renderer 구현을 복사하지 않아도 된다.
2. Renderer는 게임별 의미를 몰라도 된다.
3. Debug draw와 FX도 같은 제출 구조에 얹을 수 있다.
4. RHI backend 교체 시 Product Client 영향이 줄어든다.
5. draw submission 최적화가 Engine renderer 내부 문제로 수렴한다.

## 예시로 이해하기

LoL Client 입장에서는 minion, champion, turret, projectile이 모두 다른 의미를 가진다.

하지만 renderer 입장에서는 다음과 같이 보이면 충분하다.

```text
view
mesh item 0: world matrix + mesh handle + material/texture
mesh item 1: world matrix + mesh handle + material/texture
fx item 0: world matrix + texture + tint
debug item 0: world matrix + color
```

Elden Client도 마찬가지다.

보스, 플레이어, 문, 바위, 이펙트가 Product Client에서는 다른 의미를 갖지만 renderer로 넘어갈 때는 render item으로 수렴한다.

## 면접에서 말할 포인트

이 도메인은 "렌더링을 했다"보다 한 단계 더 깊다.

면접에서는 이렇게 설명하는 것이 좋다.

> Renderer가 게임 월드의 의미를 알기 시작하면 두 번째 product client부터 복사가 시작된다. 그래서 Product Client는 render snapshot만 만들고, Engine renderer는 snapshot을 그리는 구조로 분리했다.

이 설명은 엔진 재사용성과 유지보수 관점을 동시에 보여준다.

## 이 글을 이력서 문장으로 압축하면

> Product Client가 renderer 구현을 직접 조작하지 않도록 `RenderWorldSnapshot` 기반 장면 제출 계약을 설계하고, LoL/Elden 공용 Scene Renderer 확장 방향을 마련했습니다.

## 다음 글

다음 글에서는 runtime state를 component와 system으로 나누는 ECS / Runtime Systems 도메인을 정리한다.

