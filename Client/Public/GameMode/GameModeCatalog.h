#pragma once

#include "Defines.h"
#include "GameMode/GameModeDef.h"

#include <string>
#include <vector>

class CGameModeCatalog final
{
public:
	static CGameModeCatalog& Instance();

	bool_t LoadFromJson(const wchar_t* pRelativePath = L"Data/GameModes/gameMode.json");
	void LoadDefaults();

	const GameModeDef* Find(const std::string& modeID) const;
	const GameModeDef* GetDefaultMode() const;
	const std::vector<GameModeDef>& GetModes() const { return m_vecModes; }

	bool_t IsLoaded() const { return m_bLoaded; }
	bool_t WasLoadedFromJson() const { return m_bLoadedFromJson; }

private:
	CGameModeCatalog() = default;
	~CGameModeCatalog() = default;
	CGameModeCatalog(const CGameModeCatalog&) = delete;
	CGameModeCatalog& operator=(const CGameModeCatalog&) = delete;

	bool_t LoadFromResolvedPath(const std::wstring& path);
	std::wstring ResolveGameModePath(const wchar_t* pRelativePath) const;

	std::vector<GameModeDef> m_vecModes{};
	std::string m_strDefaultModeID{ "summoners_rift" };
	bool_t m_bLoaded = false;
	bool_t m_bLoadedFromJson = false;
};
