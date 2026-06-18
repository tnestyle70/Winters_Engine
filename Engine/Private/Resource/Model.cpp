#include "Resource/Model.h"
#include "Resource/Mesh.h"
#include "Resource/Texture.h"
#include "ProfilerAPI.h"
#include "WintersPaths.h"
#include "AssetFormat/Anim/WAnimLoader.h"
#include "AssetFormat/Anim/WSkelLoader.h"
#include "AssetFormat/Material/WMaterialLoader.h"
#include "AssetFormat/Mesh/WMeshLoader.h"

#include <Windows.h>
#include <algorithm>
#include <cfloat>
#include <cstring>
#include <filesystem>
#include <cmath>
#include <d3d11.h>

using namespace Engine;

namespace
{
	constexpr size_t kCombinedStaticCoarseCullSubmeshThreshold = 512u;

	ID3D11Device* GetNativeDX11Device(IRHIDevice* pDevice)
	{
		if (!pDevice)
			return nullptr;
		return static_cast<ID3D11Device*>(
			pDevice->GetNativeHandle(eNativeHandleType::DX11Device));
	}

	std::string ReplaceExt(const std::string& strPath, const char* pExt)
	{
		const size_t dot = strPath.find_last_of('.');
		if (dot == std::string::npos) return strPath + pExt;
		return strPath.substr(0, dot) + pExt;
	}

	std::string ReplaceExtToWMesh(const std::string& strPath)
	{
		return ReplaceExt(strPath, ".wmesh");
	}

	std::wstring ToWidePath(const std::string& strPath)
	{
		return std::wstring(strPath.begin(), strPath.end());
	}

	std::string ToNarrowPath(const std::wstring& strPath)
	{
		std::string strResult;
		strResult.reserve(strPath.size());
		for (wchar_t ch : strPath)
			strResult.push_back(static_cast<char>(ch));
		return strResult;
	}

	std::wstring ResolveContentPathOrInput(const std::string& strPath)
	{
		const std::wstring wPath = ToWidePath(strPath);
		wchar_t fullPath[MAX_PATH] = {};
		if (WintersResolveContentPath(wPath.c_str(), fullPath, MAX_PATH))
			return fullPath;
		return wPath;
	}

	bool WMeshAndWSkelNamesMatch(
		const Winters::Asset::WMeshLoaded& wm,
		const Winters::Asset::WSkelLoaded& ws)
	{
		if (wm.header.bone_count != ws.header.bone_count)
			return false;
		for (u32_t i = 0; i < wm.header.bone_count; ++i)
		{
			if (wm.bones[i].name_hash != ws.bones[i].name_hash)
				return false;
		}
		return true;
	}

	bool BuildMeshesFromWMesh(IRHIDevice* pDevice,
		const Winters::Asset::WMeshLoaded& wm,
		std::vector<unique_ptr<CMesh>>& outMeshes,
		std::vector<CModel::SubmeshInfo>& outSubmeshInfos)
	{
		WINTERS_PROFILE_SCOPE("Model::BuildFromWMesh");
		outMeshes.clear();
		outSubmeshInfos.clear();
		outMeshes.reserve(wm.subMeshes.size());
		outSubmeshInfos.reserve(wm.subMeshes.size());

		u32_t iSubmesh = 0;
		const bool bIdx32 = wm.header.index_stride == 4;
		for (const auto& s : wm.subMeshes)
		{
			const uint8_t* pV = wm.pVertexBlob + s.vertex_offset;
			const uint8_t* pI = wm.pIndexBlob + s.index_offset;

			auto mesh = CMesh::Create(pDevice,
				pV, wm.header.vertex_stride, s.vertex_count,
				pI, s.index_count, bIdx32);
			if (!mesh)
				return false;

			mesh->SetMaterialIndex(s.material_index);

			CModel::SubmeshInfo info{};
			info.index = iSubmesh;
			info.materialIndex = s.material_index;
			info.materialHash = s.material_hash;
			std::memcpy(info.name, s.name, sizeof(info.name) - 1);
			info.name[sizeof(info.name) - 1] = '\0';
			outSubmeshInfos.push_back(info);

			outMeshes.push_back(std::move(mesh));
			++iSubmesh;
		}
		return true;
	}

