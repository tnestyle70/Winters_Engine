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
        return {};

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> pDecoder;
    hr = pFactory->CreateDecoderFromFilename(
        loadPath.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &pDecoder);
    if (FAILED(hr))
        return {};

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> pFrame;
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr))
        return {};

    UINT width = 0;
    UINT height = 0;
    hr = pFrame->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0)
        return {};

    Microsoft::WRL::ComPtr<IWICFormatConverter> pConverter;
    hr = pFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr))
        return {};

    hr = pConverter->Initialize(
        pFrame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
        return {};

    const u32_t rowPitchBytes = width * sizeof(u32_t);
    std::vector<u32_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height));
    hr = pConverter->CopyPixels(
        nullptr,
        rowPitchBytes,
        rowPitchBytes * height,
        reinterpret_cast<BYTE*>(pixels.data()));
    if (FAILED(hr))
        return {};

    RHITextureDesc desc{};
    desc.width = width;
    desc.height = height;
    desc.mipLevels = 1;
    desc.format = eRHIFormat::R8G8B8A8_UNorm;
    desc.usageFlags = static_cast<u32_t>(eRHITextureUsage::ShaderResource);
    desc.debugName = pDebugName;

    return pDevice->CreateTexture(desc, pixels.data(), rowPitchBytes);
}
