#pragma once

#include "RHI/RHIDescriptors.h"

#include <memory>

class IRHIDevice;

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
	IRHIDevice* m_pDevice = nullptr;
	RHIBufferHandle m_hVertexBuffer{};
	RHIBufferHandle m_hIndexBuffer{};
	RHIBufferHandle m_hCameraConstantBuffer{};
	RHITextureHandle m_hCheckerTexture{};
	RHISamplerHandle m_hCheckerSampler{};
	RHIBindGroupLayoutHandle m_hCameraBindGroupLayout{};
	RHIBindGroupHandle m_hCameraBindGroup{};
	RHIShaderHandle m_hVertexShader{};
	RHIShaderHandle m_hPixelShader{};
	RHIPipelineHandle m_hPipeline{};
	f32_t m_fRotationSeconds = 0.f;
};