	bool BuildCombinedStaticMeshFromWMesh(
		IRHIDevice* pDevice,
		const Winters::Asset::WMeshLoaded& wm,
		unique_ptr<CMesh>& outMesh,
		std::vector<CModel::CombinedSubmeshRange>& outRanges,
		std::vector<CModel::SubmeshInfo>& outSubmeshInfos)
	{
		WINTERS_PROFILE_SCOPE("Model::BuildCombinedStaticMesh");

		outMesh.reset();
		outRanges.clear();
		outSubmeshInfos.clear();

		if (!wm.pVertexBlob || !wm.pIndexBlob ||
			wm.header.vertex_stride == 0 ||
			wm.header.total_vertex_count == 0 ||
			wm.header.total_index_count == 0)
		{
			return false;
		}

		const bool_t bSourceIndex32 = wm.header.index_stride == 4;
		const bool_t bOutputIndex32 = wm.header.total_vertex_count > 65535u || bSourceIndex32;
		std::vector<uint32_t> vecIndices32;
		std::vector<uint16_t> vecIndices16;

		if (bOutputIndex32)
			vecIndices32.reserve(wm.header.total_index_count);
		else
			vecIndices16.reserve(wm.header.total_index_count);

		std::vector<u32_t> vecDrawOrder;
		vecDrawOrder.reserve(wm.subMeshes.size());
		for (u32_t i = 0; i < static_cast<u32_t>(wm.subMeshes.size()); ++i)
			vecDrawOrder.push_back(i);
		std::stable_sort(
			vecDrawOrder.begin(),
			vecDrawOrder.end(),
			[&](u32_t a, u32_t b)
			{
				const auto& left = wm.subMeshes[a];
				const auto& right = wm.subMeshes[b];
				if (left.material_index != right.material_index)
					return left.material_index < right.material_index;
				return a < b;
			});

		outRanges.reserve(wm.subMeshes.size());
		outSubmeshInfos.reserve(wm.subMeshes.size());

		for (u32_t iSubmesh = 0; iSubmesh < static_cast<u32_t>(wm.subMeshes.size()); ++iSubmesh)
		{
			const auto& s = wm.subMeshes[iSubmesh];

			CModel::SubmeshInfo info{};
			info.index = iSubmesh;
			info.materialIndex = s.material_index;
			info.materialHash = s.material_hash;
			std::memcpy(info.name, s.name, sizeof(info.name) - 1);
			info.name[sizeof(info.name) - 1] = '\0';
			outSubmeshInfos.push_back(info);
		}

		for (u32_t iSubmesh : vecDrawOrder)
		{
			const auto& s = wm.subMeshes[iSubmesh];
			const u32_t vertexBase = s.vertex_offset / wm.header.vertex_stride;
			const u32_t indexStart = bOutputIndex32
				? static_cast<u32_t>(vecIndices32.size())
				: static_cast<u32_t>(vecIndices16.size());

			const uint8_t* pIndexBytes = wm.pIndexBlob + s.index_offset;
			for (u32_t i = 0; i < s.index_count; ++i)
			{
				u32_t localIndex = 0;
				if (bSourceIndex32)
				{
					std::memcpy(
						&localIndex,
						pIndexBytes + static_cast<size_t>(i) * sizeof(uint32_t),
						sizeof(uint32_t));
				}
				else
				{
					uint16_t localIndex16 = 0;
					std::memcpy(
						&localIndex16,
						pIndexBytes + static_cast<size_t>(i) * sizeof(uint16_t),
						sizeof(uint16_t));
					localIndex = localIndex16;
				}

				const u32_t globalIndex = vertexBase + localIndex;
				if (bOutputIndex32)
				{
					vecIndices32.push_back(globalIndex);
				}
				else
				{
					if (globalIndex > 65535u)
						return false;
					vecIndices16.push_back(static_cast<uint16_t>(globalIndex));
				}
			}

			CModel::CombinedSubmeshRange range{};
			range.submeshIndex = iSubmesh;
			range.indexStart = indexStart;
			range.indexCount = s.index_count;
			range.materialIndex = s.material_index;
			outRanges.push_back(range);
		}

		const void* pIndexData = bOutputIndex32
			? static_cast<const void*>(vecIndices32.data())
			: static_cast<const void*>(vecIndices16.data());
		const u32_t indexCount = bOutputIndex32
			? static_cast<u32_t>(vecIndices32.size())
			: static_cast<u32_t>(vecIndices16.size());

		outMesh = CMesh::Create(
			pDevice,
			wm.pVertexBlob,
			wm.header.vertex_stride,
			wm.header.total_vertex_count,
			pIndexData,
			indexCount,
			bOutputIndex32);

		return outMesh != nullptr;
	}

