#pragma once

#include "IScene.h"

#include <memory>

class CRHITestCubeRenderer;

class CEldenRingAssetProbeScene final : public IScene
{
public:
	static std::unique_ptr<CEldenRingAssetProbeScene> Create();
	~CEldenRingAssetProbeScene() override;
	
	bool OnEnter() override;
	void OnExit() override;
	void OnUpdate(f32_t deltaTime) override;
	void OnRender() override;
	void OnImGui() override;

private:
	CEldenRingAssetProbeScene() = default;

	std::unique_ptr<CRHITestCubeRenderer> m_pCubeRenderer;
};
