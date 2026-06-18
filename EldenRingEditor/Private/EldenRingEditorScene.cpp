#include "EldenRingEditorScene.h"

#include <Windows.h>

std::unique_ptr<CEldenRingEditorScene> CEldenRingEditorScene::Create()
{
	return std::unique_ptr<CEldenRingEditorScene>(new CEldenRingEditorScene());
}

bool CEldenRingEditorScene::OnEnter()
{
#ifdef _DEBUG
    ::OutputDebugStringA("[EldenRingEditorScene] OnEnter\n");
#endif
    return true;
}

void CEldenRingEditorScene::OnExit()
{
#ifdef _DEBUG
    ::OutputDebugStringA("[EldenRingEditorScene] OnExit\n");
#endif
}

void CEldenRingEditorScene::OnUpdate(f32_t)
{}

void CEldenRingEditorScene::OnRender()
{}

void CEldenRingEditorScene::OnImGui()
{}