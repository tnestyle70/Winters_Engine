# Stage 6 — FFT 기반 기법 (Ocean + Bloom + DoF)

## 목표

Cooley-Tukey **FFT 직접 구현** (Compute Shader) 후 응용: Tessendorf Ocean Wave + Bloom +
Circular Bokeh DoF.

## 왜 FFT 인가

- **주파수 도메인 연산**: 큰 컨볼루션을 O(N²) → O(N log N)
- **Ocean**: Phillips 스펙트럼 + IFFT → 수만 점 표면 변위 실시간
- **Bloom**: Large-kernel Gaussian 을 FFT 컨볼루션으로 처리
- **DoF Circular Bokeh**: 비대칭 커널도 FFT 로 빠르게

## 이산 푸리에 변환 (DFT)

```
X[k] = Σ_{n=0}^{N-1} x[n] × e^(-2πi × k × n / N)
```

직접 계산 O(N²) — 너무 느림.

## Cooley-Tukey FFT (1965)

분할 정복. N = 2^m 이면 O(N log N).

```
X[k] = X_even[k] + w^k × X_odd[k]
X[k + N/2] = X_even[k] - w^k × X_odd[k]
   where w = e^(-2πi/N)
```

### Radix-2 반복형 (비트 리버설 + butterfly)

```hlsl
// Bit reversal — in-place 재배치
uint BitReverse(uint x, uint nBits)
{
    uint r = 0;
    for (uint i = 0; i < nBits; ++i) {
        r = (r << 1) | (x & 1);
        x >>= 1;
    }
    return r;
}
```

### 1D FFT Compute Shader

```hlsl
// Shaders/FFT/FFT1D.hlsl
groupshared float2 g_data[N];   // complex (real, imag)

[numthreads(N, 1, 1)]
void CS_FFT1D(uint3 id : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
    // 1. Bit-reverse load
    uint revIdx = BitReverse(id.x, LOG2_N);
    g_data[revIdx] = g_input[gid.y * N + id.x];
    GroupMemoryBarrierWithGroupSync();
    
    // 2. Butterfly stages
    for (uint stage = 1; stage <= LOG2_N; ++stage) {
        uint m = 1 << stage;        // half-butterfly size × 2
        uint mh = m >> 1;
        uint j = id.x & (mh - 1);
        uint k = (id.x & ~(mh - 1)) << 1;
        
        float angle = -2.0 * PI * j / float(m);
        float2 w = float2(cos(angle), sin(angle));
        
        if (id.x < mh) {   // 하위 절반만 계산
            uint idx0 = k | j;
            uint idx1 = idx0 | mh;
            
            float2 even = g_data[idx0];
            float2 odd  = ComplexMul(w, g_data[idx1]);
            
            g_data[idx0] = even + odd;
            g_data[idx1] = even - odd;
        }
        GroupMemoryBarrierWithGroupSync();
    }
    
    // 3. Output
    g_output[gid.y * N + id.x] = g_data[id.x];
}
```

### 2D FFT = 1D 가로 + 1D 세로

2D 신호 F(x, y) 는 행 FFT → 열 FFT 순차:

```
Pass 1: 각 행 (N 개) 에 대해 1D FFT  →  중간 결과
Pass 2: 각 열 (N 개) 에 대해 1D FFT  →  최종 F(kx, ky)
```

512×512 이미지 → 512 + 512 = 1024 개의 1D FFT Dispatch.

## 응용 1 — Ocean Wave (Tessendorf 2001)

### Phillips Spectrum (파도 에너지 분포)

```
P(k) = A × exp(-1/(k×L)²) × |k·w|² / k⁴
   where L = V² / g,  V = wind speed,  w = wind direction
```

주파수 공간에서 랜덤 높이 초기화.

```hlsl
float2 PhillipsSpectrum(float2 k, float2 windDir, float windSpeed, float amplitude)
{
    float k2 = dot(k, k);
    if (k2 < 1e-6) return 0;
    
    float g = 9.81;
    float L = windSpeed * windSpeed / g;
    float damping = 0.001;
    float l2 = L * L * damping * damping;
    
    float kDotW = dot(normalize(k), windDir);
    return amplitude * exp(-1 / (k2 * L * L)) 
         * kDotW * kDotW / (k2 * k2) 
         * exp(-k2 * l2);
}
```

