#include "WintersPCH.h"
#include "RHI/RHIBackendRegistry.h"

#include "RHI/DX11/CDX11Device.h"
#include "RHI/DX12/DX12Device.h"

#include <array>
#include <utility>

namespace
{
    RHIBackendProbeResult ProbeWin32CreateDesc(const RHIBackendCreateDesc& desc)
    {
        if (!desc.nativeWindow)
            return { false, "native_window_missing" };
        if (desc.width == 0 || desc.height == 0)
            return { false, "surface_extent_invalid" };
        return { true, "module_ready" };
    }

    class CDX11BackendModule final : public IRHIBackendModule
    {
    public:
        eEngineRHIBackend GetEngineBackend() const override
        {
            return eEngineRHIBackend::DX11;
        }

        eRHIBackend GetRuntimeBackend() const override
        {
            return eRHIBackend::DX11;
        }

        const char* GetName() const override
        {
            return "DX11";
        }

        RHIBackendProbeResult Probe(const RHIBackendCreateDesc& desc) const override
        {
            return ProbeWin32CreateDesc(desc);
        }

        std::unique_ptr<IRHIDevice> CreateDevice(
            const RHIBackendCreateDesc& desc) const override
        {
            DeviceDesc deviceDesc{};
            deviceDesc.hwnd = static_cast<HWND>(desc.nativeWindow);
            deviceDesc.width = desc.width;
            deviceDesc.height = desc.height;
            deviceDesc.vsync = desc.vsync;
            deviceDesc.fullscreen = desc.fullscreen;
            return CDX11Device::Create(deviceDesc);
        }
    };

    class CDX12BackendModule final : public IRHIBackendModule
    {
    public:
        eEngineRHIBackend GetEngineBackend() const override
        {
            return eEngineRHIBackend::DX12;
        }

        eRHIBackend GetRuntimeBackend() const override
        {
            return eRHIBackend::DX12;
        }

        const char* GetName() const override
        {
            return "DX12";
        }

        RHIBackendProbeResult Probe(const RHIBackendCreateDesc& desc) const override
        {
            return ProbeWin32CreateDesc(desc);
        }

        std::unique_ptr<IRHIDevice> CreateDevice(
            const RHIBackendCreateDesc& desc) const override
        {
            DX12DeviceDesc deviceDesc{};
            deviceDesc.hwnd = static_cast<HWND>(desc.nativeWindow);
            deviceDesc.width = desc.width;
            deviceDesc.height = desc.height;
            deviceDesc.vsync = desc.vsync;
            deviceDesc.fullscreen = desc.fullscreen;
            return CDX12Device::Create(deviceDesc);
        }
    };

    const CDX11BackendModule g_DX11BackendModule{};
    const CDX12BackendModule g_DX12BackendModule{};

    const std::array<const IRHIBackendModule*, 2> g_BackendModules = {
        &g_DX11BackendModule,
        &g_DX12BackendModule,
    };
}

const IRHIBackendModule* CRHIBackendRegistry::FindModule(eEngineRHIBackend backend)
{
    for (const IRHIBackendModule* pModule : g_BackendModules)
    {
        if (pModule && pModule->GetEngineBackend() == backend)
            return pModule;
    }
    return nullptr;
}

RHIBackendCreateResult CRHIBackendRegistry::CreateDevice(
    eEngineRHIBackend backend,
    const RHIBackendCreateDesc& desc)
{
    RHIBackendCreateResult result{};
    const IRHIBackendModule* pModule = FindModule(backend);
    if (!pModule)
        return result;

    result.moduleName = pModule->GetName();

    const RHIBackendProbeResult probe = pModule->Probe(desc);
    if (!probe.ready)
    {
        result.status = eRHIBackendCreateStatus::ProbeRejected;
        result.reason = probe.reason;
        return result;
    }

    result.pDevice = pModule->CreateDevice(desc);
    if (!result.pDevice)
    {
        result.status = eRHIBackendCreateStatus::DeviceCreationFailed;
        result.reason = "native_device_creation_failed";
        return result;
    }

    if (result.pDevice->GetBackend() != pModule->GetRuntimeBackend())
    {
        result.pDevice.reset();
        result.status = eRHIBackendCreateStatus::BackendIdentityMismatch;
        result.reason = "created_device_backend_mismatch";
        return result;
    }

    result.status = eRHIBackendCreateStatus::Success;
    result.reason = "device_created";
    return result;
}

const char* RHIBackendCreateStatusName(eRHIBackendCreateStatus status)
{
    switch (status)
    {
    case eRHIBackendCreateStatus::Success: return "success";
    case eRHIBackendCreateStatus::ModuleNotRegistered: return "module_not_registered";
    case eRHIBackendCreateStatus::ProbeRejected: return "probe_rejected";
    case eRHIBackendCreateStatus::DeviceCreationFailed: return "device_creation_failed";
    case eRHIBackendCreateStatus::BackendIdentityMismatch: return "backend_identity_mismatch";
    default: return "unknown";
    }
}