	bool IsFiniteVec3(const Vec3& v)
	{
		return std::isfinite(v.x) &&
			std::isfinite(v.y) &&
			std::isfinite(v.z);
	}

	void ExpandBounds(CModel::LocalBounds& bounds, const Vec3& v)
	{
		if (!IsFiniteVec3(v))
			return;

		if (!bounds.bValid)
		{
			bounds.vMin = v;
			bounds.vMax = v;
			bounds.bValid = true;
			return;
		}

		bounds.vMin.x = (std::min)(bounds.vMin.x, v.x);
		bounds.vMin.y = (std::min)(bounds.vMin.y, v.y);
		bounds.vMin.z = (std::min)(bounds.vMin.z, v.z);
		bounds.vMax.x = (std::max)(bounds.vMax.x, v.x);
		bounds.vMax.y = (std::max)(bounds.vMax.y, v.y);
		bounds.vMax.z = (std::max)(bounds.vMax.z, v.z);
	}

	void ExpandBounds(CModel::LocalBounds& bounds, const CModel::LocalBounds& other)
	{
		if (!other.bValid)
			return;

		ExpandBounds(bounds, other.vMin);
		ExpandBounds(bounds, other.vMax);
	}

	CModel::LocalBounds MakeBoundsFromWMeshBounds(
		const Winters::Asset::SubMeshBounds& bounds)
	{
		CModel::LocalBounds result{};
		const Vec3 vMin{
			bounds.aabb_min[0],
			bounds.aabb_min[1],
			bounds.aabb_min[2]
		};
		const Vec3 vMax{
			bounds.aabb_max[0],
			bounds.aabb_max[1],
			bounds.aabb_max[2]
		};

		if (!IsFiniteVec3(vMin) || !IsFiniteVec3(vMax) ||
			vMin.x > vMax.x || vMin.y > vMax.y || vMin.z > vMax.z)
		{
			return result;
		}

		result.vMin = vMin;
		result.vMax = vMax;
		result.bValid = true;
		return result;
	}

	CModel::LocalBounds ComputeBoundsFromVertices(
		const Winters::Asset::WMeshLoaded& wm,
		const Winters::Asset::SubMeshDesc& submesh)
	{
		CModel::LocalBounds result{};
		if (!wm.pVertexBlob || wm.header.vertex_stride == 0 || submesh.vertex_count == 0)
			return result;

		const size_t byteOffset = static_cast<size_t>(submesh.vertex_offset);
		const size_t byteCount =
			static_cast<size_t>(submesh.vertex_count) * wm.header.vertex_stride;
		if (byteOffset > wm.uVertexBlobBytes ||
			byteCount > wm.uVertexBlobBytes - byteOffset)
		{
			return result;
		}

		const uint8_t* pVertex = wm.pVertexBlob + byteOffset;
		for (u32_t i = 0; i < submesh.vertex_count; ++i)
		{
			const float* pPos = reinterpret_cast<const float*>(
				pVertex + static_cast<size_t>(i) * wm.header.vertex_stride);
			ExpandBounds(result, Vec3{ pPos[0], pPos[1], pPos[2] });
		}

		return result;
	}

	std::vector<CModel::LocalBounds> BuildSubmeshBounds(
		const Winters::Asset::WMeshLoaded& wm)
	{
		std::vector<CModel::LocalBounds> result;
		result.resize(wm.subMeshes.size());

		const bool_t bHasAuthoredBounds =
			wm.header.has_bounding != 0 &&
			wm.bounds.size() == wm.subMeshes.size();

		for (u32_t i = 0; i < static_cast<u32_t>(wm.subMeshes.size()); ++i)
		{
			if (bHasAuthoredBounds)
				result[i] = MakeBoundsFromWMeshBounds(wm.bounds[i]);

			if (!result[i].bValid)
				result[i] = ComputeBoundsFromVertices(wm, wm.subMeshes[i]);
		}

		return result;
	}

