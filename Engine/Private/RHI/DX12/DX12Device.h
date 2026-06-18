#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "RHI/IRHIDevice.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <memory>
#include <wrl/client.h>

class CDX12FrameCommandList;

struct DX12DeviceDesc
{
	HWND hwnd = nullptr;
	u32_t width = 1280;
	u32_t height = 720;
	bool_t vsync = true;
	bool_t fullscreen = false;
};

class CDX12Device final : public IRHIDevice
{
public:
	~CDX12Device() override;

	static std::unique_ptr<CDX12Device> Create(const DX12DeviceDesc& desc);

	eRHIBackend GetBackend() const override { return eRHIBackend::DX12; }
	void* GetNativeHandle(eNativeHandleType type) const override;

	void BeginFrame(f32_t r = 0.f, f32_t g = 0.f, 
		f32_t b = 0.f, f32_t a = 1.f) override;
	void EndFrame() override;

	IRHICommandList* GetFrameCommandList() override;

	RHIBufferHandle CreateBuffer(const RHIBufferDesc& desc,
		const void* pInitialData = nullptr) override;
	void DestroyBuffer(RHIBufferHandle handle) override;
	void* GetBufferNativeHandle(RHIBufferHandle handle,
		eNativeHandleType type) override;

	RHIShaderHandle CreateShader(eRHIShaderStage stage,
		const void* pBytecode,
		u32_t sizeBytes,
		const char* debugName = nullptr) override;
	void DestroyShader(RHIShaderHandle handle) override;

	RHITextureHandle CreateTexture(const RHITextureDesc& desc,
		const void* pInitialData = nullptr,
		u32_t rowPitchBytes = 0) override;
	void DestroyTexture(RHITextureHandle handle) override;
	void* GetTextureNativeHandle(RHITextureHandle handle,
		eNativeHandleType type) override;

	RHISamplerHandle CreateSampler(const RHISamplerDesc& desc) override;
	void DestroySampler(RHISamplerHandle handle) override;

	RHIPipelineHandle CreatePipeline(const RHIPipelineDesc& desc) override;
	void DestroyPipeline(RHIPipelineHandle handle) override;

	RHIRenderPassHandle CreateRenderPass(const RHIRenderPassDesc& desc) override;
	void DestroyRenderPass(RHIRenderPassHandle handle) override;

	RHIBindGroupLayoutHandle CreateBindGroupLayout(const RHIBindGroupLayoutDesc& desc) override;
	void DestroyBindGroupLayout(RHIBindGroupLayoutHandle handle) override;

	RHIBindGroupHandle CreateBindGroup(const RHIBindGroupDesc& desc) override;
	void DestroyBindGroup(RHIBindGroupHandle handle) override;

	void UpdateBindGroup(RHIBindGroupHandle handle,
		const RHIBindGroupResource* resources,
		u32_t resourceCount) override;

private:
	friend class CDX12FrameCommandList;

	struct ResourceTables;
	struct DescriptorHeaps;

	CDX12Device();

	bool_t Initialize(const DX12DeviceDesc& desc);

	bool_t CreateDevice(const DX12DeviceDesc& desc);
	bool_t CreateCommandObjects();
	bool_t CreateSwapChain(const DX12DeviceDesc& desc);
	bool_t CreateRenderTargets();
	bool_t CreateDepthStencilTarget();
	bool_t CreateFence();
	bool_t CreateDescriptorHeaps();

	void WriteBindGroupDescriptors(
		const RHIBindGroupLayoutDesc& layoutDesc,
		const RHIBindGroupDesc& groupDesc,
		D3D12_CPU_DESCRIPTOR_HANDLE srvBaseCpu,
		D3D12_CPU_DESCRIPTOR_HANDLE samplerBaseCpu);

	void WaitForGpu();

	static constexpr u32_t kFrameCount = 2;

	Microsoft::WRL::ComPtr<IDXGIFactory4> m_pFactory;
	Microsoft::WRL::ComPtr<IDXGIAdapter1> m_pAdapter;
	Microsoft::WRL::ComPtr<ID3D12Device> m_pDevice;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pCommandQueue;
	Microsoft::WRL::ComPtr<IDXGISwapChain3> m_pSwapChain;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pRTVHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pDSVHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_pRenderTargets[kFrameCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> m_pDepthStencil;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_pCommandAllocators[kFrameCount];
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pCommandList;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence;

	std::unique_ptr<ResourceTables> m_pTables;
	std::unique_ptr<DescriptorHeaps> m_pHeaps;
	std::unique_ptr<CDX12FrameCommandList> m_pFrameCommandList;

	HANDLE m_hFenceEvent = nullptr;
	u64_t m_uFenceValue = 1;
	u32_t m_iFrameIndex = 0;
	u32_t m_iRTVDescriptorSize = 0;
	u32_t m_iWidth = 1280;
	u32_t m_iHeight = 720;
	bool_t m_bVSync = true;
	bool_t m_bFrameRecording = false;

	D3D12_VIEWPORT m_Viewport{};
	D3D12_RECT m_ScissorRect{};
};
