#include "Resource/Texture.h"
#include "RHI/RHITypes.h"
#include "WintersPaths.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <objbase.h>

#pragma push_macro("new")
#undef new
#include <directxtk/WICTextureLoader.h>
#include <directxtk/DDSTextureLoader.h>
#pragma pop_macro("new")

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
}

CTexture::~CTexture()
{
	if (m_pSampler)
	{
		static_cast<ID3D11SamplerState*>(m_pSampler)->Release();
		m_pSampler = nullptr;
	}
	if (m_pSRV)
	{
		static_cast<ID3D11ShaderResourceView*>(m_pSRV)->Release();
		m_pSRV = nullptr;
	}
}

void CTexture::Bind(IRHIDevice* pDevice, u32_t iSlot)
{
	ID3D11DeviceContext* pContext = GetNativeDX11Context(pDevice);
	if (!pContext)
		return;

	ID3D11ShaderResourceView* pSRV = static_cast<ID3D11ShaderResourceView*>(m_pSRV);
	ID3D11SamplerState* pSampler = static_cast<ID3D11SamplerState*>(m_pSampler);
	if (pSRV)
		pContext->PSSetShaderResources(iSlot, 1, &pSRV);
	if (pSampler)
		pContext->PSSetSamplers(iSlot, 1, &pSampler);
}

unique_ptr<CTexture> CTexture::Create(IRHIDevice* pDevice, const wstring& strFilePath,
	eTexSamplerMode eMode,
	eTexColorSpace eColorSpace)
{
	CScopedCOMInit comInit;

	ID3D11Device* pNativeDevice = GetNativeDX11Device(pDevice);
	if (!pNativeDevice)
		return nullptr;

	auto pInstance = unique_ptr<CTexture>(new CTexture());
	wstring loadPath = strFilePath;
	wchar_t resolvedPath[MAX_PATH] = {};
	if (WintersResolveContentPath(strFilePath.c_str(), resolvedPath, MAX_PATH))
		loadPath = resolvedPath;
	//확장자 판별
	const size_t dot = loadPath.find_last_of(L'.');
	wstring ext = (dot != wstring::npos) ? loadPath.substr(dot) : L"";
	HRESULT hr = E_FAIL;
	const bool_t bIgnoreSRGB =
		eColorSpace == eTexColorSpace::ShaderLocalSRGB ||
		eColorSpace == eTexColorSpace::IgnoreSRGB;

	if (ext == L".dds" || ext == L".DDS")
	{
		if (bIgnoreSRGB)
		{
			hr = DirectX::CreateDDSTextureFromFileEx(
				pNativeDevice,
				loadPath.c_str(),
				0,
				D3D11_USAGE_DEFAULT,
				D3D11_BIND_SHADER_RESOURCE,
				0,
				0,
				DirectX::DDS_LOADER_IGNORE_SRGB,
				nullptr,
				reinterpret_cast<ID3D11ShaderResourceView**>(&pInstance->m_pSRV));
		}
		else
		{
			hr = DirectX::CreateDDSTextureFromFile(pNativeDevice, loadPath.c_str(), nullptr,
				reinterpret_cast<ID3D11ShaderResourceView**>(&pInstance->m_pSRV));
		}
	}
	else
	{
		if (bIgnoreSRGB)
		{
			const DirectX::WIC_LOADER_FLAGS flags =
				DirectX::WIC_LOADER_IGNORE_SRGB | DirectX::WIC_LOADER_FORCE_RGBA32;
			hr = DirectX::CreateWICTextureFromFileEx(
				pNativeDevice,
				loadPath.c_str(),
				0,
				D3D11_USAGE_DEFAULT,
				D3D11_BIND_SHADER_RESOURCE,
				0,
				0,
				flags,
				nullptr,
				reinterpret_cast<ID3D11ShaderResourceView**>(&pInstance->m_pSRV));
		}
		else
		{
			hr = DirectX::CreateWICTextureFromFile(pNativeDevice, loadPath.c_str(), nullptr,
				reinterpret_cast<ID3D11ShaderResourceView**>(&pInstance->m_pSRV));
		}
	}

	if (FAILED(hr))
	{
		OutputDebugStringA("[CTexture] FAILED to load texture\n");
		return nullptr;
	}
	OutputDebugStringA("[CTexture] Texture loaded successfully\n");

	//샘플러 생성 — UI/Decal 는 Clamp, 월드 타일링은 Wrap
	const D3D11_TEXTURE_ADDRESS_MODE addr =
		(eMode == eTexSamplerMode::Clamp) ? D3D11_TEXTURE_ADDRESS_CLAMP
		                                  : D3D11_TEXTURE_ADDRESS_WRAP;

	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = addr;
	sampDesc.AddressV = addr;
	sampDesc.AddressW = addr;
	sampDesc.MaxAnisotropy = 4;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	hr = pNativeDevice->CreateSamplerState(&sampDesc,
		reinterpret_cast<ID3D11SamplerState**>(&pInstance->m_pSampler));
	if (FAILED(hr))
		return nullptr;

	return pInstance;
}

