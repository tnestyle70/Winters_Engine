#include "EldenRingProbeScene.h"

#include "EldenRingRHITestCubeRenderer.h"
#include "GameInstance.h"
#include "RHI/IRHIDevice.h"

#include <Windows.h>

std::unique_ptr<CEldenRingAssetProbeScene> CEldenRingAssetProbeScene::Create()
{
    return std::unique_ptr<CEldenRingAssetProbeScene>(new CEldenRingAssetProbeScene());
}

CEldenRingAssetProbeScene::~CEldenRingAssetProbeScene() = default;

bool CEldenRingAssetProbeScene::OnEnter()
{
    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();
    if (!pDevice)
        return false;

    m_pCubeRenderer = CRHITestCubeRenderer::Create(pDevice);

#if defined(_DEBUG)
    if (m_pCubeRenderer && m_pCubeRenderer->IsReady())
        OutputDebugStringA("[EldenRingAssetProbeScene] RHI cube scene ready\n");
    else
        OutputDebugStringA("[EldenRingAssetProbeScene] RHI cube scene failed\n");
#endif

    return m_pCubeRenderer != nullptr;
}

void CEldenRingAssetProbeScene::OnExit()
{
    m_pCubeRenderer.reset();
}

void CEldenRingAssetProbeScene::OnUpdate(f32_t deltaTime)
{
    if (m_pCubeRenderer)
        m_pCubeRenderer->Update(deltaTime);
}

void CEldenRingAssetProbeScene::OnRender()
{
    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();

    if (m_pCubeRenderer)
        m_pCubeRenderer->Render(pDevice);
}

void CEldenRingAssetProbeScene::OnImGui()
{
}
