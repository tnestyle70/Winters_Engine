#pragma once

#include "WintersEngine.h"

class CEldenRingEditorApp final : public IWintersApp
{
public:
	CEldenRingEditorApp() = default;
	~CEldenRingEditorApp() override = default;

	bool OnInit() override;
	void OnUpdate(f32_t deltaTime) override;
	void OnRender() override;
	void OnImGui() override;
	void OnShutdown() override;
};