	bool_t IsBoundsVisibleInClip(
		const CModel::LocalBounds& bounds,
		const Mat4& matLocalToClip,
		f32_t fMinVisibleNdcExtent = 0.f)
	{
		if (!bounds.bValid)
			return true;

		const XMMATRIX localToClip = matLocalToClip.ToXMMATRIX();
		const Vec3& mn = bounds.vMin;
		const Vec3& mx = bounds.vMax;
		bool_t bOutsideLeft = true;
		bool_t bOutsideRight = true;
		bool_t bOutsideBottom = true;
		bool_t bOutsideTop = true;
		bool_t bOutsideNear = true;
		bool_t bOutsideFar = true;
		bool_t bCrossesNearPlane = false;
		f32_t fMinX = FLT_MAX;
		f32_t fMinY = FLT_MAX;
		f32_t fMaxX = -FLT_MAX;
		f32_t fMaxY = -FLT_MAX;

		for (u32_t i = 0; i < 8u; ++i)
		{
			const f32_t x = (i & 1u) ? mx.x : mn.x;
			const f32_t y = (i & 2u) ? mx.y : mn.y;
			const f32_t z = (i & 4u) ? mx.z : mn.z;
			const XMVECTOR clip = XMVector4Transform(
				XMVectorSet(x, y, z, 1.f),
				localToClip);

			XMFLOAT4 c{};
			XMStoreFloat4(&c, clip);
			bCrossesNearPlane = bCrossesNearPlane || c.w <= 0.0001f;

			if (c.x >= -c.w) bOutsideLeft = false;
			if (c.x <= c.w) bOutsideRight = false;
			if (c.y >= -c.w) bOutsideBottom = false;
			if (c.y <= c.w) bOutsideTop = false;
			if (c.z >= 0.f) bOutsideNear = false;
			if (c.z <= c.w) bOutsideFar = false;

			if (c.w > 0.0001f)
			{
				const f32_t invW = 1.f / c.w;
				const f32_t ndcX = c.x * invW;
				const f32_t ndcY = c.y * invW;
				fMinX = (std::min)(fMinX, ndcX);
				fMaxX = (std::max)(fMaxX, ndcX);
				fMinY = (std::min)(fMinY, ndcY);
				fMaxY = (std::max)(fMaxY, ndcY);
			}
		}

		if (bCrossesNearPlane)
			return true;

		if (bOutsideLeft || bOutsideRight ||
			bOutsideBottom || bOutsideTop ||
			bOutsideNear || bOutsideFar)
		{
			return false;
		}

		if (fMinVisibleNdcExtent > 0.f &&
			fMinX <= fMaxX &&
			fMinY <= fMaxY)
		{
			const f32_t fExtentX = fMaxX - fMinX;
			const f32_t fExtentY = fMaxY - fMinY;
			if (fExtentX < fMinVisibleNdcExtent &&
				fExtentY < fMinVisibleNdcExtent)
			{
				return false;
			}
		}

		return true;
	}
}

void CModel::Render(IRHIDevice* pDevice)
{
	WINTERS_PROFILE_SCOPE("Model::RenderAllMeshes");

	if (m_pCombinedStaticMesh)
	{
		const VisibilityMask mask = MakeAllVisibleMask();
		RenderCombinedStaticWithMask(pDevice, mask);
		return;
	}

	WINTERS_PROFILE_COUNT("Model::DrawMeshCalls", static_cast<uint64_t>(m_vecMeshes.size()));

	CTexture* pLastTexture = nullptr;
	u64_t uMaterialBinds = 0;
	for (u32_t i = 0; i < static_cast<u32_t>(m_vecMeshes.size()); ++i)
	{
		CTexture* pTexture = ResolveMaterialTexture(i);
		if (pTexture && pTexture != pLastTexture)
		{
			pTexture->Bind(pDevice, 0);
			pLastTexture = pTexture;
			++uMaterialBinds;
		}
		m_vecMeshes[i]->Render(pDevice);
	}

	WINTERS_PROFILE_COUNT("Model::MaterialBinds", uMaterialBinds);
}

