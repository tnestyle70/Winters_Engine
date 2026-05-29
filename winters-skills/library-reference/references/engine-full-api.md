# WintersEngine Full API Reference

---

## 공개 API (Engine/Include/)

### WintersMath.h (헤더 온리)
```cpp
struct Vec2 { float x, y; operator+/-/*(float); };
struct Vec3 {
    float x, y, z;
    operator+/-/*(float), operator+=;
    Length(), Normalized(), ToXMVECTOR(), ToXMFLOAT3();
    static Dot, Cross;
};
struct Vec4 { float x, y, z, w; ToXMFLOAT4(); };
struct Mat4 {
    XMFLOAT4X4 m;
    operator*(Mat4), ToXMMATRIX();
    static Identity, Translation, Scale, RotationX/Y/Z, LookAt, Perspective, Orthographic;
    TransformPoint, TransformDirection, Inverse, Transpose;
};
```

### CTransform (`CTransform.h`)
```cpp
class WINTERS_API CTransform {
    void SetPosition/SetRotation/SetScale(...);  // → m_bDirty = true
    const Mat4& GetWorldMatrix();                 // Dirty면 SRT 재계산
};
```

### CCamera (`CCamera.h`)
```cpp
class WINTERS_API CCamera {
    void SetPerspective(fovY, aspect, nearZ, farZ);
    void Update(float dt, const CInput& input);  // RMB+마우스 회전, WASD 이동
    Mat4 GetViewProjection();
    Vec3 GetForward/GetRight/GetUp() const;
};
```

### CInput (`CInput.h`)
```cpp
class WINTERS_API CInput {
    static CInput& Get();
    bool IsKeyDown/IsKeyUp(uint8 vKey) const;
    float32 GetMouseDeltaX/Y() const;
    bool IsRButtonDown() const;
    void EndFrame();  // 엔진 프레임 끝 호출
};
```

### CubeRenderer (`CubeRenderer.h`, pImpl)
```cpp
class WINTERS_API CubeRenderer {
    [[nodiscard]] bool Init(const wchar_t* hlsl = L"Shaders/Default3D.hlsl");
    void UpdateTransform(const Mat4& world);
    void UpdateCamera(const Mat4& vp);
    void Render();
    void Shutdown();
};
```

### TriangleRenderer (`TriangleRenderer.h`, pImpl)
```cpp
class WINTERS_API TriangleRenderer {
    bool Init(const wchar_t* hlsl = L"Shaders/Triangle.hlsl");
    void Render();   // SV_VertexID 기반, VB 없음
    void Shutdown();
};
```

### IWintersApp / WintersRun
```cpp
class WINTERS_API IWintersApp { virtual OnInit, OnUpdate, OnRender, OnShutdown; };
WINTERS_API int32 WintersRun(IWintersApp* app, const EngineConfig& config);
```

---

## 내부 API (Engine/Header/)

### CDX11Device
```cpp
class CDX11Device {
    bool Initialize(const DeviceDesc&);
    void BeginFrame(r, g, b, a);  // RT+DS Clear + 뷰포트 재설정
    void EndFrame();               // Present (SwapEffect: DISCARD)
    ID3D11Device* GetDevice() const;
    ID3D11DeviceContext* GetContext() const;
    ID3D11RenderTargetView* GetBackRTV() const;
    ID3D11DepthStencilView* GetDSV() const;
};
```

### DX11Shader
```cpp
class DX11Shader {
    bool Load(dev, hlsl, vs="VS", ps="PS");
    void Bind/Unbind(ctx) const;
    ID3DBlob* GetVSBlob() const;
    ID3D11VertexShader* GetVS() const;
    ID3D11PixelShader* GetPS() const;
};
```

### DX11Buffer
```cpp
class DX11Buffer {
    bool CreateVertex(dev, data, stride, count);
    bool CreateIndex(dev, data, count, use32=false);
    void BindVertex/BindIndex(ctx) const;
    void DrawIndexed(ctx) const;  // IB 없으면 Draw 폴백
};
```

### DX11Pipeline
```cpp
class DX11Pipeline {
    bool Create(dev, vsBlob);    // POSITION + COLOR
    bool Create3D(dev, vsBlob);  // POSITION + NORMAL + COLOR + DSS
    void Bind(ctx) const;
};
```

### DX11ConstantBuffer\<T\> (헤더 온리)
```cpp
template<typename T>  // sizeof(T) % 16 == 0
class DX11ConstantBuffer {
    [[nodiscard]] bool Create(dev);
    void Update(ctx, const T& data);
    void BindVS/BindPS/Bind(ctx, UINT slot) const;
};
// CBPerFrame { viewProjection } → b0 | CBPerObject { world } → b1
```

### CubeGeometry (헤더 온리)
```cpp
namespace CubeGeometry {
    struct Vertex3D { float pos[3], normal[3], color[4]; };  // 40 bytes
    constexpr array<Vertex3D, 24> Vertices;  // 6면 × 4v
    constexpr array<uint16_t, 36> Indices;
}
```

### CEngineApp
```cpp
class CEngineApp {
    bool Initialize(IWintersApp*, const EngineConfig&);
    int32 Run();  // PumpMessages → Tick → Update → Render → EndFrame
    static CEngineApp& Get();
    CDX11Device& GetDevice();
    CWin32Window& GetWindow();
};
```

---

## 셰이더

| 파일 | 입력 | cbuffer |
|------|------|---------|
| Triangle.hlsl | SV_VertexID (하드코딩 정점) | 없음 |
| Default3D.hlsl | POSITION + NORMAL + COLOR | b0: ViewProjection, b1: World |
