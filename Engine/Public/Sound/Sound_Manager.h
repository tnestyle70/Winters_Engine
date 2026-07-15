#pragma once
#include "Engine_Defines.h"
#include "SoundChannel.h"

// FMOD 전방 선언 — 공개 헤더에 fmod.hpp 포함 피함
namespace FMOD
{
    class System;
    class Sound;
    class Channel;
}

NS_BEGIN(Engine)

// ─────────────────────────────────────────────────────────────
//  CSound_Manager
//    - FMOD System 초기화/해제
//    - Resource/Sound/ 상대 키를 첫 재생 시 canonical 루트에서 lazy 로드
//    - 고정 채널(eSoundChannel) + 자동 채널(PlayEffect) 분리
//    - Winters 경계 규칙: 내부 매니저 (SDK 배포 금지).
//      Client 는 CGameInstance 포워딩 메서드로만 접근.
// ─────────────────────────────────────────────────────────────
class CSound_Manager
{
private:
    CSound_Manager();
public:
    ~CSound_Manager();

    static unique_ptr<CSound_Manager> Create();

public:
    // 고정 채널 재생 — 같은 채널이 재생 중이면 정지 후 교체
    void PlaySoundOn(const wstring_t& strSoundKey, eSoundChannel eChannel, f32_t fVolume);
    // 자동 채널 재생 — 겹침 허용
    void PlayEffect (const wstring_t& strSoundKey, f32_t fVolume);
    // BGM 채널 (eSoundChannel::BGM) 에 루프 재생
    void PlayBGM    (const wstring_t& strSoundKey, f32_t fVolume);

    void StopChannel(eSoundChannel eChannel);
    void StopAll();
    void SetChannelVolume(eSoundChannel eChannel, f32_t fVolume);
    void SetMasterVolume (f32_t fVolume);

    // 매 프레임 Tick (CGameInstance 가 위임 호출)
    void Tick();

private:
    HRESULT Initialize();

    // 키(Resource/Sound 상대 경로, 예: L"BGM/Title.wav")를 캐시에서 찾거나
    // canonical Resource 루트(WintersResolveContentPath)에서 lazy 로드한다.
    // 실패는 키별 최초 1회만 Debug 로그를 남기고 nullptr를 반환한다.
    FMOD::Sound* FindOrLoadSound(const wstring_t& strSoundKey);

private:
    FMOD::System*                          m_pSystem = nullptr;
    FMOD::Channel*                         m_pChannels[SOUND_CHANNEL_COUNT]{};
    map<wstring_t, FMOD::Sound*>           m_mapSounds;
};

NS_END
