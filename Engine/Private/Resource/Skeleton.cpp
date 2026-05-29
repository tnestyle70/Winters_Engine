#include "Resource/Skeleton.h"

//이거 꼭 써야 하나? Directx:: 이것도 Engine_Defines.h에 있는데 현업 필수인가? 
//using namespace Engine;
//using namespace DirectX;

unique_ptr<CSkeleton> CSkeleton::Create()
{
	auto pInstance = unique_ptr<CSkeleton>(new CSkeleton());
	XMStoreFloat4x4(&pInstance->m_matGlobalInverseRoot, XMMatrixIdentity());
	return pInstance;
}

i32_t CSkeleton::AddBone(const string& strName, i32_t iParentIndex,
	const XMFLOAT4X4& matOffset, const XMFLOAT4X4& matRestLocal)
{
	i32_t iIndex = (i32_t)m_vecBones.size();
	BoneInfo bone;
	bone.strName = strName;
	bone.iParentIndex = iParentIndex;
	bone.matOffset = matOffset;
	bone.matRestLocal = matRestLocal;
	m_vecBones.push_back(bone);
	m_mapBoneIndex[strName] = iIndex;

	return iIndex;
}

i32_t CSkeleton::FindBoneIndex(const string& strName) const
{
	auto it = m_mapBoneIndex.find(strName);
	if (it == m_mapBoneIndex.end())
		return -1;
	return it->second;
}

void CSkeleton::ComputeFinalTransforms(const vector<XMFLOAT4X4>& vecLocalTransforms,
	vector<XMFLOAT4X4>& vecOutFinal) const
{
	u32_t iBoneCount = (u32_t)m_vecBones.size();
	vecOutFinal.resize(iBoneCount);
	vector<XMMATRIX> vecGlobal(iBoneCount);

	XMMATRIX matGlobalInvRoot = XMLoadFloat4x4(&m_matGlobalInverseRoot);

	for (u32_t i = 0; i < iBoneCount; ++i)
	{
		XMMATRIX matLocal = XMLoadFloat4x4(&vecLocalTransforms[i]);
		if (m_vecBones[i].iParentIndex >= 0)
			vecGlobal[i] = matLocal * vecGlobal[m_vecBones[i].iParentIndex];
		else
			vecGlobal[i] = matLocal;

		XMMATRIX matOffset = XMLoadFloat4x4(&m_vecBones[i].matOffset);
		// Assimp 표준: Final = Offset × Global × GlobalInverseRoot
		XMStoreFloat4x4(&vecOutFinal[i], matOffset * vecGlobal[i] * matGlobalInvRoot);
	}
}
