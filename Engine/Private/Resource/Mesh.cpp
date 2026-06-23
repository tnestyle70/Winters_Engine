#include "Resource/Mesh.h"
#include "RHI/RHITypes.h"
#include "RHI/DX11/DX11Buffer.h"
#include "ProfilerAPI.h"

#include <vector>

struct CMesh::Impl
{
	DX11Buffer vb;
	DX11Buffer ib;
	IRHIDevice* pDevice = nullptr;
	RHIMeshSlice rhiSlice{};
};

CMesh::CMesh()
	: m_pImpl(new Impl())
{
}

CMesh::~CMesh()
{
	if (m_pImpl && m_pImpl->pDevice)
	{
		if (m_pImpl->rhiSlice.hIndexBuffer.IsValid())
			m_pImpl->pDevice->DestroyBuffer(m_pImpl->rhiSlice.hIndexBuffer);
		if (m_pImpl->rhiSlice.hVertexBuffer.IsValid())
			m_pImpl->pDevice->DestroyBuffer(m_pImpl->rhiSlice.hVertexBuffer);
	}

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

	WINTERS_PROFILE_COUNT("Mesh::DrawCalls", 1);
	WINTERS_PROFILE_COUNT("Mesh::SubmittedIndices", m_iIndexCount);

	m_pImpl->vb.BindVertex(pContext, m_iVertexStride);
	m_pImpl->ib.BindIndex(pContext);
	m_pImpl->ib.DrawIndexed(pContext);
}

void CMesh::RenderIndexRange(IRHIDevice* pDevice, u32_t iStartIndex, u32_t iIndexCount)
{
	ID3D11DeviceContext* pContext = static_cast<ID3D11DeviceContext*>(
		pDevice ? pDevice->GetNativeHandle(eNativeHandleType::DX11DeviceContext) : nullptr);
	if (!pContext || !m_pImpl || iIndexCount == 0)
		return;

	WINTERS_PROFILE_COUNT("Mesh::DrawCalls", 1);
	WINTERS_PROFILE_COUNT("Mesh::SubmittedIndices", iIndexCount);

	m_pImpl->vb.BindVertex(pContext, m_iVertexStride);
	m_pImpl->ib.BindIndex(pContext);
	m_pImpl->ib.DrawIndexedRange(pContext, iStartIndex, iIndexCount);
}

RHIMeshSlice CMesh::GetRHIMeshSlice(u32_t iStartIndex, u32_t iIndexCount) const
{
	if (!m_pImpl ||
		!m_pImpl->rhiSlice.hVertexBuffer.IsValid() ||
		!m_pImpl->rhiSlice.hIndexBuffer.IsValid())
	{
		return {};
	}

	if (iStartIndex >= m_pImpl->rhiSlice.indexCount)
		return {};

	const u32_t availableCount = m_pImpl->rhiSlice.indexCount - iStartIndex;
	const u32_t resolvedCount =
		(iIndexCount == 0 || iIndexCount > availableCount)
			? availableCount
			: iIndexCount;
	if (resolvedCount == 0)
		return {};

	RHIMeshSlice slice = m_pImpl->rhiSlice;
	slice.firstIndex += iStartIndex;
	slice.indexCount = resolvedCount;
	return slice;
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

	pInstance->m_pImpl->pDevice = pDevice;

	RHIBufferDesc vertexDesc{};
	vertexDesc.sizeBytes = iVertexStride * iVertexCount;
	vertexDesc.usage = eRHIBufferUsage::Vertex;
	vertexDesc.memoryUsage = eRHIMemoryUsage::Default;
	vertexDesc.debugName = "CMesh.RHI.VertexBuffer";
	pInstance->m_pImpl->rhiSlice.hVertexBuffer =
		pDevice->CreateBuffer(vertexDesc, pVertices);

	std::vector<u32_t> vecIndices32;
	const void* pRHIIndexData = pIndices;
	if (!bUse32Bit)
	{
		const u16_t* pSource = static_cast<const u16_t*>(pIndices);
		vecIndices32.resize(iIndexCount);
		for (u32_t i = 0; i < iIndexCount; ++i)
			vecIndices32[i] = pSource[i];
		pRHIIndexData = vecIndices32.data();
	}

	RHIBufferDesc indexDesc{};
	indexDesc.sizeBytes = iIndexCount * static_cast<u32_t>(sizeof(u32_t));
	indexDesc.usage = eRHIBufferUsage::Index;
	indexDesc.memoryUsage = eRHIMemoryUsage::Default;
	indexDesc.debugName = "CMesh.RHI.IndexBuffer";
	pInstance->m_pImpl->rhiSlice.hIndexBuffer =
		pDevice->CreateBuffer(indexDesc, pRHIIndexData);
	pInstance->m_pImpl->rhiSlice.vertexStride = iVertexStride;
	pInstance->m_pImpl->rhiSlice.indexCount = iIndexCount;
	pInstance->m_pImpl->rhiSlice.vertexLayout = eRenderVertexLayout::PositionNormalUv;

	return pInstance;
}
