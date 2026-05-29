#pragma once
#include "IScene.h"
#include "Scene/Loader.h"
#include <memory>

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

class CScene_Loading final : public IScene
{
public:
    using SceneFactory = std::function<std::unique_ptr<IScene>()>;

    static std::unique_ptr<CScene_Loading> Create(eSceneID     eNextSceneID,
        SceneFactory pFactory);
    ~CScene_Loading() override = default;

    bool OnEnter()          override;
    void OnExit()           override;
    void OnUpdate(f32_t dt) override;
    void OnRender()         override {}
    void OnImGui()          override;

private:
    CScene_Loading() = default;

    eSceneID                 m_eNextSceneID = eSceneID::End;
    std::unique_ptr<CLoader> m_pLoader;
    bool                     m_bTransitioned = false;
};