#pragma once
#include "Engine_Defines.h"
#include "Resource/Bone.h"

NS_BEGIN(Engine)

class CSkeleton
{
private:
	CSkeleton() = default;
public:
	~CSkeleton() = default;
	
	static unique_ptr<CSkeleton> Create();

	i32_t AddBone(const string& strName, i32_t iParentIndex,
		const DirectX::XMFLOAT4X4& matOffset,
		const DirectX::XMFLOAT4X4& matRestLocal);
	i32_t FindBoneIndex(const string& strName) const;

	void SetGlobalInverseRoot(const DirectX::XMFLOAT4X4& mat) { m_matGlobalInverseRoot = mat; }

	void ComputeFinalTransforms(
		const vector<XMFLOAT4X4>& vecLocalTransforms,
		vector<XMFLOAT4X4>& vecOutFinal,
		vector<XMFLOAT4X4>* pOutGlobal = nullptr) const;
	void ComputeFinalTransformsWithScratch(
		const vector<XMFLOAT4X4>& vecLocalTransforms,
		vector<XMFLOAT4X4>& vecOutFinal,
		vector<XMMATRIX>& vecGlobalScratch,
		vector<XMFLOAT4X4>* pOutGlobal = nullptr) const;

	u32_t GetBoneCount() const { return (u32_t)m_vecBones.size(); }
	const BoneInfo& GetBone(u32_t iIndex) const { return m_vecBones[iIndex]; }

private:
	vector<BoneInfo> m_vecBones;
	unordered_map<string, i32_t> m_mapBoneIndex;
	DirectX::XMFLOAT4X4 m_matGlobalInverseRoot;
};

NS_END
