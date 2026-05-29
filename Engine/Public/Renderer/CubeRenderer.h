#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"

struct Mat4;

// ─────────────────────────────────────────────────────────────────
//  CubeRenderer  |  텍스처 없는 컬러 큐브 렌더링
//
//  pImpl 패턴으로 DX11 타입을 Client에서 숨긴다.
//  CubeGeometry + DX11Buffer(IndexBuffer) + DX11ConstantBuffer + Default3D.hlsl
//
//  사용 예시 (CGameApp):
//    CubeRenderer cube;
//    cube.Init(L"Shaders/Default3D.hlsl");
//
//    // OnRender:
//    cube.UpdateTransform(transform.GetWorldMatrix());
//    cube.UpdateCamera(camera.GetViewProjection());
//    cube.Render();
// ─────────────────────────────────────────────────────────────────

class WINTERS_ENGINE CubeRenderer
{
public:
    CubeRenderer();
    ~CubeRenderer();

    [[nodiscard]] bool Init(const wchar_t* hlslPath = L"Shaders/Default3D.hlsl");

    void UpdateTransform(const Mat4& worldMatrix);
    void UpdateCamera(const Mat4& viewProjection);
    void Render();
    void Shutdown();

private:
    struct Impl;
    Impl* m_pImpl = nullptr;
};
