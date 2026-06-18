#include "Editor/ImGuiLayer.h"
#include "RHI/RHITypes.h"
#include "WintersPaths.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_dx12.h"

#include <Windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <vector>
#include <wrl/client.h>
//초기화 : CreateContext->StyleColors->Win32 init -> DX Init
//Frame : DX11 NewFrame -> Win32 NewFrame -> ImGui::NewFrame
//Finish : DX11 ShutDown -> Win32 ShutDown -> DestoryContext

namespace
{
	ID3D11Device* GetNativeDX11Device(IRHIDevice* pDevice)
	{
		if (!pDevice)
			return nullptr;
		return static_cast<ID3D11Device*>(
			pDevice->GetNativeHandle(eNativeHandleType::DX11Device));
	}

	ID3D11DeviceContext* GetNativeDX11Context(IRHIDevice* pDevice)
	{
		if (!pDevice)
			return nullptr;
		return static_cast<ID3D11DeviceContext*>(
			pDevice->GetNativeHandle(eNativeHandleType::DX11DeviceContext));
	}

	ID3D12Device* GetNativeDX12Device(IRHIDevice* pDevice)
	{
		if (!pDevice)
			return nullptr;
		return static_cast<ID3D12Device*>(
			pDevice->GetNativeHandle(eNativeHandleType::DX12Device));
	}

	ID3D12CommandQueue* GetNativeDX12CommandQueue(IRHIDevice* pDevice)
	{
		if (!pDevice)
			return nullptr;
		return static_cast<ID3D12CommandQueue*>(
			pDevice->GetNativeHandle(eNativeHandleType::DX12CommandQueue));
	}

	ID3D12GraphicsCommandList* GetNativeDX12CommandList(IRHIDevice* pDevice)
	{
		if (!pDevice)
			return nullptr;
		return static_cast<ID3D12GraphicsCommandList*>(
			pDevice->GetNativeHandle(eNativeHandleType::DX12CommandList));
	}

	// imgui_impl_dx12(1.92+)는 SRV descriptor 할당을 앱 콜백에 위임한다.
	struct ImGuiDX12SrvHeapAllocator
	{
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pHeap;
		D3D12_CPU_DESCRIPTOR_HANDLE CpuStart{};
		D3D12_GPU_DESCRIPTOR_HANDLE GpuStart{};
		UINT Increment = 0;
		std::vector<UINT> FreeIndices;

		bool Create(ID3D12Device* pDevice, UINT capacity)
		{
			D3D12_DESCRIPTOR_HEAP_DESC desc{};
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors = capacity;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			if (FAILED(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap))))
				return false;

			CpuStart = pHeap->GetCPUDescriptorHandleForHeapStart();
			GpuStart = pHeap->GetGPUDescriptorHandleForHeapStart();
			Increment = pDevice->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			FreeIndices.clear();
			FreeIndices.reserve(capacity);
			for (UINT i = 0; i < capacity; ++i)
				FreeIndices.push_back(capacity - 1 - i);

			return true;
		}

		void Destroy()
		{
			pHeap.Reset();
			FreeIndices.clear();
			Increment = 0;
		}

		void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCpu, D3D12_GPU_DESCRIPTOR_HANDLE* pOutGpu)
		{
			IM_ASSERT(!FreeIndices.empty());
			const UINT index = FreeIndices.back();
			FreeIndices.pop_back();
			pOutCpu->ptr = CpuStart.ptr + static_cast<SIZE_T>(index) * Increment;
			pOutGpu->ptr = GpuStart.ptr + static_cast<UINT64>(index) * Increment;
		}

		void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE)
		{
			const UINT index = static_cast<UINT>((cpu.ptr - CpuStart.ptr) / Increment);
			FreeIndices.push_back(index);
		}
	};

	ImGuiDX12SrvHeapAllocator g_ImGuiSrvHeapAllocator;

	bool ResolveUtf8Path(const wchar_t* pRelativePath, char* pOut, int iOutSize)
	{
		if (!pRelativePath || !pOut || iOutSize <= 0)
			return false;

		wchar_t resolvedPath[MAX_PATH] = {};
		const wchar_t* pPath = pRelativePath;
		if (WintersResolveContentPath(pRelativePath, resolvedPath, MAX_PATH))
			pPath = resolvedPath;

		return WideCharToMultiByte(
			CP_UTF8,
			0,
			pPath,
			-1,
			pOut,
			iOutSize,
			nullptr,
			nullptr) > 0;
	}

	void LoadCleanDefaultFont()
	{
		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();

		io.Fonts->Clear();
		style.FontSizeBase = 17.0f;
		style.FontScaleMain = 1.0f;
		style.FontScaleDpi = 1.0f;

		char utf8Path[MAX_PATH * 4] = {};
		ImFontConfig config{};
		config.Flags |= ImFontFlags_NoLoadError;
		config.OversampleH = 2;
		config.OversampleV = 1;

		ImFont* pFont = nullptr;
		if (ResolveUtf8Path(
			L"Resource/Texture/UI/ux/fonts/notosanscjk-regular.ttf",
			utf8Path,
			static_cast<int>(sizeof(utf8Path))))
		{
			pFont = io.Fonts->AddFontFromFileTTF(
				utf8Path,
				17.0f,
				&config,
				io.Fonts->GetGlyphRangesKorean());
		}

		if (!pFont)
			pFont = io.Fonts->AddFontDefaultVector();

		io.FontDefault = pFont;
	}
}

