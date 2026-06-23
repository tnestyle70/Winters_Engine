#pragma once

#include "Cinematic/CSequenceAsset.h"
#include "Cinematic/ISeqBindingResolver.h"
#include "Cinematic/ISeqEventSink.h"

class WINTERS_ENGINE CSequencePlayer
{
public:
    void Play(const CSequenceAsset* pAsset,
        ISeqBindingResolver* pResolver,
        ISeqEventSink* pEventSink = nullptr);
    void Stop();
    void Tick(f32_t fDeltaSec);
    void Seek(f64_t dTimeSec);

    bool_t IsPlaying() const { return m_bPlaying; }
    f64_t GetTimeSec() const { return m_dTimeSec; }

    void SetCameraProjectionDefaults(f32_t fAspect, f32_t fNearZ, f32_t fFarZ);

private:
    void Evaluate(bool_t bFireDiscrete);
    void EvaluateCamera(const SeqTrack& track) const;
    void EvaluateAnim(const SeqTrack& track, u32_t iTrackIndex);
    void EvaluateFx(const SeqTrack& track, u32_t iTrackIndex);
    void EvaluateAudio(const SeqTrack& track, u32_t iTrackIndex);
    void EvaluateEvent(const SeqTrack& track, u32_t iTrackIndex);
    void EvaluateVisibility(const SeqTrack& track) const;
    void EvaluateTimeDilation(const SeqTrack& track) const;

    void BeginNewDiscreteCycle();
    bool_t ShouldFireDiscreteKey(eSeqTrackType eType, u32_t iTrackIndex, u32_t iKeyIndex, f64_t dKeySec);

    static f64_t ClampTime(const CSequenceAsset& asset, f64_t dTimeSec);
    static f32_t SampleScalar(f64_t dTimeSec,
        f64_t dStartSec,
        f64_t dEndSec,
        f32_t fStart,
        f32_t fEnd,
        eSeqInterp eInterp);
    static Vec3 SampleVec3(f64_t dTimeSec,
        f64_t dStartSec,
        f64_t dEndSec,
        const Vec3& vStart,
        const Vec3& vEnd,
        eSeqInterp eInterp);
    static bool_t DidCrossKey(f64_t dPrevSec, f64_t dTimeSec, f64_t dKeySec);

private:
    struct FiredDiscreteKey
    {
        eSeqTrackType eType = eSeqTrackType::Camera;
        u32_t iTrackIndex = 0;
        u32_t iKeyIndex = 0;
        u32_t iCycle = 0;
    };

    const CSequenceAsset* m_pAsset = nullptr;
    ISeqBindingResolver* m_pResolver = nullptr;
    ISeqEventSink* m_pEventSink = nullptr;

    f64_t m_dTimeSec = 0.0;
    f64_t m_dPrevTimeSec = 0.0;
    bool_t m_bPlaying = false;
    bool_t m_bFireInitialDiscreteKeys = false;
    u32_t m_iDiscreteCycle = 0;
    std::vector<FiredDiscreteKey> m_firedDiscreteKeys;

    f32_t m_fDefaultAspect = 16.f / 9.f;
    f32_t m_fDefaultNearZ = 0.1f;
    f32_t m_fDefaultFarZ = 1000.f;
};
