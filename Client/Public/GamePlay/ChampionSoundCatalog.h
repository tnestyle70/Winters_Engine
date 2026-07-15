#pragma once

#include "Defines.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"

#include <array>
#include <string>

// 챔피언별 액션 사운드 슬롯 — Data/LoL/Sound/ChampionSoundMap.json 의 sounds 키와 1:1
enum class eChampionSoundSlot : u8_t
{
	BasicAttack = 0,
	SkillQ,
	SkillW,
	SkillE,
	SkillR,
	Death,
	MAX
};

// 챔피언 사운드 매핑 카탈로그
//  - Data/LoL/Sound/ChampionSoundMap.json 을 1회 로드 (Find 첫 호출에서 lazy)
//  - 키는 Resource/Sound 상대 경로 → CGameInstance::PlayEffect 에 그대로 전달
class CChampionSoundCatalog final
{
public:
	static CChampionSoundCatalog& Instance();

	bool_t LoadFromJson(const wchar_t* pRelativePath = L"Data/LoL/Sound/ChampionSoundMap.json");

	// 미로드 상태면 첫 호출에서 1회 로드. 매핑이 없으면 nullptr.
	const std::wstring* Find(eChampion champion, eChampionSoundSlot slot);
	f32_t GetVolume() const { return m_fVolume; }
	bool_t IsLoaded() const { return m_bLoaded; }

private:
	CChampionSoundCatalog() = default;
	~CChampionSoundCatalog() = default;
	CChampionSoundCatalog(const CChampionSoundCatalog&) = delete;
	CChampionSoundCatalog& operator=(const CChampionSoundCatalog&) = delete;

	bool_t LoadFromResolvedPath(const std::wstring& path);

	static constexpr size_t kSlotCount = static_cast<size_t>(eChampionSoundSlot::MAX);
	static constexpr size_t kChampionSlots = 256; // eChampion 은 u8 기반

	std::array<std::array<std::wstring, kSlotCount>, kChampionSlots> m_Keys{};
	f32_t m_fVolume = 0.8f;
	bool_t m_bLoaded = false;
};
