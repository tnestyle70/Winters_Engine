#include "WintersPCH.h"
#include "Sound/Sound_Manager.h"
#include "WintersPaths.h"

#include <fmod.hpp>
#include <fmod_errors.h>
#include <Windows.h>

#include <set>
#include <string>

NS_BEGIN(Engine)

CSound_Manager::CSound_Manager() = default;

CSound_Manager::~CSound_Manager()
{
    for (auto& pair : m_mapSounds)
    {
        if (pair.second) pair.second->release();
    }
    m_mapSounds.clear();

    if (m_pSystem)
    {
        m_pSystem->close();
        m_pSystem->release();
        m_pSystem = nullptr;
    }
}

unique_ptr<CSound_Manager> CSound_Manager::Create()
{
    auto p = unique_ptr<CSound_Manager>(new CSound_Manager());
    if (FAILED(p->Initialize())) return nullptr;
    return p;
}

HRESULT CSound_Manager::Initialize()
{
    FMOD_RESULT r = FMOD::System_Create(&m_pSystem);
    if (r != FMOD_OK || !m_pSystem) return E_FAIL;

    r = m_pSystem->init(1024, FMOD_INIT_NORMAL, nullptr);
    if (r != FMOD_OK) return E_FAIL;

    return S_OK;
}

void CSound_Manager::Tick()
{
    if (m_pSystem) m_pSystem->update();
}

// ─────────────────────────────────────────────────────────────
//  재생
// ─────────────────────────────────────────────────────────────
void CSound_Manager::PlaySoundOn(const wstring_t& strSoundKey, eSoundChannel eChannel, f32_t fVolume)
{
    if (!m_pSystem) return;
    FMOD::Sound* pSound = FindOrLoadSound(strSoundKey);
    if (!pSound) return;

    const u32_t idx = static_cast<u32_t>(eChannel);
    if (idx >= SOUND_CHANNEL_COUNT) return;

    bool bPlaying = false;
    if (m_pChannels[idx]) m_pChannels[idx]->isPlaying(&bPlaying);
    if (bPlaying) m_pChannels[idx]->stop();

    m_pSystem->playSound(pSound, nullptr, false, &m_pChannels[idx]);
    if (m_pChannels[idx]) m_pChannels[idx]->setVolume(fVolume);
}

void CSound_Manager::PlayEffect(const wstring_t& strSoundKey, f32_t fVolume)
{
    if (!m_pSystem) return;
    FMOD::Sound* pSound = FindOrLoadSound(strSoundKey);
    if (!pSound) return;

    FMOD::Channel* pChannel = nullptr;
    m_pSystem->playSound(pSound, nullptr, false, &pChannel);
    if (pChannel) pChannel->setVolume(fVolume);
}

void CSound_Manager::PlayBGM(const wstring_t& strSoundKey, f32_t fVolume)
{
    if (!m_pSystem) return;
    FMOD::Sound* pSound = FindOrLoadSound(strSoundKey);
    if (!pSound) return;

    // 단일 BGM 채널 규칙: 새 BGM 재생 전 기존 채널 정지 (겹침 방지).
    const u32_t idx = static_cast<u32_t>(eSoundChannel::BGM);
    if (m_pChannels[idx]) m_pChannels[idx]->stop();
    m_pSystem->playSound(pSound, nullptr, false, &m_pChannels[idx]);
    if (m_pChannels[idx])
    {
        m_pChannels[idx]->setMode(FMOD_LOOP_NORMAL);
        m_pChannels[idx]->setVolume(fVolume);
    }
}

void CSound_Manager::StopChannel(eSoundChannel eChannel)
{
    const u32_t idx = static_cast<u32_t>(eChannel);
    if (idx < SOUND_CHANNEL_COUNT && m_pChannels[idx])
        m_pChannels[idx]->stop();
}

void CSound_Manager::StopAll()
{
    for (u32_t i = 0; i < SOUND_CHANNEL_COUNT; ++i)
        if (m_pChannels[i]) m_pChannels[i]->stop();
}

void CSound_Manager::SetChannelVolume(eSoundChannel eChannel, f32_t fVolume)
{
    const u32_t idx = static_cast<u32_t>(eChannel);
    if (idx < SOUND_CHANNEL_COUNT && m_pChannels[idx])
        m_pChannels[idx]->setVolume(fVolume);
}

void CSound_Manager::SetMasterVolume(f32_t fVolume)
{
    if (!m_pSystem) return;
    FMOD::ChannelGroup* pMaster = nullptr;
    m_pSystem->getMasterChannelGroup(&pMaster);
    if (pMaster) pMaster->setVolume(fVolume);
}

// ─────────────────────────────────────────────────────────────
//  Resource/Sound/ lazy 로드
//    - 과거 시작 시 전량 스캔은 exe 폴더(<Bin/Debug>) 기준 경로 조합이라
//      canonical 루트(Client/Bin/Resource)를 한 번도 찾지 못했다(전면 무음).
//    - 파일 해석은 Engine 공용 WintersResolveContentPath 재사용
//      ("Sound\" 접두 상대 경로 지원). 실패 진단은 키별 최초 1회 bounded.
// ─────────────────────────────────────────────────────────────
namespace
{
    std::set<wstring_t> s_SoundLoadFailLoggedKeys;
}

FMOD::Sound* CSound_Manager::FindOrLoadSound(const wstring_t& strSoundKey)
{
    auto it = m_mapSounds.find(strSoundKey);
    if (it != m_mapSounds.end())
        return it->second;

    if (!m_pSystem || strSoundKey.empty())
        return nullptr;

    const wstring_t relative = L"Sound/" + strSoundKey;
    wchar_t resolved[MAX_PATH] = {};
    if (!WintersResolveContentPath(relative.c_str(), resolved, MAX_PATH))
    {
        if (s_SoundLoadFailLoggedKeys.insert(strSoundKey).second)
            OutputDebugStringW((L"[Sound] resolve failed: " + relative + L"\n").c_str());
        return nullptr;
    }

    // FMOD 는 UTF-8 경로. wstring → UTF-8 변환.
    char utf8Path[MAX_PATH * 2] = {};
    WideCharToMultiByte(CP_UTF8, 0, resolved, -1,
                        utf8Path, sizeof(utf8Path), nullptr, nullptr);

    FMOD::Sound* pSound = nullptr;
    const FMOD_RESULT r = m_pSystem->createSound(utf8Path, FMOD_DEFAULT, nullptr, &pSound);
    if (r != FMOD_OK || !pSound)
    {
        if (s_SoundLoadFailLoggedKeys.insert(strSoundKey).second)
        {
            OutputDebugStringW((L"[Sound] createSound failed: " + strSoundKey +
                L" fmod=" + std::to_wstring(static_cast<int>(r)) + L"\n").c_str());
        }
        return nullptr;
    }

    m_mapSounds.emplace(strSoundKey, pSound);
    OutputDebugStringW((L"[Sound] loaded: " + strSoundKey + L"\n").c_str());
    return pSound;
}

NS_END
