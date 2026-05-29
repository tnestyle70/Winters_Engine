#include "Resource/Mesh.h"
#include "RHI/RHITypes.h"
#include "RHI/DX11/DX11Buffer.h"

struct CMesh::Impl
{
	DX11Buffer vb;
	DX11Buffer ib;
};

CMesh::CMesh()
	: m_pImpl(new Impl())
{
}

CMesh::~CMesh()
{
	delete m_pImpl;
	m_pImpl = nullptr;
}

namespace
{
	ID3D11Device* GetNativeDX11Device(IRHIDevice* pDevice)
	{
		if (!pDevice)
			return nullptr;
		return static_cast<ID3D11Device*>(
			pDevice->GetNativeHandle(eNativeHandleType::DX11Device));
	}
}

void CMesh::Render(IRHIDevice* pDevice)
{
	ID3D11DeviceContext* pContext = static_cast<ID3D11DeviceContext*>(
		pDevice ? pDevice->GetNativeHandle(eNativeHandleType::DX11DeviceContext) : nullptr);
	if (!pContext || !m_pImpl)
		return;

	m_pImpl->vb.BindVertex(pContext, m_iVertexStride);
	m_pImpl->ib.BindIndex(pContext);
	m_pImpl->ib.DrawIndexed(pContext);
}

unique_ptr<CMesh> CMesh::Create(IRHIDevice* pDevice, const void* pVertices, u32_t iVertexStride, u32_t iVertexCount,
	const void* pIndices, u32_t iIndexCount, bool_t bUse32Bit)
{
	ID3D11Device* pNativeDevice = GetNativeDX11Device(pDevice);
	if (!pNativeDevice)
		return nullptr;

	auto pInstance = unique_ptr<CMesh>(new CMesh());

	pInstance->m_iVertexStride = iVertexStride;
	pInstance->m_iIndexCount = iIndexCount;

	if (!pInstance->m_pImpl->vb.CreateVertex(pNativeDevice, pVertices, iVertexStride, iVertexCount))
		return nullptr;

	if (!pInstance->m_pImpl->ib.CreateIndex(pNativeDevice, pIndices, iIndexCount, bUse32Bit))
		return nullptr;

	return pInstance;
}
