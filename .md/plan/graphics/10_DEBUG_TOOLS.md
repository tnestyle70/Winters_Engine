# Graphics 디버그 도구 — RenderGraph 뷰어 + Capture + Hot Reload

## 목표

렌더러를 **실시간으로 관찰/조작/재구성** 가능하게. CLAUDE.md "빌드 1번으로 모든 값 튜닝" 철학의
핵심 실현.

## Renderer Debugger 메인 창

```
┌─ Renderer Debugger ─────────────────────────────────────┐
│ [Pause Render]  [Take Screenshot]  [Export HDR (.exr)]  │
├─────────────────────────────────────────────────────────┤
│ Tabs: [RenderGraph] [Buffers] [Materials] [Lights]      │
│       [TAA] [Bloom] [PathTracer] [GI] [Ocean] [Profile] │
├─────────────────────────────────────────────────────────┤
│ [탭 내용]                                               │
└─────────────────────────────────────────────────────────┘
```

## RenderGraph 탭

패스 DAG 시각화 + 각 패스 on/off:

```
[Frame Graph 2026-04-17 14:23:05]

  GBufferFill ────┬─ Normal    ──┬─ SSAO         ────┐
                  │              │                    │
                  ├─ Albedo    ──┼─ Lighting     ────┤
                  │              │                    │
                  ├─ Depth     ──┼─ HiZBuild     ──── SSR ───┤
                  │                                            │
                  └─ MotionVec ─────────── TAA ─────────────── │
                                                                │
  DDGI_Probe ─── DDGI_Sample ──────────────── Lighting ────────┤
                                                                │
                                               Bloom ──── Tonemap ─── Backbuffer

[체크박스로 각 패스 bypass]
[우클릭 패스 → 자원 추적]
```

각 노드 클릭 시 상세 정보 (input/output 자원, dispatch size, GPU 시간).

## Buffers 탭

G-Buffer 및 중간 버퍼 확인:

```
[Buffer Viewer]
Buffer: [GBuffer_Normal ▼]

Available buffers:
  GBuffer_Albedo      RGB8    1920×1080
  GBuffer_Normal      RG16F   1920×1080    (오팬 버퍼)
  GBuffer_Depth       D32F    1920×1080
  GBuffer_MotionVec   RG16F   1920×1080
  HDR_Lighting        RGB16F  1920×1080
  HDR_Bloom           RGB16F  960×540
  SSAO_Out            R8      1920×1080
  SSR_Out             RGB16F  1920×1080
  DDGI_Irradiance     R11G11B10  (probe×8×8×6 atlas)
  TAA_History         RGB16F  1920×1080
  ...

Display Options:
  [Channel R ▼] (R/G/B/A/RGB/RGBA/Depth)
  [Range: 0.0 ~ 1.0]     (slider for tone mapping)
  [Log Scale: □]         (HDR 값 시각화 용)
  
[256×256 미니 프리뷰]
[Pixel Inspector]
  (x, y) = (1024, 512)
  Value = (0.824, 0.512, 0.123)
```

Depth buffer 는 near/far 재투영 후 표시. Motion Vector 는 RG 를 색으로 매핑.

## Materials 탭

현재 씬의 모든 재질 리스트 + 실시간 조작:

```
[Material List]
  Irelia_Body          GGX  albedo=(0.75,0.60,0.55) m=0.1 r=0.3
  Irelia_Blade         GGX  albedo=(0.85,0.85,0.85) m=0.9 r=0.1
  Terrain_Grass        Lambert (diffuse only)
  ...

[Selected: Irelia_Body]
  BaseColor Texture    [browse...]    [unused → tint]
  Normal Texture       [normal.png ▼]  [strength: 1.0]
  MetallicRoughnessAO  [mra.png ▼]
  Emissive Factor      (0, 0, 0)
  ────────────────────────────────────
  Metallic Override     [────●─] 0.10  ← 실시간 반영
  Roughness Override    [──●───] 0.30
  ────────────────────────────────────
  [Preview Sphere] [미니 256×256 뷰포트]
```

## Lights 탭

모든 광원 제어:

```
Lights (3):
  Sun          Directional  dir=(0.3,-0.8,0.5)  intensity=10.0
  TorchA       Point        pos=(5,1,0)          range=20  color=(1.0,0.5,0.2)
  ShopLight    Spot         ...

[Selected: Sun]
  Direction     [drag 3D ball]
  Color         [color picker]
  Intensity     [────●──] 10.0 (lumens/m²)
  Cast Shadow   [x]
  Shadow Bias   [────●──] 0.001
```

## TAA 탭

