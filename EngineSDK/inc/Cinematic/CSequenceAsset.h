#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"

#include <string>
#include <vector>

enum class eSeqTrackType : u8_t
{
    Camera = 0,
    Anim,
    Fx,
    Audio,
    Event,
    Visibility,
    TimeDilation,
};

enum class eSeqInterp : u8_t
{
    Constant = 0,
    Linear,
    Cubic,
};

struct SeqCameraKey
{
    f64_t dTimeSec = 0.0;
    Vec3 vPos{};
    Vec3 vRotEulerDeg{};
    f32_t fFovDeg = 60.f;
    eSeqInterp eInterp = eSeqInterp::Linear;
    bool_t bCut = false;
};

struct SeqAnimKey
{
    f64_t dTimeSec = 0.0;
    std::string strAnim;
    bool_t bLoop = false;
    bool_t bReverse = false;
    f32_t fSpeed = 1.f;
};

struct SeqFxKey
{
    f64_t dTimeSec = 0.0;
    std::string strWfx;
    std::string strAnchor;
    bool_t bOneShot = true;
};

struct SeqAudioKey
{
    f64_t dTimeSec = 0.0;
    std::string strSound;
    std::string strChannel = "Effect";
    f32_t fVolume = 1.f;
};

struct SeqEventKey
{
    f64_t dTimeSec = 0.0;
    std::string strEvent;
    std::string strPayload;
};

struct SeqVisibilityKey
{
    f64_t dTimeSec = 0.0;
    bool_t bVisible = true;
};

struct SeqTimeDilationKey
{
    f64_t dTimeSec = 0.0;
    f32_t fScale = 1.f;
    eSeqInterp eInterp = eSeqInterp::Linear;
};

struct SeqTrack
{
    eSeqTrackType eType = eSeqTrackType::Camera;
    std::string strName;
    std::string strBinding;

    std::vector<SeqCameraKey> cameraKeys;
    std::vector<SeqAnimKey> animKeys;
    std::vector<SeqFxKey> fxKeys;
    std::vector<SeqAudioKey> audioKeys;
    std::vector<SeqEventKey> eventKeys;
    std::vector<SeqVisibilityKey> visibilityKeys;
    std::vector<SeqTimeDilationKey> timeDilationKeys;
};

class CSequenceAsset
{
public:
    static WINTERS_ENGINE bool_t LoadFromJson(const std::string& strPath, CSequenceAsset& outAsset);

    WINTERS_ENGINE bool_t SaveToJson(const std::string& strPath) const;
    WINTERS_ENGINE bool_t Validate(std::vector<std::string>* pOutErrors = nullptr) const;
    WINTERS_ENGINE std::vector<std::string> GetValidationErrors() const;
    WINTERS_ENGINE void Clear();
    WINTERS_ENGINE void SortKeys();

public:
    std::string strName;
    f64_t dDurationSec = 0.0;
    u32_t iDisplayRate = 60;
    bool_t bLoop = false;
    std::vector<SeqTrack> tracks;

private:
    std::vector<std::string> m_arrLoadValidationErrors;
};
