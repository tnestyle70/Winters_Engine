# Winters Engine 해부기 4 - DX11에서 RHI로

RHI의 본질은 "DX11을 숨기는 추상화"가 아니다.

Winters에서 RHI는 다음 문제를 해결하기 위한 경계다.

> 렌더링 backend의 선택이 Product Client와 gameplay 구조를 오염시키지 않게 막는 것

## 문제 정의

DX11로 빠르게 구현하면 처음에는 편하다.

`ID3D11Device`, `ID3D11ShaderResourceView`, `DX11Pipeline` 같은 concrete type을 바로 넘기면 원하는 화면을 빨리 볼 수 있다.

하지만 이 방식은 시간이 지날수록 비용이 커진다.

- Client public header가 DX11 타입에 묶인다.
- UI와 resource loading이 backend 교체를 방해한다.
- DX12/Vulkan/console 방향으로 확장하기 어렵다.
- Elden Client가 LoL DX11 renderer 구현을 복사해야 한다.
- editor tool과 runtime도 DX11 context에 강하게 묶인다.

이 문제는 "나중에 바꾸자"로 해결하기 어렵다. concrete type이 public API에 퍼진 뒤에는 바꾸는 순간 전체 모듈이 흔들린다.

## Winters의 접근

Winters는 RHI 경계를 세워 renderer backend와 product code를 분리한다.

핵심 개념:

- `IRHIDevice`
- `IRHICommandList`
- `IRHIQueue`
- `IRHISwapChain`
- RHI handle
- RHI descriptor
- pipeline state
- render pass
- bind group

관련 파일:

- `EngineSDK/inc/RHI/IRHIDevice.h`
- `EngineSDK/inc/RHI/IRHICommandList.h`
- `EngineSDK/inc/RHI/RHIHandles.h`
- `EngineSDK/inc/RHI/RHIDescriptors.h`

핵심 인터페이스는 다음과 같은 형태다.

```cpp
class WINTERS_ENGINE IRHIDevice
{
public:
    virtual ~IRHIDevice() = default;

    virtual eRHIBackend GetBackend() const = 0;
    virtual void* GetNativeHandle(eNativeHandleType type) const = 0;

    virtual void BeginFrame(f32_t r = 0.0f,
                            f32_t g = 0.0f,
                            f32_t b = 0.0f,
                            f32_t a = 1.0f) = 0;
    virtual void EndFrame() = 0;

    virtual IRHICommandList* CreateCommandList() { return nullptr; }
    virtual RHIPipelineHandle CreatePipeline(const RHIPipelineDesc& desc) = 0;
    virtual RHIRenderPassHandle CreateRenderPass(const RHIRenderPassDesc& desc) = 0;
    virtual RHIBindGroupHandle CreateBindGroup(const RHIBindGroupDesc& desc) = 0;
};
```

## 중요한 점

RHI는 공짜가 아니다.

DX11을 바로 쓰면 빠르게 구현할 수 있는 부분도 handle과 descriptor로 감싸야 한다. backend별 기능 차이도 다뤄야 한다. 추상화를 잘못 잡으면 오히려 복잡도만 늘어난다.

그럼에도 Winters에서 RHI가 필요한 이유는 명확하다.

LoL과 Elden이 같은 renderer 계층을 공유해야 하고, Client public API가 DX11에 묶이면 장기 확장이 막히기 때문이다.

## 문제를 어떻게 나눴나

RHI는 "모든 backend를 완벽히 지원한다"가 첫 목표가 아니다.

초기 목표는 다음이다.

1. DX11 경로를 유지한다.
2. public API에서 DX11 concrete type 누수를 줄인다.
3. renderer가 handle/descriptor 기반으로 자원을 다루게 한다.
4. RenderWorldSnapshot과 연결해 Product Client가 backend를 몰라도 되게 한다.
5. DX12/Elden 방향을 위한 경계만 먼저 만든다.

이렇게 하면 현재 동작하는 DX11 client를 버리지 않으면서도 미래의 backend 확장 가능성을 확보할 수 있다.

## 면접에서 말할 포인트

이 글의 핵심은 "RHI를 만들었다"가 아니다.

더 중요한 설명은 이것이다.

> DX11 concrete dependency가 Client/Public API까지 퍼지는 것을 구조적 문제로 보고, backend-neutral renderer로 이동할 수 있는 경계를 설계했다.

이 말은 단순 렌더링 기능보다 실무적인 설계 감각을 보여준다.

## 이 글을 이력서 문장으로 압축하면

> DX11 concrete type이 Client/Public API를 오염시키는 문제를 정의하고, `IRHIDevice`와 RHI handle/descriptor 기반 backend-neutral 렌더링 경계를 설계했습니다.

## 다음 글

다음 글에서는 Product Client와 renderer 사이를 `RenderWorldSnapshot`으로 분리한 이유를 설명한다.