```
[TAA Config]
Enabled                  [x]
Jitter Sequence          [Halton(2,3) ▼] / [Sobol]
Blend Alpha              [───●───] 0.1
Clipping Method          [Variance AABB ▼] / [Min-Max AABB] / [None]
Variance Gamma           [──●────] 1.5
Disocclusion Threshold   [───●───] 0.05 (depth)
Sharpening               [─●─────] 0.2 (0 = off)

[Debug Overlays]
  [ ] Show Motion Vector
  [ ] Show Disocclusion (red = rejected history)
  [ ] Show History Only (no current frame)
  [ ] Show Ghosting (current - history diff)
```

## Bloom 탭

```
[Bloom Config]
Method           [Kawase Pyramid ▼] / [FFT Convolution]
Threshold HDR    [──●────] 1.0
Intensity        [─●─────] 0.3
Radius           [───●───] 80 px
Kernel           [Gaussian ▼] / [Star] / [Hexagonal] / [Custom...]

[Custom Kernel Preview]  [이미지 로드 버튼]
[Output Preview]
```

## PathTracer 탭

```
[Path Tracer Settings]
Max Bounces         [─●─────] 8
Samples per Frame   [1 ▼]
Total SPP           1234 / ∞ (accumulating)
Time Elapsed        2m 14s
Russian Roulette    [x] Start at bounce 3

[Sampling]
Method              [Halton ▼] / [Sobol] / [Stratified] / [Uniform]
NEE                 [x] (Next Event Estimation)
MIS                 [x] (Multiple Importance Sampling)

[BVH Info]
Nodes: 48,293    Primitives: 180,456    Depth: 32    Build time: 120 ms

[Camera]
[ ] Lock camera (pause accumulation on move)
[Reset Accumulation]

[Output]
[Save HDR as .exr]
[Save Tonemapped as .png]
```

## GI 탭 (DDGI / VXGI)

```
[DDGI Config]
Probe Grid            [16 × 8 × 16 ▼]    2048 probes
Rays per Probe        [128 ▼]
Hysteresis            [──●────] 0.8    (temporal smoothing)
Normal Bias           [─●─────] 0.01
Self-shadow Bias      [─●─────] 0.05

[Debug]
  [ ] Show probe grid (spheres)
  [ ] Show probe irradiance (octahedral atlas)
  [ ] Show probe visibility (Chebyshev)
  [ ] Diffuse GI Only (skip direct)
```

## Ocean 탭

```
[Phillips Spectrum]
Wind Speed       [────●───] 15 m/s
Wind Direction   [──●─────] 45°
Amplitude        [─●──────] 0.5
Grid Size        [256 ▼] / [512 ▼] / [1024 ▼]

[Rendering]
Tessellation Level   [───●───] 32
Foam Threshold       [──●────] -0.3
SSS Amount           [───●───] 0.4
Sky Reflection       [texture ▼]

[Generated Textures]
  [Displacement Map (XZY)]      preview 256×256
  [Normal Map]                  preview 256×256
  [Foam / Jacobian]             preview 256×256
```

## Profile 탭

프레임 타임라인 + 각 패스 GPU 시간:

```
[Frame 3201 — 16.78 ms]

 CPU ─────────────────────────────────────────────
  Game Update       2.1 ms
  Physics Step      0.8 ms
  RenderGraph Build 0.3 ms
  Submit            0.2 ms
  
 GPU ─────────────────────────────────────────────
  GBuffer Fill            3.1 ms ▬▬▬▬▬▬
  Shadow Map              1.4 ms ▬▬▬
  SSAO                    0.8 ms ▬
  DDGI Probe Update       2.2 ms ▬▬▬▬
  DDGI Sample             0.6 ms ▬
  Direct Light            1.8 ms ▬▬▬▬
  SSR                     1.3 ms ▬▬▬
  TAA                     0.9 ms ▬▬
  Bloom                   1.1 ms ▬▬
  Tonemap                 0.3 ms
  UI Composit             0.2 ms

 History (last 120 frames)
 [그래프: CPU 총 시간 + GPU 총 시간]
```

ImPlot 으로 시계열 그래프.

## Shader Hot Reload

개발 중 .hlsl 파일 변경 → 런타임 자동 재컴파일 + 리로드.

```cpp
class CShaderHotReload
{
public:
    void Initialize(const std::string& shadersDir);
    void Tick();   // 파일 변경 감지

    // 특정 셰이더의 change callback 등록
    using ReloadCallback = std::function<void(const std::string& path, ID3DBlob* newBlob)>;
    void RegisterCallback(const std::string& path, ReloadCallback cb);

private:
    std::unordered_map<std::string, std::filesystem::file_time_type> m_lastModified;
    std::string m_shadersDir;
};
```

