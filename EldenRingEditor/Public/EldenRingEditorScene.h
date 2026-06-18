#pragma once

#include "IScene.h"

#include <memory>

class CEldenRingEditorScene final : public IScene
{
public:
	~CEldenRingEditorScene() = default;

	static std::unique_ptr<CEldenRingEditorScene> Create();

	bool OnEnter() override;
	void OnExit() override;
	void OnUpdate(f32_t deltaTime) override;
	void OnRender() override;
	void OnImGui() override;

private:
	CEldenRingEditorScene() = default;
};