void CModel::RenderCombinedStaticWithMask(IRHIDevice* pDevice, const VisibilityMask& mask)
{
	WINTERS_PROFILE_SCOPE("Model::RenderCombinedStatic");

	if (!m_pCombinedStaticMesh)
		return;

	CTexture* pLastTexture = nullptr;
	CTexture* pPendingTexture = nullptr;
	u32_t iPendingStart = 0;
	u32_t iPendingCount = 0;
	u64_t uVisibleSubmeshes = 0;
	u64_t uCombinedDrawCalls = 0;
	u64_t uMaterialBinds = 0;

	auto FlushPending = [&]()
	{
		if (iPendingCount == 0)
			return;

		if (pPendingTexture && pPendingTexture != pLastTexture)
		{
			pPendingTexture->Bind(pDevice, 0);
			pLastTexture = pPendingTexture;
			++uMaterialBinds;
		}

		m_pCombinedStaticMesh->RenderIndexRange(pDevice, iPendingStart, iPendingCount);
		++uCombinedDrawCalls;
		iPendingCount = 0;
	};

	const u32_t rangeCount = static_cast<u32_t>(m_vecCombinedSubmeshRanges.size());
	for (u32_t i = 0; i < rangeCount; ++i)
	{
		const CombinedSubmeshRange& range = m_vecCombinedSubmeshRanges[i];
		if (!IsSubmeshVisible(mask, range.submeshIndex))
			continue;

		if (range.indexCount == 0)
			continue;

		CTexture* pTexture = ResolveMaterialTexture(range.submeshIndex);
		const bool_t bCanMerge =
			iPendingCount > 0 &&
			pTexture == pPendingTexture &&
			iPendingStart + iPendingCount == range.indexStart;

		if (bCanMerge)
		{
			iPendingCount += range.indexCount;
		}
		else
		{
			FlushPending();
			pPendingTexture = pTexture;
			iPendingStart = range.indexStart;
			iPendingCount = range.indexCount;
		}

		++uVisibleSubmeshes;
	}

	FlushPending();

	WINTERS_PROFILE_COUNT("Model::VisibleMeshCalls", uVisibleSubmeshes);
	WINTERS_PROFILE_COUNT("Model::CombinedDrawCalls", uCombinedDrawCalls);
	WINTERS_PROFILE_COUNT("Model::MaterialBinds", uMaterialBinds);
}

void CModel::RenderWithMask(IRHIDevice* pDevice, const VisibilityMask& mask)
{
	WINTERS_PROFILE_SCOPE("Model::RenderWithMask");

	if (m_pCombinedStaticMesh)
	{
		RenderCombinedStaticWithMask(pDevice, mask);
		return;
	}

	if (IsAllVisibleMask(mask))
	{
		Render(pDevice);
		return;
	}

	uint64_t visibleMeshes = 0;
	CTexture* pLastTexture = nullptr;
	u64_t uMaterialBinds = 0;
	for (u32_t i = 0; i < static_cast<u32_t>(m_vecMeshes.size()); ++i)
	{
		if (!IsSubmeshVisible(mask, i))
			continue;

		++visibleMeshes;
		CTexture* pTexture = ResolveMaterialTexture(i);
		if (pTexture && pTexture != pLastTexture)
		{
			pTexture->Bind(pDevice, 0);
			pLastTexture = pTexture;
			++uMaterialBinds;
		}
		m_vecMeshes[i]->Render(pDevice);
	}

	WINTERS_PROFILE_COUNT("Model::VisibleMeshCalls", visibleMeshes);
	WINTERS_PROFILE_COUNT("Model::MaterialBinds", uMaterialBinds);
}

VisibilityMask CModel::BuildClipVisibilityMask(
	const Mat4& matLocalToClip,
	bool_t* pOutAnyVisible) const
{
	VisibilityMask mask{};
	bool_t bAnyVisible = false;

	if (!m_LocalBounds.bValid)
	{
		if (pOutAnyVisible)
			*pOutAnyVisible = true;
		return MakeAllVisibleMask();
	}

	if (!IsBoundsVisibleInClip(m_LocalBounds, matLocalToClip))
	{
		if (pOutAnyVisible)
			*pOutAnyVisible = false;
		return mask;
	}

	if (m_vecSubmeshBounds.empty())
	{
		if (pOutAnyVisible)
			*pOutAnyVisible = true;
		return MakeAllVisibleMask();
	}

	const size_t cSubmeshBounds = m_vecSubmeshBounds.size();
	if (m_pCombinedStaticMesh &&
		cSubmeshBounds >= kCombinedStaticCoarseCullSubmeshThreshold)
	{
		WINTERS_PROFILE_COUNT("Model::ClipBypassLargeCombinedStatic", 1u);
		WINTERS_PROFILE_COUNT(
			"Model::ClipBypassSubmeshes",
			static_cast<u64_t>(cSubmeshBounds));
		if (pOutAnyVisible)
			*pOutAnyVisible = true;
		return MakeAllVisibleMask();
	}

	if (cSubmeshBounds > kMeshVisibilityMaxSubmeshes)
	{
		if (pOutAnyVisible)
			*pOutAnyVisible = true;
		return MakeAllVisibleMask();
	}

	const u32_t submeshCount = static_cast<u32_t>(cSubmeshBounds);
	const bool_t bAllowTinyCull = submeshCount >= 128u;
	const f32_t fMinVisibleNdcExtent = bAllowTinyCull ? 0.0035f : 0.f;
	u64_t uVisibleSubmeshes = 0;
	u64_t uRejectedSubmeshes = 0;
	for (u32_t i = 0; i < submeshCount; ++i)
	{
		const bool_t bVisible =
			!m_vecSubmeshBounds[i].bValid ||
			IsBoundsVisibleInClip(
				m_vecSubmeshBounds[i],
				matLocalToClip,
				fMinVisibleNdcExtent);
		SetSubmeshVisible(mask, i, bVisible);
		bAnyVisible = bAnyVisible || bVisible;
		if (bVisible)
			++uVisibleSubmeshes;
		else
			++uRejectedSubmeshes;
	}

	WINTERS_PROFILE_COUNT("Model::ClipVisibleSubmeshes", uVisibleSubmeshes);
	WINTERS_PROFILE_COUNT("Model::ClipRejectedSubmeshes", uRejectedSubmeshes);

	if (pOutAnyVisible)
		*pOutAnyVisible = bAnyVisible;
	return mask;
}

