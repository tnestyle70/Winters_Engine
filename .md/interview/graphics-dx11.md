# 그래픽스 & DirectX 11 — 기술면접 대비

> 대상: 자체 DX11 엔진(Winters) 제작 경험이 있는 게임 클라이언트/서버 프로그래머 지망자.
> 이 문서의 모든 "내 프로젝트" 근거는 실제 코드(`Engine/Private/RHI/DX11/CDX11Device.cpp`, `Engine/Private/Renderer/ModelRenderer.cpp`, `Engine/Private/RHI/RHITextureLoader.cpp`, `Engine/Private/RHI/DX11/DX11Shader.cpp`, `Engine/Private/RHI/DX11/DX11ConstantBuffer.h`)와 프로젝트 세션 기록에서 확인한 사실만 인용했다.

## 출제 경향 개요

국내 게임사 클라이언트 기술면접에서 그래픽스/DX11은 다음 패턴으로 출제된다.

1. **파이프라인 암기 확인 → 즉시 심화**: "파이프라인 스테이지를 말해보세요"는 시작일 뿐이고, 진짜 질문은 "깊이 테스트는 어느 스테이지에서?", "clip()을 쓰면 성능에 무슨 일이 생기나?" 같은 꼬리질문이다.
2. **리소스 관리 실무 감각**: USAGE 플래그 선택, Map vs UpdateSubresource, 상수버퍼 갱신 전략은 "실제로 렌더러를 짜봤는가"를 가르는 단골 문제다.
3. **비용 모델 이해**: 드로우콜이 왜 비싼지, 배칭/인스턴싱을 언제 쓰는지, 프레임 병목을 어떻게 계측하는지. "몇 ms에서 몇 ms로 줄였고 병목이 무엇이었다"를 수치로 말할 수 있으면 강력하다.
4. **디버깅 경험**: "화면에 안 나오면 어떻게 디버깅하나?"는 거의 모든 회사가 묻는다. RenderDoc 기반의 체계적 절차 + 실제로 잡아본 버그 사례가 정답이다.
5. **API 진화 맥락**: DX11 → DX12/Vulkan 차이(명시적 동기화, PSO, 커맨드리스트, 멀티스레딩)를 묻고, RHI 추상화 설계 경험이 있으면 크게 어필된다.

자체 엔진 제작자는 "책으로 아는 것"과 "겪어본 것"을 구분해서 답해야 한다. 이 문서는 겪어본 것(Z-fighting, 17.8ms→9ms, UV-alpha 함정, 본 팔레트 SRV 전환)을 개념에 붙여서 정리한다.

---

## 핵심 개념 정리

### 1. DX11 렌더링 파이프라인 전 스테이지

**정의**: DX11 그래픽스 파이프라인은 IA → VS → (HS → Tessellator → DS) → GS → (SO) → RS → PS → OM 순서로 정점 데이터를 픽셀로 변환하는 고정+프로그래머블 혼합 파이프라인이다. Compute Shader(CS)는 이 파이프라인 밖에서 `Dispatch()`로 독립 실행된다.

| 스테이지 | 성격 | 역할 |
|---|---|---|
| **IA (Input Assembler)** | 고정 | 바인딩된 정점/인덱스 버퍼에서 정점을 읽어 프리미티브(삼각형 등)로 조립. 입력 레이아웃으로 메모리 바이트 → 시맨틱 매핑, 토폴로지(TriangleList 등) 결정 |
| **VS (Vertex Shader)** | 프로그래머블 | 정점 단위 변환. 보통 World×View×Proj로 클립 공간 좌표 출력(`SV_Position`). 스키닝(본 팔레트 블렌딩)도 여기서 |
| **HS (Hull Shader)** | 프로그래머블 | 테셀레이션 1단계. 패치 제어점 처리 + 테셀레이션 계수(얼마나 쪼갤지) 출력 |
| **Tessellator** | 고정 | HS가 준 계수대로 도메인을 세분화해 새 정점 위상 생성 |
| **DS (Domain Shader)** | 프로그래머블 | 테셀레이터가 만든 각 점의 (u,v) 좌표에서 실제 정점 위치 평가(변위 매핑 등) |
| **GS (Geometry Shader)** | 프로그래머블 | 프리미티브 단위 입출력. 프리미티브를 증폭/제거 가능(포인트→빌보드 쿼드 확장 등). 출력 순서 보장 때문에 하드웨어에서 느린 편이라 실무에선 인스턴싱/CS로 대체하는 추세 |
| **SO (Stream Output)** | 고정 | GS(또는 VS) 출력을 래스터라이즈하지 않고 버퍼로 다시 기록. GPU 파티클의 조상 격 기법 |
| **RS (Rasterizer)** | 고정 | 클리핑 → 원근 나눗셈(w-divide) → 뷰포트 변환 → 후면 컬링 → 삼각형을 픽셀(프래그먼트)로 분해하고 정점 속성을 원근 보정 보간 |
| **PS (Pixel Shader)** | 프로그래머블 | 픽셀 단위 색 계산. 텍스처 샘플링, 라이팅, `clip()/discard` 가능 |
| **OM (Output Merger)** | 고정(상태 설정 가능) | 깊이/스텐실 테스트, 블렌딩을 수행하고 최종 색을 렌더타겟에 기록 |

**동작 원리에서 면접용 핵심 뉘앙스**:
- 논리적으로 깊이 테스트는 OM(PS 뒤)이지만, 실제 하드웨어는 **Early-Z**로 PS 실행 전에 깊이를 기각한다. 단 PS가 `SV_Depth`를 쓰거나 `clip()/discard`를 쓰면 Early-Z가 제한된다(깊이 기록 관점의 보수적 처리). → 알파 테스트가 많은 식생/FX가 비싼 이유.
- RS의 보간은 원근 보정(perspective-correct) 보간이다. `noperspective` 지정자로 끌 수도 있다.
- HS/DS/GS/SO는 안 쓰면 그냥 통과(비활성)다. Winters도 VS+PS만 사용한다.

**예시 (Winters)**: `Engine/Private/RHI/DX11/CDX11Device.cpp`의 `CDX11PipelineState::Apply()`가 한 드로우에 필요한 스테이지 상태를 한 번에 세팅한다 — `IASetInputLayout`/`IASetPrimitiveTopology`(IA), `VSSetShader`/`PSSetShader`(VS/PS), `RSSetState`(RS), `OMSetDepthStencilState`/`OMSetBlendState`(OM). 즉 "파이프라인 스테이지 = 상태 객체 세트"라는 사실이 코드 구조에 그대로 반영돼 있다.

**게임 개발 맥락**: 파이프라인 이해는 곧 "버그가 어느 스테이지에서 죽는지" 이분탐색의 지도다. Winters에서 "메쉬가 호출은 되는데 안 보임" 버그는 IA/VS가 아니라 PS의 `clip()`에서 픽셀이 전부 버려진 사례였다(아래 §12).

### 2. Device vs DeviceContext (immediate / deferred)

**정의**:
- `ID3D11Device` = **리소스 생성** 담당 (버퍼/텍스처/셰이더/상태 객체 Create*). **free-threaded** — 여러 스레드에서 동시에 호출해도 안전.
- `ID3D11DeviceContext` = **렌더링 명령 발행** 담당 (Set*/Draw*/Map/ClearRTV). **스레드 안전하지 않음** — 한 컨텍스트는 한 스레드에서만.

**Immediate vs Deferred**:
- Immediate context는 명령이 드라이버 커맨드 버퍼에 바로 쌓여 GPU로 흘러가는 단일 컨텍스트.
- Deferred context는 다른 스레드에서 명령을 `ID3D11CommandList`로 녹화한 뒤 immediate context에서 `ExecuteCommandList`로 재생. 다만 DX11에서는 드라이버가 결국 명령을 직렬화·재검증하는 경우가 많아 DX12 커맨드리스트만큼 병렬 이득이 크지 않다(드라이버 의존적). 이게 DX12가 명시적 커맨드리스트로 재설계된 이유 중 하나.

**예시 (Winters)**: `CDX11Device.cpp`의 `CDX11FrameCommandList`는 immediate context를 `IRHICommandList` 인터페이스로 감싼다. 주석 그대로 "DX11 immediate context를 IRHICommandList로 감싼다. Begin/End/RenderPass는 즉시 모드라 no-op". 즉 **RHI 인터페이스는 커맨드리스트 모양(DX12 친화)으로 설계하고, DX11 백엔드에서는 즉시 실행으로 구현**하는 전형적 전략이다.

**게임 개발 맥락**: 로딩 스레드에서 텍스처/버퍼 생성(Device)은 안전하지만, 그 리소스를 그리는 것(Context)은 렌더 스레드에서만 해야 한다. Winters의 리소스 로딩도 이 규칙을 따른다.

### 3. 리소스와 뷰 (Buffer/Texture, SRV/RTV/DSV/UAV)

**정의**: DX11 리소스는 메모리 덩어리(Buffer, Texture1D/2D/3D)이고, **뷰(View)는 그 메모리를 "어떤 용도·어떤 포맷으로 해석할지" 선언하는 객체**다.
- **SRV** (Shader Resource View): 셰이더에서 읽기 (`t#` 레지스터).
- **RTV** (Render Target View): OM이 색을 쓰는 대상.
- **DSV** (Depth Stencil View): OM이 깊이/스텐실을 읽고 쓰는 대상.
- **UAV** (Unordered Access View): 셰이더에서 임의 위치 읽기/쓰기 (`u#`, 주로 CS).

**왜 뷰가 필요한가**: 같은 텍스처를 패스 A에서는 DSV(깊이 기록), 패스 B에서는 SRV(그림자 샘플링)로 쓰려면 **typeless 포맷**으로 만들고 뷰마다 다른 해석을 붙인다. 예: 텍스처는 `R24G8_TYPELESS`, DSV는 `D24_UNORM_S8_UINT`, SRV는 `R24_UNORM_X8_TYPELESS`. 또한 같은 서브리소스를 RTV와 SRV로 **동시에** 바인딩하는 것은 해저드라 런타임이 SRV를 강제로 언바인드한다(포스트프로세싱 핑퐁이 필요한 이유).

**예시 (Winters)**: `CDX11Device.cpp`의 `ToDXGIFormat()`에 `R24G8_Typeless` / `D24_UNorm_S8_UInt` / `R24_UNorm_X8_Typeless` 3종이 나란히 정의돼 있다 — 깊이 텍스처를 SRV로 재해석하는 (그림자/깊이 리드백) 조합을 RHI 포맷 레벨에서 준비해둔 것. 백버퍼는 `CreateRenderTarget()`에서 스왑체인 버퍼 → RTV, 깊이는 `CreateDepthStencil()`에서 `D24_UNORM_S8_UINT` 텍스처 → DSV로 만든다. 스키닝 본 팔레트는 `ModelRenderer.cpp`의 `BoneMatrixSRVBuffer`처럼 **구조화 버퍼(MiscFlags=BUFFER_STRUCTURED) + SRV** 조합이다.

**USAGE 4종과 CPU/GPU 접근 매트릭스**:

| USAGE | GPU 읽기 | GPU 쓰기 | CPU 접근 | 갱신 수단 | 용도 |
|---|---|---|---|---|---|
| `DEFAULT` | O | O | X | `UpdateSubresource`, `CopyResource` | 정적 메시, RT/DS |
| `IMMUTABLE` | O | X | X | 생성 시 초기 데이터만 | 절대 안 바뀌는 리소스 |
| `DYNAMIC` | O | X | 쓰기(Map) | `Map(WRITE_DISCARD/NO_OVERWRITE)` | 매 프레임 갱신(상수버퍼, 본 팔레트) |
| `STAGING` | X | X | 읽기/쓰기 | `CopyResource` 후 `Map` 읽기 | GPU→CPU 리드백 |

