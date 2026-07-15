#include "WintersPCH.h"

#include "RHI/RHITextureLoader.h"
#include "WintersPaths.h"

#include <wincodec.h>
#include <wrl/client.h>

#include <string>
#include <vector>

namespace
{
    class CScopedCOMInit final
    {
    public:
        CScopedCOMInit()
        {
            const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            m_bNeedsUninit = SUCCEEDED(hr);
        }

        ~CScopedCOMInit()
        {
            if (m_bNeedsUninit)
                CoUninitialize();
        }

    private:
        bool_t m_bNeedsUninit = false;
    };

    void LogTextureLoadFailure(const char* pStage, const wchar_t* pPath, HRESULT hr)
    {
        char msg[512]{};
        sprintf_s(msg, "[RHITextureLoader] FAIL: %s hr=0x%08X path=%ls\n",
            pStage,
            static_cast<unsigned>(hr),
            pPath ? pPath : L"(null)");
        OutputDebugStringA(msg);
    }
}

RHITextureHandle RHI_CreateTextureFromFile(
    IRHIDevice* pDevice,
    const wchar_t* pFilePath,
    const char* pDebugName)
{
    if (!pDevice || !pFilePath || pFilePath[0] == 0)
        return {};

    CScopedCOMInit comInit;
    std::wstring loadPath = pFilePath;
    wchar_t resolvedPath[MAX_PATH] = {};
    if (WintersResolveContentPath(pFilePath, resolvedPath, MAX_PATH))
        loadPath = resolvedPath;

    Microsoft::WRL::ComPtr<IWICImagingFactory2> pFactory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory2,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pFactory));
    if (FAILED(hr))
    {
        LogTextureLoadFailure("CoCreateInstance(WICImagingFactory2)", loadPath.c_str(), hr);
        return {};
    }

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> pDecoder;
    hr = pFactory->CreateDecoderFromFilename(
        loadPath.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &pDecoder);
    if (FAILED(hr))
    {
        LogTextureLoadFailure("CreateDecoderFromFilename", loadPath.c_str(), hr);
        return {};
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> pFrame;
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr))
    {
        LogTextureLoadFailure("GetFrame", loadPath.c_str(), hr);
        return {};
    }

    UINT width = 0;
    UINT height = 0;
    hr = pFrame->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0)
    {
        LogTextureLoadFailure("GetSize", loadPath.c_str(), hr);
        return {};
    }

    Microsoft::WRL::ComPtr<IWICFormatConverter> pConverter;
    hr = pFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr))
    {
        LogTextureLoadFailure("CreateFormatConverter", loadPath.c_str(), hr);
        return {};
    }

    hr = pConverter->Initialize(
        pFrame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
    {
        LogTextureLoadFailure("FormatConverter.Initialize", loadPath.c_str(), hr);
        return {};
    }

    const u32_t rowPitchBytes = width * sizeof(u32_t);
    std::vector<u32_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height));
    hr = pConverter->CopyPixels(
        nullptr,
        rowPitchBytes,
        rowPitchBytes * height,
        reinterpret_cast<BYTE*>(pixels.data()));
    if (FAILED(hr))
    {
        LogTextureLoadFailure("CopyPixels", loadPath.c_str(), hr);
        return {};
    }

    RHITextureDesc desc{};
    desc.width = width;
    desc.height = height;
    desc.mipLevels = 1;
    desc.format = eRHIFormat::R8G8B8A8_UNorm;
    desc.usageFlags = static_cast<u32_t>(eRHITextureUsage::ShaderResource);
    desc.debugName = pDebugName;

    return pDevice->CreateTexture(desc, pixels.data(), rowPitchBytes);
}
