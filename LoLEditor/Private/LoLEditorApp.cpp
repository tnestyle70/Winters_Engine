#include "LoLEditorApp.h"

#include "LoLMapEditorScene.h"
#include "GameInstance.h"

#include <Windows.h>

#include <memory>

namespace
{
	constexpr uint32 kEditorSceneID = 0;
}

bool CLoLEditorApp::OnInit()
{
#ifdef _DEBUG
	::OutputDebugStringA("[LoLEditor] OnInit\n");
#endif

	return SUCCEEDED(CGameInstance::Get()->Change_Scene(
		kEditorSceneID,
		std::unique_ptr<IScene>(new CLoLMapEditorScene())));
}

void CLoLEditorApp::OnShutdown()
{
#ifdef _DEBUG
	::OutputDebugStringA("[LoLEditor] OnShutdown\n");
#endif

	CGameInstance::Get()->Change_Scene(kEditorSceneID, nullptr);
}