**Map vs UpdateSubresource 트레이드오프**:
- `Map(WRITE_DISCARD)`: 드라이버가 **버퍼 리네이밍**(새 메모리 할당 후 교체)을 해줘서 GPU가 이전 프레임 데이터를 쓰는 중이어도 CPU가 스톨 없이 쓴다. 매 프레임 전체 갱신에 최적. 단 전체를 다시 써야 함(이전 내용 무효).
- `Map(WRITE_NO_OVERWRITE)`: GPU가 아직 안 읽은 영역에만 이어쓰기(동적 VB에 스프라이트 append 등). 규약을 어기면 레이스.
- `UpdateSubresource`: DEFAULT 리소스용. 드라이버가 내부 스테이징에 복사 후 GPU 타이밍에 맞춰 반영 — 복사가 한 번 더 있고, 크거나 빈번한 갱신엔 불리. 드물게 갱신되는 DEFAULT 리소스에 적합.

**예시 (Winters)**: `CDX11FrameCommandList::UpdateBuffer()`가 이 분기를 그대로 구현한다 — `pBuffer->dynamic`이면 `Map(WRITE_DISCARD)`+`memcpy`+`Unmap`, 아니면 `UpdateSubresource`. 또 `CDX11Device::CreateBuffer()`는 상수버퍼 크기를 `(ByteWidth + 15u) & ~15u`로 16바이트 정렬 올림한다(D3D11 상수버퍼 크기 규약).

**게임 개발 맥락**: "매 프레임 바뀌는가?"가 USAGE 선택의 전부다. Winters에서 상수버퍼·본 팔레트는 DYNAMIC+Map, 메시 정점/인덱스는 DEFAULT(생성 시 초기 데이터), GPU 타이밍 쿼리 결과 회수는 사실상 리드백 계열(쿼리 GetData)이다.

### 4. 상수버퍼 갱신 전략 (per-frame / per-object 분리)

**정의**: 상수버퍼(cbuffer)는 셰이더 상수 블록. **갱신 빈도별로 버퍼를 분리**하는 것이 표준 전략이다 — 프레임당 1회(뷰·프로젝션, 라이트), 오브젝트당 1회(월드 행렬, 머티리얼), 드로우 서브패스당(FX 파라미터 등).

**왜**: 하나의 거대 cbuffer에 다 넣으면 카메라만 바뀌어도 전체를 다시 업로드해야 하고, 오브젝트 수만큼 중복 업로드가 발생한다. 분리하면 업로드 바이트와 바인딩 횟수가 최소화된다.

**규칙(HLSL 패킹)**: cbuffer는 16바이트(float4) 단위 패킹. 멤버가 16바이트 경계를 넘으면 다음 레지스터로 밀린다. C++ 구조체와 HLSL cbuffer의 레이아웃 불일치는 "값이 이상하게 들어오는" 클래식 버그 — C++ 쪽에 패딩을 명시하고 `static_assert`로 방어한다.

**예시 (Winters)**:
- `Engine/Private/RHI/DX11/DX11ConstantBuffer.h`: `template<typename T> class DX11ConstantBuffer`가 `static_assert(sizeof(T) % 16 == 0, "Constant buffer size must be 16-byte aligned")`로 컴파일 타임 방어. 생성은 `D3D11_USAGE_DYNAMIC + D3D11_CPU_ACCESS_WRITE`, 갱신은 `Map(WRITE_DISCARD)`.
- `Engine/Private/Renderer/ModelRenderer.cpp`: `DX11ConstantBuffer<CBPerFrame> cbPerFrame` / `DX11ConstantBuffer<CBPerObject> cbPerObject`로 분리. `UpdateCamera()`가 viewProjection·카메라 위치·라이트를 CBPerFrame에, `UpdateTransform()`이 월드 행렬·`worldInvTranspose`·머티리얼 오버라이드를 CBPerObject에 쓴다. 바인딩 슬롯은 b0(per-frame), b1(per-object)로 고정 (`cbPerFrame.Bind(pContext, 0)` / `cbPerObject.Bind(pContext, 1)`). FX 파라미터는 별도 `CBFxParams(b2)` (2026-04-26 세션 기록).
- 주석에도 설계 의도가 남아 있다: "인스턴스별 상수 버퍼 (데이터가 프레임/오브젝트마다 다르므로 인스턴스별)" — 셰이더/파이프라인은 CEngineApp가 소유하고 공유, CB만 인스턴스별.

**게임 개발 맥락**: 이 분리는 DX12의 루트 시그니처/디스크립터 분할, 언리얼의 View/Primitive UniformBuffer 분리와 같은 사상이다. "갱신 빈도별 데이터 분리"는 API를 넘어서는 렌더러 설계 원칙.

### 5. 상태 객체 (Rasterizer / Blend / DepthStencil)

**정의**: DX11은 파이프라인 고정 기능 설정을 **불변(immutable) 상태 객체**로 미리 구워서 바인딩한다.
- `ID3D11RasterizerState`: FillMode(솔리드/와이어), CullMode(None/Front/Back), 와인딩 방향, DepthClip, 깊이 바이어스(그림자용), 시저.
- `ID3D11DepthStencilState`: 깊이 테스트 on/off, DepthFunc(LESS 등), **DepthWriteMask**(테스트는 하되 기록 안 함 — 반투명용), 스텐실 연산.
- `ID3D11BlendState`: SrcBlend/DestBlend/BlendOp (RGB와 알파 채널 분리 설정 가능), RT별 독립 블렌드, 쓰기 마스크.

**왜 객체인가**: 생성 시점에 드라이버가 검증·최적화를 끝내므로 런타임 바인딩이 싸다. 상태를 낱개 레지스터로 바꾸던 DX9의 드라이버 오버헤드를 줄인 설계고, DX12 PSO(전체 파이프라인을 통짜로 굽기)로 가는 중간 단계다.

**예시 (Winters)**: `CDX11PipelineState::InitializeNative()` (`CDX11Device.cpp`)가 desc 하나로 3종 상태를 한꺼번에 굽는다:
- Rasterizer: `FILL_SOLID`, 컬모드 변환(`ToDX11CullMode`), `DepthClipEnable=TRUE`.
- DepthStencil: `DepthEnable = (dsvFormat != Unknown)`, `DepthWriteMask = depthWrite ? ALL : ZERO`, `DepthFunc = ToDX11ComparisonFunc(depthOp)`.
- Blend: 3프리셋 — Opaque(`ONE/ZERO`), AlphaBlend(`SRC_ALPHA / INV_SRC_ALPHA`), Additive(`SRC_ALPHA / ONE`).
FX 렌더러는 반투명을 위해 깊이 기록 없는 DSS(`pDSSNoWrite`)와 블렌드 캐시(`pBlendCache`)를 따로 들고 드로우 전후 백업·복원한다(2026-04-26 세션 기록, `CFxStaticMeshRenderer`).

**게임 개발 맥락**: 상태 객체는 **조합 폭발**을 조심해야 한다. 프리셋 enum(Winters의 `eRHIBlendMode`, `eBlendPreset`)으로 좁혀 캐싱하는 것이 실무 패턴. 실제로 Winters에서 FX용 blend enum을 새로 만들려다 기존 `eBlendPreset`(Opaque/AlphaBlend/Premultiplied/Additive)이 이미 있어 재사용으로 정리한 사례가 있다(2026-04-26 세션 기록 — "신규 enum 신설 전 기존 인프라 grep" 교훈).

### 6. 드로우콜 비용의 정체, 배칭과 인스턴싱

**정의**: 드로우콜 자체는 GPU에겐 명령 하나지만, CPU 쪽에서 (1) 상태 변경 검증/해싱, (2) 드라이버의 커맨드 버퍼 인코딩, (3) 리소스 해저드 추적, (4) (과거) 커널 전환 비용이 붙는다. DX11은 드라이버가 이걸 상당 부분 대신 해주는 대가로 드로우당 CPU 비용이 크고, 이것이 "드로우콜을 줄여라"의 실체다.

**정확한 이해 (면접 포인트)**: 비싼 건 Draw 함수 호출 자체보다 **드로우 사이의 상태 변경**이다. 같은 파이프라인/텍스처로 연속 드로우하면 드라이버 부담이 급감한다. 그래서 정렬 기준이 "머티리얼(파이프라인) → 텍스처 → 메시" 순.

**대응 기법**:
- **정적 배칭**: 같은 머티리얼 메시들을 하나의 VB/IB로 미리 합침 (메모리 ↑, 컬링 유연성 ↓).
- **인스턴싱**: 같은 메시를 N개 그릴 때 `DrawIndexedInstanced(indexCount, N, ...)` + 인스턴스별 데이터(월드 행렬)는 두 번째 정점 스트림 또는 구조화 버퍼로. 드로우 1번으로 N개.
- **텍스처 아틀라스/배열**: 텍스처 전환 자체를 제거.

**예시 (Winters)**: RHI 드로우 인터페이스가 처음부터 인스턴싱 시그니처다 — `CDX11FrameCommandList::DrawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance)`가 내부에서 `DrawIndexedInstanced`를 호출한다(`CDX11Device.cpp`). 미니언 수십 마리가 도는 LoL 클론 특성상 인스턴싱 확장을 전제로 한 설계. 또 서브메시 단위 절두체 컬링(`ModelRenderer::RenderFrustumCulled` → `BuildClipVisibilityMask`)으로 안 보이는 서브메시의 드로우 자체를 스킵한다.

**게임 개발 맥락**: 드로우콜 수보다 먼저 물을 것 — "CPU 바운드인가 GPU 바운드인가". Winters의 17.8ms 병목은 드로우콜이 아니라 CPU 스키닝 갱신이었다(§13). 계측 없이 배칭부터 하는 건 순서가 틀렸다.

### 7. 깊이버퍼와 Z-fighting

**정의**: 깊이버퍼는 픽셀별 최소 깊이를 기억해 가려진 픽셀을 버리는 가시성 해결 장치. 깊이값은 클립 공간 z/w가 뷰포트 변환으로 [0,1]에 매핑된 값인데, **원근 투영에서 z/w는 뷰 z에 대해 쌍곡선 분포**라 near 근처에 정밀도가 몰리고 far 근처는 희박하다.

**Z-fighting 원인 2종 (구분이 면접 포인트)**:
1. **정밀도 부족**: 서로 다른 깊이의 두 면이 양자화 후 같은 값이 되는 경우 → near를 키우거나(far/near 비 축소), 32bit float 깊이, reversed-Z(float 분포와 z 분포 상쇄)로 해결 가능.
2. **진짜 동일 평면(coplanar)**: 두 폴리곤이 수학적으로 같은 평면 → **정밀도를 아무리 올려도 해결 불가**. 데이터(지오메트리)를 고치거나, 깊이 바이어스/스텐실/렌더 순서로 강제 분리해야 한다.

**예시 (Winters, 실전)**: 소환사의 협곡 맵 로딩에서 Z-fighting 발생 (2026-04-16 세션 기록 `project_phase_b4_map.md`). 원인: 변환된 맵의 `Layer1~8` 노드(646개 메시)가 기본 지형(1,077개 메시)과 **동일 평면에 통째로 겹쳐** 있었음. Near/Far 조정으로는 해결 불가(코플라나 문제) → glb 로딩 단계 `ProcessNode()`에서 이름에 "Layer"가 들어간 노드를 스킵하는 데이터 레벨 해법으로 완전 제거 + 메시 수 37% 감소라는 부수 이득. **"정밀도 문제인지 데이터 문제인지 먼저 판별했다"**가 이 사례의 핵심 스토리다.

**Winters의 깊이 설정 근거**: `CDX11Device::CreateDepthStencil()`이 `DXGI_FORMAT_D24_UNORM_S8_UINT` 텍스처를 만들고, `BeginFrame()`이 매 프레임 `ClearDepthStencilView(..., 1.f, 0)`으로 클리어한다 (`CDX11Device.cpp`).

**게임 개발 맥락**: 데칼·바닥 마킹(LoL의 사거리 표시 등)은 태생이 coplanar라서 깊이 바이어스나 depth test LESS_EQUAL + 렌더 순서로 처리한다. reversed-Z는 DX에서 클립 z∈[0,1]이라 특히 효과적(OpenGL은 [-1,1]이라 glClipControl 필요)이라는 것까지 말하면 심화 가산점.

### 8. 알파 블렌딩과 렌더 순서

