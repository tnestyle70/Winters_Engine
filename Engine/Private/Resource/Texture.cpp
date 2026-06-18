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

	if (m_bMipsPending)
		EnsureMipsOnBind(pDevice);

	ID3D11ShaderResourceView* pSRV = static_cast<ID3D11ShaderResourceView*>(m_pSRV);
	ID3D11SamplerState* pSampler = static_cast<ID3D11SamplerState*>(m_pSampler);
	if (pSRV)
		pContext->PSSetShaderResources(iSlot, 1, &pSRV);
	if (pSampler)
		pContext->PSSetSamplers(iSlot, 1, &pSampler);
}

// 1-mip으로 로드된 텍스쳐를 풀 mip 체인 텍스쳐로 교체하고 GenerateMips 수행.
// immediate context를 사용하므로 반드시 렌더 스레드(Bind 시점)에서만 호출한다.
void CTexture::EnsureMipsOnBind(IRHIDevice* pDevice)
{
	m_bMipsPending = false;

	ID3D11Device* pNativeDevice = GetNativeDX11Device(pDevice);
	ID3D11DeviceContext* pContext = GetNativeDX11Context(pDevice);
	ID3D11ShaderResourceView* pOldSRV = static_cast<ID3D11ShaderResourceView*>(m_pSRV);
	if (!pNativeDevice || !pContext || !pOldSRV)
		return;

	ID3D11Resource* pResource = nullptr;
	pOldSRV->GetResource(&pResource);
	if (!pResource)
		return;

	ID3D11Texture2D* pTex2D = nullptr;
	pResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pTex2D));
	pResource->Release();
	if (!pTex2D)
		return;

	D3D11_TEXTURE2D_DESC desc{};
	pTex2D->GetDesc(&desc);

	// 이미 mip 체인이 있거나(DDS 등) MSAA/1x1이면 그대로 둔다.
	UINT formatSupport = 0;
	if (desc.MipLevels != 1 ||
		desc.SampleDesc.Count != 1 ||
		(desc.Width <= 1 && desc.Height <= 1) ||
		FAILED(pNativeDevice->CheckFormatSupport(desc.Format, &formatSupport)) ||
		(formatSupport & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN) == 0)
	{
		pTex2D->Release();
		return;
	}

	D3D11_TEXTURE2D_DESC mipDesc = desc;
	mipDesc.MipLevels = 0;
	mipDesc.Usage = D3D11_USAGE_DEFAULT;
	mipDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	mipDesc.CPUAccessFlags = 0;
	mipDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

	ID3D11Texture2D* pMipTex = nullptr;
	if (FAILED(pNativeDevice->CreateTexture2D(&mipDesc, nullptr, &pMipTex)) || !pMipTex)
	{
		pTex2D->Release();
		return;
	}

	pContext->CopySubresourceRegion(pMipTex, 0, 0, 0, 0, pTex2D, 0, nullptr);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = static_cast<UINT>(-1);

	ID3D11ShaderResourceView* pNewSRV = nullptr;
	if (SUCCEEDED(pNativeDevice->CreateShaderResourceView(pMipTex, &srvDesc, &pNewSRV)) && pNewSRV)
	{
		pContext->GenerateMips(pNewSRV);
		pOldSRV->Release();
		m_pSRV = pNewSRV;
	}

	pMipTex->Release();
	pTex2D->Release();
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

	// 로더는 mip 없이 1-mip으로 만들므로 첫 Bind에서 mip 체인을 생성한다.
	pInstance->m_bMipsPending = true;

	//샘플러 생성 — UI/Decal 는 Clamp, 월드 타일링은 Wrap
	const D3D11_TEXTURE_ADDRESS_MODE addr =
		(eMode == eTexSamplerMode::Clamp) ? D3D11_TEXTURE_ADDRESS_CLAMP
		                                  : D3D11_TEXTURE_ADDRESS_WRAP;

	D3D11_SAMPLER_DESC sampDesc = {};
	// 월드 텍스쳐는 경사면 minification에서 울렁임을 막기 위해 anisotropic 필수.
	// (MIN_MAG_MIP_LINEAR에서는 MaxAnisotropy가 무시된다)
	sampDesc.Filter = (eMode == eTexSamplerMode::Clamp)
		? D3D11_FILTER_MIN_MAG_MIP_LINEAR
		: D3D11_FILTER_ANISOTROPIC;
	sampDesc.AddressU = addr;
	sampDesc.AddressV = addr;
	sampDesc.AddressW = addr;
	sampDesc.MaxAnisotropy = 8;
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

	pInstance->m_bMipsPending = true;

	const D3D11_TEXTURE_ADDRESS_MODE addr =
		(eMode == eTexSamplerMode::Clamp) ? D3D11_TEXTURE_ADDRESS_CLAMP
		                                  : D3D11_TEXTURE_ADDRESS_WRAP;

	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = (eMode == eTexSamplerMode::Clamp)
		? D3D11_FILTER_MIN_MAG_MIP_LINEAR
		: D3D11_FILTER_ANISOTROPIC;
	sampDesc.AddressU = addr;
	sampDesc.AddressV = addr;
	sampDesc.AddressW = addr;
	sampDesc.MaxAnisotropy = 8;
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
