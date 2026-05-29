#ifndef Engine_Struct_h__
#define Engine_Struct_h__

#include "Engine_Typedef.h"

namespace Engine
{
	// 추후 정점 구조체, cbuffer 구조체, 메시 데이터 등 추가 예정
	struct VTXMESH
	{
		float3_t vPosition; //position
		float3_t vNormal; //normal 
		float2_t vTexCoord; // TEXCOORD0 (UV는 2D)
		float3_t vTangent; //tangent?? 어떤 걸 의미하는 걸까? 
	};
	//스키닝 매시 정점(76 bytes)
	struct VTXANIM
	{
		float3_t vPosition;
		float3_t vNormal;
		float2_t vTexCoord;
		float3_t vTangent;
		u32_t iBoneIndices[4]; //blendindices
		f32_t fBoneWeights[4]; //blendweight
	};
	//본 행렬 상수 버퍼(16kb, dx11 64kb 제한 이내 )
	struct CBBoneMatrices
	{
		DirectX::XMFLOAT4X4 bones[256];
	};
}

#endif // Engine_Struct_h__
