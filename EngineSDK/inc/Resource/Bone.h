#pragma once
#include "Engine_Defines.h"

NS_BEGIN(Engine)

struct BoneInfo
{
	string strName;
	i32_t iParentIndex = -1;
	DirectX::XMFLOAT4X4 matOffset;		// Inverse Bind Pose
	DirectX::XMFLOAT4X4 matRestLocal;	// 노드 기본 로컬 트랜스폼 (Rest Pose)
};

NS_END