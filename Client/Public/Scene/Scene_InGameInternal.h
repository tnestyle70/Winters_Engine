#pragma once
// Scene_InGameInternal.h — Scene_InGame*.cpp 분할 TU가 공유하는 헬퍼.
// Stage 1: 2개 이상 TU가 쓰는 anonymous-namespace 헬퍼를 중복정의 드리프트 없이 단일화한다.
// 설계: .md/plan/refactor/15_INGAME_SCENE_THINNING_DESIGN.md
#include "GameObject/ChampionDef.h"

// champion catalog/registry/table을 순서대로 조회해 client ChampionDef를 찾는다.
// Network(UpdateNetworkChampionLocomotion) / LocalSkills(ApplyLocalPrediction) /
// Lifecycle(BindPlayerToECSChampion) / Input(FirePlayerAction)가 공유한다.
const ChampionDef* FindClientChampionDef(eChampion champion);

// 미니맵/사망 오버레이/렌더가 공유하는 화면 크기 폴백 (ImGui DisplaySize가 0일 때).
inline constexpr f32_t kFallbackScreenWidth = 1280.f;
inline constexpr f32_t kFallbackScreenHeight = 720.f;

// Kalista 로컬 패시브 대시 지속(초). OnUpdate(origin)와 LocalSkills TU가 공유한다.
f32_t& LocalKalistaPassiveDashDurationSec();
void SetLocalPassiveDashDuration(f32_t duration);
f32_t GetLocalPassiveDashDuration();

// 커맨드라인 토큰 존재 여부. OnUpdate 스모크(shell)와 OnEnter 부트스트랩(Lifecycle TU)이 공유한다.
bool_t HasCommandLineToken(const wchar_t* token);
