#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "Renderer/BlendTypes.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>

class WINTERS_ENGINE CBlendStateCache final
{
private:
	CBlendStateCache() = default;

public:
	CBlendStateCache(const CBlendStateCache&) = delete;
	CBlendStateCache& operator=(const CBlendStateCache&) = delete;
	~CBlendStateCache() = default;

	static std::unique_ptr<CBlendStateCache> Create(ID3D11Device* pDevice);

	void Bind(ID3D11DeviceContext* pContext, eBlendPreset ePreset) const;
	ID3D11BlendState* Get(eBlendPreset ePreset) const;

private:
	HRESULT Initialize(ID3D11Device* pDevice);

	Microsoft::WRL::ComPtr<ID3D11BlendState>
		m_pStates[static_cast<size_t>(eBlendPreset::Count)];
};