unique_ptr<CTexture> CTexture::CreateFromMemory(IRHIDevice* pDevice, const void* pData, size_t dataSize,
	eTexSamplerMode eMode)
{
	CScopedCOMInit comInit;

	ID3D11Device* pNativeDevice = GetNativeDX11Device(pDevice);
	if (!pNativeDevice || !pData || dataSize == 0)
		return nullptr;

	if (!pData || dataSize == 0)
		return nullptr;

	auto pInstance = unique_ptr<CTexture>(new CTexture());
	HRESULT hr = E_FAIL;

	//DDS 매직 체크 : DDS 0x20534444
	if (dataSize >= 4 && memcmp(pData, "DDS ", 4) == 0)
		hr = DirectX::CreateDDSTextureFromMemory(pNativeDevice,
			static_cast<const uint8_t*>(pData), dataSize,
			nullptr, reinterpret_cast<ID3D11ShaderResourceView**>(&pInstance->m_pSRV));
	else
		hr = DirectX::CreateWICTextureFromMemory(pNativeDevice,
			static_cast<const uint8_t*>(pData), dataSize,
			nullptr, reinterpret_cast<ID3D11ShaderResourceView**>(&pInstance->m_pSRV));

	if (FAILED(hr))
	{
		OutputDebugStringA("Texture Load Failed");
		return nullptr;
	}

	const D3D11_TEXTURE_ADDRESS_MODE addr =
		(eMode == eTexSamplerMode::Clamp) ? D3D11_TEXTURE_ADDRESS_CLAMP
		                                  : D3D11_TEXTURE_ADDRESS_WRAP;

	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = addr;
	sampDesc.AddressV = addr;
	sampDesc.AddressW = addr;
	sampDesc.MaxAnisotropy = 4;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	hr = pNativeDevice->CreateSamplerState(&sampDesc,
		reinterpret_cast<ID3D11SamplerState**>(&pInstance->m_pSampler));
	if (FAILED(hr))
		return nullptr;

	return pInstance;
}

unique_ptr<CTexture> CTexture::CreateDefault(IRHIDevice* pDevice)
{
	ID3D11Device* pNativeDevice = GetNativeDX11Device(pDevice);
	if (!pNativeDevice)
		return nullptr;

	auto pInstance = unique_ptr<CTexture>(new CTexture());

	// 1x1 흰색 텍스처 (폴백용)
	u32_t white = 0xFFFFFFFF;
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = 1;
	texDesc.Height = 1;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_IMMUTABLE;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = &white;
	initData.SysMemPitch = 4;

	ID3D11Texture2D* pTex = nullptr;
	pNativeDevice->CreateTexture2D(&texDesc, &initData, &pTex);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	pNativeDevice->CreateShaderResourceView(
		pTex,
		&srvDesc,
		reinterpret_cast<ID3D11ShaderResourceView**>(&pInstance->m_pSRV));

	if (pTex) pTex->Release();

	// 샘플러
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	pNativeDevice->CreateSamplerState(
		&sampDesc,
		reinterpret_cast<ID3D11SamplerState**>(&pInstance->m_pSampler));

	return pInstance;
}