**정의**: 블렌딩은 OM에서 `최종색 = Src×SrcBlend (op) Dest×DestBlend`. 일반 알파 블렌딩은 `Src×α + Dest×(1-α)` — **순서 의존적(비가환)** 연산이다.

**렌더 순서 원칙과 이유**:
- **불투명: front-to-back** — 앞엣것을 먼저 그리면 뒤엣것이 Early-Z에서 기각되어 PS 실행이 줄어든다(오버드로우 감소). 결과는 순서 무관하게 같지만 성능이 다르다.
- **반투명: back-to-front** — 블렌딩 식이 "이미 그려진 배경(Dest) 위에 겹쳐 얹기"이므로 뒤에서 앞 순서여야 수학적으로 맞다. 순서가 틀리면 색이 틀린다(정확성 문제).
- **반투명은 깊이 기록(depth write) OFF, 깊이 테스트는 ON** — 기록을 켜면 앞의 반투명이 뒤의 반투명을 깊이에서 기각해 아예 사라지고, 테스트를 끄면 불투명 벽 뒤의 FX가 비쳐 보인다.

**예시 (Winters)**: `CDX11PipelineState`의 AlphaBlend(`SRC_ALPHA/INV_SRC_ALPHA`)와 Additive(`SRC_ALPHA/ONE`) 프리셋 (`CDX11Device.cpp`). Additive는 가환(순서 무관)이라 FX 파티클에 애용된다 — 이렐리아 E 빔이 Additive `{1.5, 1.2, 1.0}` 광원 표현(2026-04-26 세션 기록). FX 메시 드로우는 깊이 기록 없는 DSS(`pDSSNoWrite`)를 쓰고 드로우 후 원래 상태로 복원한다.

**꼬리 개념**: 완전한 순서 독립 투명(OIT)은 depth peeling, per-pixel linked list, weighted blended OIT 등이 있지만 비싸서 게임에선 "정렬 + additive로 도망가기"가 실무 답이다.

### 9. 텍스처 — 밉맵, 필터링, sRGB

**밉맵**: 텍스처의 1/2씩 축소된 피라미드. GPU는 픽셀 셰이더에서 UV의 화면 공간 미분(ddx/ddy)으로 밉 레벨을 고른다. 효과: (1) 미니피케이션 시 반짝임(알리아싱) 제거 — 텍셀:픽셀 비율이 1:1에 가깝게 유지, (2) **캐시 효율** — 먼 오브젝트가 작은 밉을 읽으므로 메모리 대역폭 절약. 비용: 메모리 +33%.

**필터링**:
- Point: 최근접 텍셀. 픽셀아트/UI용.
- Linear(bilinear): 4텍셀 보간. + 밉 레벨 간 보간 = trilinear.
- Anisotropic: 비스듬한 각도(바닥 텍스처)에서 시야 방향으로 여러 샘플 — 기울어진 표면의 흐림 해결. Winters의 `ToDX11Filter()`가 Point/Linear/Anisotropic 3종을 매핑하고 `CreateSampler()`에서 `MaxAnisotropy`를 설정한다(`CDX11Device.cpp`).

**sRGB / 감마**: 텍스처 원본(아티스트가 모니터 보고 만든 색)은 감마 인코딩 상태다. 라이팅 연산은 **선형 공간**에서 해야 물리적으로 맞다. `_SRGB` 포맷으로 만들면 샘플 시 자동 감마→선형 변환, sRGB RT에 쓰면 자동 선형→감마 변환. 이걸 안 하면 "라이팅이 어딘가 물빠진/타버린" 결과가 나온다. 단 노말맵·마스크맵 같은 데이터 텍스처는 sRGB로 해석하면 안 된다(색이 아니라 수치이므로).

**예시 (Winters, 정직한 현재 상태)**: `Engine/Private/RHI/RHITextureLoader.cpp`는 WIC로 디코딩해 `GUID_WICPixelFormat32bppRGBA`로 변환 후 `eRHIFormat::R8G8B8A8_UNorm`, `mipLevels=1`로 업로드한다. 즉 **현재 밉맵 미생성 + sRGB 미적용** 상태 — 탑뷰 고정 카메라(LoL식)라 미니피케이션 반짝임이 상대적으로 덜 드러나지만, 개선 항목으로 인지하고 있다고 말하는 것이 정직하고 오히려 점수가 된다(밉 생성은 `D3D11_RESOURCE_MISC_GENERATE_MIPS` + `GenerateMips`, 또는 오프라인 베이크).

### 10. 셰이더 컴파일과 시맨틱/입력 레이아웃 매칭

**정의**: HLSL은 `D3DCompileFromFile`(런타임) 또는 fxc/dxc(오프라인)로 바이트코드로 컴파일된다. **입력 레이아웃**은 IA가 정점 버퍼의 바이트를 VS 입력 시맨틱(POSITION, NORMAL, TEXCOORD0, BLENDINDICES...)에 매핑하는 규칙이고, `CreateInputLayout`은 **VS 바이트코드를 인자로 받아** 레이아웃과 VS 입력 시그니처가 맞는지 생성 시점에 검증한다.

**예시 (Winters)**:
- `Engine/Private/RHI/DX11/DX11Shader.cpp`: `D3DCompileFromFile`로 vs_5_0/ps_5_0 런타임 컴파일. Debug 빌드는 `D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION`(RenderDoc에서 소스 레벨 디버깅 가능), Release는 `OPTIMIZATION_LEVEL3`. 주석에 핵심 설계가 있다: "**VS 바이트코드(m_pVSBlob)는 Load 후에도 유지된다 → DX11Pipeline이 InputLayout 생성 시 이 블롭이 필요하기 때문**". 컴파일 실패 시 에러 블롭을 `OutputDebugStringA`로 출력하고 `__debugbreak()`.
- `CDX11Device.cpp`의 `CDX11PipelineState::InitializeNative()`도 같은 이유로 `pVSObject->bytecode`를 `CreateInputLayout`에 넘긴다.

**런타임 컴파일의 함정 (실전 경험)**: 셰이더는 빌드 타임 검증을 안 거치므로 HLSL 오타(`erodeNoize`)가 **빌드 0 에러로 통과하고 F5 실행 시** `D3DCompileFromFile` 실패로 터졌다. 추가 함정: PostBuild `xcopy /D`(타임스탬프 비교)가 OutDir의 옛 .hlsl을 안 갱신해 "고쳐도 같은 에러 반복" — 강제 동기화로 해결(2026-04-26 세션 기록). 실무에서 오프라인 컴파일 + 셰이더 캐시로 가는 이유가 이것(시작 시간, 배포 검증, D3DCompiler DLL 의존 제거).

### 11. 스키닝 — 매트릭스 팔레트

**정의**: 스켈레탈 애니메이션에서 각 정점은 최대 4개 본 인덱스(BLENDINDICES)와 가중치(BLENDWEIGHT)를 갖고, VS에서 `Σ weight_i × BonePalette[index_i] × pos`로 변형된다. 팔레트의 각 행렬은 `본의 현재 월드(또는 모델) 변환 × 역바인드 포즈(inverse bind pose)`.

**팔레트 전달 수단의 트레이드오프**:
- **cbuffer**: 슬롯당 최대 4096 float4 = 행렬 1024개가 이론 한계지만, 실무는 256~512개 배열로 잡는 경우가 많고 **본 수가 넘치면 드로우를 쪼개야 한다**.
- **구조화 버퍼 + SRV**: 크기 제약이 사실상 없고 VS에서 `StructuredBuffer<float4x4>`로 인덱싱. 본 많은 캐릭터도 1드로우.

**예시 (Winters, 실제 전환 경험)**: `ModelRenderer.cpp`의 `BoneMatrixSRVBuffer` — 주석 원문: "**GPU bone palette: structured buffer SRV (t8) so 512+ bone large-character rigs skin in one draw. Replaces the old 256/512 cbuffer palette.**" 구현: `kMaxGPUBones = 1024`, `D3D11_USAGE_DYNAMIC + BIND_SHADER_RESOURCE + MISC_BUFFER_STRUCTURED`, `StructureByteStride = sizeof(XMFLOAT4X4)`, 매 프레임 `Map(WRITE_DISCARD)`로 `CAnimator::GetFinalBoneMatrices()`를 업로드하고 `VSSetShaderResources(8, ...)`로 t8에 바인딩. 즉 **cbuffer 팔레트의 본 수 한계를 실제로 맞아보고 구조화 버퍼 SRV로 전환**한 사례.

**게임 개발 맥락**: 스키닝의 비용은 GPU(VS의 행렬 블렌딩)보다 **CPU의 본 행렬 갱신(애니메이션 샘플링 + 계층 누적)**이 먼저 병목이 되는 경우가 많다 — Winters의 17.8ms 사건이 정확히 이 케이스(§13).

### 12. RenderDoc 디버깅 방법론

**원칙**: "호출은 되는데 안 보인다"는 CPU 디버거(중단점, 로그)로 못 잡는 클래스의 버그다. 픽셀은 GPU 파이프라인 안에서 죽기 때문. **프레임 캡처로 스테이지별 이분탐색**을 해야 한다.

**표준 절차 (안 보이는 메시)**:
1. Event Browser에서 해당 드로우콜이 **존재하는지** 확인 (없으면 CPU 문제).
2. **IA**: Mesh Viewer에서 입력 정점이 정상인지 (스케일 0, NaN, 인덱스 깨짐).
3. **VS Out**: 클립 공간 위치가 화면 안인지 (투영 행렬, w=0).
4. **RS**: 컬링/뷰포트/시저에 잘렸는지 (와인딩 반대 → CullMode None으로 실험).
5. **PS**: 입력 텍스처가 진짜 바인딩됐는지, 출력색이 있는지. Pixel History와 셰이더 디버깅으로 특정 픽셀이 **어느 테스트에서 기각됐는지**(depth fail / stencil fail / discard) 확인.
6. **OM**: 블렌드 상태가 결과를 0으로 만들었는지, 쓰기 마스크.

**예시 (Winters, 실전)**: 이렐리아 E 검이 안 보인 버그(2026-04-26 세션 기록, `feedback_lol_fx_texture_pattern.md`). Spawn/Render 전부 호출됨, 화면 0픽셀. CPU 가설 4개(노드 transform/스케일/회전/단위) 전부 틀림. 진짜 원인: LoL 추출물의 `render/*.png`는 mesh diffuse가 아니라 **스프라이트 캡처 이미지**였고, FBX UV가 그 이미지의 **알파 0 영역만** 가리켜 `Mesh3D.hlsl`의 `clip(texColor.a - 0.05f)`가 전 픽셀을 버림. 해결은 진짜 머티리얼 텍스처(`*_texture.png`/`*_mult.png`)로 교체. 진단은 **UV bbox와 PNG 알파 bbox를 직접 계측**해서 미스매치를 증명하는 방식이었고, 세션 교훈으로 "이 클래스 버그는 다음부터 RenderDoc/PIX 프레임 캡처를 1순위로"가 박제됐다. 면접에서는 "CPU 추론 1.5시간 낭비 → 데이터 계측 30분 해결"의 대비로 말하면 강력하다.

**보조 도구**: DX11 디버그 레이어(`D3D11_CREATE_DEVICE_DEBUG`) — Winters는 `CDX11Device::CreateDeviceAndSwapChain()`에서 Debug 빌드에 자동 활성 + Graphics Tools 미설치 환경 폴백까지 처리한다(`CDX11Device.cpp`).

### 13. 프레임 최적화 실전 — 계측 우선

**원칙**: 최적화 순서는 "계측 → 병목 확정 → 그 병목만 수술". 계측 수단: CPU 프로파일러(스코프 트리 + 카운터), GPU 타임스탬프 쿼리, 프레임 캡처.

