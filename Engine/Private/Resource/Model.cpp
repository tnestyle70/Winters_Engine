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
#include <cstring>
#include <filesystem>
#include <d3d11.h>

using namespace Engine;

namespace
{
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
}

void CModel::Render(IRHIDevice* pDevice)
{
	for (u32_t i = 0; i < (u32_t)m_vecMeshes.size(); ++i)
	{
		BindMaterial(pDevice, i);
		m_vecMeshes[i]->Render(pDevice);
	}
}

void CModel::RenderWithMask(IRHIDevice* pDevice, const VisibilityMask& mask)
{
	if (IsAllVisibleMask(mask))
	{
		Render(pDevice);
		return;
	}

	for (u32_t i = 0; i < (u32_t)m_vecMeshes.size(); ++i)
	{
		if (!IsSubmeshVisible(mask, i))
			continue;

		BindMaterial(pDevice, i);
		m_vecMeshes[i]->Render(pDevice);
	}
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

void CModel::BindMaterial(IRHIDevice* pDevice, u32_t iMeshIndex)
{
	if (iMeshIndex < m_vecMeshTextureOverrides.size() && m_vecMeshTextureOverrides[iMeshIndex])
	{
		m_vecMeshTextureOverrides[iMeshIndex]->Bind(pDevice, 0);
		return;
	}

	if (m_pOverrideTexture)
	{
		m_pOverrideTexture->Bind(pDevice, 0);
		return;
	}

	if (iMeshIndex >= m_vecMeshes.size())
		return;

	u32_t matIdx = m_vecMeshes[iMeshIndex]->GetMaterialIndex();
	if (matIdx < m_vecTextures.size() && m_vecTextures[matIdx])
		m_vecTextures[matIdx]->Bind(pDevice, 0);
	else if (m_pDefaultTexture)
		m_pDefaultTexture->Bind(pDevice, 0);
}

void CModel::SetMeshTexture(u32_t iMeshIndex, CTexture* pTexture)
{
	if (iMeshIndex >= m_vecMeshes.size())
		return;

	if (m_vecMeshTextureOverrides.size() <= iMeshIndex)
		m_vecMeshTextureOverrides.resize(m_vecMeshes.size(), nullptr);

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

	if (!BuildMeshesFromWMesh(pDevice, wm, m_vecMeshes, m_vecSubmeshInfos))
	{
		OutputDebugStringW((L"[CModel] .wmesh build failed: " + resolvedWMeshPath + L"\n").c_str());
		return E_FAIL;
	}

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
		+ " meshes=" + to_string(m_vecMeshes.size())
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
