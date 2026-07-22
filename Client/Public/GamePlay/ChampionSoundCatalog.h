#pragma once

#include "Defines.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"

#include <array>
#include <string>
#include <vector>

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

enum class eChampionVoiceSlot : u8_t
{
	Move = 0,
	BasicAttack,
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
	const std::wstring* SelectVoice(eChampion champion, eChampionVoiceSlot slot, u32_t selector);
	f32_t GetVolume() const { return m_fVolume; }
	f32_t GetVoiceVolume() const { return m_fVoiceVolume; }
	f32_t GetMaxAudibleDistance() const { return m_fMaxAudibleDistance; }
	f32_t GetMoveVoiceDelayMinSec() const { return m_fMoveVoiceDelayMinSec; }
	f32_t GetMoveVoiceDelayMaxSec() const { return m_fMoveVoiceDelayMaxSec; }
	bool_t IsLoaded() const { return m_bLoaded; }

private:
	CChampionSoundCatalog() = default;
	~CChampionSoundCatalog() = default;
	CChampionSoundCatalog(const CChampionSoundCatalog&) = delete;
	CChampionSoundCatalog& operator=(const CChampionSoundCatalog&) = delete;

	bool_t LoadFromResolvedPath(const std::wstring& path);
	bool_t LoadVoicesFromJson(const wchar_t* pRelativePath = L"Data/LoL/Sound/ChampionVoiceMap.json");
	bool_t LoadVoicesFromResolvedPath(const std::wstring& path);

	static constexpr size_t kSlotCount = static_cast<size_t>(eChampionSoundSlot::MAX);
	static constexpr size_t kVoiceSlotCount = static_cast<size_t>(eChampionVoiceSlot::MAX);
	static constexpr size_t kChampionSlots = 256; // eChampion 은 u8 기반

	std::array<std::array<std::wstring, kSlotCount>, kChampionSlots> m_Keys{};
	std::array<std::array<std::vector<std::wstring>, kVoiceSlotCount>, kChampionSlots> m_VoiceKeys{};
	f32_t m_fVolume = 0.8f;
	f32_t m_fVoiceVolume = 0.9f;
	f32_t m_fMaxAudibleDistance = 24.f;
	f32_t m_fMoveVoiceDelayMinSec = 8.f;
	f32_t m_fMoveVoiceDelayMaxSec = 10.f;
	bool_t m_bLoaded = false;
	bool_t m_bVoiceLoaded = false;
};
