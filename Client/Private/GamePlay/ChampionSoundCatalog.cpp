#include "GamePlay/ChampionSoundCatalog.h"

#pragma push_macro("new")
#undef new
#include "Network/Backend/json.hpp"
#pragma pop_macro("new")
#include "WintersPaths.h"

#include <Windows.h>
#include <cmath>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;

namespace
{
	struct ChampionSoundName
	{
		const char* pszName;
		eChampion   eChamp;
	};

	// ChampionSoundMap.json 의 champion 필드 <-> eChampion.
	// 표기는 사운드 폴더명(Resource/Sound/LoL/Champions/<Name>)과 동일.
	constexpr ChampionSoundName kChampionSoundNames[] =
	{
		{ "Irelia",   eChampion::IRELIA },
		{ "Yasuo",    eChampion::YASUO },
		{ "Kalista",  eChampion::KALISTA },
		{ "Sylas",    eChampion::SYLAS },
		{ "Viego",    eChampion::VIEGO },
		{ "Annie",    eChampion::ANNIE },
		{ "Ashe",     eChampion::ASHE },
		{ "Fiora",    eChampion::FIORA },
		{ "Garen",    eChampion::GAREN },
		{ "Riven",    eChampion::RIVEN },
		{ "Zed",      eChampion::ZED },
		{ "Ezreal",   eChampion::EZREAL },
		{ "Yone",     eChampion::YONE },
		{ "Jax",      eChampion::JAX },
		{ "MasterYi", eChampion::MASTERYI },
		{ "Kindred",  eChampion::KINDRED },
		{ "LeeSin",   eChampion::LEESIN },
	};

	eChampion ChampionFromSoundName(const std::string& name)
	{
		for (const ChampionSoundName& entry : kChampionSoundNames)
		{
			if (name == entry.pszName)
				return entry.eChamp;
		}
		return eChampion::NONE;
	}

	struct SlotJsonKey
	{
		const char*        pszKey;
		eChampionSoundSlot eSlot;
	};

	constexpr SlotJsonKey kSlotJsonKeys[] =
	{
		{ "basicAttack", eChampionSoundSlot::BasicAttack },
		{ "skillQ",      eChampionSoundSlot::SkillQ },
		{ "skillW",      eChampionSoundSlot::SkillW },
		{ "skillE",      eChampionSoundSlot::SkillE },
		{ "skillR",      eChampionSoundSlot::SkillR },
		{ "death",       eChampionSoundSlot::Death },
	};

	struct VoiceSlotJsonKey
	{
		const char*        pszKey;
		eChampionVoiceSlot eSlot;
	};

	constexpr VoiceSlotJsonKey kVoiceSlotJsonKeys[] =
	{
		{ "move",        eChampionVoiceSlot::Move },
		{ "basicAttack", eChampionVoiceSlot::BasicAttack },
		{ "skillQ",      eChampionVoiceSlot::SkillQ },
		{ "skillW",      eChampionVoiceSlot::SkillW },
		{ "skillE",      eChampionVoiceSlot::SkillE },
		{ "skillR",      eChampionVoiceSlot::SkillR },
		{ "death",       eChampionVoiceSlot::Death },
	};

	std::wstring Utf8ToWide(const std::string& utf8)
	{
		if (utf8.empty())
			return {};

		const int wideLen = MultiByteToWideChar(
			CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
		if (wideLen <= 0)
			return {};

		std::wstring wide(static_cast<size_t>(wideLen), L'\0');
		MultiByteToWideChar(
			CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), wide.data(), wideLen);
		return wide;
	}
}

CChampionSoundCatalog& CChampionSoundCatalog::Instance()
{
	static CChampionSoundCatalog s_Instance;
	return s_Instance;
}

bool_t CChampionSoundCatalog::LoadFromJson(const wchar_t* pRelativePath)
{
	// 실패해도 로드 시도는 1회로 고정 (Find 의 lazy-load 게이트)
	m_bLoaded = true;
	for (auto& slots : m_Keys)
	{
		for (std::wstring& key : slots)
			key.clear();
	}

	bool_t bSoundLoaded = false;
	wchar_t szResolvedPath[MAX_PATH] = {};
	if (!pRelativePath || !WintersResolveContentPath(pRelativePath, szResolvedPath, MAX_PATH))
	{
		OutputDebugStringW(L"[ChampionSound] map resolve failed: Data/LoL/Sound/ChampionSoundMap.json\n");
	}
	else if (!LoadFromResolvedPath(szResolvedPath))
	{
		OutputDebugStringW((L"[ChampionSound] map load failed: " + std::wstring(szResolvedPath) + L"\n").c_str());
	}
	else
	{
		bSoundLoaded = true;
	}

	const bool_t bVoiceLoaded = LoadVoicesFromJson();
	return bSoundLoaded || bVoiceLoaded;
}

const std::wstring* CChampionSoundCatalog::Find(eChampion champion, eChampionSoundSlot slot)
{
	if (!m_bLoaded)
		LoadFromJson();

	const size_t championIdx = static_cast<size_t>(champion);
	const size_t slotIdx = static_cast<size_t>(slot);
	if (championIdx >= kChampionSlots || slotIdx >= kSlotCount)
		return nullptr;

	const std::wstring& key = m_Keys[championIdx][slotIdx];
	return key.empty() ? nullptr : &key;
}

