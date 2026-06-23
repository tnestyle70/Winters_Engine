#pragma once

#include "WintersTypes.h"

#include <memory>

class IRHIDevice;
class CRHIMaterialResource;
class CRHIMeshResource;
class CRHISceneRenderer;

class CRHITestCubeRenderer final
{
public:
	~CRHITestCubeRenderer();

	static std::unique_ptr<CRHITestCubeRenderer> Create(IRHIDevice* pDevice);

	bool IsReady() const;
	void Update(f32_t deltaTime);
	void Render(IRHIDevice* pDevice);

private:
	CRHITestCubeRenderer() = default;

	bool Initialize(IRHIDevice* pDevice);
	void Shutdown();

private:
	static constexpr u32_t kMaterialCount = 2;

	IRHIDevice* m_pDevice = nullptr;
	std::unique_ptr<CRHIMeshResource> m_pMeshResource;
	std::unique_ptr<CRHIMaterialResource> m_pMaterials[kMaterialCount];
	std::unique_ptr<CRHISceneRenderer> m_pSceneRenderer;
	f32_t m_fRotationSeconds = 0.f;
};