**예시 (Winters, 17.8ms → 9ms 복구, 2026-04-24 세션 기록 `project_session_2026_04_24.md`)**:
1. 자체 CPU 프로파일러(`WINTERS_PROFILE_SCOPE` + 카운터)로 11곳 계측 → **의심하던 내비게이션은 무죄**(각 0.003ms), **`Minion::AnimUpdate`가 16ms로 프레임의 90%** = 스키닝 본 행렬 갱신이 병목으로 확정.
2. 수술 1: `CEngineApp`에서 Update/Render가 **2번씩 중복 호출**되던 버그 제거.
3. 수술 2: `RenderComponent::bAnimated` 플래그 도입 — 맵/구조물/정글 등 정적 엔티티는 `false`로 스폰하고 애니 갱신 루프에서 스킵. `Anim::UpdateCalls` 카운터 107 → 대폭 감소.
4. 결과: 17.8ms → 9ms (~110fps). 같은 세션에서 자체 바이너리 포맷 `.wmesh`(제로카피 로드, 이렐리아 FBX 60MB → 1.2MB, 27개 에셋 전수 변환)로 로딩도 최적화.

**GPU 측 계측 (Winters)**: `CDX11Device.cpp`의 GPU 타이밍 — `D3D11_QUERY_TIMESTAMP_DISJOINT` + `TIMESTAMP` 쌍을 프레임 시작/끝(`BeginFrame`/`EndFrame`)에 발행하고, 슬롯 링버퍼로 여러 프레임 지연 후 `GetData(..., D3D11_ASYNC_GETDATA_DONOTFLUSH)`로 논블로킹 회수 → `WINTERS_PROFILE_COUNT("GPU::FrameUs", gpuUs)`. **당장 결과를 기다리면 CPU-GPU 동기화 스톨**이므로 지연 회수가 핵심이고, disjoint 플래그(클럭 변동)면 그 프레임 값은 버린다.

**게임 개발 맥락**: "CPU 바운드 vs GPU 바운드" 판별이 1단계다. CPU 프레임 시간 ≫ GPU 시간이면 배칭/멀티스레딩/로직 최적화, 반대면 오버드로우/셰이더/해상도. Winters 사건은 전자였다.

### 14. RHI 추상화 설계 — 왜 레이어를 두나

**정의**: RHI(Render Hardware Interface)는 렌더러 상위 코드와 그래픽 API 사이의 추상 경계. 목적: (1) 멀티 API(DX11/DX12/Vulkan) 백엔드 교체, (2) 상위 코드에서 네이티브 타입 격리(컴파일 방화벽), (3) API 진화 대응.

**설계 포인트**:
- 인터페이스는 **더 명시적인 API(DX12/Vulkan) 모양**으로 잡아야 한다. DX11 모양으로 잡으면 DX12 백엔드를 낄 때 전면 재설계다. 커맨드리스트, 파이프라인 상태(PSO 스타일 통짜 desc), 바인드 그룹(디스크립터 셋 유사), 리소스 상태 전이 API를 미리 넣어두고 DX11 백엔드에서는 no-op/즉시 실행으로 구현.
- 리소스는 포인터 대신 **핸들**로 — 세대(generation) 검증, DLL 경계 안전, 지연 파괴 대응.

**예시 (Winters)**: `CDX11Device.cpp` 상단 주석에 방향이 명문화돼 있다: "현재: CDX11Device가 모든 DX11 호출을 직접 수행 / 향후: IRHIDevice → CDX11Device / CDX12Device". 실제 구조:
- 리소스는 전부 핸들 + 테이블: `CRHIResourceTable<CDX11BufferObject, RHIBufferTag>` 등 buffer/shader/texture/sampler 4테이블 (`ResourceTables`).
- `CDX11FrameCommandList`가 `IRHICommandList`를 구현하되 `Begin/End/BeginRenderPass/EndRenderPass`는 즉시 모드 no-op, `TransitionResource`도 no-op 스텁 — **DX12에서 의미를 갖는 API를 인터페이스에 선반영**.
- `RHIPipelineDesc` 하나로 셰이더+입력레이아웃+래스터/깊이/블렌드를 통짜 기술 → DX12 PSO와 1:1 대응 가능.
- 바인딩은 `BindGroupLayout`/`BindGroup`(슬롯+가시성 마스크) — Vulkan 디스크립터 셋 레이아웃과 동형. `SetBindGroup`이 가시성 비트에 따라 VS/PS에 나눠 바인딩.
- 어댑터 선택도 DXGI 레벨에서 명시적: `IDXGIFactory6::EnumAdapterByGpuPreference(HIGH_PERFORMANCE)`로 노트북 하이브리드 GPU에서 dGPU를 우선 선택하고 실패 시 폴백 (`SelectHighPerformanceDX11Adapter`).

**DX11 vs DX12/Vulkan 요점 비교**:

| | DX11 | DX12/Vulkan |
|---|---|---|
| 드라이버 역할 | 해저드 추적, 메모리 관리, 상태 검증을 드라이버가 | 앱이 전부(배리어, 힙, 펜스) |
| 멀티스레딩 | deferred context (효과 제한적) | 커맨드리스트 병렬 녹화가 1급 시민 |
| 파이프라인 | 상태 객체 낱개 | PSO 통짜 (셰이더 조합 사전 컴파일) |
| 바인딩 | 슬롯 단위 Set* | 디스크립터 힙/셋 |
| 동기화 | 암묵적 | 명시적 (fence, barrier) |

### 15. 그림자와 포스트프로세싱 (기본 개념)

**섀도우 매핑**: 패스 1 — 광원 시점에서 깊이만 렌더(깊이 텍스처 = 섀도우 맵). 패스 2 — 본 렌더에서 픽셀의 월드 좌표를 광원 클립 공간으로 변환해 섀도우 맵 깊이와 비교, 더 멀면 그림자. 아티팩트: **shadow acne**(자기 그림자 줄무늬 — 깊이 양자화) → 깊이 바이어스/slope-scaled bias(RasterizerState의 DepthBias), 과한 바이어스는 **peter panning**(그림자가 발에서 떨어짐). 경계 부드럽게는 PCF(주변 비교 샘플 평균), 대면적은 CSM(카메라 절두체를 거리별 분할). Winters의 `eRHIFormat`에 `R24G8_Typeless`/`R24_UNorm_X8_Typeless`가 있는 것이 "깊이를 SRV로 샘플링"하는 이 구도를 위한 준비다.

**포스트프로세싱**: 씬을 오프스크린 RT에 렌더 → 그 RT를 SRV로 풀스크린 트라이앵글에 입혀 PS에서 가공(톤매핑, 블룸, 색보정). 같은 텍스처를 RTV+SRV 동시 바인딩할 수 없으므로 **핑퐁 버퍼** 2장을 교대로. HDR이면 중간 RT를 `R16G16B16A16_FLOAT`로 — Winters `eRHIFormat`에 이미 존재하는 포맷.

### 16. 스왑체인과 Present

**정의**: 스왑체인은 백버퍼(그리는 중) ↔ 프론트버퍼(표시 중) 교체 장치. `Present(syncInterval, flags)`에서 syncInterval 1 = VSync(티어링 없음, 최대 리프레시레이트로 제한), 0 = 즉시.

**BLT vs FLIP 모델**: 구형 `DISCARD/SEQUENTIAL`은 DWM으로 복사(blt)가 낄 수 있고, `FLIP_DISCARD`(Win10+)는 버퍼 소유권을 OS와 교환해 복사 없이 표시 — 지연/효율 우수. 단 FLIP은 버퍼 수 2+, Present 후 백버퍼 RTV 재취득 등 규약이 더 엄격하다.

**예시 (Winters)**: `CDX11Device::CreateDeviceAndSwapChain()`은 현재 `DXGI_SWAP_EFFECT_DISCARD`를 쓰며 주석으로 이유를 남겼다 — "초기 안정화 단계에서는 전통적인 DISCARD 스왑체인을 사용한다. (FLIP 모델은 백버퍼 순환/재바인딩 정책을 더 엄격히 맞춰야 함)". 파일 상단 주석에는 FLIP_DISCARD 전환 방향이 적혀 있어 "트레이드오프를 알고 미룬 결정"임을 말할 수 있다. `BeginFrame()`이 매 프레임 RTV+뷰포트를 재바인딩하는 것도 안정성 주석과 함께다. `EndFrame()`은 `Present(m_bVSync ? 1 : 0, 0)` 후 `DXGI_ERROR_DEVICE_REMOVED/RESET`을 감지한다(디바이스 로스트 대응의 최소 단위).

---

## 예상 질문 & 모범답변

### Q1. DX11 렌더링 파이프라인을 순서대로 설명해보세요.

**답**: IA → VS → HS/테셀레이터/DS → GS → (SO) → RS → PS → OM 순입니다. IA가 정점/인덱스 버퍼를 입력 레이아웃에 따라 프리미티브로 조립하고, VS가 정점을 클립 공간으로 변환합니다. HS/DS는 테셀레이션, GS는 프리미티브 증폭/제거용이며 안 쓰면 통과합니다. RS는 고정 기능으로 클리핑·원근 나눗셈·뷰포트 변환·컬링 후 픽셀로 분해하며 속성을 원근 보정 보간합니다. PS가 픽셀 색을 계산하고, OM이 깊이/스텐실 테스트와 블렌딩으로 렌더타겟에 병합합니다.

**한 단계 더**: 논리적 순서와 하드웨어 실제가 다른 지점을 짚습니다 — 깊이 테스트는 명세상 OM이지만 하드웨어는 Early-Z로 PS 전에 기각하고, PS가 `clip()`이나 `SV_Depth`를 쓰면 이 최적화가 제한됩니다.

**내 프로젝트**: Winters의 `CDX11PipelineState::Apply()`(`Engine/Private/RHI/DX11/CDX11Device.cpp`)가 IA(입력레이아웃/토폴로지), VS/PS, RS, OM(DSS/블렌드) 상태를 한 번에 바인딩하는 구조라, "파이프라인 = 상태 집합"으로 체화하고 있습니다.

**꼬리질문 대비**: "GS를 실무에서 잘 안 쓰는 이유는?" → 출력 순서 보장 요구로 하드웨어 병렬화가 어려워 느리고, 빌보드 확장 등은 인스턴싱/VS 트릭/CS로 대체 가능하기 때문.

### Q2. Device와 DeviceContext의 차이는? 왜 나눴을까요?

**답**: Device는 리소스 생성(Create*), Context는 명령 발행(Set*/Draw/Map)입니다. 결정적 차이는 스레딩 계약 — Device는 free-threaded라 로딩 스레드에서 텍스처를 만들어도 되지만, Context는 스레드 안전하지 않아 한 스레드에서만 씁니다. 생성(검증 비용 커도 드물게)과 발행(매 프레임 수천 번)의 성격이 달라 계약을 분리한 설계입니다.

**트레이드오프**: deferred context로 명령 녹화를 다른 스레드에 나눌 수 있지만, DX11은 드라이버가 결국 직렬화·재검증하는 경우가 많아 이득이 드라이버 의존적입니다. 이 한계가 DX12의 명시적 커맨드리스트 재설계 동기입니다.

**내 프로젝트**: Winters는 immediate context를 `IRHICommandList`로 감싼 `CDX11FrameCommandList`를 둡니다(Begin/End는 no-op). 인터페이스를 커맨드리스트 모양으로 잡아두고 DX11에선 즉시 실행으로 구현 — DX12 백엔드를 붙일 때 상위 코드를 안 바꾸기 위한 결정입니다.

### Q3. SRV, RTV, DSV, UAV는 각각 무엇이고, 왜 리소스와 뷰를 분리했나요?

**답**: SRV는 셰이더 읽기, RTV는 색 기록 대상, DSV는 깊이/스텐실 대상, UAV는 임의 읽기/쓰기(주로 CS)입니다. 뷰 분리의 이유는 **같은 메모리를 용도·포맷 다르게 재해석**하기 위해서입니다. 대표 예가 섀도우 맵: 텍스처를 `R24G8_TYPELESS`로 만들고 그리기 패스에선 DSV(`D24_UNORM_S8_UINT`), 샘플링 패스에선 SRV(`R24_UNORM_X8_TYPELESS`)를 붙입니다.

**함정 대비**: 같은 서브리소스를 RTV와 SRV로 동시에 바인딩하면 해저드라 런타임이 SRV를 강제 언바인드하고 디버그 레이어가 경고합니다. 포스트프로세싱에서 핑퐁 버퍼를 쓰는 이유입니다.

