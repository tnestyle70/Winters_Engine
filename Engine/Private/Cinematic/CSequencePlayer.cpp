#include "Cinematic/CSequencePlayer.h"

#include "Renderer/CCamera.h"
#include "Renderer/ModelRenderer.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr f32_t kPi = 3.14159265358979323846f;
    constexpr f64_t kSeqKeyEpsilon = 0.000000001;

    f32_t DegToRad(f32_t fDegrees)
    {
        return fDegrees * (kPi / 180.f);
    }

    Vec3 ForwardFromEulerDeg(const Vec3& vEulerDeg)
    {
        const f32_t fPitch = DegToRad(vEulerDeg.x);
        const f32_t fYaw = DegToRad(vEulerDeg.y);
        const f32_t fCosPitch = std::cos(fPitch);

        Vec3 vForward{
            fCosPitch * std::sin(fYaw),
            std::sin(fPitch),
            fCosPitch * std::cos(fYaw)
        };

        if (vForward.Length() <= 0.0001f)
            return Vec3{ 0.f, 0.f, 1.f };
        return vForward.Normalized();
    }

    template <typename TKey>
    const TKey* FindLastStepKey(const std::vector<TKey>& keys, f64_t dTimeSec)
    {
        const TKey* pResult = nullptr;
        for (const TKey& key : keys)
        {
            if (key.dTimeSec <= dTimeSec)
                pResult = &key;
            else
                break;
        }
        return pResult;
    }
}

void CSequencePlayer::Play(
    const CSequenceAsset* pAsset,
    ISeqBindingResolver* pResolver,
    ISeqEventSink* pEventSink)
{
    m_pAsset = pAsset;
    m_pResolver = pResolver;
    m_pEventSink = pEventSink;
    m_dTimeSec = 0.0;
    m_dPrevTimeSec = 0.0;
    m_bPlaying = (m_pAsset != nullptr);
    m_bFireInitialDiscreteKeys = (m_pAsset != nullptr);
    m_iDiscreteCycle = 0;
    m_firedDiscreteKeys.clear();

    Evaluate(false);
}

void CSequencePlayer::Stop()
{
    m_bPlaying = false;
    m_bFireInitialDiscreteKeys = false;
}

void CSequencePlayer::Tick(f32_t fDeltaSec)
{
    if (!m_bPlaying || !m_pAsset)
        return;

    if (fDeltaSec < 0.f)
        fDeltaSec = 0.f;

    const f64_t dDuration = m_pAsset->dDurationSec;
    f64_t dRemainingSec = static_cast<f64_t>(fDeltaSec);

    auto evaluateRange = [this](f64_t dPrevSec, f64_t dCurSec)
    {
        m_dPrevTimeSec = dPrevSec;
        m_dTimeSec = dCurSec;
        Evaluate(true);
    };

    if (dDuration <= 0.0)
    {
        const f64_t dPrev = m_bFireInitialDiscreteKeys ? -kSeqKeyEpsilon : m_dTimeSec;
        m_bFireInitialDiscreteKeys = false;
        evaluateRange(dPrev, m_dTimeSec + dRemainingSec);
        return;
    }

    f64_t dPrevForNextRange = m_bFireInitialDiscreteKeys ? -kSeqKeyEpsilon : m_dTimeSec;
    m_bFireInitialDiscreteKeys = false;

    if (m_pAsset->bLoop)
    {
        for (;;)
        {
            const f64_t dToEndSec = dDuration - m_dTimeSec;
            if (dRemainingSec > dToEndSec)
            {
                evaluateRange(dPrevForNextRange, dDuration);
                dRemainingSec -= std::max(0.0, dToEndSec);

                BeginNewDiscreteCycle();
                evaluateRange(-kSeqKeyEpsilon, 0.0);

                if (dRemainingSec <= kSeqKeyEpsilon)
                    return;

                dPrevForNextRange = 0.0;
                continue;
            }

            evaluateRange(dPrevForNextRange, m_dTimeSec + dRemainingSec);
            return;
        }
    }

    const f64_t dNext = m_dTimeSec + dRemainingSec;
    if (dNext >= dDuration)
    {
        evaluateRange(dPrevForNextRange, dDuration);
        m_bPlaying = false;
        return;
    }

    evaluateRange(dPrevForNextRange, dNext);
}

