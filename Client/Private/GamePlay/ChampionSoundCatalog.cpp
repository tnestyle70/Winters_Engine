#include "GamePlay/ChampionSoundCatalog.h"

#pragma push_macro("new")
#undef new
#include "Network/Backend/json.hpp"
#pragma pop_macro("new")
#include "WintersPaths.h"

#include <Windows.h>
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

	wchar_t szResolvedPath[MAX_PATH] = {};
	if (!pRelativePath || !WintersResolveContentPath(pRelativePath, szResolvedPath, MAX_PATH))
	{
		OutputDebugStringW(L"[ChampionSound] map resolve failed: Data/LoL/Sound/ChampionSoundMap.json\n");
		return false;
	}

	if (!LoadFromResolvedPath(szResolvedPath))
	{
		OutputDebugStringW((L"[ChampionSound] map load failed: " + std::wstring(szResolvedPath) + L"\n").c_str());
		return false;
	}

	return true;
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
