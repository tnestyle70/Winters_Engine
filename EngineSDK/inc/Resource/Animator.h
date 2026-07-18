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

	const vector<XMFLOAT4X4>& GetFinalBoneMatrices() const { EnsurePoseEvaluated(); return m_vecFinalMatrices; }
	const vector<XMFLOAT4X4>& GetGlobalBoneMatrices() const { EnsurePoseEvaluated(); return m_vecGlobalMatrices; }

	// 지연 포즈 평가: Update 는 시간만 전진하고 dirty 를 남긴다. 포즈(Evaluate +
	// ComputeFinalTransforms)는 소비 지점(본 업로드/본 질의)에서 프레임당 1회 계산되므로,
	// 컬링돼 소비자가 없는 인스턴스는 포즈 비용이 0이다. Engine TU 전용 호출.
	void EnsurePoseEvaluated() const;
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

	// [ModelAnimPanel] 툴 전용 세터 — Phase T-2 패턴(인라인, dllexport 불요).
	// SetPlaySpeedRawForTool(0.f) = 일시정지: Update가 시간을 0만큼 진행하되 포즈는
	// 매 프레임 재평가하므로 SetCurrentTimeTicksForTool 스크럽이 즉시 반영된다.
	void SetPlaySpeedRawForTool(f32_t fSpeed) { m_fPlaySpeed = fSpeed; }
	void SetCurrentTimeTicksForTool(f64_t dTicks) { m_dCurrentTime = dTicks; m_bPoseDirty = true; }
private:
	CSkeleton* m_pSkeleton = nullptr;
	CAnimation* m_pCurrentAnim = nullptr;
	f64_t m_dCurrentTime = 0.0f;
	bool_t m_bLoop = true;
	bool_t m_bPlaying = false;
	f32_t m_fPlaySpeed = 1.f;
	// 포즈 저장소는 지연 평가 캐시 — const 질의 경로(TryGetBoneGlobalTransform 등)에서
	// 갱신되므로 mutable.
	mutable bool_t m_bPoseDirty = false;
	mutable vector<XMFLOAT4X4> m_vecLocalTransforms;
	mutable vector<XMFLOAT4X4> m_vecGlobalMatrices;
	mutable vector<XMFLOAT4X4> m_vecFinalMatrices;
	mutable vector<XMMATRIX> m_vecGlobalScratch;
};

NS_END
