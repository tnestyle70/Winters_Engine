#pragma once

#include "WintersAPI.h"
#include "Renderer/RenderWorldSnapshot.h"

#include <memory>

class IRHIDevice;

class WINTERS_ENGINE CRHISceneRenderer final
{
public:
    ~CRHISceneRenderer();

    CRHISceneRenderer(const CRHISceneRenderer&) = delete;
    CRHISceneRenderer& operator=(const CRHISceneRenderer&) = delete;

    static std::unique_ptr<CRHISceneRenderer> Create(IRHIDevice* pDevice);

    bool_t IsReady() const;
    void Render(IRHIDevice* pDevice, const RenderWorldSnapshot& snapshot);
    void Shutdown();

private:
    CRHISceneRenderer() = default;

    struct Impl;
    Impl* m_pImpl = nullptr;
};
