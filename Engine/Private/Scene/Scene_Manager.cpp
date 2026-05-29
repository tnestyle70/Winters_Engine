#include "Scene/Scene_Manager.h"
#include "GameInstance.h"

CScene_Manager::~CScene_Manager()
{
    if (m_pStaticScene)
        m_pStaticScene->OnExit();
    Safe_Reset(m_pStaticScene);

    if (m_pCurrentScene)
        m_pCurrentScene->OnExit();
    Safe_Reset(m_pCurrentScene);
}

HRESULT CScene_Manager::Change_Scene(uint32_t iNextSceneID, unique_ptr<IScene> pScene)
{
    //이전 씬 정리
    if (m_pCurrentScene)
    {
        m_pCurrentScene->OnExit();
        //이전 sceneID로 리소스 정리
        CGameInstance::Get()->Clear_Resources(m_iCurrentSceneID);
        //Safe_Reset으로 명시적 파괴
        Safe_Reset(m_pCurrentScene);
    }

    m_pCurrentScene = std::move(pScene);
    m_iCurrentSceneID = iNextSceneID;

    if (m_pCurrentScene)
        m_pCurrentScene->OnEnter();

    return S_OK;
}

void CScene_Manager::Update(f32_t dt)
{
    if (m_pStaticScene)
        m_pStaticScene->OnUpdate(dt);
    if (m_pCurrentScene) 
        m_pCurrentScene->OnUpdate(dt);
}

void CScene_Manager::LateUpdate(f32_t dt)
{
    if (m_pStaticScene)
        m_pStaticScene->OnLateUpdate(dt);
    if (m_pCurrentScene) 
        m_pCurrentScene->OnLateUpdate(dt);
}

void CScene_Manager::Render()
{
    if (m_pStaticScene)
        m_pStaticScene->OnRender();
    if (m_pCurrentScene) 
        m_pCurrentScene->OnRender();
}

void CScene_Manager::ImGui()
{
    if (m_pStaticScene)
        m_pStaticScene->OnImGui();
    if (m_pCurrentScene) 
        m_pCurrentScene->OnImGui();
}

HRESULT CScene_Manager::Set_StaticScene(unique_ptr<IScene> pScene)
{
    if (!pScene)
        return E_FAIL;
    if (!pScene->OnEnter())
        return E_FAIL;
    m_pStaticScene = move(pScene);

    return S_OK;
}

unique_ptr<CScene_Manager> CScene_Manager::Create()
{
    return unique_ptr<CScene_Manager>(new CScene_Manager());
}