**내 프로젝트**: Winters `eRHIFormat`에 위 typeless 3종 조합이 정의되어 있고(`CDX11Device.cpp`의 `ToDXGIFormat`), 본 팔레트는 구조화 버퍼+SRV(t8), 백버퍼→RTV, `D24_UNORM_S8_UINT`→DSV로 각 뷰를 실사용 중입니다.

### Q4. USAGE_DEFAULT / IMMUTABLE / DYNAMIC / STAGING은 언제 쓰나요?

**답**: 기준은 "누가 얼마나 자주 갱신하나"입니다. 절대 안 바뀌면 IMMUTABLE(드라이버가 최적 배치 가능), GPU가 주로 쓰고 CPU 갱신이 드물면 DEFAULT+`UpdateSubresource`, CPU가 매 프레임 쓰면 DYNAMIC+`Map(WRITE_DISCARD)`, GPU→CPU 리드백은 STAGING+`CopyResource` 후 Map 읽기입니다.

**내 프로젝트**: Winters는 정점/인덱스는 DEFAULT(생성 시 초기 데이터), 상수버퍼와 본 팔레트는 DYNAMIC입니다. `CDX11Device::CreateBuffer()`가 `desc.dynamic`에 따라 `D3D11_USAGE_DYNAMIC + CPU_ACCESS_WRITE`와 DEFAULT를 분기하고, `UpdateBuffer()`가 dynamic이면 Map, 아니면 UpdateSubresource로 자동 분기합니다.

**꼬리질문 대비**: "STAGING으로 백버퍼를 읽으면 무슨 문제?" → CopyResource 후 즉시 Map하면 GPU 완료 대기로 파이프라인 스톨. 몇 프레임 지연 회수(링버퍼)가 정석 — Winters GPU 타이밍 쿼리도 같은 이유로 슬롯 링 + `DONOTFLUSH` 폴링입니다.

### Q5. Map(WRITE_DISCARD)와 UpdateSubresource의 차이와 트레이드오프를 설명해보세요.

**답**: `Map(WRITE_DISCARD)`는 드라이버가 버퍼 리네이밍(새 메모리로 교체)을 해줘서 GPU가 이전 데이터를 읽는 중이어도 CPU가 스톨 없이 씁니다. 전체 재기록 전제이며 DYNAMIC 전용입니다. `UpdateSubresource`는 DEFAULT용으로, 드라이버가 내부 스테이징에 복사했다가 GPU 타이밍에 반영 — 복사 1회가 추가되고 큰/빈번한 갱신에 불리하지만 부분 갱신과 "가끔 갱신"에 편합니다. `WRITE_NO_OVERWRITE`는 GPU가 안 읽은 영역에 이어쓰기(동적 VB append)로, 규약 위반 시 레이스가 납니다.

**내 프로젝트**: `DX11ConstantBuffer::Update()`와 `BoneMatrixSRVBuffer::Update()` 모두 매 프레임 전체 갱신이라 WRITE_DISCARD를 씁니다. RHI 레벨 `UpdateBuffer()`는 리소스 성격에 따라 두 경로를 자동 선택합니다.

### Q6. 상수버퍼는 왜 16바이트 정렬인가요? C++ 구조체와 안 맞으면 어떻게 되나요?

**답**: GPU 상수 레지스터가 float4 단위라 HLSL cbuffer는 16바이트 단위로 패킹되고, 멤버가 경계를 걸치면 다음 레지스터로 밀립니다. C++ 구조체 레이아웃이 이 규칙과 다르면 셰이더가 엉뚱한 오프셋을 읽어 "가끔 이상한 값" 류의 잡기 어려운 버그가 됩니다. 방어책은 C++ 쪽 명시적 패딩 + 크기 정적 검증입니다.

**내 프로젝트**: `DX11ConstantBuffer<T>`에 `static_assert(sizeof(T) % 16 == 0)`를 박아 컴파일 타임에 걸러냅니다(`Engine/Private/RHI/DX11/DX11ConstantBuffer.h`). 버퍼 생성 측도 `CreateBuffer`에서 `(ByteWidth + 15) & ~15`로 크기를 올림합니다.

**꼬리질문 대비**: "float3 다음에 float를 두면?" → 같은 레지스터에 붙어 총 16바이트로 패킹됨. "float 다음 float3은?" → float3이 경계를 걸치므로 다음 레지스터로 밀리고 12바이트 패딩 발생.

### Q7. 상수버퍼를 per-frame / per-object로 나누는 이유는?

**답**: 갱신 빈도가 다른 데이터를 한 버퍼에 섞으면, 가장 자주 바뀌는 멤버의 빈도로 전체를 재업로드하게 됩니다. 뷰·프로젝션·라이트는 프레임당 1회, 월드 행렬·머티리얼 파라미터는 오브젝트당 1회로 나누면 업로드 바이트와 바인드가 최소화됩니다. 이는 DX12 루트 시그니처 설계, 언리얼의 View/Primitive 유니폼 분리와 같은 원칙입니다.

**내 프로젝트**: `ModelRenderer.cpp`가 `CBPerFrame`(b0: viewProjection, cameraWorld, 라이트 배열, screenSize)과 `CBPerObject`(b1: world, worldInvTranspose, 머티리얼 오버라이드)로 분리했고, FX 파라미터는 별도 b2(`CBFxParams`)입니다. 셰이더/파이프라인은 `CEngineApp`이 소유해 전 인스턴스가 공유하고, CB만 인스턴스별로 두는 소유권 설계까지 함께 정리되어 있습니다.

### Q8. 입력 레이아웃 생성에 왜 VS 바이트코드가 필요한가요?

**답**: `CreateInputLayout`은 "버퍼의 바이트 배치 → 시맨틱" 매핑이 실제 VS 입력 시그니처와 호환되는지 **생성 시점에 검증**하기 위해 VS 바이트코드를 요구합니다. 시맨틱 이름/인덱스/타입이 다르면 생성이 실패하므로, 드로우 타임 미스매치 크래시 대신 초기화 시점에 잡힙니다.

**내 프로젝트**: `DX11Shader.cpp` 주석에 이 설계가 그대로 있습니다 — "VS 바이트코드(m_pVSBlob)는 Load 후에도 유지된다 → DX11Pipeline이 InputLayout 생성 시 이 블롭이 필요하기 때문". RHI 경로도 동일하게 `CDX11ShaderObject::bytecode`를 보관해 `CDX11PipelineState::InitializeNative()`에서 사용합니다.

**꼬리질문 대비**: "같은 정점 포맷에 셰이더가 여러 개면 레이아웃을 셰이더마다 만들어야 하나?" → 시그니처가 호환되면 재사용 가능. 실무는 (정점 포맷 × 시그니처) 캐시를 둔다.

### Q9. 드로우콜은 왜 비싼가요? 무엇이 진짜 비용인가요?

**답**: GPU 명령 하나가 비싼 게 아니라, 드로우 직전까지의 **CPU 측 상태 변경 처리**가 비쌉니다 — 드라이버의 상태 검증·해싱, 해저드 추적, 커맨드 인코딩. 그래서 핵심은 드로우 개수 자체보다 "드로우 사이 상태 변화량"이고, 머티리얼→텍스처 순 정렬로 상태 변경을 몰아 없애는 것이 배칭의 본질입니다.

**왜/트레이드오프**: 정적 배칭은 메모리와 컬링 유연성을 희생하고, 인스턴싱은 같은 메시+머티리얼 조건이 필요합니다.

**내 프로젝트**: Winters RHI의 `DrawIndexed`는 시그니처부터 `instanceCount`를 받아 `DrawIndexedInstanced`로 내려갑니다 — 미니언 다수가 뜨는 LoL 클론이라 인스턴싱 확장을 전제했습니다. 다만 우리 실측 병목은 드로우콜이 아니라 CPU 스키닝 갱신이었어서(17.8ms 사건), "계측으로 병목을 확정하기 전에 배칭부터 하지 않는다"를 원칙으로 삼고 있습니다.

### Q10. 인스턴싱은 어떻게 동작하고 언제 쓰나요?

**답**: `DrawIndexedInstanced(indexCount, instanceCount, ...)`로 같은 메시를 N번 그리되, 인스턴스별 데이터(월드 행렬, 색)는 per-instance 정점 스트림(`D3D11_INPUT_PER_INSTANCE_DATA`) 또는 구조화 버퍼+`SV_InstanceID` 인덱싱으로 공급합니다. 조건: 같은 메시+같은 파이프라인. 잔디, 미니언 무리, 총알 등 동종 대량 오브젝트에 적합합니다.

**트레이드오프**: 인스턴스별로 머티리얼/셰이더가 갈리면 못 묶습니다. 텍스처만 다르면 텍스처 배열로 우회 가능합니다.

**내 프로젝트**: RHI 드로우 시그니처에 instanceCount가 이미 있고(위 Q9), 스키닝 팔레트를 구조화 버퍼로 옮겨둔 상태라 "인스턴스별 본 오프셋"을 추가하면 스킨드 인스턴싱으로 확장 가능한 구조입니다.

### Q11. 깊이버퍼의 원리와, 깊이 정밀도가 non-linear한 이유는?

**답**: 픽셀별 최소 깊이를 저장하고 비교 함수(LESS 등)로 가려진 픽셀을 기각합니다. 저장되는 값은 클립 공간 z/w인데, 원근 투영에서 z/w는 뷰 공간 z의 쌍곡선 함수라 near 근처에 정밀도가 집중되고 far로 갈수록 희박해집니다. far/near 비가 크면(near 0.01 등) 원거리 정밀도가 무너집니다.

**해결 계열**: near 올리기, D32_FLOAT, reversed-Z(near=1, far=0 매핑 + GREATER 비교 — float의 0 근처 고정밀과 상쇄).

**내 프로젝트**: Winters는 `D24_UNORM_S8_UINT` + 클리어 1.0(`CDX11Device::CreateDepthStencil`/`BeginFrame`), 비교 함수는 파이프라인 desc의 `depthOp`로 설정합니다. LoL식 탑뷰라 깊이 범위가 좁아 24bit로 충분하지만, 광활한 씬이면 reversed-Z를 검토했을 것이라고 확장해 답할 수 있습니다.

### Q12. Z-fighting을 실제로 겪고 해결한 경험이 있나요?

**답 (실화 기반)**: 있습니다. 소환사의 협곡 맵(.mapgeo→glb 변환)을 로딩했을 때 지형이 심하게 깜빡였습니다. 먼저 정밀도 문제인지 확인하려 near/far를 조정했는데 **전혀 개선되지 않았습니다**. 이유는 정밀도가 아니라 데이터였기 때문입니다 — 변환된 에셋의 `Layer1~8` 노드 646개 메시가 기본 지형 1,077개 메시와 **수학적으로 동일 평면**에 통째로 겹쳐 있었습니다. coplanar는 깊이 비트를 아무리 늘려도 해결이 안 되므로, 로딩 단계 `ProcessNode()`에서 "Layer" 노드를 스킵하는 데이터 레벨 해법으로 완전 제거했고, 부수적으로 메시 37% 감소라는 성능 이득도 있었습니다.

**면접 포인트**: "Z-fighting = 바이어스"로 반사적으로 답하지 않고, **정밀도 문제 vs 진짜 coplanar를 먼저 판별**했다는 진단 순서를 강조합니다.

**꼬리질문 대비**: "데칼처럼 의도적으로 coplanar면?" → DepthBias/SlopeScaledDepthBias, LESS_EQUAL+렌더 순서, 스텐실, 또는 폴리곤을 살짝 띄우는 지오메트리 해법.

### Q13. 불투명은 front-to-back, 반투명은 back-to-front로 그리는 이유는?

**답**: 불투명 front-to-back은 **성능** 문제입니다 — 앞엣것을 먼저 그려 깊이버퍼를 채우면 뒤엣것이 Early-Z에서 기각되어 PS 실행(오버드로우)이 줄어듭니다. 결과는 순서와 무관하게 같습니다. 반투명 back-to-front는 **정확성** 문제입니다 — `Src×α + Dest×(1-α)`는 "이미 있는 배경 위에 얹기"이므로 뒤부터 그려야 수식이 맞고, 순서가 틀리면 색 자체가 틀립니다.

