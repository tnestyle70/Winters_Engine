#include "Scene/Scene_Loading.h"
#include "GameInstance.h"
#include <cstdio>

unique_ptr<CScene_Loading> CScene_Loading::Create(eSceneID eNextSceneID, SceneFactory pFactory)
{
    auto pInstance = unique_ptr<CScene_Loading>(new CScene_Loading());
    pInstance->m_eNextSceneID = eNextSceneID;
    pInstance->m_pLoader = CLoader::Create(eNextSceneID, std::move(pFactory));
    return pInstance;
}

bool CScene_Loading::OnEnter()
{
    m_bTransitioned = false;
    return true;
}

void CScene_Loading::OnExit()
{
    // Loader 소멸자가 Job 완료를 대기 (임시: 스핀 대기)
    m_pLoader.reset();
}

void CScene_Loading::OnUpdate(f32_t /*dt*/)
{
    if (!m_pLoader || m_bTransitioned) return;

    if (m_pLoader->IsFinished())
    {
        m_bTransitioned = true;
        auto pNext = m_pLoader->Build_NextScene();

        CGameInstance::Get()->Change_Scene(
            static_cast<uint32_t>(m_pLoader->Get_NextSceneID()),
            std::move(pNext));
        // 이 지점 이후 this 는 파괴될 수 있음 — 멤버 접근 금지
    }
}

void CScene_Loading::OnImGui()
{
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 size(420.f, 90.f);
    ImGui::SetNextWindowPos(
        ImVec2((io.DisplaySize.x - size.x) * 0.5f, (io.DisplaySize.y - size.y) * 0.5f),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("##Loading", nullptr, flags);
    ImGui::Text("Loading Assets...");
    ImGui::Separator();

    const f32_t fProgress = m_pLoader ? m_pLoader->Get_Progress() : 0.f;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.0f %%", fProgress * 100.f);
    ImGui::ProgressBar(fProgress, ImVec2(-1.f, 24.f), buf);
    ImGui::End();
}