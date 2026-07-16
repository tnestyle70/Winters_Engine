#pragma once

#include "WintersEngine.h"

namespace LoLEditor
{
	constexpr uint32 kWindowWidth = 1600;
	constexpr uint32 kWindowHeight = 900;
}

class CLoLEditorApp final : public IWintersApp
{
public:
	CLoLEditorApp() = default;
	~CLoLEditorApp() override = default;

	bool OnInit() override;
	void OnShutdown() override;
};
