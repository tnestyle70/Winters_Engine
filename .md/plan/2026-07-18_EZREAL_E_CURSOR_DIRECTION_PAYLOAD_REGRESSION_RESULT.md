Session - 이즈리얼 E 커서 방향 payload 회귀 복구 결과
관련: 2026-07-18_EZREAL_E_CURSOR_DIRECTION_PAYLOAD_REGRESSION_PLAN.md

## 1. 예측 vs 실측

- 적중: `GroundTarget` 명령이 `groundPos`와 `DirectionXZ(origin, ground)`을 함께 채우도록 반영됐다. 기존 `[Cast]` 로그도 두 값을 동시에 출력한다.
- 적중: generated 이즈리얼 E는 계속 `GroundTarget`이고, 서버는 `groundTarget-origin`과 `min(4.75, requestedDistance)`로 착지를 결정한다. 서버 게임플레이 계약은 수정하지 않았다.
- 적중: authored `GroundTarget` 스킬은 5개이며 챔피언 코드 검색에서 0벡터 `direction`을 sentinel로 비교하는 경로는 발견되지 않았다.
- 정적 실측: 대상 파일 `git diff --check` PASS. 빌드·인게임 실측은 실행 중인 사용자 Release 세션 보호를 위해 보류했다.

## 2. 판결

수정 반영. Data Driven이 바꾼 E의 절대좌표 계약을 되돌리지 않고, 누락된 보조 방향 payload만 복구했다. 런타임 최종 판정은 다음 Client 빌드 후 `PENDING`이다.

## 3. ⑤ 갱신

영향 범위는 `GroundTarget` 5개 스킬의 보조 방향 필드다. 정적 검색상 sentinel 충돌은 없지만, 다음 빌드에서 이즈리얼 E 전·후·좌·우 및 4.75 이내/밖과 Zed W·Kindred R·Annie R·Viego R의 기존 방향/FX가 유지되는지 확인해야 한다.