bool CImGuiLayer::Initialize(void* hWnd, IRHIDevice* pDevice)
{
	if (m_bInitialized)
		return true;

	if (!pDevice)
		return false;

	const eRHIBackend eBackend = pDevice->GetBackend();
	if (eBackend != eRHIBackend::DX11 && eBackend != eRHIBackend::DX12)
		return false;

	ID3D11Device* pNativeDevice = nullptr;
	ID3D11DeviceContext* pNativeContext = nullptr;
	if (eBackend == eRHIBackend::DX11)
	{
		pNativeDevice = GetNativeDX11Device(pDevice);
		pNativeContext = GetNativeDX11Context(pDevice);
		if (!pNativeDevice || !pNativeContext)
			return false;
	}
	//1. ImGui  Context Create
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

	//2. Set Theme
	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 4.0f;
	style.ChildRounding = 3.0f;
	style.FrameRounding = 3.0f;
	style.GrabRounding = 3.0f;
	style.PopupRounding = 3.0f;
	style.ScrollbarRounding = 3.0f;

	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.0f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.0f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.0f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);

	LoadCleanDefaultFont();

	//3. Platform + Renderer Backend Initialize
	ImGui_ImplWin32_Init(hWnd);

	if (eBackend == eRHIBackend::DX11)
	{
		ImGui_ImplDX11_Init(pNativeDevice, pNativeContext);
	}
	else
	{
		ID3D12Device* pDX12Device = GetNativeDX12Device(pDevice);
		ID3D12CommandQueue* pDX12Queue = GetNativeDX12CommandQueue(pDevice);
		if (!pDX12Device || !pDX12Queue ||
			!g_ImGuiSrvHeapAllocator.Create(pDX12Device, 64))
		{
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
			return false;
		}

		ImGui_ImplDX12_InitInfo initInfo{};
		initInfo.Device = pDX12Device;
		initInfo.CommandQueue = pDX12Queue;
		initInfo.NumFramesInFlight = 2;
		initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		initInfo.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		initInfo.SrvDescriptorHeap = g_ImGuiSrvHeapAllocator.pHeap.Get();
		initInfo.SrvDescriptorAllocFn =
			[](ImGui_ImplDX12_InitInfo*,
				D3D12_CPU_DESCRIPTOR_HANDLE* pOutCpu,
				D3D12_GPU_DESCRIPTOR_HANDLE* pOutGpu)
			{
				g_ImGuiSrvHeapAllocator.Alloc(pOutCpu, pOutGpu);
			};
		initInfo.SrvDescriptorFreeFn =
			[](ImGui_ImplDX12_InitInfo*,
				D3D12_CPU_DESCRIPTOR_HANDLE cpu,
				D3D12_GPU_DESCRIPTOR_HANDLE gpu)
			{
				g_ImGuiSrvHeapAllocator.Free(cpu, gpu);
			};

		if (!ImGui_ImplDX12_Init(&initInfo))
		{
			g_ImGuiSrvHeapAllocator.Destroy();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
			return false;
		}
	}

	m_pDevice = pDevice;
	m_bInitialized = true;

	return true;
}

void CImGuiLayer::BeginFrame()
{
	if (!m_bInitialized)
		return;
	if (m_pDevice && m_pDevice->GetBackend() == eRHIBackend::DX12)
		ImGui_ImplDX12_NewFrame();
	else
		ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void CImGuiLayer::EndFrame()
{
	if (!m_bInitialized)
		return;

	ImGui::Render();

	if (m_pDevice && m_pDevice->GetBackend() == eRHIBackend::DX12)
	{
		// CDX12Device::BeginFrame이 열어 둔 프레임 커맨드리스트에 기록한다.
		ID3D12GraphicsCommandList* pCommandList = GetNativeDX12CommandList(m_pDevice);
		if (pCommandList)
		{
			ID3D12DescriptorHeap* ppHeaps[] = { g_ImGuiSrvHeapAllocator.pHeap.Get() };
			pCommandList->SetDescriptorHeaps(1, ppHeaps);
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommandList);
		}
	}
	else
	{
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}
}

void CImGuiLayer::ShutDown()
{
	if (!m_bInitialized)
		return;

	if (m_pDevice && m_pDevice->GetBackend() == eRHIBackend::DX12)
	{
		ImGui_ImplDX12_Shutdown();
		g_ImGuiSrvHeapAllocator.Destroy();
	}
	else
	{
		ImGui_ImplDX11_Shutdown();
	}
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	m_pDevice = nullptr;
	m_bInitialized = false;
}

bool CImGuiLayer::WantsCaptureMouse() const
{
	if (!m_bInitialized)
		return false;
	return ImGui::GetIO().WantCaptureMouse;
}

bool CImGuiLayer::WantsCaptureKeyboard() const
{
	if (!m_bInitialized)
		return false;
	return ImGui::GetIO().WantCaptureKeyboard;
}