### 초기 복소수 생성

```hlsl
float2 h0_tilde(float2 k, ...) {
    float2 xi = GaussianRandom2(k);   // 표준정규 분포
    return xi * sqrt(PhillipsSpectrum(k, ...) / 2);
}
```

### 시간 변화 — Dispersion Relation

```
ω(k) = √(g × |k|)
h̃(k, t) = h̃₀(k) × e^(iωt) + h̃₀(-k)* × e^(-iωt)
```

### IFFT → 높이 맵

```
h(x, t) = IFFT{ h̃(k, t) }
```

Compute shader 에서 매 프레임:
1. `k` 그리드 순회 → `h̃(k, t)` 계산
2. 2D IFFT 적용 → 높이 텍스처
3. 추가로 x, z 방향 변위 계산 (Choppy waves) → 파도 날카로움

### 법선 계산 (Gradient)

파도 기울기:
```
∂h/∂x = IFFT{ ikₓ × h̃ }
∂h/∂z = IFFT{ ikᵤ × h̃ }
Normal = normalize((-∂h/∂x, 1, -∂h/∂z))
```

### Jacobian / Foam

```
J = (1 + ∂D_x/∂x) × (1 + ∂D_z/∂z) - (∂D_x/∂z) × (∂D_z/∂x)
```

J < 0 인 곳은 거품 (Foam) 생성 위치.

### 렌더링

텍스처 기반 displacement + 법선 + FFT 결과 샘플:
```hlsl
// Vertex shader
float4 VS_Ocean(VertexInput input) {
    float2 uv = input.pos.xz / g_worldScale;
    float3 disp = g_displacementTex.SampleLevel(s_linear, uv, 0).xyz;
    float3 worldPos = input.pos + disp;
    return mul(g_viewProj, float4(worldPos, 1));
}

// Pixel shader  
float4 PS_Ocean(VSOutput input) {
    float3 N = g_normalTex.Sample(s_linear, input.uv).xyz * 2 - 1;
    float foam = saturate(-g_foamTex.Sample(...).r);
    // Fresnel + Sky reflection + Subsurface
    // ...
}
```

## 응용 2 — Bloom

HDR 픽셀 중 임계 초과 부분만 확산 블러 → 빛 바림.

### 기본 방식 (Kawase / Dual Filter)

여러 단계 다운샘플 + 업샘플 (Pyramidal Gaussian 근사).
간단하지만 정확도 제한.

### FFT Bloom (정석)

```
Bloom(x) = Image(x) ⊗ Kernel(x)
         = IFFT{ FFT(Image) × FFT(Kernel) }
```

Kernel = Gaussian 또는 "star kernel" (인공 렌즈 플레어 모사).

장점:
- **Kernel 크기 무관** — 수백 픽셀 Gaussian 도 동일 비용
- 커스텀 kernel (렌즈 반짝, 캐릭터 실루엣) 가능

단점:
- FFT 자체 비용 (1080p 에선 버거움)
- 메모리 (복소수 저장)

### 하이브리드

작은 블러는 Kawase, 큰 블러 (전체 화면 가우시안) 는 FFT.

## 응용 3 — Depth of Field (Circular Bokeh)

DoF 의 카메라 렌즈 "흐림 원" 이 비대칭이면 FFT 유리.

```
Blur(x, y) = ∫ Image(x', y') × Circular_Aperture(x-x', y-y', size(depth)) dx' dy'
```

원형 조리개 kernel:
```hlsl
float CircularAperture(float2 offset, float radius)
{
    return length(offset) <= radius ? 1 : 0;
}
```

이게 FFT 공간에서 multiplication → IFFT.

또는 **Tile-Based DoF** (깊이 버킷별 Disperse + Gather) 선택.

## 구현 스케치

