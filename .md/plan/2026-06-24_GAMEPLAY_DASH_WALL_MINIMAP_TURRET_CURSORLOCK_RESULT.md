Session - 2026-06-24 Dash 벽 끼임 / 미니맵 좌표·원형 초상화 / 포탑 이펙트 높이 / L키 커서 lock 5개 항목 반영 결과 보고서.

1. 항목별 반영 상태

1-1. Dash 벽 끼임 (도착 시 가장 가까운 walkable 셀로 스냅)

- 신규 공통 헬퍼 Shared/GameSim/Systems/Move/DashArrival.h (inline SnapDashArrivalToWalkable) 추가. 인터페이스 변경 없이 기존 IWalkableQuery::TryResolveMoveTarget(내부적으로 CNavGrid::TryFindNearestWalkableCell 호출)을 재사용한다. 도착 위치가 이미 walkable이면 손대지 않고, 벽이면 dash 시작점 기준 nearest walkable로 스냅 후 TrySampleHeight로 Y 보정.
- dash 도착 분기 9곳에 적용: LeeSin, Sylas, Yasuo, Viego, Fiora, Jax(각자 DashComponent), Yone(SoulReturn anchorPosition 복귀는 제외하고 일반 dash만), Irelia(IreliaSimComponent.dashStartPos 기반), Kalista(SkillCooldownSystem passive dash, endPos를 스냅값으로 갱신).
- 매 프레임 중간 클램프(TryClampMoveSegmentXZ, 벽 통과 차단)는 그대로 두고 도착 시점에만 1회 스냅한다.
- 참고: 과제에 언급된 Ezreal E는 Shared/GameSim/Champions에 시뮬 구현이 없어(미구현) 본 작업 대상에서 제외했다.

1-2. 미니맵 좌표 동기화 (탑이 아래로/바텀이 위로 표시되던 버그)

- 원인: Client/Public/UI/MinimapPanel.h의 MinimapProjection 3점이 비직교·비등배(skew)라 world Z축이 1.66배 부풀려져 top/bottom이 중앙으로 압축됨(anti-diagonal world 길이 313.8 vs base diagonal 188.8).
- 수정: 중심 (104.5, 0), 45° 회전, 직교·등배(반대각 ≈ 181)로 재교정. 이 한 곳만 고치면 미니맵 아이콘·카메라박스·FogOfWar 오버레이·미니맵 클릭이동이 동일 calibration으로 정합된다(Scene_InGameLifecycle이 이 값을 FowProjection으로 복사).
  vWorldAtUv00{ 104.50f, 181.02f } / vWorldAtUv10{ 285.52f, 0.00f } / vWorldAtUv01{ -76.52f, 0.00f }
- 검증식: Top+67→V≈0.247, Bot-67→V≈0.75 (0.5 기준 대칭), Center→UV(0.5,0.5).

1-3. 미니맵 플레이어 원형 초상화 (미니맵 좌표 수정 이후 교체)

- Client/Private/UI/MinimapPanel.cpp: 챔피언 마커를 팀색 단색 원에서 원형 초상화로 교체. UI_Draw_RawImageCircle이 텍스처 SRV를 받으면 삼각형 팬 UV 방사 매핑으로 자동 원형 크롭하므로 셰이더/알파마스크 불필요.
- eChampion→CTexture 캐시(s_ChampionPortraits) + EnsureChampionPortrait 헬퍼 추가(EnsureMinimapBaseTexture 패턴). 경로는 기존 GetRosterChampionPortraitPath(LobbyRosterHelpers, BanPick에서 사용 중) 재사용.
- DrawIcon 챔피언 분기: 외곽 테두리 원은 유지하고 안쪽 채움 원을 초상화 SRV로 그린다. 사망 시 회색조 틴트, 초상화 로드 실패 시 기존 팀색 단색 원으로 폴백.
- ShutdownRuntime에 s_ChampionPortraits.clear() 추가.

1-4. 포탑 이펙트 시전 위치 높이기

- Engine/Private/ECS/Systems/TurretAISystem.cpp: 투사체/이펙트 시작 Y를 turretPos.y + 2.5f → + 4.0f. 이 값이 currentPos→ProjectileSpawn pos→클라 startY로 전파되어 움직이는 투사체 비주얼·wfx cue가 함께 상승.
- Client/Private/Network/Client/EventApplier.cpp(SpawnTurretTopBeam): 머즐 섬광은 owner attach라 별도이므로 pos.y(+2.5f→+4.0f)와 vAttachOffset.y(2.5f→4.0f)를 동일하게 상승.

1-5. L키 커서 lock 토글

- Client/Private/Scene/Scene_InGameInput.cpp(UpdateCombatInput): if(!bImGuiKbd) 블록의 'P' 처리 직후 L키 토글 추가. 함수 static bool로 잠금 상태 보관, 잠금 시 GetClientRect→ClientToScreen→ClipCursor(&rcClip)로 클라 창 영역에 커서 confine, 해제 시 ClipCursor(nullptr). <Windows.h>는 이미 포함됨. ShowCursor 숨김은 LoL식 단순 lock에 불필요하여 제외.

2. 검증 결과

2-1. 코드 위생
- git diff --check: 통과(공백 오류 없음, LF→CRLF 경고만).

2-2. 서버 빌드
- MSBuild Server.vcxproj /p:Configuration=Debug /p:Platform=x64: 성공(SERVER_EXIT=0).
- TurretAISystem/IreliaGameSim/YoneGameSim/SkillCooldownSystem 등 변경 파일 컴파일 확인.

2-3. 클라이언트 빌드
- MSBuild Client.vcxproj /p:Configuration=Debug /p:Platform=x64: 성공(CLIENT_EXIT=0).
- EventApplier/Scene_InGameInput/MinimapPanel 컴파일 확인. eChampion 키 unordered_map도 정상 컴파일.
- 기존 C4275(DLL interface) 경고는 그대로 남아 있음(무관).

3. 남은 런타임 확인

- 각 dash(요네 E/R, 야스오 E, 사일러스 E, 피오라 Q, 비에고 W, 리신/이렐리아/잭스 등)가 벽 도착 시 가장 가까운 walkable로 빠지는지 인게임 확인. nearest-walkable 검색 반경은 TryResolveMoveTarget 기본값을 따른다(엉뚱한 곳으로 튀면 반경 조정 검토).
- 미니맵 top/bottom이 실제 맵과 일치하는지 1프레임 캡처로 확인. 45° 등배 가정이라 base 텍스처 정렬과 미세 오차가 있으면 Projection 3점 추가 조정.
- 미니맵에서 각 챔피언이 원형 초상화로 보이는지, 사망 시 회색조, 적팀 챔프 id가 채워지는지 확인.
- 포탑 이펙트 +4.0f 높이가 포탑 모델 꼭대기와 맞는지 확인(잠정값, 소켓 높이에 맞춰 조정).
- L키로 커서 lock 토글이 동작하는지, Alt-Tab/리사이즈 후 OS 자동 해제 시 재적용이 필요한지 확인(현재는 토글 시 1회 ClipCursor).
