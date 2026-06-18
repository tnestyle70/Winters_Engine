#include "WintersPCH.h"
#include "RHI/RHIShaderCompiler.h"

#include <cstring>
#include <d3dcompiler.h>
#include <wrl/client.h>

#pragma comment(lib, "d3dcompiler.lib")

bool RHI_CompileHlslShader(
    const char* pSource,
    const char* pEntryPoint,
    const char* pTarget,
    std::vector<u8_t>& outBytecode,
    std::string* pOutErrors)
{
    outBytecode.clear();

    if (pOutErrors)
        pOutErrors->clear();

    if (!pSource || !pEntryPoint || !pTarget)
        return false;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> pBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> pErrors;

    const HRESULT hr = D3DCompile(
        pSource,
        std::strlen(pSource),
        "RHI_HLSL",
        nullptr,
        nullptr,
        pEntryPoint,
        pTarget,
        flags,
        0,
        &pBlob,
        &pErrors);

    if (FAILED(hr))
    {
        if (pOutErrors && pErrors)
        {
            pOutErrors->assign(
                static_cast<const char*>(pErrors->GetBufferPointer()),
                pErrors->GetBufferSize());
        }
        return false;
    }

    outBytecode.resize(pBlob->GetBufferSize());
    std::memcpy(outBytecode.data(), pBlob->GetBufferPointer(), pBlob->GetBufferSize());
    return true;
}
