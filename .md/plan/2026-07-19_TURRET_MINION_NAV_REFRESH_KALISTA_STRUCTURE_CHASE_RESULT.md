# 2026-07-19 파괴 구조물 내비 갱신 + 구조물 추적 도착 보정 RESULT

관련 계획: 2026-07-19_TURRET_MINION_NAV_REFRESH_KALISTA_STRUCTURE_CHASE_PLAN.md

```text
1. 예측 vs 실측:
- 파괴 구조물 cell 해제·복구 probe → 적중. 동일 시드 1,800 tick 두 실행 모두 `STRUCTURE_NAV_PROBE status=PASS live_blocked=1 dead_released=1 restored_blocked=1`을 출력했다.
- 미니언 진행 회귀 기준 → 적중. 두 실행 모두 `minion_lane_slots=6`, `max_minion_lane_stall_ticks=18`(예산 180 이하), `minion_opposed_yaw=0`이었다.
- tick p99 33,333us 이하 → 재실행에서 적중. run 1은 25,375.233us, run 2는 28,615.082us였다. 다만 첫 실행은 호스트 부하로 deadline miss 12/1,800이 발생했고, 유휴 재실행도 각 2회·3회 miss가 남아 max-tick 지표는 환경 민감했다.
- 구조물 중심과 보행 가능 도착점의 2.0m 오프셋 보정 → 적중. `SimLab --gamefeel-only`가 시작·repath 공통 수식과 최악 도착점의 raw target 사거리 포함을 검사해 `[AttackChaseResolved] PASS: offset=2.000 radius=4.950`으로 통과했다.
- Debug x64 전체 솔루션 빌드 → 수정 후 적중. 계획의 project-name solution target은 MSB4057이므로 `/t:Build`로 정정했고, 첫 실제 빌드가 `YoneSoulPresentationTag` 선언 include 누락을 잡아 보강했다. 최종 전체 빌드는 오류 0, 기존 EngineSDK DLL-interface C4251/C4275 경고 135로 통과했다. `git diff --check`도 whitespace 오류 없이 exit 0이다.
- 동일 시드 결정성 wrapper PASS 예측 → 빗나감. 개별 두 run은 모두 `RESULT status=PASS`이고 replay hash도 `AFFC4AD9B7944C0A`로 같지만 final world hash는 달랐다. 두 `final_state.bin`을 store 단위로 비교한 결과 차이 133바이트는 전부 기존 `ChampionAssistCreditComponent::Credit`의 raw POD padding 위치였고 SourceEntity·LastDamageTick·SourceTeam 값은 동일했다. 이번 내비/추적 로직의 상태 분기는 아니며 체크포인트 byte-hash의 기존 거짓 음성이라 직렬화 포맷 변경은 범위 밖으로 남겼다.
- normal F5에서 파괴 타워를 실제 통과하는 미니언과 칼리스타 애니메이션 육안 QA → NOT_RUN. 서버·백엔드·다중 클라이언트가 필요한 상호작용 세션은 이번 자동/빌드 검증에서 실행하지 않았다.
2. 판결: 수정 반영 — terrain 원본에서 생존 구조물만 재-carve하고 path/minion flow/AI goal을 상태 변화 때 갱신하는 구현, 공통 추적 반경 수식, focused probe와 전체 Debug 빌드는 유지한다. 잘못된 MSBuild target과 빌드가 찾은 include만 보정했으며 기능 자동 게이트는 통과했다.
3. ⑤ 갱신: 매 minion-wave tick의 구조물 O(S) hash scan은 유지되지만 grid/flow rebuild는 구조물 생존 상태 변화에만 발생한다. 구조물이 런타임에 살아 있는 채 이동하면 위치는 hash에 없으므로 이 설계가 틀리며, authored waypoint sanitize의 수명 문제와 실제 화면 횡단은 F5 QA가 필요하다. 별도 기술부채로 raw POD keyframe padding을 의미 기반 codec으로 바꾸기 전에는 same-seed final byte hash가 gameplay 결정성의 신뢰 가능한 단독 판정기가 아니다.
```
