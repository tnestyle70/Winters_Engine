#include "GameMode/GameModeCatalog.h"

#pragma push_macro("new")
#undef new
#include "Network/Backend/json.hpp"
#pragma pop_macro("new")
#include "WintersPaths.h"

#include <filesystem>
#include <fstream>
#include <utility>

using json = nlohmann::json;

CGameModeCatalog& CGameModeCatalog::Instance()
{
	static CGameModeCatalog s_Instance;
	return s_Instance;
}

bool_t CGameModeCatalog::LoadFromJson(const wchar_t* pRelativePath)
{
	const std::wstring strResolvedPath = ResolveGameModePath(pRelativePath);
	if (strResolvedPath.empty())
	{
		LoadDefaults();
		return false;
	}

	if (!LoadFromResolvedPath(strResolvedPath))
	{
		LoadDefaults();
		return false;
	}

	m_bLoaded = true;
	m_bLoadedFromJson = true;
	return true;
}

void CGameModeCatalog::LoadDefaults()
{
	m_vecModes.clear();
	m_strDefaultModeID = "summoners_rift";

	GameModeDef summonersRift{};
	summonersRift.strModeID = "summoners_rift";
	summonersRift.strDisplayName = "Summoner's Rift";
	summonersRift.strMapID = "sr_stage1";
	summonersRift.strRulesetID = "lol_standard_5v5";
	summonersRift.strQueueName = "Normal";
	summonersRift.uTeamSize = 5;
	summonersRift.bAvailable = true;
	summonersRift.bMatchmakingEnabled = true;
	m_vecModes.push_back(summonersRift);

	GameModeDef practice{};
	practice.strModeID = "practice_tool";
	practice.strDisplayName = "Practice Tool";
	practice.strMapID = "sr_stage1";
	practice.strRulesetID = "lol_practice";
	practice.strQueueName = "Practice";
	practice.uTeamSize = 1;
	practice.bAvailable = true;
	practice.bPracticeMode = true;
	m_vecModes.push_back(practice);

	GameModeDef aram{};
	aram.strModeID = "aram";
	aram.strDisplayName = "ARAM";
	aram.strMapID = "howling_abyss";
	aram.strRulesetID = "lol_aram";
	aram.strQueueName = "ARAM";
	aram.uTeamSize = 5;
	aram.bAvailable = false;
	m_vecModes.push_back(aram);

	m_bLoaded = true;
	m_bLoadedFromJson = false;
}

const GameModeDef* CGameModeCatalog::Find(const std::string& modeID) const
{
	for (const GameModeDef& mode : m_vecModes)
	{
		if (mode.strModeID == modeID)
			return &mode;
	}

	return nullptr;
}

const GameModeDef* CGameModeCatalog::GetDefaultMode() const
{
	if (const GameModeDef* pMode = Find(m_strDefaultModeID))
		return pMode;

	for (const GameModeDef& mode : m_vecModes)
	{
		if (mode.bAvailable)
			return &mode;
	}

	return m_vecModes.empty() ? nullptr : &m_vecModes.front();
}

bool_t CGameModeCatalog::LoadFromResolvedPath(const std::wstring& path)
{
	std::ifstream file{ std::filesystem::path(path) };
	if (!file.is_open())
		return false;

	json root{};
	try
	{
		file >> root;
	}
	catch (const json::exception&)
	{
		return false;
	}

	const json* pModes = nullptr;
	if (root.is_object())
	{
		m_strDefaultModeID = root.value("defaultModeID", "summoners_rift");
		if (root.contains("modes") && root["modes"].is_array())
			pModes = &root["modes"];
	}
	else if (root.is_array())
	{
		pModes = &root;
	}

	if (!pModes)
		return false;

	std::vector<GameModeDef> vecParsedModes;
	for (const json& source : *pModes)
	{
		if (!source.is_object())
			continue;

		GameModeDef mode{};
		mode.strModeID = source.value("id", "");
		mode.strDisplayName = source.value("displayName", mode.strModeID);
		mode.strMapID = source.value("mapID", "");
		mode.strRulesetID = source.value("rulesetID", "");
		mode.strQueueName = source.value("queueName", mode.strDisplayName);
		mode.uTeamSize = source.value("teamSize", 1u);
		mode.bAvailable = source.value("available", false);
		mode.bMatchmakingEnabled = source.value("matchmakingEnabled", false);
		mode.bPracticeMode = source.value("practiceMode", false);

		if (!mode.strModeID.empty())
			vecParsedModes.push_back(mode);
	}

	if (vecParsedModes.empty())
		return false;

	m_vecModes = std::move(vecParsedModes);
	return true;
}

std::wstring CGameModeCatalog::ResolveGameModePath(const wchar_t* pRelativePath) const
{
	wchar_t szResolvedPath[MAX_PATH] = {};
	if (pRelativePath && WintersResolveContentPath(pRelativePath, szResolvedPath, MAX_PATH))
		return szResolvedPath;

	return {};
}
