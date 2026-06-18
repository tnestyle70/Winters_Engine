#include "EldenRingEditorApp.h"

#include "EldenRingEditorScene.h"
#include "GameInstance.h"

#include <Windows.h>

namespace
{
	constexpr uint32 kEditorSceneID = 0;
}

bool CEldenRingEditorApp::OnInit()
{
#ifdef _DEBUG
	::OutputDebugStringA("[EldenRingEditor] OnInit\n");
#endif

	return SUCCEEDED(CGameInstance::Get()->Change_Scene(
		kEditorSceneID,
		CEldenRingEditorScene::Create()));
}

void CEldenRingEditorApp::OnUpdate(f32_t)
{}

void CEldenRingEditorApp::OnRender()
{}

void CEldenRingEditorApp::OnImGui()
{}

void CEldenRingEditorApp::OnShutdown()
{
#ifdef _DEBUG
	::OutputDebugStringA("[EldenRingEditor] OnShutdown\n");
#endif

	CGameInstance::Get()->Change_Scene(kEditorSceneID, nullptr);
}
