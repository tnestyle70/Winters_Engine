#pragma once
#include "Engine_Defines.h"

NS_BEGIN(Engine)

struct VectorKey
{
	f64_t dTime; //fDeltaTime? 
	float3_t vValue; //vValue??
};

struct QuatKey
{
	f64_t dTime;
	float4_t vValue; //what is the meaning of vValue?
};

struct BoneChannel
{
	string strBoneName;
	i32_t iBoneIndex = -1;
	vector<VectorKey> vecPositionKeys;
	vector<QuatKey> vecRotationKeys;
	vector<VectorKey> vecScaleKeys;
};

class CSkeleton;

class CAnimation
{
private:
	CAnimation() = default;
public:
	~CAnimation() = default;
	static unique_ptr<CAnimation> Create(const string& strName, f64_t dDuration, f64_t dTicksPerSecond);

	void AddChannel(const BoneChannel& channel);
	void ResolveBoneIndices(const CSkeleton* pSkeleton);
	void Evaluate(f64_t dTime, vector<XMFLOAT4X4>& vecOut,
		u32_t iBoneCount, const CSkeleton* pSkeleton) const;

	const string& GetName() const { return m_strName; }
	f64_t			GetDuration() const { return m_dDuration; }
	f64_t			GetTicksPerSecond() const { return m_dTicksPerSecond; }

private:
	XMVECTOR InterpolatePosition(const BoneChannel& ch, f64_t t) const;
	XMVECTOR InterpolateRotation(const BoneChannel& ch, f64_t t) const;
	XMVECTOR InterpolateScale(const BoneChannel& ch, f64_t t) const;

	string m_strName;
	f64_t m_dDuration = 0.0;
	f64_t m_dTicksPerSecond = 25.0;
	vector<BoneChannel> m_vecChannels;
};

NS_END