const std::wstring* CChampionSoundCatalog::SelectVoice(
	eChampion champion,
	eChampionVoiceSlot slot,
	u32_t selector)
{
	if (!m_bLoaded)
		LoadFromJson();
	else if (!m_bVoiceLoaded)
		LoadVoicesFromJson();

	const size_t championIdx = static_cast<size_t>(champion);
	const size_t slotIdx = static_cast<size_t>(slot);
	if (championIdx >= kChampionSlots || slotIdx >= kVoiceSlotCount)
		return nullptr;

	const std::vector<std::wstring>& keys = m_VoiceKeys[championIdx][slotIdx];
	if (keys.empty())
		return nullptr;

	return &keys[static_cast<size_t>(selector) % keys.size()];
}

bool_t CChampionSoundCatalog::LoadFromResolvedPath(const std::wstring& path)
{
	std::ifstream file{ std::filesystem::path(path) };
	if (!file.is_open())
		return false;

	// value() 는 키가 있고 타입이 다르면 throw 하므로 추출까지 try 로 감싼다
	try
	{
		json root{};
		file >> root;

		if (!root.is_object())
			return false;

		m_fVolume = root.value("volume", 0.8f);
		m_fMaxAudibleDistance = root.value("maxAudibleDistance", 24.f);
		if (!std::isfinite(m_fMaxAudibleDistance) || m_fMaxAudibleDistance <= 0.f)
			m_fMaxAudibleDistance = 24.f;

		if (!root.contains("champions") || !root["champions"].is_array())
			return false;

		u32_t loadedCount = 0;
		for (const json& source : root["champions"])
		{
			if (!source.is_object())
				continue;

			const eChampion champ = ChampionFromSoundName(source.value("champion", ""));
			if (champ == eChampion::NONE)
				continue;
			if (!source.contains("sounds") || !source["sounds"].is_object())
				continue;

			const json& sounds = source["sounds"];
			for (const SlotJsonKey& slotKey : kSlotJsonKeys)
			{
				const std::string key = sounds.value(slotKey.pszKey, "");
				if (key.empty())
					continue;

				m_Keys[static_cast<size_t>(champ)][static_cast<size_t>(slotKey.eSlot)] = Utf8ToWide(key);
				++loadedCount;
			}
		}

		return loadedCount > 0;
	}
	catch (const json::exception&)
	{
		return false;
	}
}

bool_t CChampionSoundCatalog::LoadVoicesFromJson(const wchar_t* pRelativePath)
{
	m_bVoiceLoaded = true;
	for (auto& slots : m_VoiceKeys)
	{
		for (std::vector<std::wstring>& keys : slots)
			keys.clear();
	}

	wchar_t szResolvedPath[MAX_PATH] = {};
	if (!pRelativePath || !WintersResolveContentPath(pRelativePath, szResolvedPath, MAX_PATH))
	{
		OutputDebugStringW(L"[ChampionVoice] map resolve failed: Data/LoL/Sound/ChampionVoiceMap.json\n");
		return false;
	}

	if (!LoadVoicesFromResolvedPath(szResolvedPath))
	{
		OutputDebugStringW((L"[ChampionVoice] map load failed: " + std::wstring(szResolvedPath) + L"\n").c_str());
		return false;
	}

	return true;
}

bool_t CChampionSoundCatalog::LoadVoicesFromResolvedPath(const std::wstring& path)
{
	std::ifstream file{ std::filesystem::path(path) };
	if (!file.is_open())
		return false;

	try
	{
		json root{};
		file >> root;
		if (!root.is_object())
			return false;

		m_fVoiceVolume = root.value("voiceVolume", 0.9f);
		const f32_t legacyMoveDelaySec = root.value("moveCooldownSec", 8.f);
		m_fMoveVoiceDelayMinSec = root.value("moveDelayMinSec", legacyMoveDelaySec);
		m_fMoveVoiceDelayMaxSec = root.value("moveDelayMaxSec", 10.f);
		if (m_fMoveVoiceDelayMinSec < 0.f)
			m_fMoveVoiceDelayMinSec = 0.f;
		if (m_fMoveVoiceDelayMaxSec < m_fMoveVoiceDelayMinSec)
			m_fMoveVoiceDelayMaxSec = m_fMoveVoiceDelayMinSec;
		if (!root.contains("champions") || !root["champions"].is_array())
			return false;

		u32_t loadedCount = 0;
		for (const json& source : root["champions"])
		{
			if (!source.is_object())
				continue;

			const eChampion champ = ChampionFromSoundName(source.value("champion", ""));
			if (champ == eChampion::NONE)
				continue;
			if (!source.contains("voices") || !source["voices"].is_object())
				continue;

			const json& voices = source["voices"];
			for (const VoiceSlotJsonKey& slotKey : kVoiceSlotJsonKeys)
			{
				if (!voices.contains(slotKey.pszKey) || !voices[slotKey.pszKey].is_array())
					continue;

				std::vector<std::wstring>& keys =
					m_VoiceKeys[static_cast<size_t>(champ)][static_cast<size_t>(slotKey.eSlot)];
				for (const json& value : voices[slotKey.pszKey])
				{
					if (!value.is_string())
						continue;
					const std::wstring key = Utf8ToWide(value.get<std::string>());
					if (key.empty())
						continue;
					keys.push_back(key);
					++loadedCount;
				}
			}
		}

		return loadedCount > 0;
	}
	catch (const json::exception&)
	{
		return false;
	}
}
