Session - 2026-06-24 체력바 아군/적 색상 구분 + Red Team 비대칭 버그 수정 결과 보고서.

1. 요구사항과 해석

- "블루팀/레드팀 진영별, 아군과 적팀 체력바 색상을 다르게" → 챔피언 체력바는 이미 로컬 플레이어 기준 아군(초록)/적(빨강)으로 구현돼 있었으나, 미니언/포탑은 절대 진영(Blue=파랑/Red=빨강) 고정이었다. 이를 챔피언과 동일하게 '로컬 플레이어 기준 아군/적'으로 통일했다. 색/텍스처(파랑=아군, 빨강=적)는 유지하고 분기 기준만 절대 진영→로컬 상대로 바꿨다.
- 이 변경은 사용자가 함께 요청한 "Red Team 버그"와 정확히 일치한다: 기존 절대 진영 고정 때문에 로컬 플레이어가 Red팀이면 아군 미니언/포탑이 적색으로 표시되는 색 반전 버그였다.

2. 반영 내용

2-1. 체력바 아군/적 색상 (Engine/Private/Manager/UI/UI_Manager.cpp)

- 머리 위 체력바는 Engine CUI_Manager가 World→Screen 투영으로 2D 오버레이로 그린다. RHI/ImGui 이중 경로라 같은 로직을 네 함수에 모두 수정했다.
- DrawMinionHealthBarsRHI / DrawMinionHealthBars / DrawTurretHealthBarsRHI / DrawTurretHealthBars: 각 함수에 const eTeam localTeam = UI_Resolve_LocalTeam(m_pWorld) 추가, 분기 기준 (team == eTeam::Blue) → (team != eTeam::TEAM_END && team == localTeam)로 교체. 색/텍스처 값은 유지(아군=파랑/Blue텍스처, 적=빨강/Red텍스처). 변수명은 의미를 반영해 bBlueTeam → bAllyTeam.
- 챔피언 체력바(DrawHealthBarsRHI/DrawHealthBars)는 이미 동일 기준이라 미변경. 화면 하단 로컬 HUD(DrawChampionHUDRHI)는 본인 것이라 대상 아님.

2-2. Red Team 버그: 네트워크 경로 m_PlayerTeam 미설정 (Client/Private/Scene/Scene_InGameLifecycle.cpp)

- m_PlayerTeam 대입이 로스터 스폰 경로에만 있어, 네트워크 클라이언트가 Red(적) 진영으로 배정되면 m_PlayerTeam이 기본값 eTeam::Blue에 머물러 IsEnemyOfPlayer/GetPlayerTeam 기반 아군/적 판정이 뒤집혔다.
- BindPlayerToECSChampion(네트워크/로컬 공통 경로)에서 ChampionComponent를 읽는 지점에 m_PlayerTeam = champion.team 대입을 추가해 네트워크 경로에서도 로컬 팀이 채워지게 했다. (체력바 자체는 UI_Resolve_LocalTeam(LocalPlayerTag 기반)을 쓰므로 m_PlayerTeam에 비의존이지만, 입력/타겟팅 등 m_PlayerTeam을 쓰는 다른 경로의 Red 진영 반전을 함께 해소.)

2-3. Red Team 버그: Nexus 렌더 컬링 비대칭 (Client/Private/Manager/Structure_Manager.cpp)

- Render()와 AppendRenderSnapshotMeshes() 두 경로에서 Blue Nexus만 프러스텀 컬링을 우회(항상 렌더)하고 Red Nexus는 일반 컬링 대상이라, Red 넥서스가 프러스텀 경계에서 비대칭 컬링/pop-in이 발생했다.
- 분기 조건을 (team == eTeam::Blue && kind == Nexus) → (kind == Nexus)로 바꿔 양 팀 넥서스 모두 컬링 없이 항상 렌더하도록 대칭화. Blue의 기존 동작은 그대로이고 Red만 동일 우대를 받는다.

3. 검증 결과

3-1. 코드 위생
- git diff --check: 통과(공백 오류 없음, LF→CRLF 경고만). UI_Manager.cpp에 bBlueTeam 잔존 0건 확인.

3-2. 서버 빌드
- MSBuild Server.vcxproj /p:Configuration=Debug /p:Platform=x64: 성공(SERVER_EXIT=0). UI_Manager.cpp 컴파일 확인.

3-3. 클라이언트 빌드
- MSBuild Client.vcxproj /p:Configuration=Debug /p:Platform=x64: 성공(CLIENT_EXIT=0). UI_Manager/Structure_Manager/Scene_InGameLifecycle 컴파일 확인. 기존 C4275(DLL interface) 경고는 그대로 남음(무관).

4. 남은 런타임 확인

- Blue 플레이어와 Red 플레이어 각각으로 접속해 미니언/포탑 체력바가 '아군=파랑, 적=빨강'으로 일관되게 보이는지 확인(특히 Red 플레이어 화면에서 아군이 파랑인지).
- 챔피언(초록/빨강)과 미니언/포탑(파랑/빨강)의 색 톤 차이가 의도대로인지, 혹은 전부 한 체계로 통일하고 싶은지 확인.
- 네트워크에서 Red 진영 배정 시 m_PlayerTeam 의존 로직(입력/타겟팅 등)이 정상인지 확인.
- 양 팀 넥서스가 프러스텀 경계/원거리에서 대칭으로 렌더되는지 확인.

5. 메모

- 사용자가 '진짜 절대 진영색(Blue 진영=파랑, Red 진영=빨강 고정)'을 원했다면 방향이 반대다. 본 작업은 LoL 관례 및 챔피언 체력바와의 일관성을 근거로 '로컬 기준 아군/적'으로 통일했다. 절대 진영 고정을 원하면 챔피언 체력바를 포함해 반대로 재조정 필요.