```cpp
// Engine/Public/Renderer/FFT/FFT2D.h
class CFFT2D
{
public:
    static unique_ptr<CFFT2D> Create(CDX11Device* dev, u32_t width, u32_t height);

    // In-place 2D FFT / IFFT
    void Forward(ID3D11UnorderedAccessView* dataUAV);
    void Inverse(ID3D11UnorderedAccessView* dataUAV);

    // Convolution: A ⊗ B
    void Convolve(
        ID3D11ShaderResourceView*  aSRV,
        ID3D11ShaderResourceView*  bSRV,
        ID3D11UnorderedAccessView* outUAV);

private:
    u32_t m_width, m_height;
    ComPtr<ID3D11ComputeShader> m_fft1DCS;
};
```

## CPU Reference

C++ FFT 검증용 (디버깅, 단위 테스트):

```cpp
// Engine/Public/Renderer/FFT/CooleyTukey.h
namespace fft
{
    void FFT1D(std::vector<std::complex<f32_t>>& data);
    void IFFT1D(std::vector<std::complex<f32_t>>& data);

    void FFT2D(std::vector<std::complex<f32_t>>& data, u32_t w, u32_t h);
    void IFFT2D(std::vector<std::complex<f32_t>>& data, u32_t w, u32_t h);
}

TEST(FFT, Impulse_Transform)
{
    std::vector<std::complex<f32_t>> x(64, 0);
    x[0] = {1, 0};   // Delta function
    fft::FFT1D(x);
    // 모든 빈이 1 이어야 함
    for (auto& c : x) EXPECT_NEAR(c.real(), 1.f, 1e-4f);
}
```

## 성능

| 기법 | 해상도 | GPU 시간 |
|---|---|---|
| Ocean 2D IFFT 256×256 | — | 0.3 ms |
| Ocean 2D IFFT 512×512 | — | 1.2 ms |
| Bloom FFT 1920×1080 | — | 3~5 ms |
| DoF FFT | 1080p | 2~3 ms |

해상도가 올라가면 O(N² log N). VR 등 고해상도엔 신중.

## 디버그

- **FFT Magnitude Visualization**: |F(k)|² 로그 스케일 표시 (주파수 특성 확인)
- **Ocean Slider**: wind speed / direction / amplitude / choppiness 실시간 조작
- **Bloom Kernel Preview**: 커스텀 kernel 로드 후 2D 이미지로 표시

## ImGui 튜너

```
[Ocean Config]
Wind Speed      [────●───] 15.0 m/s
Wind Direction  [──●─────] 45°
Amplitude       [─●──────] 0.5
Choppiness      [────●───] 1.3
Grid Size       [256 ▼] / [512 ▼]
Foam Threshold  [────●───] 0.1
[Bake]

[Bloom Config]
Threshold       [─●──────] 1.0 (HDR)
Kernel          [Gaussian ▼] / [Star] / [Hexagonal] / [Custom...]
Radius          [───●────] 80 px
Intensity       [──●─────] 0.3
```

## 구현 순서

1. CPU `std::complex<f32_t>` 기반 1D FFT (Cooley-Tukey)
2. CPU 2D FFT (row → col)
3. 단위 테스트 (impulse, sinusoid)
4. GPU 1D FFT Compute (group shared memory)
5. GPU 2D FFT (2 dispatch)
6. Phillips spectrum 생성
7. Ocean IFFT displacement
8. 법선 / Jacobian 계산
9. Ocean 렌더 패스 (displacement tess + PBR)
10. FFT Bloom (임계 분리 → kernel → IFFT)
11. DoF Circular Bokeh (선택)

## 참고 문헌

- **Cooley & Tukey 1965** — An Algorithm for the Machine Calculation of Complex Fourier Series
- **Tessendorf 2001** — Simulating Ocean Water
- **Jensen & Golias 2001** — Deep-Water Animation and Rendering (Foam)
- **Kasaoka & Yoshida 2010** — GPU FFT Ocean Wave
- **Marius Bjørge 2015** — Bandwidth-Efficient Rendering (Dual Filter Bloom)
- **GPU Gems 3** Chapter 20 — Fast Fourier Transforms
