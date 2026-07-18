#include "Resource/Animator.h"
#include "Resource/Skeleton.h"
#include "Resource/Animation.h"

#include <cmath>

namespace
{
	f64_t ClampAnimationTime(f64_t time, f64_t duration)
	{
		if (duration <= 0.0)
			return 0.0;
		if (time < 0.0)
			return 0.0;
		if (time > duration)
			return duration;
		return time;
	}
}

unique_ptr<CAnimator> CAnimator::Create(CSkeleton* pSkeleton)
{
	auto pInstance = unique_ptr<CAnimator>(new CAnimator());

	pInstance->m_pSkeleton = pSkeleton;
	u32_t n = pSkeleton->GetBoneCount();
	pInstance->m_vecLocalTransforms.resize(n);
	pInstance->m_vecGlobalMatrices.resize(n);
	pInstance->m_vecFinalMatrices.resize(n);
	pInstance->m_vecGlobalScratch.resize(n);

	for (u32_t i = 0; i < n; ++i)
	{
		XMStoreFloat4x4(&pInstance->m_vecLocalTransforms[i], XMMatrixIdentity());
		XMStoreFloat4x4(&pInstance->m_vecGlobalMatrices[i], XMMatrixIdentity());
		XMStoreFloat4x4(&pInstance->m_vecFinalMatrices[i], XMMatrixIdentity());
	}

	return pInstance;
}

void CAnimator::Update(f32_t fDeltaTime)
{
	if (!m_bPlaying || !m_pCurrentAnim || !m_pSkeleton)
		return;

	m_dCurrentTime += (f64_t)fDeltaTime * m_pCurrentAnim->GetTicksPerSecond() * (f64_t)m_fPlaySpeed;

	const f64_t dur = m_pCurrentAnim->GetDuration();
	if (dur <= 0.0)
	{
		m_dCurrentTime = 0.0;
		m_bPlaying = false;
		return;
	}

	if (m_dCurrentTime >= dur)
	{
		if (m_bLoop)
			m_dCurrentTime = std::fmod(m_dCurrentTime, dur);
		else
		{
			m_dCurrentTime = dur;
			m_bPlaying = false;
		}
	}
	else if (m_dCurrentTime <= 0.0)
	{
		if (m_bLoop)
		{
			m_dCurrentTime = std::fmod(m_dCurrentTime, dur);
			if (m_dCurrentTime < 0.0)
				m_dCurrentTime += dur;
		}
		else
		{
			m_dCurrentTime = 0.0;
			m_bPlaying = false;
		}
	}
	// 지연 포즈 평가: 시간만 전진하고 dirty 를 남긴다. 실제 Evaluate +
	// ComputeFinalTransforms 는 EnsurePoseEvaluated(소비 지점)에서 프레임당 1회.
	m_bPoseDirty = true;
}

void CAnimator::EnsurePoseEvaluated() const
{
	if (!m_bPoseDirty || !m_pCurrentAnim || !m_pSkeleton)
		return;

	const u32_t n = m_pSkeleton->GetBoneCount();
	m_pCurrentAnim->Evaluate(m_dCurrentTime, m_vecLocalTransforms, n, m_pSkeleton);
	m_pSkeleton->ComputeFinalTransformsWithScratch(
		m_vecLocalTransforms,
		m_vecFinalMatrices,
		m_vecGlobalScratch,
		&m_vecGlobalMatrices);
	m_bPoseDirty = false;
}

void CAnimator::PlayAnimation(CAnimation* pAnim, bool_t bLoop)
{
	PlayAnimation(pAnim, bLoop, 0.0, 1.f);
}

void CAnimator::PlayAnimation(CAnimation* pAnim, bool_t bLoop, f64_t dStartTime, f32_t fPlaySpeed)
{
	m_pCurrentAnim = pAnim;
	m_bLoop = bLoop;
	SetPlaySpeed(fPlaySpeed);
	m_dCurrentTime = 0.0;
	m_bPlaying = (pAnim != nullptr);

	if (!pAnim)
		return;

	m_dCurrentTime = ClampAnimationTime(dStartTime, pAnim->GetDuration());
	m_bPoseDirty = true;
}

void CAnimator::Stop()
{
	m_bPlaying = false;
}

u32_t CAnimator::GetBoneCount() const
{
	return m_pSkeleton ? m_pSkeleton->GetBoneCount() : 0;
}

i32_t CAnimator::FindBoneIndex(const string& strBoneName) const
{
	return m_pSkeleton ? m_pSkeleton->FindBoneIndex(strBoneName) : -1;
}

bool_t CAnimator::TryGetBoneGlobalTransform(const string& strBoneName, XMFLOAT4X4& outMatrix) const
{
	const i32_t iBoneIndex = FindBoneIndex(strBoneName);
	if (iBoneIndex < 0)
		return false;

	EnsurePoseEvaluated();
	if (static_cast<size_t>(iBoneIndex) >= m_vecGlobalMatrices.size())
		return false;

	outMatrix = m_vecGlobalMatrices[static_cast<size_t>(iBoneIndex)];
	return true;
}

// [Phase T-2] GetCurrentFrame / HasFramePassed 는 헤더 인라인화 (DLL export 회피).
