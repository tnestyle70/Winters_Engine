#include "WintersPCH.h"
#include "Sound/Sound_Manager.h"
#include "WintersPaths.h"

#include <fmod.hpp>
#include <fmod_errors.h>
#include <Windows.h>

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

    LoadSoundFolder();
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
    auto it = m_mapSounds.find(strSoundKey);
    if (it == m_mapSounds.end()) return;

    const u32_t idx = static_cast<u32_t>(eChannel);
    if (idx >= SOUND_CHANNEL_COUNT) return;

    bool bPlaying = false;
    if (m_pChannels[idx]) m_pChannels[idx]->isPlaying(&bPlaying);
    if (bPlaying) m_pChannels[idx]->stop();

    m_pSystem->playSound(it->second, nullptr, false, &m_pChannels[idx]);
    if (m_pChannels[idx]) m_pChannels[idx]->setVolume(fVolume);
}

void CSound_Manager::PlayEffect(const wstring_t& strSoundKey, f32_t fVolume)
{
    if (!m_pSystem) return;
    auto it = m_mapSounds.find(strSoundKey);
    if (it == m_mapSounds.end()) return;

    FMOD::Channel* pChannel = nullptr;
    m_pSystem->playSound(it->second, nullptr, false, &pChannel);
    if (pChannel) pChannel->setVolume(fVolume);
}

void CSound_Manager::PlayBGM(const wstring_t& strSoundKey, f32_t fVolume)
{
    if (!m_pSystem) return;
    auto it = m_mapSounds.find(strSoundKey);
    if (it == m_mapSounds.end()) return;

    const u32_t idx = static_cast<u32_t>(eSoundChannel::BGM);
    m_pSystem->playSound(it->second, nullptr, false, &m_pChannels[idx]);
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
//  Resource/Sound/ 재귀 로드
// ─────────────────────────────────────────────────────────────
void CSound_Manager::LoadSoundFolder()
{
    // exe 디렉터리 기준 Resource/Sound/ 탐색.
    // WintersResolveContentPath 는 파일만 해결하므로 여기선 직접 구성.
    wchar_t exePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) return;

    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (!lastSlash) return;
    *(lastSlash + 1) = L'\0';

    wstring_t base = wstring_t(exePath) + L"Resource\\Sound\\";
    OutputDebugStringW((L"[Sound] scanning: " + base + L"\n").c_str());
    LoadSoundFolderRecursive(base, L"");
}

void CSound_Manager::LoadSoundFolderRecursive(const wstring_t& strFolderPath,
                                               const wstring_t& strRelativePath)
{
    wstring_t search = strFolderPath + strRelativePath + L"*";

    WIN32_FIND_DATAW fd{};
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do
    {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L".."))
            continue;

        wstring_t nextRel = strRelativePath + fd.cFileName;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            LoadSoundFolderRecursive(strFolderPath, nextRel + L"/");
        }
        else
        {
            wstring_t filePath = strFolderPath + nextRel;

            // FMOD 는 UTF-8 경로. wstring → UTF-8 변환.
            char utf8Path[MAX_PATH * 2] = {};
            WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1,
                                utf8Path, sizeof(utf8Path), nullptr, nullptr);

            FMOD::Sound* pSound = nullptr;
            FMOD_RESULT r = m_pSystem->createSound(utf8Path, FMOD_DEFAULT, nullptr, &pSound);
            if (r == FMOD_OK && pSound)
            {
                m_mapSounds.emplace(nextRel, pSound);
                OutputDebugStringW((L"[Sound] loaded: " + nextRel + L"\n").c_str());
            }
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

NS_END
