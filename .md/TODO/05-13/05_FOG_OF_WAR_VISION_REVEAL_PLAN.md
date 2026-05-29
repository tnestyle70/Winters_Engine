# 05 전장의 안개 시야 공개 계획서

Current sequence

01/08: 미니언 지형 높이 및 맵 콜라이더 투영
-> 02/08: 우클릭 공격 추격
-> 03/08: B 귀환
-> 04/08: 미니맵
-> 05/08 현재: 실제로 보이는 전장의 안개
-> 06/08: HUD 아틀라스/초상화/스킬 아이콘
-> 07/08: 스킬 레벨업 및 데미지 +5
-> 08/08: 인게임 상점 및 아이템 스탯

Smoke 정책: 자동 smoke 검증은 폐지한다. 전장의 안개는 유저가 인게임 화면으로 직접 확인한다.

Bot AI is a GameCommand producer and must not directly mutate gameplay truth.
즉, Bot AI는 `GameCommand` 생산자이며 이동/공격/데미지 같은 gameplay truth를 직접 바꾸면 안 된다.

Goal

전장의 안개가 실제 화면에 보이게 한다. 로컬 팀 시야가 없는 곳은 검은색 또는 어두운 overlay로 덮고, 아군 챔피언/미니언/타워의 시야 원이 이동하면서 해당 안개를 걷어낸다.

Non-goals

- 와드 설치, 브러시 은신 전체 규칙은 이 slice에서 구현하지 않는다.
- 네트워크 snapshot 자체를 시야 밖에서 숨기는 서버 권위 visibility는 아직 하지 않는다. 먼저 visual fog를 완성한다.
- explored/unexplored 아트 polish는 단순 검정/어두움 구분까지만 한다.

Why this order

현재 코드는 fog texture를 계산하지만, 화면을 검게 덮는 연결이 부족하다. 이 slice는 이미 존재하는 vision data를 실제 플레이 피드백으로 바꾸는 작업이다.

Current-code evidence

- [VisionSystem.cpp](C:/Users/user/Desktop/Winters/Engine/Private/ECS/Systems/VisionSystem.cpp:30)는 vision tick과 FOW texture update를 수행한다.
- [VisionSystem.cpp](C:/Users/user/Desktop/Winters/Engine/Private/ECS/Systems/VisionSystem.cpp:247)는 기존 visible cell을 `255`에서 `127`로 낮춰 explored 상태를 유지한다.
- [VisionSystem.cpp](C:/Users/user/Desktop/Winters/Engine/Private/ECS/Systems/VisionSystem.cpp:256)는 `VisionSourceComponent`의 원형 시야를 texture에 반영한다.
- [FogOfWarRenderer.cpp](C:/Users/user/Desktop/Winters/Engine/Private/Renderer/FogOfWarRenderer.cpp:80)는 FOW texture를 D3D11 SRV로 업로드한다.
- [Scene_InGame.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp:1073)는 dirty FOW texture를 renderer에 업로드한다.
- [GameRoom.cpp](C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp:559)의 `BuildServerVisibleToAll` 때문에 서버 gameplay visibility는 현재 모두 보이는 쪽에 가깝다.
- [Minion_Manager.cpp](C:/Users/user/Desktop/Winters/Client/Private/Manager/Minion_Manager.cpp:830)는 local minion에 `VisionSourceComponent`를 붙인다.
- [Structure_Manager.cpp](C:/Users/user/Desktop/Winters/Client/Private/Manager/Structure_Manager.cpp:362)는 structure에 `VisionSourceComponent`를 붙인다.

Files touched

- 필요 시 수정: [VisionComponents.h](C:/Users/user/Desktop/Winters/Engine/Public/ECS/Components/VisionComponents.h)
- 수정: [VisionSystem.cpp](C:/Users/user/Desktop/Winters/Engine/Private/ECS/Systems/VisionSystem.cpp)
- 수정: [FogOfWarRenderer.h](C:/Users/user/Desktop/Winters/Engine/Public/Renderer/FogOfWarRenderer.h)
- 수정: [FogOfWarRenderer.cpp](C:/Users/user/Desktop/Winters/Engine/Private/Renderer/FogOfWarRenderer.cpp)
- 수정: [Scene_InGame.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp)
- 수정: [Scene_InGame.h](C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h)
- 서버 visibility까지 확장할 때 수정: [GameRoom.cpp](C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp)

Insertion/replacement anchors

- `Scene_InGame.cpp`: main world render 이후, ImGui/HUD overlay 전에 `m_pFogOfWarRenderer`로 전체 맵 또는 screen-space fog layer를 그린다.
- `FogOfWarRenderer.cpp::UpdateTexture`: SRV upload 경로는 유지하고, renderer가 overlay pass를 소유할 경우 draw method를 추가한다.
- `VisionSystem.cpp::UpdateFowTexture`: 0/127/255 의미는 유지한다.
- `GameRoom.cpp::BuildServerVisibleToAll`: visual fog slice에서는 건드리지 않는다. network hiding은 별도 slice로 분리한다.

Implementation outline

1. 기존 `CVisionSystem`을 reveal data의 원천으로 유지한다.
2. `CFogOfWarRenderer`가 SRV와 간단한 draw call 또는 UI-compatible texture handle을 제공한다.
3. Scene은 fog value가 0이면 높은 alpha black, 127이면 중간 alpha dark, 255이면 transparent로 overlay를 그린다.
4. 아군 챔피언/미니언/타워의 `VisionSourceComponent::sightRange`가 원형 영역을 드러낸다.
5. 미니맵은 기본 PNG가 붙은 뒤 같은 fog texture를 mask로 재사용할 수 있다.

Verification commands and expected results

- 자동 smoke 없음.
- 구현 후 위생 확인: `git diff --check`.
- 유저 수동 검증: 시작 위치에서 먼 맵이 검게 덮여 있고, 아군 챔피언/미니언/타워 주변 원형 시야만 밝아지는지 확인한다.
- 예상 결과: 멀리 있는 적/지형은 어둡게 가려지고, 아군 시야가 이동하면 안개가 걷힌다.

Rollback scope

Scene fog draw call만 제거하고 기존 FOW texture 계산은 남긴다.

Next slice

전장의 시야가 읽히면 06/08 HUD 구현으로 넘어간다.