void CSequencePlayer::Seek(f64_t dTimeSec)
{
    if (!m_pAsset)
        return;

    m_dTimeSec = ClampTime(*m_pAsset, dTimeSec);
    m_dPrevTimeSec = m_dTimeSec;
    m_bFireInitialDiscreteKeys = false;
    BeginNewDiscreteCycle();
    Evaluate(false);
}

void CSequencePlayer::SetCameraProjectionDefaults(f32_t fAspect, f32_t fNearZ, f32_t fFarZ)
{
    if (fAspect > 0.f)
        m_fDefaultAspect = fAspect;
    if (fNearZ > 0.f)
        m_fDefaultNearZ = fNearZ;
    if (fFarZ > m_fDefaultNearZ)
        m_fDefaultFarZ = fFarZ;
}

void CSequencePlayer::Evaluate(bool_t bFireDiscrete)
{
    if (!m_pAsset)
        return;

    for (size_t iTrack = 0; iTrack < m_pAsset->tracks.size(); ++iTrack)
    {
        const SeqTrack& track = m_pAsset->tracks[iTrack];
        const u32_t iTrackIndex = static_cast<u32_t>(iTrack);
        switch (track.eType)
        {
        case eSeqTrackType::Camera:
            EvaluateCamera(track);
            break;
        case eSeqTrackType::Anim:
            if (bFireDiscrete)
                EvaluateAnim(track, iTrackIndex);
            break;
        case eSeqTrackType::Fx:
            if (bFireDiscrete)
                EvaluateFx(track, iTrackIndex);
            break;
        case eSeqTrackType::Audio:
            if (bFireDiscrete)
                EvaluateAudio(track, iTrackIndex);
            break;
        case eSeqTrackType::Event:
            if (bFireDiscrete)
                EvaluateEvent(track, iTrackIndex);
            break;
        case eSeqTrackType::Visibility:
            EvaluateVisibility(track);
            break;
        case eSeqTrackType::TimeDilation:
            EvaluateTimeDilation(track);
            break;
        default:
            break;
        }
    }
}

void CSequencePlayer::EvaluateCamera(const SeqTrack& track) const
{
    if (!m_pResolver || track.cameraKeys.empty())
        return;

    CCamera* pCamera = m_pResolver->ResolveCamera(track.strBinding);
    if (!pCamera)
        return;

    const std::vector<SeqCameraKey>& keys = track.cameraKeys;
    SeqCameraKey sample = keys.front();

    if (keys.size() == 1 || m_dTimeSec <= keys.front().dTimeSec)
    {
        sample = keys.front();
    }
    else if (m_dTimeSec >= keys.back().dTimeSec)
    {
        sample = keys.back();
    }
    else
    {
        for (size_t i = 0; i + 1 < keys.size(); ++i)
        {
            const SeqCameraKey& from = keys[i];
            const SeqCameraKey& to = keys[i + 1];
            if (m_dTimeSec > to.dTimeSec)
                continue;

            if (to.bCut && m_dTimeSec < to.dTimeSec)
            {
                sample = from;
                break;
            }

            sample.dTimeSec = m_dTimeSec;
            sample.vPos = SampleVec3(m_dTimeSec, from.dTimeSec, to.dTimeSec,
                from.vPos, to.vPos, from.eInterp);
            sample.vRotEulerDeg = SampleVec3(m_dTimeSec, from.dTimeSec, to.dTimeSec,
                from.vRotEulerDeg, to.vRotEulerDeg, from.eInterp);
            sample.fFovDeg = SampleScalar(m_dTimeSec, from.dTimeSec, to.dTimeSec,
                from.fFovDeg, to.fFovDeg, from.eInterp);
            sample.eInterp = from.eInterp;
            sample.bCut = false;
            break;
        }
    }

    f32_t fAspect = m_fDefaultAspect;
    f32_t fNearZ = m_fDefaultNearZ;
    f32_t fFarZ = m_fDefaultFarZ;
    m_pResolver->ResolveCameraProjection(track.strBinding, fAspect, fNearZ, fFarZ);

    const f32_t fFovDeg = sample.fFovDeg > 0.f ? sample.fFovDeg : 60.f;
    const f32_t fFovRad = DegToRad(fFovDeg);
    const Vec3 vForward = ForwardFromEulerDeg(sample.vRotEulerDeg);
    pCamera->Ready(sample.vPos, sample.vPos + vForward, Vec3{ 0.f, 1.f, 0.f },
        fFovRad, fAspect, fNearZ, fFarZ);
}

