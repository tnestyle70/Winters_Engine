#pragma once
#include "Engine_Defines.h"

NS_BEGIN(Engine)

class CSkeleton;
class CAnimation;

class CAnimator
{
private:
	CAnimator() = default;
public:
	~CAnimator() = default;

	static unique_ptr<CAnimator> Create(CSkeleton* pSkeleton);

	void Update(f32_t fDeltaTime);
	void PlayAnimation(CAnimation* pAnim, bool_t bLoop = true);
	void PlayAnimation(CAnimation* pAnim, bool_t bLoop, f64_t dStartTime, f32_t fPlaySpeed);
	void Stop();

	const vector<XMFLOAT4X4>& GetFinalBoneMatrices() const { return m_vecFinalMatrices; }
	const vector<XMFLOAT4X4>& GetGlobalBoneMatrices() const { return m_vecGlobalMatrices; }
	i32_t FindBoneIndex(const string& strBoneName) const;
	bool_t TryGetBoneGlobalTransform(const string& strBoneName, XMFLOAT4X4& outMatrix) const;
	u32_t GetBoneCount() const;
	bool_t IsPlaying() const { return m_bPlaying; }

	f64_t GetCurrentTime() const { return m_dCurrentTime; }
	const CAnimation* GetCurrentAnimation() const { return m_pCurrentAnim; }

	// [Phase T-2] 인라인 정의 — WINTERS_ENGINE dllexport 없이 Client TU 에서 직접 호출 가능.
	// Client 가 ModelRenderer::GetAnimator() 로 받은 포인터를 통해 호출 → DLL 경계 없음.
	f32_t GetCurrentFrame() const { return (f32_t)m_dCurrentTime; }
	bool_t HasFramePassed(f32_t frame, f32_t prevFrame) const
	{
		const f32_t cur = (f32_t)m_dCurrentTime;
		return prevFrame < frame && frame <= cur;
	}
	//재생 속도 튜닝 - ImGui 슬라이더로 실시간 변경
	void SetPlaySpeed(f32_t s)
	{
		m_fPlaySpeed = (s > -0.01f && s < 0.01f)
			? ((s < 0.f) ? -0.01f : 0.01f)
			: s;
	}
	f32_t GetPlaySpeed() const { return m_fPlaySpeed; }
private:
	CSkeleton* m_pSkeleton = nullptr;
	CAnimation* m_pCurrentAnim = nullptr;
	f64_t m_dCurrentTime = 0.0f;
	bool_t m_bLoop = true;
	bool_t m_bPlaying = false;
	f32_t m_fPlaySpeed = 1.f;
	vector<XMFLOAT4X4> m_vecLocalTransforms;
	vector<XMFLOAT4X4> m_vecGlobalMatrices;
	vector<XMFLOAT4X4> m_vecFinalMatrices;
	vector<XMMATRIX> m_vecGlobalScratch;
};

NS_END
