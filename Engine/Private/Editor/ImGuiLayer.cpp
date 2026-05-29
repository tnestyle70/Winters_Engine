#include "Editor/ImGuiLayer.h"
#include "RHI/RHITypes.h"
#include "WintersPaths.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <Windows.h>
#include <d3d11.h>
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

	ID3D11Device* pNativeDevice = GetNativeDX11Device(pDevice);
	ID3D11DeviceContext* pNativeContext = GetNativeDX11Context(pDevice);
	if (!pNativeDevice || !pNativeContext)
		return false;
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
	ImGui_ImplDX11_Init(pNativeDevice, pNativeContext);

	m_bInitialized = true;

	return true;
}

void CImGuiLayer::BeginFrame()
{
	if (!m_bInitialized)
		return;
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void CImGuiLayer::EndFrame()
{
	if (!m_bInitialized)
		return;

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void CImGuiLayer::ShutDown()
{
	if (!m_bInitialized)
		return;

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

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
