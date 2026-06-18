Session - Cube Rendering to first Elden Ring static WMesh smoke

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/EldenRingClient/Public/EldenRingProbeScene.h

기존 코드:

```cpp
#include <memory>
```

아래에 추가:

```cpp
#include <string>
```

기존 코드:

```cpp
class CRHITestCubeRenderer;
```

아래에 추가:

```cpp
namespace Engine
{
	class CFxStaticMeshRenderer;
}
```

기존 코드:

```cpp
	std::unique_ptr<CRHITestCubeRenderer> m_pCubeRenderer;
```

아래에 추가:

```cpp
	std::unique_ptr<Engine::CFxStaticMeshRenderer> m_pStaticMeshRenderer;
	std::string m_strSmokeMeshPath;
	std::wstring m_wstrSmokeDiffusePath;
	bool m_bSmokeMeshReady = false;
```

의도:
- 현재 큐브 렌더러는 DX12 생존 기준점으로 유지한다.
- 첫 Elden Ring 에셋은 `CRHIFxMeshResourceCache`가 이미 지원하는 `.wmesh` 정적 메시로만 확인한다.
- 캐릭터, 멀기트, 트리가드는 이번 세션에서 보스/스켈레탈 검증 대상으로 올리지 않는다. 현재 `CWMeshLoader`의 `MAX_BONES = 512`와 c2130/c3251 고본수 산출물 이슈가 있어 첫 smoke를 막을 수 있다.

1-2. C:/Users/tnest/Desktop/Winters/EldenRingClient/Private/EldenAssetProbeScene.cpp

기존 코드:

```cpp
#include "EldenRingRHITestCubeRenderer.h"
#include "GameInstance.h"
#include "RHI/IRHIDevice.h"
```

아래에 추가:

```cpp
#include "imgui.h"
#include "Renderer/FxStaticMeshRenderer.h"
```

기존 코드:

```cpp
    m_pCubeRenderer = CRHITestCubeRenderer::Create(pDevice);
```

아래에 추가:

```cpp
    m_pStaticMeshRenderer = Engine::CFxStaticMeshRenderer::Create(
        pDevice,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    m_strSmokeMeshPath =
        "Client/Bin/Resource/EldenRing/Assets/LimgraveStatic/"
        "AEG003_063_aeg003_063-geombnd-dcx_AEG003_063/Model/"
        "AEG003_063_aeg003_063-geombnd-dcx_AEG003_063.wmesh";
    m_wstrSmokeDiffusePath.clear();

    m_bSmokeMeshReady = m_pStaticMeshRenderer &&
        m_pStaticMeshRenderer->PreloadMesh(m_strSmokeMeshPath, m_wstrSmokeDiffusePath);

#if defined(_DEBUG)
    OutputDebugStringA(m_bSmokeMeshReady
        ? "[EldenRingAssetProbeScene] Static WMesh smoke ready\n"
        : "[EldenRingAssetProbeScene] Static WMesh smoke failed\n");
#endif
```

기존 코드:

```cpp
void CEldenRingAssetProbeScene::OnExit()
{
    m_pCubeRenderer.reset();
}
```

아래로 교체:

```cpp
void CEldenRingAssetProbeScene::OnExit()
{
    m_pStaticMeshRenderer.reset();
    m_pCubeRenderer.reset();
    m_strSmokeMeshPath.clear();
    m_wstrSmokeDiffusePath.clear();
    m_bSmokeMeshReady = false;
}
```

기존 코드:

```cpp
void CEldenRingAssetProbeScene::OnRender()
{
    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();

    if (m_pCubeRenderer)
        m_pCubeRenderer->Render(pDevice);
}
```

아래로 교체:

```cpp
void CEldenRingAssetProbeScene::OnRender()
{
    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();

    if (m_pCubeRenderer)
        m_pCubeRenderer->Render(pDevice);

    if (m_pStaticMeshRenderer && m_bSmokeMeshReady)
    {
        const Mat4 matView = Mat4::LookAt(
            Vec3{ 0.f, 4.f, -8.f },
            Vec3{ 0.f, 0.f, 0.f },
            Vec3{ 0.f, 1.f, 0.f });
        const Mat4 matProj = Mat4::Perspective(1.04719755f, 16.f / 9.f, 0.1f, 1000.f);
        const Mat4 matViewProj = matView * matProj;
        const Mat4 matWorld = Mat4::Scale(0.02f, 0.02f, 0.02f);

        Engine::FxMeshDrawParams params{};
        params.matWorld = matWorld;
        params.vTint = Vec4{ 1.f, 1.f, 1.f, 1.f };
        params.iBlendPreset = 0u;

        m_pStaticMeshRenderer->BeginFrame(matViewProj, Vec3{ 0.f, 0.f, -4.f });
        m_pStaticMeshRenderer->DrawMesh(m_strSmokeMeshPath.c_str(), params);
        m_pStaticMeshRenderer->EndFrame();
    }
}
```

기존 코드:

```cpp
void CEldenRingAssetProbeScene::OnImGui()
{
}
```

아래로 교체:

```cpp
void CEldenRingAssetProbeScene::OnImGui()
{
    ImGui::Begin("Elden Ring Asset Probe");
    ImGui::TextUnformatted("DX12 cube + static WMesh smoke");
    ImGui::Separator();
    ImGui::Text("Mesh: %s", m_strSmokeMeshPath.c_str());
    ImGui::Text("Ready: %s", m_bSmokeMeshReady ? "yes" : "no");
    ImGui::End();
}
```

1-3. C:/Users/tnest/Desktop/Winters/Engine/Private/AssetFormat/Mesh/WMeshLoader.cpp

기존 코드:

```cpp
    static constexpr uint32_t MAX_BONES = 512;
```

삭제할 코드/범위:

```cpp
이번 세션에서는 삭제/수정하지 않는다.
첫 smoke는 bones=0인 Limgrave AEG 정적 메시로 제한한다.
c2130 Margit, c3251 TreeGuard, 고본수 character WMesh는 별도 skeletal loader 세션에서 다룬다.
```

2. 검증

2-1. 사전 확인

```powershell
Test-Path "Client/Bin/Resource/EldenRing/Assets/LimgraveStatic/AEG003_063_aeg003_063-geombnd-dcx_AEG003_063/Model/AEG003_063_aeg003_063-geombnd-dcx_AEG003_063.wmesh"
```

2-2. 검증 명령

```powershell
msbuild Winters.sln /t:EldenRingClient /p:Configuration=Debug-DX12 /p:Platform=x64
```

2-3. 런타임 확인

```text
1. 기존 회전 큐브가 계속 보인다.
2. OutputDebugStringA에 Static WMesh smoke ready가 출력된다.
3. .wmesh load fail 또는 .wmesh has no drawable mesh 로그가 없거나, 있으면 해당 AEG 후보를 다른 bones=0 WMesh로 교체한다.
4. ImGui 패널에 smoke mesh path와 ready 상태가 표시된다.
```

2-4. 다음 세션 게이트

```text
이 세션이 통과해야 Map Assembly, World Partition, PBR material resolver를 얹는다.
첫 정적 메시가 실패하면 Editor 기능을 늘리지 말고 CRHIFxMeshResourceCache, CWMeshLoader, DX12 bind group/texture path를 먼저 좁혀서 고친다.
```