void CSequencePlayer::EvaluateAnim(const SeqTrack& track, u32_t iTrackIndex)
{
    if (!m_pResolver)
        return;

    ModelRenderer* pModel = m_pResolver->ResolveModel(track.strBinding);
    if (!pModel)
        return;

    for (size_t i = 0; i < track.animKeys.size(); ++i)
    {
        const SeqAnimKey& key = track.animKeys[i];
        if (ShouldFireDiscreteKey(eSeqTrackType::Anim, iTrackIndex, static_cast<u32_t>(i), key.dTimeSec) &&
            !key.strAnim.empty())
        {
            pModel->PlayAnimationByNameAdvanced(
                key.strAnim,
                key.bLoop,
                key.bReverse,
                key.fSpeed);
        }
    }
}

void CSequencePlayer::EvaluateFx(const SeqTrack& track, u32_t iTrackIndex)
{
    if (!m_pResolver)
        return;

    for (size_t i = 0; i < track.fxKeys.size(); ++i)
    {
        const SeqFxKey& key = track.fxKeys[i];
        if (ShouldFireDiscreteKey(eSeqTrackType::Fx, iTrackIndex, static_cast<u32_t>(i), key.dTimeSec) &&
            !key.strWfx.empty())
        {
            m_pResolver->TriggerFx(track.strBinding, key);
        }
    }
}

void CSequencePlayer::EvaluateAudio(const SeqTrack& track, u32_t iTrackIndex)
{
    if (!m_pResolver)
        return;

    for (size_t i = 0; i < track.audioKeys.size(); ++i)
    {
        const SeqAudioKey& key = track.audioKeys[i];
        if (ShouldFireDiscreteKey(eSeqTrackType::Audio, iTrackIndex, static_cast<u32_t>(i), key.dTimeSec) &&
            !key.strSound.empty())
        {
            m_pResolver->TriggerAudio(track.strBinding, key);
        }
    }
}

void CSequencePlayer::EvaluateEvent(const SeqTrack& track, u32_t iTrackIndex)
{
    if (!m_pEventSink)
        return;

    for (size_t i = 0; i < track.eventKeys.size(); ++i)
    {
        const SeqEventKey& key = track.eventKeys[i];
        if (ShouldFireDiscreteKey(eSeqTrackType::Event, iTrackIndex, static_cast<u32_t>(i), key.dTimeSec) &&
            !key.strEvent.empty())
        {
            m_pEventSink->PushCandidate(key.strEvent, key.strPayload);
        }
    }
}

void CSequencePlayer::EvaluateVisibility(const SeqTrack& track) const
{
    if (!m_pResolver || track.visibilityKeys.empty())
        return;

    const SeqVisibilityKey* pKey = FindLastStepKey(track.visibilityKeys, m_dTimeSec);
    if (!pKey)
        pKey = &track.visibilityKeys.front();

    m_pResolver->SetVisibility(track.strBinding, pKey->bVisible);
}

