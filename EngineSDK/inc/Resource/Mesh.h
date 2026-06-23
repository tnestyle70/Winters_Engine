#pragma once
#include "Engine_Defines.h"
#include "RHI/IRHIDevice.h"
#include "Renderer/RenderWorldSnapshot.h"

NS_BEGIN(Engine)

class CMesh
{
private:
	CMesh();

public:
	~CMesh();
	CMesh(const CMesh&) = delete;
	CMesh& operator=(const CMesh&) = delete;

	void Render(IRHIDevice* pDevice);
	void RenderIndexRange(IRHIDevice* pDevice, u32_t iStartIndex, u32_t iIndexCount);

	RHIMeshSlice GetRHIMeshSlice(u32_t iStartIndex = 0, u32_t iIndexCount = 0) const;
	u32_t GetIndexCount() const { return m_iIndexCount; }
	u32_t GetMaterialIndex() const { return m_iMaterialIndex; }
	void	SetMaterialIndex(u32_t iIndex) { m_iMaterialIndex = iIndex; }

	static unique_ptr<CMesh> Create(
		IRHIDevice* pDevice,
		const void* pVertices, u32_t iVertexStride, u32_t iVertexCount,
		const void* pIndices, u32_t iIndexCount, bool_t bUse32Bit = true);

private:
	struct Impl;
	Impl* m_pImpl = nullptr;
	u32_t m_iVertexStride = 0;
	u32_t m_iIndexCount = 0;
	u32_t m_iMaterialIndex = 0;
};

NS_END