**꼬리질문 대비**: "반투명 정렬이 불가능한 교차 케이스는?" → 폴리곤 단위 분할, additive로 회피(가환), 또는 OIT(weighted blended 등). "additive는 왜 정렬이 필요 없나?" → 덧셈은 교환법칙이 성립하므로.

**내 프로젝트**: Winters FX는 이 성질을 활용해 이렐리아 E 빔 등 발광 이펙트를 Additive(`SRC_ALPHA/ONE`, `CDX11Device.cpp` 블렌드 프리셋)로 처리해 정렬 부담을 줄였습니다.

### Q14. 반투명 오브젝트를 그릴 때 깊이 쓰기를 끄는 이유는?

**답**: 깊이 기록을 켜면 먼저 그린 반투명이 깊이버퍼를 채워, 그 뒤에 있는 반투명 픽셀이 depth test에서 통째로 기각됩니다 — 유리 뒤 유리가 사라지는 버그. 그래서 반투명은 **테스트 ON(불투명에 가려지긴 해야 함) + 기록 OFF**가 표준입니다.

**내 프로젝트**: Winters 파이프라인 desc에 `depthWrite`가 분리돼 있어 `DepthWriteMask = depthWrite ? ALL : ZERO`로 구워지고(`CDX11PipelineState::InitializeNative`), FX 메시 렌더러는 깊이 기록 없는 DSS(`pDSSNoWrite`)를 백업·복원 방식으로 씁니다(2026-04-26 세션 기록).

### Q15. 밉맵은 왜 필요한가요? 없으면 무슨 일이 생기나요?

**답**: 먼 오브젝트는 픽셀 하나에 텍셀 수십 개가 대응하는데, 밉 없이 최상위 해상도에서 샘플하면 프레임마다 다른 텍셀이 뽑혀 **반짝임(temporal aliasing)**이 생깁니다. 밉맵은 사전 평균된 축소본을 ddx/ddy 기반 LOD 선택으로 샘플해 이를 해결하고, 작은 밉은 캐시 지역성이 좋아 **대역폭도 절약**됩니다. 비용은 메모리 +33%.

**내 프로젝트 (정직하게)**: Winters의 WIC 로더(`Engine/Private/RHI/RHITextureLoader.cpp`)는 현재 `mipLevels=1`로 업로드합니다. 탑뷰 고정 줌 카메라라 미니피케이션 변동이 작아 체감 이슈가 낮았지만, `MISC_GENERATE_MIPS`+`GenerateMips` 또는 오프라인 베이크(자체 포맷 .wmesh처럼 텍스처도 사전 처리)로 개선할 항목으로 인지하고 있습니다. — 한계를 아는 것이 모르는 것보다 낫다는 스탠스로.

### Q16. 텍스처 필터링 종류와 anisotropic 필터링이 해결하는 문제는?

**답**: Point(최근접), Bilinear(4텍셀 보간), Trilinear(+밉 간 보간), Anisotropic(비등방성). 바닥처럼 비스듬히 보는 표면은 픽셀의 텍스처 공간 footprint가 길쭉한 타원인데, trilinear는 등방(원형) 가정이라 과하게 흐려집니다. Aniso는 긴 축 방향으로 여러 샘플을 찍어 선명도를 유지합니다(2x~16x = 샘플 수).

**내 프로젝트**: Winters RHI 샘플러가 Point/Linear/Anisotropic 3종 + `MaxAnisotropy`, 주소 모드 Wrap/Clamp/Border를 지원합니다(`CDX11Device.cpp`의 `ToDX11Filter`/`ToDX11AddressMode`/`CreateSampler`).

### Q17. sRGB와 감마 보정을 파이프라인 관점에서 설명해보세요.

**답**: 모니터는 비선형(감마) 응답이고 앨범 텍스처는 그 화면 기준으로 제작된 감마 인코딩 데이터입니다. 라이팅·블렌딩은 물리량이므로 **선형 공간**에서 계산해야 합니다. 올바른 흐름: 컬러 텍스처는 `_SRGB` 포맷 → 샘플 시 HW가 선형으로 변환 → 선형에서 라이팅 → sRGB 백버퍼/스왑체인에 쓰며 HW가 감마 인코딩. 노말맵·러프니스 등 데이터 텍스처는 sRGB 지정 금지(수치이지 색이 아님).

**흔한 함정**: 감마를 안 지키면 중간톤이 죽거나 라이트가 과하게 타는 룩이 되고, 아티스트가 라이트 세기로 보정하려다 더 꼬입니다.

**내 프로젝트**: Winters는 현재 `R8G8B8A8_UNORM`(비 sRGB) 파이프라인이라, PBR 머티리얼(`CMaterialPBR`) 고도화 시 sRGB 정리가 선행 과제라고 스스로 지적할 수 있습니다.

### Q18. 백페이스 컬링은 어느 스테이지에서, 어떤 기준으로 동작하나요?

**답**: RS(래스터라이저)에서, 화면 투영 후 삼각형 정점의 와인딩 방향(시계/반시계)으로 앞/뒷면을 판정해 기각합니다. `RasterizerState`의 CullMode(None/Front/Back)와 FrontCounterClockwise로 제어합니다. 닫힌 메시의 뒷면 픽셀 처리를 통째로 절약하는 가장 싼 컬링입니다.

**실전 연결**: 임포트한 에셋이 뒤집혀 보이거나 안 보일 때 1순위 의심 대상입니다. Winters RHI는 파이프라인 desc의 `cullMode`(`ToDX11CullMode`)로 제어하고, 안 보임 디버깅 시 CULL_NONE 실험이 체크리스트에 있습니다(§12 방법론). 양면 재질(천, 나뭇잎)은 CULL_NONE + 노말 뒤집기 셰이더 처리.

### Q19. GPU 스키닝 파이프라인을 데이터 흐름으로 설명해보세요. 본 팔레트는 어떻게 전달하나요?

**답**: CPU에서 애니메이션 샘플링 → 본 계층 누적 → `현재 본 변환 × 역바인드 포즈`로 팔레트 행렬 배열 생성 → GPU 업로드. VS에서 정점의 BLENDINDICES/BLENDWEIGHT(보통 4개)로 팔레트를 가중 합산해 위치·노말을 변형합니다. 팔레트 전달은 cbuffer 배열(본 수 한계 有) 또는 구조화 버퍼 SRV(사실상 무제한) 두 방식이 있습니다.

**내 프로젝트 (강한 실전 근거)**: Winters는 cbuffer 팔레트(256/512개)에서 **구조화 버퍼 SRV(t8, 최대 1024본)로 전환**했습니다. `ModelRenderer.cpp`의 `BoneMatrixSRVBuffer` 주석 그대로 "512+ bone large-character rigs skin in one draw. Replaces the old 256/512 cbuffer palette." 구현은 DYNAMIC+STRUCTURED 버퍼를 매 프레임 `Map(WRITE_DISCARD)`으로 갱신, `VSSetShaderResources(8,...)` 바인딩입니다. 본이 많은 대형 리그를 드로우 분할 없이 1드로우로 처리하기 위한 결정이었습니다.

**꼬리질문 대비**: "노말도 같은 행렬로 변형해도 되나?" → 균등 스케일이면 OK, 비균등이면 역전치 필요(Q20). "CPU 스키닝과 비교하면?" → CPU는 유연(피킹/클로스 후처리)하지만 대량 캐릭터에서 병목 — 실제로 우리 병목이 CPU 쪽 본 갱신이었습니다(Q23).

### Q20. 노말 변환에 왜 월드 행렬의 역전치를 쓰나요?

**답**: 노말은 방향이 아니라 "표면에 수직"이라는 제약을 가진 벡터입니다. 비균등 스케일이 있는 월드 행렬 M으로 노말을 그대로 변환하면 수직성이 깨집니다. 접선 t는 M으로 변환되고 n·t=0을 보존하려면 노말은 (M⁻¹)ᵀ로 변환해야 함이 유도됩니다. M이 회전+균등 스케일만이면 M과 역전치가 (스케일 팩터 차이 빼고) 같아 생략 가능합니다.

**내 프로젝트**: `ModelRenderer::UpdateTransform()`이 매 오브젝트마다 `XMMatrixTranspose(XMMatrixInverse(world))`를 계산해 `CBPerObject.worldInvTranspose`로 올립니다(`ModelRenderer.cpp`). 셰이더가 아닌 CPU에서 1회 계산해 두는 위치 선정(정점마다 역행렬 계산 회피)까지 언급하면 좋습니다.

### Q21. GPU 프레임 시간은 어떻게 측정하나요? 그냥 CPU 타이머로 Present 전후를 재면 안 되나요?

**답**: 안 됩니다 — CPU와 GPU는 비동기라 CPU 타이머는 "명령 제출 시간"만 잽니다. 정석은 timestamp query: `D3D11_QUERY_TIMESTAMP` 쌍을 구간 앞뒤에 `End()`로 찍고, `D3D11_QUERY_TIMESTAMP_DISJOINT`로 클럭 주파수와 유효성(클럭 변동 시 disjoint)을 받아 `(end-begin)/frequency`로 환산합니다. 결과 회수는 **몇 프레임 지연 + 논블로킹 폴링**이어야 CPU-GPU 동기화 스톨을 피합니다.

**내 프로젝트**: `CDX11Device.cpp`에 정확히 이 구현이 있습니다 — `BeginFrame/EndFrame`에서 disjoint+timestamp 쌍 발행, 슬롯 링버퍼(`m_GpuTimingSlots`)로 프레임 중첩 관리, `GetData(..., D3D11_ASYNC_GETDATA_DONOTFLUSH)`로 준비된 것만 회수, disjoint거나 `endTicks <= beginTicks`면 폐기, 결과는 `WINTERS_PROFILE_COUNT("GPU::FrameUs")`로 자체 프로파일러 카운터에 적재됩니다.

### Q22. 화면에 아무것도 안 나옵니다. 어떻게 디버깅하시겠어요?

**답 (절차로)**: 먼저 CPU/GPU 경계를 가릅니다. RenderDoc으로 프레임 캡처 → (1) 드로우콜이 이벤트 목록에 있는가(없으면 CPU 로직) → (2) IA 메시 프리뷰에서 정점 정상인가 → (3) VS 출력 클립 좌표가 화면 안인가(투영/카메라) → (4) 래스터 단계에서 컬링/뷰포트에 잘렸는가 → (5) PS 입력 텍스처 바인딩과 출력색, Pixel History로 어느 테스트(depth/stencil/discard)에서 기각됐는가 → (6) 블렌드/쓰기 마스크. 병행으로 DX11 디버그 레이어 경고 확인.

**내 프로젝트 (실화)**: 이렐리아 E 검 미표시 버그에서 CPU 가설(트랜스폼/스케일/회전/단위) 4개를 1.5시간 소모하고 전부 틀렸습니다. 원인은 PS의 `clip(texColor.a - 0.05f)` — LoL 추출물의 `render/*.png`가 mesh diffuse가 아닌 스프라이트 캡처였고 UV가 알파 0 영역만 가리켜 전 픽셀이 버려졌습니다. UV bbox vs 알파 bbox 계측으로 30분 만에 확정했고, 이후 팀 규칙으로 "픽셀이 안 보이면 셰이더 Read + 데이터 계측 우선, GPU 캡처 도구 1순위"를 박제했습니다(`.claude` gotchas 및 debug-pipeline 스킬로 제도화). 이 "프로세스를 고쳤다"는 부분까지 말하는 것이 포인트입니다.

### Q23. 프레임 최적화 경험을 구체적 수치로 말해보세요. 병목을 어떻게 찾았나요?

