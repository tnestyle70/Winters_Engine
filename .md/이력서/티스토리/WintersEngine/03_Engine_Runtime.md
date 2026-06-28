# Winters Engine 해부기 3 - Engine Runtime

Engine Runtime은 게임 로직을 많이 담는 곳이 아니다.

Winters에서 Engine Runtime의 본질은 다음이다.

> 게임의 의미를 몰라도 프레임을 돌리고, 리소스를 관리하고, 렌더링과 UI와 에디터 서비스를 제공하는 공용 실행 기반

## 문제 정의

게임 클라이언트 코드가 커지면 계속 같은 질문이 나온다.

> 이 기능은 Engine에 있어야 하나, Client에 있어야 하나?

이 질문을 대충 넘기면 Engine은 금방 특정 게임 전용 코드가 된다.

예를 들어 UI texture loading이 필요하다고 해서 Client가 DX11 SRV를 직접 만들기 시작하면 Client public API가 DX11에 묶인다. AI debug panel이 필요하다고 해서 Engine UI가 GameSim component를 직접 조회하면 Engine은 product gameplay를 알아야 한다.

그 순간 Engine은 공용 엔진이 아니라 LoL 전용 런타임이 된다.

## Winters의 기준

Winters에서는 Engine이 generic primitive와 service를 제공하고, Client가 product-specific state를 만든다.

Engine에 둘 수 있는 것:

- window, input primitive, frame loop
- RHI와 renderer
- resource cache
- ECS primitive
- world partition
- asset streaming
- FX graph/runtime
- UI rendering substrate
- profiler/debug output infrastructure

Client에 둬야 하는 것:

- 이 UI 패널에 어떤 데이터를 보여줄지
- 이 entity가 어떤 챔피언인지
- 서버 snapshot을 어떤 animation/FX로 보여줄지
- LoL과 Elden의 scene/camera/input 차이

## 코드 구조

Engine public 도메인은 다음 폴더들에 드러난다.

```text
Engine/Public/Core
Engine/Public/Platform
Engine/Public/Renderer
Engine/Public/RHI
Engine/Public/Resource
Engine/Public/World
Engine/Public/FX
Engine/Public/ECS
EngineSDK/inc
```

여기서 `EngineSDK/inc`는 중요하다.

Engine public header가 Client에서 소비되는 형태로 배포되는 경계이기 때문이다. 즉 Engine public API를 바꾸면 단순히 Engine만 빌드하는 것으로 끝나지 않는다. SDK sync, Client build, Server build, tool build까지 영향을 확인해야 한다.

## 의존 방향

Winters의 Engine Runtime 원칙은 다음과 같다.

```text
Engine
  owns generic runtime/render/resource primitives

Client
  consumes Engine and Shared
  owns product presentation

Shared/GameSim
  owns deterministic gameplay contract
  does not include Engine/Renderer/UI/DX types
```

이 기준 덕분에 Engine은 LoL champion이나 Elden boss를 몰라도 된다. 대신 transform, render item, resource handle, RHI descriptor, UI draw substrate 같은 generic concept를 제공한다.

## 실제 규칙

프로젝트 규칙에는 다음 방향이 명시되어 있다.

- Engine은 LoL Client나 Server product code를 include하지 않는다.
- Engine UI panel은 Product manager나 GameSim component를 직접 조회하지 않는다.
- 필요한 데이터는 Client가 view state로 만들어 전달한다.
- Engine public header 변경은 `EngineSDK/inc` 동기화와 함께 검증한다.

관련 문서:

- `AGENTS.md`
- `.md/architecture/WINTERS_CODEBASE_COMPASS.md`
- `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md`

## 왜 면접에서 중요한가

Engine Runtime 도메인은 "C++로 엔진 클래스를 많이 만들었다"보다 더 중요한 것을 보여준다.

그것은 public API와 의존 방향의 비용을 알고 있다는 점이다.

실무에서도 공용 모듈은 기능보다 경계가 더 중요하다. 공용 모듈에 제품별 예외가 들어가기 시작하면 재사용성은 빠르게 무너진다.

## 이 글을 이력서 문장으로 압축하면

> Engine public API와 Product Client 의존 방향을 분리하고, SDK sync/build 검증을 고려한 C++ 공용 런타임 경계를 설계했습니다.

## 다음 글

다음 글에서는 DX11 고정 구조를 어떻게 RHI 경계로 분리했는지 설명한다.