i32_t CModel::FindSubmeshByMaterialHash(u64_t materialHash) const
{
	for (u32_t i = 0; i < static_cast<u32_t>(m_vecSubmeshInfos.size()); ++i)
	{
		if (m_vecSubmeshInfos[i].materialHash == materialHash)
			return static_cast<i32_t>(i);
	}
	return -1;
}

void CModel::DumpSubmeshes(const char* pLabel) const
{
	OutputDebugStringA("[CModel] Submesh dump");
	if (pLabel)
	{
		OutputDebugStringA(" ");
		OutputDebugStringA(pLabel);
	}
	OutputDebugStringA("\n");

	for (const auto& info : m_vecSubmeshInfos)
	{
		char msg[192]{};
		sprintf_s(msg,
			"  [%u] name='%s' material=%u hash=%llu\n",
			info.index,
			info.name[0] ? info.name : "(empty)",
			info.materialIndex,
			static_cast<unsigned long long>(info.materialHash));
		OutputDebugStringA(msg);
	}
}

CTexture* CModel::ResolveMaterialTexture(u32_t iMeshIndex) const
{
	if (iMeshIndex < m_vecMeshTextureOverrides.size() && m_vecMeshTextureOverrides[iMeshIndex])
		return m_vecMeshTextureOverrides[iMeshIndex];

	if (m_pOverrideTexture)
		return m_pOverrideTexture;

	u32_t matIdx = 0;
	if (iMeshIndex < m_vecSubmeshInfos.size())
	{
		matIdx = m_vecSubmeshInfos[iMeshIndex].materialIndex;
	}
	else if (iMeshIndex < m_vecMeshes.size())
	{
		matIdx = m_vecMeshes[iMeshIndex]->GetMaterialIndex();
	}
	else
	{
		return nullptr;
	}

	if (matIdx < m_vecTextures.size() && m_vecTextures[matIdx])
		return m_vecTextures[matIdx].get();

	return m_pDefaultTexture.get();
}

void CModel::BindMaterial(IRHIDevice* pDevice, u32_t iMeshIndex)
{
	CTexture* pTexture = ResolveMaterialTexture(iMeshIndex);
	if (pTexture)
		pTexture->Bind(pDevice, 0);
}

void CModel::SetMeshTexture(u32_t iMeshIndex, CTexture* pTexture)
{
	const u32_t meshCount = GetMeshCount();
	if (iMeshIndex >= meshCount)
		return;

	if (m_vecMeshTextureOverrides.size() <= iMeshIndex)
		m_vecMeshTextureOverrides.resize(meshCount, nullptr);

	m_vecMeshTextureOverrides[iMeshIndex] = pTexture;
}

CAnimation* CModel::GetAnimation(u32_t iIndex) const
{
	if (iIndex >= m_vecAnimations.size()) return nullptr;
	return m_vecAnimations[iIndex].get();
}

i32_t CModel::FindAnimationIndex(const string& strName) const
{
	for (u32_t i = 0; i < (u32_t)m_vecAnimations.size(); ++i)
	{
		if (m_vecAnimations[i]->GetName().find(strName) != string::npos)
			return (i32_t)i;
	}
	return -1;
}