**답 (STAR식)**: 미니언 다수 스폰 후 프레임이 17.8ms까지 떨어졌습니다. 추측 대신 자체 CPU 프로파일러(스코프 트리+카운터)를 11개 지점에 심었습니다. 결과: 유력 용의자였던 내비게이션(A*)은 각 0.003ms로 무죄, **`Minion::AnimUpdate`가 16ms — 프레임의 90%**로 스키닝 본 행렬 갱신이 병목이었습니다. 수술은 둘: (1) `CEngineApp`의 Update/Render **중복 호출 버그** 제거, (2) `RenderComponent::bAnimated` 플래그로 맵/구조물/정글 등 정적 엔티티의 애니 갱신을 스킵(카운터 `Anim::UpdateCalls` 107 → 급감). 결과 **17.8ms → 9ms(~110fps)**. 같은 사이클에서 로딩도 자체 `.wmesh` 포맷(제로카피, 이렐리아 60MB→1.2MB)으로 최적화했습니다.

**메시지**: "계측 → 범인 확정 → 최소 수술 → 카운터로 효과 검증"의 루프. 카운터가 없었으면 내비게이션을 며칠 팠을 겁니다.

**꼬리질문 대비**: "그 다음 병목은?" → 남은 애니 갱신의 잡시스템 병렬화, 절두체 컬링 확대(현재 서브메시 마스크 컬링 `BuildClipVisibilityMask` 구현됨), 쿼드트리 공간 분할.

### Q24. RHI 추상화 레이어를 왜 두나요? 어떻게 설계했나요?

**답**: 세 가지 — API 교체 가능성(DX12/Vulkan), 상위 코드에서 네이티브 타입 격리(컴파일 방화벽 + DLL 경계 안전), 테스트/도구 용이성. 핵심 설계 판단은 "**인터페이스는 더 명시적인 API 모양으로**"입니다. DX11 모양으로 추상화하면 DX12를 낄 때 전면 재설계이므로, 커맨드리스트/PSO식 파이프라인 desc/바인드 그룹/리소스 전이를 인터페이스에 먼저 넣고 DX11 백엔드는 no-op 또는 즉시 실행으로 구현합니다.

**내 프로젝트**: Winters의 `IRHIDevice → CDX11Device`(주석에 CDX12Device 방향 명문화), 리소스는 전부 핸들+테이블(`CRHIResourceTable` 4종), `IRHICommandList`를 immediate context 래퍼(`CDX11FrameCommandList`)로 구현 — `Begin/End/BeginRenderPass/TransitionResource`는 DX11에선 no-op이지만 DX12에서 의미를 갖도록 미리 존재합니다. 바인딩은 `BindGroupLayout/BindGroup`(슬롯+가시성 마스크)로 Vulkan 디스크립터 셋과 동형입니다. 파이프라인은 `RHIPipelineDesc` 하나로 셰이더+레이아웃+래스터/깊이/블렌드를 통짜 기술해 DX12 PSO와 1:1 매핑됩니다.

**꼬리질문 대비**: "추상화 비용은?" → 핸들 룩업/가상 호출이 있지만 드로우당 나노초 수준이고, 진짜 비용은 "최소공배수 함정"(백엔드별 고유 기능 못 씀) — `GetNativeHandle`류 탈출구를 열어두되 사용처를 격리하는 것으로 관리합니다(Winters도 `eNativeHandleType::DX11Device/DX11DeviceContext` 탈출구를 ModelRenderer 등 한정된 곳에서 사용).

### Q25. DX11과 DX12의 차이를 설명하고, DX11 경험이 DX12 학습에 어떻게 이어지는지 말해보세요.

**답**: DX12는 드라이버가 해주던 일을 앱이 명시적으로 합니다 — 리소스 상태 전이(배리어), 메모리 힙 관리, CPU-GPU 동기화(펜스), 디스크립터 힙, 커맨드리스트 병렬 녹화, PSO 사전 컴파일. 대가로 CPU 오버헤드 감소와 멀티스레드 확장을 얻습니다. DX11에서 암묵적으로 일어나던 것(WRITE_DISCARD의 리네이밍 = 수동 링버퍼 업로드 힙, 상태 객체 = PSO의 조각들, 해저드 자동 해결 = 배리어)을 알고 있으면 DX12는 "그걸 내 손으로"일 뿐입니다.

**내 프로젝트**: Winters RHI가 이미 DX12 모양(커맨드리스트, 통짜 파이프라인 desc, 바인드 그룹, TransitionResource API)으로 잡혀 있어, DX12 백엔드는 no-op이던 자리에 실제 구현을 채우는 작업으로 스코프가 정의되어 있습니다. 또 GPU 타이밍 쿼리의 "지연 회수 링버퍼"는 DX12 펜스 기반 프레임 인플라이트 관리와 같은 사고방식입니다.

### Q26. 스왑체인과 Present, VSync를 설명해보세요. FLIP 모델은 무엇이 다른가요?

**답**: 스왑체인은 백버퍼/프론트버퍼 교체 장치이고 `Present(1,0)`이면 VSync 동기(티어링 없음, 리프레시레이트 상한), `Present(0,0)`이면 즉시입니다. 구형 BLT 모델(DISCARD/SEQUENTIAL)은 DWM 합성 시 복사가 낄 수 있고, FLIP_DISCARD(Win10+)는 버퍼 소유권 교환으로 복사 없이 표시되어 지연·효율이 좋습니다. 대신 버퍼 2+ 요구, 프레임마다 백버퍼가 순환하므로 RTV 관리 규약이 엄격합니다.

**내 프로젝트 (트레이드오프를 아는 보류)**: Winters는 현재 DISCARD를 쓰고, 코드 주석에 이유를 남겼습니다 — "초기 안정화 단계에서는 전통적인 DISCARD 스왑체인. (FLIP 모델은 백버퍼 순환/재바인딩 정책을 더 엄격히 맞춰야 함)". `BeginFrame`이 매 프레임 RTV/뷰포트를 재바인딩하는 것도 같은 안정성 결정이고, FLIP 전환은 파일 상단 주석에 로드맵으로 명시돼 있습니다. `EndFrame`은 `DXGI_ERROR_DEVICE_REMOVED/RESET`을 감지합니다 — 디바이스 로스트(드라이버 크래시, TDR) 대응의 시작점.

### Q27. 셰이더를 런타임 컴파일하는 것과 오프라인 컴파일의 장단점은? 실제로 겪은 문제가 있나요?

**답**: 런타임(`D3DCompileFromFile`)은 이터레이션이 빠르고(핫 리로드) 개발기에 유리, 오프라인(fxc/dxc + 캐시)은 시작 시간 단축, 배포 시 컴파일 실패 위험 제거, 소스 은닉, D3DCompiler DLL 의존 제거가 장점입니다. 상용은 오프라인+파생 캐시가 표준입니다.

**내 프로젝트 (실제 사고 2건)**: Winters는 개발 편의로 런타임 컴파일(`DX11Shader::CompileShader`, Debug는 `D3DCOMPILE_DEBUG|SKIP_OPTIMIZATION`, 실패 시 에러 블롭 출력+`__debugbreak`)인데, 이 때문에 (1) HLSL 오타가 **빌드는 0 에러로 통과**하고 실행 시점에 터진 사고, (2) PostBuild `xcopy /D`가 타임스탬프 비교만 해서 **OutDir의 옛 .hlsl이 계속 컴파일되어 "고쳐도 같은 에러"**가 반복된 사고를 겪었고, 강제 동기화 규칙을 gotchas로 박제했습니다. "런타임 컴파일 = 빌드 파이프라인 검증 부재"라는 트레이드오프를 몸으로 아는 사례입니다.

### Q28. 섀도우 매핑의 원리와 대표 아티팩트, 해결책은?

**답**: 광원 시점 깊이 패스로 섀도우 맵을 만들고, 본 패스에서 픽셀을 광원 클립 공간으로 투영해 저장 깊이와 비교합니다. 아티팩트: **acne**(자기 그림자 줄무늬 — 깊이 양자화·경사)는 DepthBias/SlopeScaledDepthBias 또는 노말 방향 오프셋으로, 과보정하면 **peter panning**(그림자 분리)이 생겨 균형이 필요합니다. 경계 계단은 PCF(비교 샘플 평균, HW `SampleCmp`), 넓은 씬은 CSM(절두체 분할별 맵)으로 해상도를 배분합니다.

**내 프로젝트 연결**: 아직 본격 섀도우 패스는 없지만, 깊이를 SRV로 재해석하는 typeless 포맷 3종(`R24G8_Typeless`/`D24_UNorm_S8_UInt`/`R24_UNorm_X8_Typeless`)이 RHI 포맷에 준비되어 있고, RasterizerState를 파이프라인 desc로 굽는 구조라 바이어스 추가 지점이 명확합니다 — "구현 안 한 것"과 "설계에 자리 잡아둔 것"을 구분해 답합니다.

### Q29. 포스트프로세싱은 어떻게 구현하나요? 같은 텍스처를 읽고 쓰면 왜 안 되나요?

**답**: 씬을 오프스크린 RT(HDR이면 `R16G16B16A16_FLOAT`)에 렌더 → 그 텍스처를 SRV로 풀스크린 트라이앵글에 바인딩해 PS에서 톤매핑/블룸/색보정 → 백버퍼로. 같은 서브리소스의 RTV+SRV 동시 바인딩은 read-write 해저드라 런타임이 SRV를 강제 해제(디버그 레이어 경고)하므로, 체인이 길면 핑퐁 RT 2장을 교대합니다. 블룸은 다운샘플 체인에서 밝은 픽셀 추출→블러→합성.

**내 프로젝트 연결**: Winters RHI에 HDR 포맷(`R16G16B16A16_Float`)과 렌더패스 desc가 이미 있고, FX 렌더에서 SRV 언바인딩 규율(드로우 후 null SRV 복원 — `ModelRenderer.cpp`의 AO SRV 처리)을 지키고 있어 해저드 규칙을 실무로 다루고 있습니다.

### Q30. 텍스처 로딩 파이프라인을 설명해보세요. WIC 로딩에서 주의할 점은?

**답**: 파일 디코딩(WIC/DDS 로더) → 포맷 통일(예: 32bpp RGBA) → rowPitch 계산해 서브리소스 초기 데이터로 GPU 텍스처 생성 → SRV 생성. 주의점: (1) WIC는 COM이라 스레드별 CoInitialize 상태 필요, (2) rowPitch는 width×bpp지만 포맷별 정렬 차이 주의, (3) DDS(BC 압축)는 WIC가 아닌 전용 로더, (4) 컬러/데이터 텍스처의 sRGB 구분.

**내 프로젝트**: `Engine/Private/RHI/RHITextureLoader.cpp`가 정확히 이 절차입니다 — `CScopedCOMInit`(RAII로 CoInitializeEx/CoUninitialize), WIC 디코더 → `GUID_WICPixelFormat32bppRGBA` 변환 → `CopyPixels`(rowPitch=width×4) → RHI `CreateTexture(R8G8B8A8_UNorm)`. 각 실패 단계는 `LogTextureLoadFailure`로 HRESULT+경로를 남깁니다 — "조용한 실패 금지"는 에러 처리 정책으로도 박제되어 있습니다. 과거 UI PNG 로드 실패 디버깅(2026-04-24 세션)에서 경로 이스케이프(`\U`) 문제 등 로딩 실패의 흔한 원인 목록도 경험했습니다.

### Q31. "메시가 검게/하얗게 나온다"면 무엇부터 의심하나요?

**답**: 증상별 분기 — (1) 완전 검정: 텍스처 바인딩 실패(SRV null → 샘플 0) 또는 라이팅 NdotL 0(노말 깨짐/역전치 누락/노말맵 sRGB 오지정), (2) 완전 흰색/단색: UV가 한 점으로 붕괴(입력 레이아웃 오프셋 오류)나 클리어 컬러 노출, (3) 특정 각도에서만: 컬링/노말 방향. RenderDoc으로 PS 입력 텍스처와 UV 값을 직접 보는 게 최단 경로입니다.

**내 프로젝트**: 넥서스/바론 텍스처 진단(2026-04-20 세션 기록)에서 원인을 "Destroyed 상태 중첩 메시 + 바론 눈 메시의 무텍스처"로 **진단 로그를 심어 데이터로 확정**한 경험이 있습니다. 추측으로 셰이더를 고치지 않고 원인을 먼저 계측하는 원칙의 사례입니다.

