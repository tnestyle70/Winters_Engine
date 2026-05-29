# 06 챔피언 HUD 아틀라스 및 아이콘 계획서

Current sequence

01/08: 미니언 지형 높이 및 맵 콜라이더 투영
-> 02/08: 우클릭 공격 추격
-> 03/08: B 귀환
-> 04/08: 미니맵
-> 05/08: 전장의 안개
-> 06/08 현재: HUD 아틀라스/초상화/스킬 아이콘
-> 07/08: 스킬 레벨업 및 데미지 +5
-> 08/08: 인게임 상점 및 아이템 스탯

Smoke 정책: 자동 smoke 검증은 폐지한다. HUD 외형은 유저가 인게임에서 직접 확인한다.

Bot AI is a GameCommand producer and must not directly mutate gameplay truth.
즉, Bot AI는 `GameCommand` 생산자이며 이동/공격/데미지 같은 gameplay truth를 직접 바꾸면 안 된다.

Goal

준비된 UI atlas를 사용해 인게임 HUD를 구성한다. 현재 챔피언의 초상화와 Q/W/E/R 스킬 아이콘을 배치하고, HP/MP/레벨/골드/쿨다운 텍스트를 atlas 위에 올린다.

Non-goals

- 모든 챔피언의 최종 LoL 수준 좌표/아트 polish는 이 slice에서 완성하지 않는다.
- 상점 인벤토리 슬롯 구현은 이 slice에서 하지 않는다.
- 스킬 레벨업 서버 권위 처리는 다음 slice에서 한다.

Why this order

UI manager에는 이미 HUD render path가 있다. 이 slice는 placeholder/base bar에 가까운 HUD를 실제 챔피언별 UI composition으로 바꿔, 다음 레벨업/상점 slice의 clickable region 기반을 만든다.

Current-code evidence

- [UI_Manager.h](C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h:93)는 `ChampionHUDState`를 정의한다.
- [UI_Manager.cpp](C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp:582)는 RHI로 HUD base를 그린다.
- [UI_Manager.cpp](C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp:663)는 HUD overlay 텍스트와 스킬 상호작용을 그린다.
- [UI_Manager.cpp](C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp:721)는 이미 gold 텍스트를 표시한다.
- [UI_Manager.cpp](C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp:735)는 이미 skill rank를 읽는다.
- 기존 참고 문서: [HUD_ATLAS_COMPOSITION_PLAN.md](C:/Users/user/Desktop/Winters/.md/TODO/05-12/HUD_ATLAS_COMPOSITION_PLAN.md), [HUD_STATUS_XP_SKILL_APPLIED.md](C:/Users/user/Desktop/Winters/.md/TODO/05-12/HUD_STATUS_XP_SKILL_APPLIED.md)

Files touched

- 수정: [UI_Manager.h](C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h)
- 수정: [UI_Manager.cpp](C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp)
- 추가: [ChampionHUDAssets.h](C:/Users/user/Desktop/Winters/Client/Public/UI/ChampionHUDAssets.h) 또는 동등한 client-side registry
- 추가: [ChampionHUDAssets.cpp](C:/Users/user/Desktop/Winters/Client/Private/UI/ChampionHUDAssets.cpp)
- UI manager에 asset 주입 API가 필요하면 수정: [GameInstance.h](C:/Users/user/Desktop/Winters/Engine/Public/GameInstance.h), [GameInstance.cpp](C:/Users/user/Desktop/Winters/Engine/Private/GameInstance.cpp)
- texture path 등록/추가: `Client/Bin/Resource/Texture/UI/`

Insertion/replacement anchors

- `CUI_Manager::DrawChampionHUDRHI`: `m_pSRV_ChampionHUDBase`를 그린 뒤 portrait와 skill icon을 그린다.
- `CUI_Manager::DrawChampionHUDOverlay`: 텍스트, cooldown, rank pip는 atlas art 위에 유지한다.
- `CUI_Manager::BuildChampionHUDState` 근처: 현재 champion id와 skill state가 `ChampionHUDState`에 들어가게 확인한다.
- `CUI_Manager::Initialize` asset loading 근처: atlas/portrait/icon SRV를 1회 로드한다.

Implementation outline

1. portrait, Q/W/E/R, summoner spell, item slot, HP/MP bar, XP bar의 reference-space 좌표를 정의한다.
2. HUD atlas와 챔피언별 portrait/icon texture를 로드한다.
3. `ChampionHUDState.Champion`으로 올바른 portrait/icon set을 선택한다.
4. RHI draw path는 이미지를 그리고, ImGui overlay path는 텍스트/cooldown/rank/click region을 그린다.
5. 누락된 챔피언 asset은 crash가 아니라 명확한 fallback texture로 표시한다.

Verification commands and expected results

- 자동 smoke 없음.
- 구현 후 위생 확인: `git diff --check`.
- 유저 수동 검증: Irelia/Ashe/Ezreal로 진입해 portrait와 Q/W/E/R icon이 챔피언에 맞고 텍스트와 겹치지 않는지 확인한다.
- 예상 결과: debug window 없이 HUD가 보이고, primitive placeholder 느낌이 줄어든다.

Rollback scope

`ChampionHUDAssets` registry를 제거하고 `DrawChampionHUDRHI`를 HUD base + 기존 overlay text만 그리는 상태로 되돌린다.

Next slice

icon slot과 clickable HUD region이 안정되면 07/08 스킬 업그레이드로 넘어간다.
