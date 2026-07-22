#pragma once

#include "EngineConfig.h"
#include "RHI/IRHIDevice.h"

#include <memory>
#include <string>

struct RHIBackendCreateDesc
{
    void* nativeWindow = nullptr;
    u32_t width = 0;
    u32_t height = 0;
    bool_t vsync = true;
    bool_t fullscreen = false;
};

struct RHIBackendProbeResult
{
    bool_t ready = false;
    std::string reason = "backend_probe_rejected";
};

enum class eRHIBackendCreateStatus : u32_t
{
    Success = 0,
    ModuleNotRegistered,
    ProbeRejected,
    DeviceCreationFailed,
    BackendIdentityMismatch,
};

struct RHIBackendCreateResult
{
    eRHIBackendCreateStatus status = eRHIBackendCreateStatus::ModuleNotRegistered;
    std::string moduleName = "None";
    std::string reason = "backend_module_not_registered";
    std::unique_ptr<IRHIDevice> pDevice{};

    bool_t Succeeded() const
    {
        return status == eRHIBackendCreateStatus::Success && pDevice != nullptr;
    }
};

class IRHIBackendModule
{
public:
    virtual ~IRHIBackendModule() = default;

    virtual eEngineRHIBackend GetEngineBackend() const = 0;
    virtual eRHIBackend GetRuntimeBackend() const = 0;
    virtual const char* GetName() const = 0;
    virtual RHIBackendProbeResult Probe(const RHIBackendCreateDesc& desc) const = 0;
    virtual std::unique_ptr<IRHIDevice> CreateDevice(
        const RHIBackendCreateDesc& desc) const = 0;
};

class CRHIBackendRegistry final
{
public:
    static const IRHIBackendModule* FindModule(eEngineRHIBackend backend);
    static RHIBackendCreateResult CreateDevice(
        eEngineRHIBackend backend,
        const RHIBackendCreateDesc& desc);
};

const char* RHIBackendCreateStatusName(eRHIBackendCreateStatus status);
