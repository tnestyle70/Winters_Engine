#pragma once
#include "Engine_Defines.h"
#include "RHI/IRHIDevice.h"
#include "Resource/Texture.h"
#include "Resource/Mesh.h"
#include "Resource/Bone.h"
#include "Resource/Skeleton.h"
#include "Resource/Animation.h"
#include "Resource/Animator.h"
#include "ECS/Components/MeshGroupVisibilityComponent.h"
#include "WintersMath.h"

namespace Winters::Asset
{
	struct WMeshLoaded;
	struct WSkelLoaded;
}

NS_BEGIN(Engine)

class CModel
{
private:
	CModel() = default;
public:
	~CModel();

	struct SubmeshInfo
	{
		u32_t index = 0;
		u32_t materialIndex = 0;
		u64_t materialHash = 0;
		char name[20]{};
	};

	struct LocalBounds
	{
		Vec3 vMin{};
		Vec3 vMax{};
		bool_t bValid = false;
	};

	struct CombinedSubmeshRange
	{
		u32_t submeshIndex = 0;
		u32_t indexStart = 0;
		u32_t indexCount = 0;
		u32_t materialIndex = 0;
	};
	
	void Render(IRHIDevice* pDevice);
	void RenderWithMask(IRHIDevice* pDevice, const VisibilityMask& mask);
	u32_t AppendRenderSnapshotMeshes(
		RenderWorldSnapshot& snapshot,
		const Mat4& matWorld,
		const VisibilityMask& mask,
		u32_t maxItems = 0) const;

	u32_t GetMeshCount() const
	{
		return !m_vecSubmeshInfos.empty()
			? (u32_t)m_vecSubmeshInfos.size()
			: (u32_t)m_vecMeshes.size();
	}
	u32_t GetAnimationCount() const { return m_iAnimCount; }
	const vector<SubmeshInfo>& GetSubmeshInfos() const { return m_vecSubmeshInfos; }
	bool_t HasValidAABB() const { return m_LocalBounds.bValid; }
	Vec3 GetLocalAABBMin() const { return m_LocalBounds.bValid ? m_LocalBounds.vMin : Vec3{}; }
	Vec3 GetLocalAABBMax() const { return m_LocalBounds.bValid ? m_LocalBounds.vMax : Vec3{}; }
	VisibilityMask BuildClipVisibilityMask(const Mat4& matLocalToClip,
		bool_t* pOutAnyVisible = nullptr) const;
	i32_t FindSubmeshByMaterialHash(u64_t materialHash) const;
	void DumpSubmeshes(const char* pLabel = nullptr) const;

	void BindMaterial(IRHIDevice* pDevice, u32_t iMeshIndex);

	//텍스쳐 매핑
	void SetMeshTexture(u32_t iMeshIndex, CTexture* pTexture);

	// 강제 텍스처 (FBX에 텍스처 경로 없을 때)
	void SetOverrideTexture(CTexture* pTexture) { m_pOverrideTexture = pTexture; }

	//스켈레톤, 애니메이션에 대한 접근
	bool_t HasSkeleton() const { return m_pSkeleton != nullptr; }
	CSkeleton* GetSkeleton() const { return m_pSkeleton.get(); }
	CAnimator* GetAnimator() const { return m_pAnimator.get(); }
	CAnimation* GetAnimation(u32_t iIndex) const;
	i32_t FindAnimationIndex(const string& strName) const;

	using LoadYieldCallback = bool_t(*)();

	static unique_ptr<CModel> Create(
		IRHIDevice* pDevice,
		const string& strFilePath,
		LoadYieldCallback pYield = nullptr);

private:
	HRESULT LoadModel(
		IRHIDevice* pDevice,
		const string& strFilePath,
		LoadYieldCallback pYield);
	HRESULT LoadCookedTextures(IRHIDevice* pDevice,
		const string& strMeshPath,
		const Winters::Asset::WMeshLoaded& wm,
		LoadYieldCallback pYield);
	void LoadCookedAnimations(const string& strMeshPath,
		const Winters::Asset::WSkelLoaded& ws);
	CTexture* ResolveMaterialTexture(u32_t iMeshIndex) const;
	RHITextureHandle ResolveMaterialRHITexture(u32_t iMeshIndex) const;
	void ReleaseRHIResources();
	void RenderCombinedStaticWithMask(IRHIDevice* pDevice, const VisibilityMask& mask);

	//스켈레톤, 애니메이션 로딩
	unique_ptr<CSkeleton> BuildSkeletonFromStage3(
		const Winters::Asset::WSkelLoaded& ws,
		const Winters::Asset::WMeshLoaded& wm);

	unique_ptr<CMesh> m_pCombinedStaticMesh;
	vector<CombinedSubmeshRange> m_vecCombinedSubmeshRanges;
	vector<unique_ptr<CMesh>> m_vecMeshes;
	vector<SubmeshInfo> m_vecSubmeshInfos;
	vector<LocalBounds> m_vecSubmeshBounds;
	LocalBounds m_LocalBounds{};
	vector<unique_ptr<CTexture>> m_vecOwnedTextures;
	vector<CTexture*> m_vecTextures;
	vector<RHITextureHandle> m_vecOwnedRHITextures;
	vector<RHITextureHandle> m_vecRHITextures;
	vector<CTexture*> m_vecMeshTextureOverrides; //비소유 메시 인덱스 기준
	unique_ptr<CTexture> m_pDefaultTexture;
	RHITextureHandle m_hDefaultRHITexture{};
	RHISamplerHandle m_hDefaultRHISampler{};
	IRHIDevice* m_pRHIDevice = nullptr;
	CTexture* m_pOverrideTexture = nullptr;		// 비소유 (ModelRenderer가 소유)
	u32_t m_iAnimCount = 0;

	unique_ptr<CSkeleton> m_pSkeleton;
	unique_ptr<CAnimator> m_pAnimator;
	vector<unique_ptr<CAnimation>> m_vecAnimations;
	bool_t m_bHasBones = false;
};

NS_END
