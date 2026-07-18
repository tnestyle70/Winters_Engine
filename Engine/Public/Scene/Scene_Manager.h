#pragma once
#include "Engine_Defines.h"
#include "IScene.h"

NS_BEGIN(Engine)

class CScene_Manager
{
public:
    CScene_Manager() = default;
    ~CScene_Manager();

    HRESULT Change_Scene(uint32_t iNextSceneID, unique_ptr<IScene> pScene);

    void Update(f32_t dt);
    void LateUpdate(f32_t dt);
    void Render();
    void ImGui();

    IScene* Get_CurrentScene() const { return m_pCurrentScene.get(); }
    uint32_t Get_CurrentSceneID() const { return m_iCurrentSceneID; }

    static unique_ptr<CScene_Manager> Create();

private://Convention Gotchas 반드시 {} 유니폼 초기화, 모든 변수 초기화!!
    unique_ptr<IScene> m_pCurrentScene = {nullptr};
    uint32_t m_iCurrentSceneID = {0};
};

NS_END