### 동작
1. 백그라운드 파일 watcher (또는 폴링 500ms)
2. 수정 감지 → 재컴파일
3. 성공: 기존 ID3D11VertexShader/PixelShader 교체 + 에디터 노티
4. 실패: 에러 로그 + 기존 셰이더 유지

```
[Shader Hot Reload — 14:23:45]
  ✗ SSAO.hlsl (Compute)
    Line 23:8 - error X3004: undeclared identifier 'undefined_var'
  ✓ TAA.hlsl (Compute) recompiled in 87 ms
```

## Frame Capture (RenderDoc / PIX 통합)

코드에서 특정 프레임 capture 트리거:

```cpp
#ifdef WINTERS_EDITOR
class CFrameCapture
{
public:
    // 이 프레임 끝에서 RenderDoc/PIX capture 시작
    void CaptureNextFrame();

    // 10 프레임 후 capture (trigger 지연)
    void CaptureAfterFrames(u32_t count);
};
#endif
```

ImGui 버튼 "Capture Next Frame" → `.rdc` / `.wpix` 파일 출력 → 외부 도구로 열어 분석.

## GPU 마커

RenderDoc 에서 시각화:

```cpp
class CScopedGPUMarker
{
public:
    CScopedGPUMarker(CommandList& cmd, const std::string& name) {
        cmd.BeginEvent(name);
    }
    ~CScopedGPUMarker() { /* cmd.EndEvent() */ }
};

#define GPU_MARKER(cmd, name) CScopedGPUMarker _marker(cmd, name)

// 사용:
void SSAOSystem::Execute(CommandList& cmd)
{
    GPU_MARKER(cmd, "SSAO");
    // ... dispatch
}
```

## 비교 뷰어 (Stage 4 ↔ Stage 5)

Path Tracer Reference 와 실시간 결과 나란히:

```
[Comparison View]

 ┌─ Reference (Path Trace, 1024 SPP) ─┐ ┌─ Realtime (DDGI + SSR) ─┐
 │                                    │ │                         │
 │     [이미지]                        │ │     [이미지]             │
 │                                    │ │                         │
 └────────────────────────────────────┘ └─────────────────────────┘
 
 [Split slider ─●──────]   (좌우 인터랙티브 비교)
 
 [Metrics]
   MSE:    0.00123
   PSNR:   38.2 dB
   LPIPS:  0.041
```

## Screenshot / HDR Export

- **LDR Screenshot** (.png): tonemapped 결과
- **HDR Screenshot** (.exr / .hdr): linear float, 편집용
- **EXR Multi-Channel**: Albedo/Normal/Depth/HDR 한 파일에 여러 레이어

Path Tracer 결과를 .exr 로 저장 → Substance / Photoshop 에서 후처리.

## 단축키

| 키 | 기능 |
|---|---|
| F1 | Debugger 토글 |
| F3 | Buffer Viewer 빠른 열기 |
| F5 | Take Screenshot |
| F6 | Frame Capture |
| F7 | Shader Hot Reload 수동 트리거 |
| F8 | Pause Render |
| Alt+1~9 | Buffer 빠른 전환 (Albedo/Normal/Depth/...) |

## 연습모드 / 에디터 통합

연습모드 씬에선 Renderer Debugger 가 기본 표시. 에디터 툴과 결합 — 재질 편집이 곧 게임 재질 변경.

## 성능 / 빌드 플래그

```cpp
#ifdef WINTERS_EDITOR
    // 디버거 + 마커 + Hot Reload 전부 활성
    #define RENDER_DEBUG(x) (x)
#else
    // 릴리스 빌드 — 전부 제거
    #define RENDER_DEBUG(x) do {} while(0)
#endif
```

GPU 마커는 Debug 빌드만, 프로파일러는 별도 `WINTERS_PROFILER` 플래그.

## 구현 순서

1. Buffer Viewer (RT 하나 선택 → ImGui 표시)
2. Material Editor (실시간 슬라이더)
3. Light Editor
4. GPU 마커 (RenderDoc 통합)
5. Profiler (GPU 쿼리 + ImPlot)
6. Shader Hot Reload
7. RenderGraph 뷰어 (패스 목록 + bypass)
8. Frame Capture 연동
9. HDR Export
10. Path Tracer vs Realtime 비교 뷰어
11. 단축키 시스템

## 참고 자료

- **RenderDoc** — 무료 그래픽스 디버거, C API 연동
- **PIX for Windows** — Microsoft GPU 프로파일러
- **Intel GPA** / **NVIDIA Nsight** — 벤더별 도구
- **ImGui + ImPlot** — 에디터 UI
- **tinyexr** / **OpenEXR** — HDR 저장
