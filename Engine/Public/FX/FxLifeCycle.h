#pragma once

#include "WintersTypes.h"
#include <algorithm>

enum class eFxLifecycleState : u8_t
{
	Active,
	Completing,
	Complete,
};

struct FxLifecycleTickResult
{
	f32_t fFadeAlpha = 1.f;
	f32_t fNormalizedAge = 0.f;
	bool_t bShouldDestroy = false;
};

inline f32_t FxGetCompletionDuration(f32_t fCompletionDuration, f32_t fFadeOut)
{
	if (fCompletionDuration > 0.f)
		return fCompletionDuration;
	return 0.f;
}

inline FxLifecycleTickResult FxAdvanceLifecycle(
	eFxLifecycleState& state, f32_t& fElapsed, f32_t& fCompletionElapsed, f32_t fLifetime,
	f32_t fFadeIn, f32_t fFadeOut, f32_t fCompletionDuration, bool_t bPendingDelete, f32_t fDeltaTime)
{
	FxLifecycleTickResult result{};

	if (state == eFxLifecycleState::Active)
	{
		fElapsed += fDeltaTime;
		if (bPendingDelete || fElapsed >= fLifetime)
			state = eFxLifecycleState::Completing;
	}

	if (state == eFxLifecycleState::Completing)
	{
		fCompletionElapsed += fDeltaTime;
		const f32_t fDuration = FxGetCompletionDuration(fCompletionDuration, fFadeOut);
		if (fDuration <= 0.f || fCompletionElapsed >= fDuration)
			state = eFxLifecycleState::Complete;
	}

	const f32_t fSafeLifetime = (fLifetime > 0.f) ? fLifetime : 1.f;
	result.fNormalizedAge = std::clamp(fElapsed / fSafeLifetime, 0.f, 1.f);

	f32_t fAlpha = 1.f;
	if (fFadeIn > 0.f && fElapsed < fFadeIn)
		fAlpha *= fElapsed / fFadeIn;

	if (state == eFxLifecycleState::Completing)
	{
		const f32_t fDuration = FxGetCompletionDuration(fCompletionDuration, fFadeOut);
		if (fDuration > 0.f)
			fAlpha *= 1.f - std::clamp(fCompletionElapsed / fDuration, 0.f, 1.f);
	}
	else if (fFadeOut > 0.f && fElapsed > (fLifetime - fFadeOut))
	{
		fAlpha *= (fLifetime - fElapsed) / fFadeOut;
	}

	result.fFadeAlpha = std::clamp(fAlpha, 0.f, 1.f);
	result.bShouldDestroy = (state == eFxLifecycleState::Complete);
	return result;
}