unique_ptr<CModel> CModel::Create(IRHIDevice* pDevice, const string& strFilePath)
{
	auto pInstance = unique_ptr<CModel>(new CModel());

	if (FAILED(pInstance->LoadModel(pDevice, strFilePath)))
		return nullptr;

	return pInstance;
}

HRESULT CModel::LoadModel(IRHIDevice* pDevice, const string& strFilePath)
{
	WINTERS_PROFILE_SCOPE("Model::LoadModel");

	ID3D11Device* pNativeDevice = GetNativeDX11Device(pDevice);
	if (!pNativeDevice)
		return E_FAIL;

	Winters::Asset::WMeshLoaded wm;
	Winters::Asset::WSkelLoaded ws;
	const std::string requestedWMeshPath = ReplaceExtToWMesh(strFilePath);
	const std::wstring resolvedWMeshPath = ResolveContentPathOrInput(requestedWMeshPath);
	if (!std::filesystem::exists(resolvedWMeshPath))
	{
		OutputDebugStringA(("[CModel] cooked .wmesh missing: " + requestedWMeshPath + "\n").c_str());
		return E_FAIL;
	}

	{
		WINTERS_PROFILE_SCOPE("Model::WMeshLoad");
		if (!Winters::Asset::CWMeshLoader::Load(resolvedWMeshPath.c_str(), wm))
		{
			OutputDebugStringW((L"[CModel] .wmesh load failed: " + resolvedWMeshPath + L"\n").c_str());
			return E_FAIL;
		}
	}

	const bool bNeedsSkeleton = wm.header.bone_count > 0;
	std::filesystem::path resolvedWSkelPath;
	if (bNeedsSkeleton)
	{
		resolvedWSkelPath = std::filesystem::path(resolvedWMeshPath).replace_extension(L".wskel");
		if (!std::filesystem::exists(resolvedWSkelPath))
		{
			OutputDebugStringW((L"[CModel] .wskel missing: " + resolvedWSkelPath.wstring() + L"\n").c_str());
			return E_FAIL;
		}

		if (!Winters::Asset::CWSkelLoader::Load(resolvedWSkelPath.c_str(), ws) ||
			!WMeshAndWSkelNamesMatch(wm, ws))
		{
			OutputDebugStringW((L"[CModel] .wmesh/.wskel mismatch: " + resolvedWMeshPath + L"\n").c_str());
			return E_FAIL;
		}
	}

	m_pDefaultTexture = CTexture::CreateDefault(pDevice);
	LoadCookedTextures(pDevice, ToNarrowPath(resolvedWMeshPath), wm);

	if (bNeedsSkeleton)
	{
		if (!BuildMeshesFromWMesh(pDevice, wm, m_vecMeshes, m_vecSubmeshInfos))
		{
			OutputDebugStringW((L"[CModel] .wmesh build failed: " + resolvedWMeshPath + L"\n").c_str());
			return E_FAIL;
		}
	}
	else
	{
		if (!BuildCombinedStaticMeshFromWMesh(
			pDevice,
			wm,
			m_pCombinedStaticMesh,
			m_vecCombinedSubmeshRanges,
			m_vecSubmeshInfos))
		{
			OutputDebugStringW((L"[CModel] combined .wmesh build failed: " + resolvedWMeshPath + L"\n").c_str());
			return E_FAIL;
		}
	}
	m_vecSubmeshBounds = BuildSubmeshBounds(wm);
	m_LocalBounds = {};
	for (const LocalBounds& bounds : m_vecSubmeshBounds)
		ExpandBounds(m_LocalBounds, bounds);

	if (bNeedsSkeleton)
	{
		m_pSkeleton = BuildSkeletonFromStage3(ws, wm);
		m_bHasBones = (m_pSkeleton != nullptr);
		if (!m_pSkeleton)
			return E_FAIL;

		LoadCookedAnimations(ToNarrowPath(resolvedWMeshPath), ws);
		m_pAnimator = CAnimator::Create(m_pSkeleton.get());
		if (m_pAnimator && !m_vecAnimations.empty())
			m_pAnimator->PlayAnimation(m_vecAnimations[0].get());
	}
	else
	{
		m_bHasBones = false;
		m_iAnimCount = 0;
	}

	OutputDebugStringA(("[CModel] cooked load OK: " + requestedWMeshPath
		+ " meshes=" + to_string(GetMeshCount())
		+ " materials=" + to_string(m_vecTextures.size())
		+ " animations=" + to_string(m_iAnimCount)
		+ " bones=" + to_string(m_pSkeleton ? m_pSkeleton->GetBoneCount() : 0)
		+ "\n").c_str());

	if (m_pSkeleton && m_pSkeleton->GetBoneCount() > 256)
		OutputDebugStringA("[CModel] WARNING: bone count > 256! cbuffer overflow!\n");

	return S_OK;
}

