# 2026-07-19 미니언 웨이브 진행·후진 방지·귀환 FX 축소·비에고 R 초기화 RESULT

## 1. 예측 vs 실측

- **빗나감 — 첫 yaw 보정:** 마지막 depenetration correction 방향만 바라보게 한 1차 구현은 tick 392/entity 70에서 `minion_opposed_yaw=1`로 실패했다. correction 하나가 그 tick 전체 순변위와 반대일 수 있었기 때문이다. `Phase_ServerUnitAI` 시작 위치부터 최종 위치까지의 aggregate 변위로 yaw를 확정하도록 수정한 뒤 500 tick smoke와 1,800 tick 2회에서 `minion_opposed_yaw=0`을 확인했다.
- **적중 — 바텀 회전/정체 원인:** A*가 이전 cell을 path에 다시 넣는 문제는 아니었다. 진행 index 없이 가장 가까운 lane segment만 고르는 flow field가 굽은 구간에서 이미 지나온 segment 방향을 낼 수 있고, 목표 거리 진전 없이 blocked counter가 초기화되는 것이 핵심이었다. 현재 waypoint와 명확히 반대인 flow를 같은 tick의 waypoint/A* fallback으로 넘기고, 실제 목표 거리 감소만 진행으로 인정했다.
- **적중 — 6개 팀·라인 통합 검증:** warm Debug 1,800 tick/seed 42 두 실행이 각각 `RESULT status=PASS`였다. 두 실행 모두 `minion_lane_slots=6`, `max_minion_lane_stall_ticks=18`, `minion_opposed_yaw=0`, replay hash `06D45DE8DCA6A46C`였다. 따라서 FollowWave 리신이 소비하는 아군 웨이브의 서버 권위 이동을 고쳤고 FollowWave 의사결정 자체는 변경하지 않았다.
- **빗나감 — 부가 raw world hash 게이트:** 두 PASS 실행의 raw world hash는 `7E9B19F4F9D567E0` / `F3968F2DC0BE73A2`로 달랐다. 리플레이와 기능 지표는 동일하며, 직전 `2026-07-18_CHAMPION_AI_LIVE_SAFE_TURRET_RECALL_ANCHOR_RESULT.md`가 작업 전 증거까지 대조해 `ChampionAssistCreditComponent`·`CombatActionComponent` POD padding 차이로 분리한 기존 checkpoint 인프라 문제와 같은 게이트다. 이번 범위에서 codec을 확장 수정하지 않았다.
- **빗나감 — 첫 Debug 성능 표본:** 첫 1,800 tick 실행은 cold Debug 표본에서 deadline miss 17회로 9회 한도를 넘었다. 재실행 두 표본은 2회/7회, p99 `24.880 ms`/`30.654 ms`로 33.333 ms 예산 안에서 각각 PASS했다.
- **적중 — 비에고:** SimLab이 강탈 성공 직후, channel 완료 전 R runtime 전체 0을 검증했고, 강탈 후 R 사용 시 cooldown이 다시 시작되며 form 해제에도 유지되는 기존 검증과 함께 `[SimLab][Viego] PASS`, 전체 `[SimLab] PASS`, exit 0이었다.
- **적중 — 귀환 FX와 빌드:** runtime recall diameter와 `recall.wfx`의 width/height를 모두 정확히 1/3로 줄였고 JSON 파싱 값은 `5.3 / 5.3`, texture 경로도 유효했다. GameSim, SimLab, Server, Client Debug x64 빌드가 모두 성공했으며 최종 `git diff --check`는 오류 없이 기존 LF/CRLF 경고만 출력했다.

## 2. 판결

**수정 반영.** 미니언 진행·최종 이동방향 yaw, 비에고 강탈 즉시 R 초기화, 귀환 FX 1/3 축소는 권위 경계를 유지한 채 기능·통합·빌드 검증을 통과했다. raw checkpoint padding 해시 문제는 동일 replay와 선행 작업 전 증거로 이번 변경의 회귀가 아님을 분리했다.

## 3. ⑤ 갱신

- 현재 waypoint와 반대인 flow만 거부하므로 기존 flow field의 빠른 경로는 유지하지만, 거의 직교하거나 잘못된 전진 flow는 stall counter가 임계값에 도달한 뒤 fallback한다. 더 빠른 코너 반응이 필요해지는 시점에는 flow field 자체에 lane 진행 index를 넣는 재설계가 필요하다.
- 한 tick 시작 위치 map은 checkpoint/gameplay truth가 아닌 Server 임시 상태다. 대신 minion 수에 비례한 map clear/lookup 비용이 생긴다. warm 1,800 tick p99는 예산 안이었지만 대규모 roster에서 별도 계측이 틀림 조건이다.
- 자동 검증은 6개 팀·라인의 active A* waypoint/line waypoint 진전과 실제 순변위 대비 yaw를 닫는다. 귀환 FX의 최종 미감과 사용자 스크린샷의 정확한 카메라 구도는 WFX 툴/F5 시각 확인 영역이다.