### Q32. 절두체 컬링을 어디서, 어떤 단위로 하는 게 좋은가요?

**답**: GPU 클리핑은 정점 처리 후라 VS 비용은 이미 지불된 뒤입니다. CPU에서 바운딩 볼륨(AABB/스피어) vs 절두체 검사로 **드로우 자체를 스킵**해야 진짜 절약입니다. 단위는 오브젝트 → 서브메시 → (대규모면) 공간 분할(쿼드트리/BVH) 순으로 정밀화하며, 검사 비용과 절약의 균형을 봅니다.

**내 프로젝트**: `ModelRenderer::RenderFrustumCulled()`가 로컬→클립 행렬로 `BuildClipVisibilityMask()`를 만들어 **서브메시 단위 가시성 마스크**로 `RenderWithMask` 합니다. 전부 밖이면 드로우 0회. 모델 로드 시 AABB를 뽑아 보관(`HasValidAABB`)하고, 다음 단계로 LoL 탑뷰 특성에 맞는 쿼드트리 도입이 로드맵에 있습니다(2026-04-24 세션 기록).

### Q33. 자체 포맷(.wmesh)을 만든 이유는? FBX를 그대로 쓰면 안 되나요?

**답**: FBX/glTF는 교환 포맷이라 런타임 파싱(Assimp)에 후처리·재배열 비용이 큽니다. 런타임 포맷의 목표는 "디스크 레이아웃 = GPU 업로드 레이아웃"으로 만들어 **제로카피 로드**하는 것입니다. POD 헤더+정점/인덱스 블록을 static_assert로 고정하고 읽자마자 버퍼 생성에 넘깁니다.

**내 프로젝트**: 2026-04-24 세션에서 `.wmesh`를 구현 — POD 구조 5종 전수 `static_assert`, 제로카피 로더(`CWMeshLoader`), 변환기 `WintersAssetConverter.exe`로 27개 에셋 전수 변환, 이렐리아 FBX 60MB → 1.2MB(애니 제외 50배). 런타임은 하이브리드: 정적 메시는 .wmesh fast-path, 본 있는 캐릭터는 Assimp 폴백 + 텍스처는 Assimp 유지(GLB 임베디드 지원). 16bit/32bit 인덱스 결정 후 오프셋 재계산(2패스) 같은 세부 트레이드오프도 다뤘습니다. 이 질문은 "엔진 프로그래머의 데이터 지향 사고"를 보여줄 기회입니다.

### Q34. 디버그 레이어는 무엇이고 어떻게 활용하나요?

**답**: `D3D11_CREATE_DEVICE_DEBUG`로 디바이스를 만들면 런타임이 API 오용(잘못된 바인딩, 해저드, 리소스 누수)을 디버그 출력으로 경고합니다. 종료 시 `ReportLiveDeviceObjects`로 누수 추적도 가능합니다. 성능 비용이 있어 Debug 빌드 한정이 보통입니다.

**내 프로젝트**: `CDX11Device::CreateDeviceAndSwapChain()`이 `_DEBUG`에서 플래그를 켜고, **Graphics Tools 미설치 환경에서 생성 실패 시 플래그를 빼고 재시도**하는 폴백까지 구현했습니다(팀원 PC에서 실패하는 흔한 사고 방어). 추가로 고성능 어댑터 선택 실패 시 OS 기본 어댑터 폴백, 어댑터명/VRAM 로깅도 초기화 경로에 있습니다 — "초기화 실패는 단계별로 로그를 남기고 폴백"이 엔진의 에러 처리 원칙입니다.

### Q35. 라이트가 여러 개면 상수버퍼로 어떻게 넘기나요? 개수가 가변이면?

**답**: 소수 고정 상한이면 cbuffer에 배열+개수(예: `PointLight lights[4]; uint count;`), 수백 개면 구조화 버퍼 SRV, 수천+화면 국소성이 크면 tiled/clustered 라이팅(CS로 타일별 라이트 목록 컬링)으로 확장합니다.

**내 프로젝트**: `ModelRenderer::UpdateCamera()`의 `CBPerFrame`이 방향광 1 + `pointLights[4]` + `pointLightCount` 패턴입니다. LoL식 씬은 라이트 수가 작아 cbuffer 상한 방식이 적정 비용이고, 늘어나면 본 팔레트에서 이미 쓴 구조화 버퍼 패턴을 재사용하면 된다고 확장 경로까지 답합니다.

---

## 내 프로젝트 연결 포인트

면접에서 그래픽스 질문이 나오면 아래 문장들로 Winters 경험을 접합한다. 모두 코드/기록으로 뒷받침 가능한 주장만 담았다.

1. **"DX11 디바이스 초기화부터 Present까지 전부 직접 작성했습니다."** — 어댑터 열거(IDXGIFactory6 고성능 우선+폴백), 디버그 레이어 폴백, 스왑체인/RTV/DSV/뷰포트, 디바이스 로스트 감지까지 (`Engine/Private/RHI/DX11/CDX11Device.cpp`).
2. **"RHI를 DX12 모양으로 추상화해 뒀습니다."** — 핸들+리소스 테이블, IRHICommandList(즉시 모드 no-op), PSO식 통짜 파이프라인 desc, 바인드 그룹/가시성 마스크, TransitionResource 스텁. "DX12 백엔드는 no-op 자리를 채우는 일"이라고 스코프를 말할 수 있다.
3. **"상수버퍼는 per-frame(b0)/per-object(b1)/FX(b2)로 분리하고, 16바이트 정렬을 static_assert로 강제합니다."** (`DX11ConstantBuffer.h`, `ModelRenderer.cpp`)
4. **"본 팔레트를 cbuffer 256/512에서 구조화 버퍼 SRV(t8, 1024본)로 전환해 대형 리그를 1드로우 스키닝합니다."** — 한계를 직접 맞고 바꾼 경험 (`ModelRenderer.cpp` `BoneMatrixSRVBuffer`).
5. **"Z-fighting을 정밀도 문제가 아닌 coplanar 데이터 문제로 판별해 로딩 단계에서 해결했고, 부수로 메시 37%를 줄였습니다."** (소환사의 협곡 Layer 노드, 2026-04-16)
6. **"17.8ms→9ms를 프로파일러 카운터로 병목(스키닝 갱신 90%)을 확정한 뒤 중복 호출 제거+bAnimated 스킵으로 복구했습니다."** — 수치·계측·수술·검증이 전부 있는 최적화 스토리 (2026-04-24).
7. **"'호출되는데 안 보이는' 버그를 PS의 clip()+UV-알파 미스매치로 확정한 뒤, 'GPU 캡처 우선' 디버깅 절차를 팀 규칙(gotchas/skill)으로 제도화했습니다."** — 버그 한 건이 아니라 프로세스 개선까지 (2026-04-26).
8. **"GPU 프레임 시간을 timestamp/disjoint 쿼리 링버퍼+논블로킹 회수로 계측해 자체 프로파일러에 통합했습니다."** (`CDX11Device.cpp`)
9. **"자체 런타임 포맷 .wmesh로 제로카피 로드를 구현해 이렐리아 60MB→1.2MB, 27개 에셋을 전수 이관했습니다."** — 데이터 지향 에셋 파이프라인 (2026-04-24).
10. **"FLIP_DISCARD 전환, 밉맵 생성, sRGB 파이프라인은 현재 미적용이며 각각의 트레이드오프와 도입 경로를 알고 있습니다."** — 한계를 아는 정직함이 신뢰를 만든다.
11. **"텍스처 로딩 실패는 단계별 HRESULT 로그, FlatBuffers 검증 실패는 bounded trace — '조용한 실패 금지'를 렌더링과 네트워크 양쪽에 같은 정책으로 적용합니다."** — 그래픽스를 넘어 엔지니어링 원칙으로 확장.

## 마지막 점검 체크리스트

면접 전날 한 줄씩 소리 내어 확인한다.

- [ ] 파이프라인 순서: IA→VS→HS/Tess/DS→GS→SO→RS→PS→OM. 깊이 테스트는 명세상 OM, 실제는 Early-Z(clip/SV_Depth가 깨뜨림).
- [ ] Device=생성(free-threaded) / Context=발행(단일 스레드). deferred context는 DX11에선 이득 제한적 → DX12 커맨드리스트의 동기.
- [ ] 뷰 4종: SRV 읽기 / RTV 색 / DSV 깊이 / UAV 임의RW. typeless로 깊이↔SRV 재해석 (R24G8_TYPELESS 조합).
- [ ] USAGE: IMMUTABLE 불변 / DEFAULT+UpdateSubresource 드문 갱신 / DYNAMIC+Map(WRITE_DISCARD=리네이밍) 매 프레임 / STAGING 리드백(지연 회수).
- [ ] cbuffer 16바이트 패킹, per-frame(b0)/per-object(b1) 분리 — Winters는 static_assert로 강제.
- [ ] 상태 객체 3종: Rasterizer(컬/필/바이어스), DepthStencil(테스트/기록 분리), Blend(Src/Dest/Op) — 생성 시 검증, 프리셋으로 조합 관리.
- [ ] 드로우콜 비용 = 드로우 사이 상태 변경. 정렬(머티리얼→텍스처) → 배칭 → 인스턴싱(DrawIndexedInstanced). 계측이 먼저.
- [ ] 깊이 정밀도는 z/w 쌍곡선 → near에 몰림. 해법: near↑, D32F, reversed-Z. **coplanar는 정밀도로 못 고침 → 데이터/바이어스** (협곡 Layer 사례).
- [ ] 불투명 front-to-back=성능(Early-Z), 반투명 back-to-front=정확성(블렌드 비가환). 반투명은 depth test ON + write OFF. additive는 가환이라 정렬 불요.
- [ ] 밉맵=알리아싱+대역폭, ddx/ddy로 LOD. aniso=비스듬한 표면. sRGB: 샘플→선형, 라이팅은 선형, 출력→감마. 노말맵은 sRGB 금지. (Winters는 밉/sRGB 미적용 — 개선 항목으로 답변)
- [ ] CreateInputLayout에 VS 바이트코드 = 생성 시점 시그니처 검증. 런타임 셰이더 컴파일 = 빌드 검증 부재(오타/OutDir 캐시 사고 경험담).
- [ ] 스키닝: 팔레트 = 본 현재 변환 × 역바인드. cbuffer 한계 → 구조화 버퍼 SRV t8 1024본 (Winters 전환 스토리). 노말은 역전치.
- [ ] GPU 시간 = timestamp+disjoint 쿼리, 링버퍼 지연 회수, DONOTFLUSH. CPU 타이머로 Present 재면 제출 시간만 잼.
- [ ] RenderDoc 절차: 드로우 존재→IA 정점→VS 클립 좌표→RS 컬링→PS 텍스처/Pixel History→OM 블렌드. 실화: clip()+UV-알파 미스매치 1.5h→계측 30min.
- [ ] 최적화 실화 수치: 17.8ms→9ms. 병목=Minion::AnimUpdate 16ms(90%), Nav 무죄(0.003ms). 수술=중복 호출 제거+bAnimated. 검증=Anim::UpdateCalls 카운터.
- [ ] RHI: 인터페이스는 DX12 모양(커맨드리스트/PSO/바인드그룹/전이), DX11 백엔드는 no-op/즉시. 리소스는 핸들+테이블. 탈출구는 GetNativeHandle로 격리.
- [ ] 스왑체인: DISCARD(현재, 안정성 주석) vs FLIP_DISCARD(무복사, 규약 엄격). Present(1)=VSync. DEVICE_REMOVED 감지.
- [ ] 섀도우: 광원 깊이 패스→비교. acne↔peter panning은 바이어스 균형, PCF, CSM.
- [ ] 포스트: 오프스크린 RT→SRV 풀스크린 패스. RTV+SRV 동시 바인딩 불가(해저드) → 핑퐁.
- [ ] .wmesh: 제로카피, 60MB→1.2MB, "디스크 레이아웃=GPU 레이아웃" 사고방식 한 문장으로.
- [ ] 모르는 질문이 오면: "구현은 안 해봤지만 원리는 이렇고, 제 엔진에선 이 지점에 들어간다"로 구조화해서 답한다.
