# 04 미니맵 및 플레이어 흰색 박스 계획서

Current sequence

01/08: 미니언 지형 높이 및 맵 콜라이더 투영
-> 02/08: 우클릭 공격 추격
-> 03/08: B 귀환
-> 04/08 현재: 우측 하단 미니맵 및 플레이어 흰색 박스
-> 05/08: 전장의 안개
-> 06/08: HUD 아틀라스/초상화/스킬 아이콘
-> 07/08: 스킬 레벨업 및 데미지 +5
-> 08/08: 인게임 상점 및 아이템 스탯

Smoke 정책: 자동 smoke 검증은 폐지한다. 미니맵 위치/스케일은 유저가 인게임에서 직접 확인한다.

Bot AI is a GameCommand producer and must not directly mutate gameplay truth.
즉, Bot AI는 `GameCommand` 생산자이며 이동/공격/데미지 같은 gameplay truth를 직접 바꾸면 안 된다.

Goal

`Client/Bin/Resource/Texture/UI/Summoners_Rift_minimap.png`를 인게임 화면 우측 하단에 띄우고, 로컬 플레이어의 월드 위치를 흰색 박스/마커로 표시한다.

Non-goals

- 카메라 사각형, 핑, 와드, 챔피언 초상화 마커, fog-minimap blending은 아직 구현하지 않는다.
- 미니맵을 legacy debug window 뒤에 숨기지 않는다.
- HUD 전체 레이아웃 교체는 이 slice에서 하지 않는다.

Why this order

귀환과 전투 이동을 확인하려면 로컬 플레이어 위치를 계속 볼 수 있어야 한다. 미니맵 world-to-UV 변환이 안정되면 다음 slice의 전장의 안개 mask도 같은 좌표계를 쓸 수 있다.

Current-code evidence

- [MinimapPanel.cpp](C:/Users/user/Desktop/Winters/Client/Private/UI/MinimapPanel.cpp:16)에 이미 `CMinimapPanel::Render`가 있다.
- [MinimapPanel.cpp](C:/Users/user/Desktop/Winters/Client/Private/UI/MinimapPanel.cpp:20)는 현재 `Minimap`이라는 ImGui debug window다.
- [MinimapPanel.cpp](C:/Users/user/Desktop/Winters/Client/Private/UI/MinimapPanel.cpp:31)는 실제 minimap PNG가 아니라 fog texture를 렌더링한다.
- [InGameDebugBridge.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameDebugBridge.cpp:37)는 legacy debug UI에서만 minimap을 호출한다.
- [Scene_InGame.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp:1488)는 legacy debug가 꺼져 있으면 바로 return하므로 현재 미니맵은 항상 보이는 HUD가 아니다.

Files touched

- 수정: [MinimapPanel.h](C:/Users/user/Desktop/Winters/Client/Public/UI/MinimapPanel.h)
- 수정: [MinimapPanel.cpp](C:/Users/user/Desktop/Winters/Client/Private/UI/MinimapPanel.cpp)
- 수정: [Scene_InGame.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp)
- 필요 시 수정: [Scene_InGame.h](C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h)
- UI_Manager 기능으로 흡수할 경우 수정: [UI_Manager.h](C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h), [UI_Manager.cpp](C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp)

Insertion/replacement anchors

- `Client/Private/UI/MinimapPanel.cpp::Render`: debug-window rendering과 always-on overlay rendering을 분리한다.
- `Client/Private/Scene/Scene_InGame.cpp::OnImGui`: `m_bShowLegacyInGameDebug` early return 전에 always-on 미니맵을 호출한다.
- `Client/Private/UI/MinimapPanel.cpp`: `ImGui::Begin("Minimap")` 기반 창을 우측 하단 foreground/background draw-list 좌표 기반 overlay로 전환한다.
- `Client/Private/UI/MinimapPanel.cpp`: 처음에는 `CVisionSystem::FOW_TEX_WORLD_SIZE`로 world XZ -> minimap UV를 변환하고, 추후 Stage bounds로 교체한다.

Implementation outline

1. 기존 UI texture loader 경로로 `Texture/UI/Summoners_Rift_minimap.png`를 로드한다.
2. viewport 우측 하단에 안정적인 크기, 예: 240x240 또는 화면 높이 비율 기반으로 그린다.
3. local player world XZ를 minimap UV로 변환한다.
4. local player marker 중심에 작은 흰색 사각형을 그린다.
5. debug entity dot은 첫 구현에서는 별도 옵션으로 유지한다.

Verification commands and expected results

- 자동 smoke 없음.
- 구현 후 위생 확인: `git diff --check`.
- 유저 수동 검증: F1/F10 debug 없이 미니맵이 우측 하단에 보이고, 플레이어가 이동/귀환할 때 흰색 박스가 따라 움직이는지 확인한다.
- 예상 결과: raw fog texture가 아니라 `Summoners_Rift_minimap.png`가 표시된다.

Rollback scope

always-on minimap 호출을 제거하고 기존 debug 전용 `CMinimapPanel::Render` 동작으로 되돌린다.

Next slice

미니맵 world-to-UV mapping이 안정되면 05/08 전장의 안개로 넘어간다.
