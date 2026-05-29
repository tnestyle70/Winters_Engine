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
	pInstance->m_vecFinalMatrices.resize(n);

	for (u32_t i = 0; i < n; ++i)
	{
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
	u32_t n = m_pSkeleton->GetBoneCount();
	m_pCurrentAnim->Evaluate(m_dCurrentTime, m_vecLocalTransforms, n, m_pSkeleton);
	m_pSkeleton->ComputeFinalTransforms(m_vecLocalTransforms, m_vecFinalMatrices);
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
}

void CAnimator::Stop()
{
	m_bPlaying = false;
}

u32_t CAnimator::GetBoneCount() const
{
	return m_pSkeleton ? m_pSkeleton->GetBoneCount() : 0;
}

// [Phase T-2] GetCurrentFrame / HasFramePassed 는 헤더 인라인화 (DLL export 회피).