void CModel::LoadCookedTextures(IRHIDevice* pDevice,
	const string& strMeshPath,
	const Winters::Asset::WMeshLoaded& wm)
{
	u32_t materialCount = 0;
	for (const auto& sub : wm.subMeshes)
		materialCount = (std::max)(materialCount, sub.material_index + 1u);
	m_vecTextures.clear();
	m_vecTextures.resize(materialCount);

	const std::string wmatPath = ReplaceExt(strMeshPath, ".wmat");
	const std::wstring wmatWPath = ToWidePath(wmatPath);

	Winters::Asset::WMaterialLoaded materials;
	if (!Winters::Asset::CWMaterialLoader::Load(wmatWPath.c_str(), materials))
	{
		OutputDebugStringW((L"[CModel] .wmat missing or invalid: " + wmatWPath + L"\n").c_str());
		return;
	}

	if (materials.header.material_count > m_vecTextures.size())
		m_vecTextures.resize(materials.header.material_count);

	for (const auto& entry : materials.entries)
	{
		if (entry.material_index >= m_vecTextures.size())
			continue;
		if (entry.diffuse_path[0] == L'\0')
			continue;

		m_vecTextures[entry.material_index] = CTexture::Create(
			pDevice,
			entry.diffuse_path,
			eTexSamplerMode::Wrap,
			eTexColorSpace::ShaderLocalSRGB);
		if (!m_vecTextures[entry.material_index])
		{
			OutputDebugStringW((L"[CModel] cooked texture load failed: "
				+ std::wstring(entry.diffuse_path) + L"\n").c_str());
		}
	}
}

void CModel::LoadCookedAnimations(const string& strMeshPath,
	const Winters::Asset::WSkelLoaded& ws)
{
	m_vecAnimations.clear();

	const std::filesystem::path animDir =
		std::filesystem::path(ToWidePath(strMeshPath)).parent_path() / L"anims";
	if (m_pSkeleton && std::filesystem::exists(animDir))
	{
		std::vector<std::filesystem::path> animPaths;
		for (const auto& entry : std::filesystem::directory_iterator(animDir))
		{
			if (entry.path().extension() == L".wanim")
				animPaths.push_back(entry.path());
		}
		std::sort(animPaths.begin(), animPaths.end());

		for (const auto& path : animPaths)
		{
			auto anim = Winters::Asset::CWAnimLoader::LoadAsAnimation(
				path.c_str(), ws.skelHash, m_pSkeleton.get(), path.stem().wstring());
			if (anim)
				m_vecAnimations.push_back(std::move(anim));
		}
	}

	m_iAnimCount = static_cast<u32_t>(m_vecAnimations.size());
	OutputDebugStringA(("[CModel] Loaded "
		+ to_string(m_iAnimCount) + " wanim files\n").c_str());
}

unique_ptr<CSkeleton> CModel::BuildSkeletonFromStage3(
	const Winters::Asset::WSkelLoaded& ws,
	const Winters::Asset::WMeshLoaded& wm)
{
	if (!ws.bones || !ws.globalRoot)
		return nullptr;
	if (ws.header.bone_count != wm.header.bone_count)
		return nullptr;

	auto skeleton = CSkeleton::Create();
	for (u32_t i = 0; i < ws.header.bone_count; ++i)
	{
		const auto& bn = ws.bones[i];
		const auto& be = wm.bones[i];

		XMFLOAT4X4 matOffset{};
		XMFLOAT4X4 matRestLocal{};
		std::memcpy(&matOffset, be.offset_matrix, sizeof(float) * 16);
		std::memcpy(&matRestLocal, bn.rest_transform, sizeof(float) * 16);

		skeleton->AddBone(bn.name, bn.parent_index, matOffset, matRestLocal);
	}

	XMFLOAT4X4 rootInv{};
	std::memcpy(&rootInv, ws.globalRoot->global_inverse_root, sizeof(float) * 16);
	skeleton->SetGlobalInverseRoot(rootInv);
	return skeleton;
}
