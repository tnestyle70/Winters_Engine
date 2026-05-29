#include "Resource/Animation.h"
#include "Resource/Skeleton.h"

//이거 진짜 해당 클래스가 어디에 속하는지 알기 쉽게 하기 위해서 적어둔 거 아니지???
using namespace Engine;
using namespace DirectX;

unique_ptr<CAnimation> CAnimation::Create(const string& strName, f64_t dDuration, 
	f64_t dTicksPerSecond)
{
	auto pInstance = unique_ptr<CAnimation>(new CAnimation());
	pInstance->m_strName = strName;
	pInstance->m_dDuration = dDuration;
	pInstance->m_dTicksPerSecond = (dTicksPerSecond > 0.0) ? dTicksPerSecond : 25.0;

	return pInstance;
}

void CAnimation::AddChannel(const BoneChannel& channel)
{
	m_vecChannels.push_back(channel);
}

void CAnimation::ResolveBoneIndices(const CSkeleton * pSkeleton)
{
	for (auto& ch : m_vecChannels)
		ch.iBoneIndex = pSkeleton->FindBoneIndex(ch.strBoneName);
}

void CAnimation::Evaluate(f64_t dTime, vector<XMFLOAT4X4>& vecOut,
	u32_t iBoneCount, const CSkeleton* pSkeleton) const
{
	vecOut.resize(iBoneCount);
	// 채널 없는 본은 Rest Pose(노드 기본 트랜스폼)로 초기화 — Identity가 아님!
	for (u32_t i = 0; i < iBoneCount; ++i)
		vecOut[i] = pSkeleton->GetBone(i).matRestLocal;

	for (const auto& ch : m_vecChannels)
	{
		if (ch.iBoneIndex < 0 || ch.iBoneIndex >= (i32_t)iBoneCount)
			continue;
		XMVECTOR pos = InterpolatePosition(ch, dTime);
		XMVECTOR rot = InterpolateRotation(ch, dTime);
		XMVECTOR scale = InterpolateScale(ch, dTime);
		XMStoreFloat4x4(&vecOut[ch.iBoneIndex],
			XMMatrixScalingFromVector(scale) * XMMatrixRotationQuaternion(rot) *
			XMMatrixTranslationFromVector(pos));
	}
}

XMVECTOR CAnimation::InterpolatePosition(const BoneChannel & ch, f64_t t) const
{
	auto& k = ch.vecPositionKeys;
	if (k.empty())
		return XMVectorZero();
	if (k.size() == 1)
		return XMLoadFloat3(&k[0].vValue);

	u32_t i = 0;
	
	for (u32_t j = 0; j < (u32_t)k.size() - 1; ++j)
	{
		if (t < k[j + 1].dTime)
		{
			i = j;
			break;
		}
	}

	f32_t f = (f32_t)((t - k[i].dTime) / (k[i + 1].dTime - k[i].dTime));
	f = max(0.f, min(1.f, f));
	return XMVectorLerp(XMLoadFloat3(&k[i].vValue), XMLoadFloat3(&k[i + 1].vValue), f);
}

XMVECTOR CAnimation::InterpolateRotation(const BoneChannel& ch, f64_t t) const
{
	auto& k = ch.vecRotationKeys;
	if (k.empty())
		return XMQuaternionIdentity();
	if (k.size() == 1)
		return XMLoadFloat4(&k[0].vValue);

	u32_t i = 0;
	for (u32_t j = 0; j < (u32_t)k.size() - 1; ++j)
	{
		if (t < k[j + 1].dTime)
		{
			i = j;
			break;
		}
	}

	f32_t f = (f32_t)((t - k[i].dTime) / (k[i + 1].dTime - k[i].dTime));
	f = max(0.f, min(1.f, f));

	return XMQuaternionSlerp(XMLoadFloat4(&k[i].vValue), XMLoadFloat4(&k[i + 1].vValue), f);
}

XMVECTOR CAnimation::InterpolateScale(const BoneChannel& ch, f64_t t) const
{
	auto& k = ch.vecScaleKeys;
	if (k.empty())
		return XMVectorSet(1, 1, 1, 0);
	if (k.size() == 1)
		return XMLoadFloat3(&k[0].vValue);

	u32_t i = 0; 
	for (u32_t j = 0; j < (u32_t)k.size() - 1; ++j)
	{
		if (t < k[j + 1].dTime)
		{
			i = j;
			break;
		}
	}

	f32_t f = (f32_t)((t - k[i].dTime) / (k[i + 1].dTime - k[i].dTime));
	f = max(0.f, min(1.f, f));
	return XMVectorLerp(XMLoadFloat3(&k[i].vValue), XMLoadFloat3(&k[i + 1].vValue), f);
}
