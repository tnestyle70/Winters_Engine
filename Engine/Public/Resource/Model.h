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
	~CModel() = default;

	struct SubmeshInfo
	{
		u32_t index = 0;
		u32_t materialIndex = 0;
		u64_t materialHash = 0;
		char name[20]{};
	};
	
	void Render(IRHIDevice* pDevice);
	void RenderWithMask(IRHIDevice* pDevice, const VisibilityMask& mask);

	u32_t GetMeshCount() const { return (u32_t)m_vecMeshes.size(); }
	u32_t GetAnimationCount() const { return m_iAnimCount; }
	const vector<SubmeshInfo>& GetSubmeshInfos() const { return m_vecSubmeshInfos; }
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

	static unique_ptr<CModel> Create(IRHIDevice* pDevice, const string& strFilePath);
private:
	HRESULT LoadModel(IRHIDevice* pDevice, const string& strFilePath);
	void LoadCookedTextures(IRHIDevice* pDevice,
		const string& strMeshPath,
		const Winters::Asset::WMeshLoaded& wm);
	void LoadCookedAnimations(const string& strMeshPath,
		const Winters::Asset::WSkelLoaded& ws);

	//스켈레톤, 애니메이션 로딩
	unique_ptr<CSkeleton> BuildSkeletonFromStage3(
		const Winters::Asset::WSkelLoaded& ws,
		const Winters::Asset::WMeshLoaded& wm);

	vector<unique_ptr<CMesh>> m_vecMeshes;
	vector<SubmeshInfo> m_vecSubmeshInfos;
	vector<unique_ptr<CTexture>> m_vecTextures;
	vector<CTexture*> m_vecMeshTextureOverrides; //비소유 메시 인덱스 기준
	unique_ptr<CTexture> m_pDefaultTexture;
	CTexture* m_pOverrideTexture = nullptr;		// 비소유 (ModelRenderer가 소유)
	u32_t m_iAnimCount = 0;

	unique_ptr<CSkeleton> m_pSkeleton;
	unique_ptr<CAnimator> m_pAnimator;
	vector<unique_ptr<CAnimation>> m_vecAnimations;
	bool_t m_bHasBones = false;
};

NS_END
