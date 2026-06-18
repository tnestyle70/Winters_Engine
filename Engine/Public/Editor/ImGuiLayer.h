#pragma once

#include "RHI/IRHIDevice.h"

class CImGuiLayer
{
public:
	CImGuiLayer() = default;
	~CImGuiLayer() { ShutDown(); }

	CImGuiLayer(const CImGuiLayer&) = delete;
	CImGuiLayer& operator=(const CImGuiLayer&) = delete;

	bool Initialize(void* hWnd, IRHIDevice* pDevice);

	void BeginFrame();
	void EndFrame();
	void ShutDown();
	bool WantsCaptureMouse() const;
	bool WantsCaptureKeyboard() const;

private:
	bool m_bInitialized = false;
	IRHIDevice* m_pDevice = nullptr;
};