void CSequencePlayer::EvaluateTimeDilation(const SeqTrack& track) const
{
    if (!m_pResolver || track.timeDilationKeys.empty())
        return;

    const std::vector<SeqTimeDilationKey>& keys = track.timeDilationKeys;
    f32_t fScale = keys.front().fScale;

    if (keys.size() == 1 || m_dTimeSec <= keys.front().dTimeSec)
    {
        fScale = keys.front().fScale;
    }
    else if (m_dTimeSec >= keys.back().dTimeSec)
    {
        fScale = keys.back().fScale;
    }
    else
    {
        for (size_t i = 0; i + 1 < keys.size(); ++i)
        {
            const SeqTimeDilationKey& from = keys[i];
            const SeqTimeDilationKey& to = keys[i + 1];
            if (m_dTimeSec <= to.dTimeSec)
            {
                fScale = SampleScalar(m_dTimeSec, from.dTimeSec, to.dTimeSec,
                    from.fScale, to.fScale, from.eInterp);
                break;
            }
        }
    }

    m_pResolver->SetTimeDilation(fScale);
}

void CSequencePlayer::BeginNewDiscreteCycle()
{
    ++m_iDiscreteCycle;
    m_firedDiscreteKeys.clear();
}

bool_t CSequencePlayer::ShouldFireDiscreteKey(
    eSeqTrackType eType,
    u32_t iTrackIndex,
    u32_t iKeyIndex,
    f64_t dKeySec)
{
    if (!DidCrossKey(m_dPrevTimeSec, m_dTimeSec, dKeySec))
        return false;

    for (const FiredDiscreteKey& fired : m_firedDiscreteKeys)
    {
        if (fired.iCycle == m_iDiscreteCycle &&
            fired.eType == eType &&
            fired.iTrackIndex == iTrackIndex &&
            fired.iKeyIndex == iKeyIndex)
        {
            return false;
        }
    }

    FiredDiscreteKey fired;
    fired.eType = eType;
    fired.iTrackIndex = iTrackIndex;
    fired.iKeyIndex = iKeyIndex;
    fired.iCycle = m_iDiscreteCycle;
    m_firedDiscreteKeys.push_back(fired);
    return true;
}

f64_t CSequencePlayer::ClampTime(const CSequenceAsset& asset, f64_t dTimeSec)
{
    if (dTimeSec < 0.0)
        return 0.0;
    if (asset.dDurationSec > 0.0 && dTimeSec > asset.dDurationSec)
        return asset.dDurationSec;
    return dTimeSec;
}

f32_t CSequencePlayer::SampleScalar(
    f64_t dTimeSec,
    f64_t dStartSec,
    f64_t dEndSec,
    f32_t fStart,
    f32_t fEnd,
    eSeqInterp eInterp)
{
    if (dEndSec <= dStartSec || eInterp == eSeqInterp::Constant)
        return fStart;

    f64_t dAlpha = (dTimeSec - dStartSec) / (dEndSec - dStartSec);
    dAlpha = std::clamp(dAlpha, 0.0, 1.0);

    if (eInterp == eSeqInterp::Cubic)
        dAlpha = dAlpha * dAlpha * (3.0 - 2.0 * dAlpha);

    return static_cast<f32_t>(fStart + (fEnd - fStart) * dAlpha);
}

Vec3 CSequencePlayer::SampleVec3(
    f64_t dTimeSec,
    f64_t dStartSec,
    f64_t dEndSec,
    const Vec3& vStart,
    const Vec3& vEnd,
    eSeqInterp eInterp)
{
    return Vec3{
        SampleScalar(dTimeSec, dStartSec, dEndSec, vStart.x, vEnd.x, eInterp),
        SampleScalar(dTimeSec, dStartSec, dEndSec, vStart.y, vEnd.y, eInterp),
        SampleScalar(dTimeSec, dStartSec, dEndSec, vStart.z, vEnd.z, eInterp)
    };
}

bool_t CSequencePlayer::DidCrossKey(f64_t dPrevSec, f64_t dTimeSec, f64_t dKeySec)
{
    return dPrevSec < dKeySec && dKeySec <= dTimeSec;
}
