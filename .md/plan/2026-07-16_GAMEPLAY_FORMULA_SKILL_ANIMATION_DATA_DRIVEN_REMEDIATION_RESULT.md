Session - 17개 챔피언 피해 공식·아이템 온힛·스킬 랭크·visual timing Data Driven cutover의 예측을 실측과 대조한다.
좌표: 신규 좌표 후보 · 축: C7 · C8
관련: 2026-07-16_GAMEPLAY_FORMULA_SKILL_ANIMATION_DATA_DRIVEN_REMEDIATION_PLAN.md · 2026-07-16_GAMEPLAY_FORMULA_DATA_DRIVEN_IMPLEMENTATION_REPORT.md

## 1. 예측 vs 실측

- 적중: 2026-07-17 재실행에서 definition pack은 `0x8E9EF70F`·17 champions·85 skills, champion data는 `0x9D6886A7`로 source/generated parity를 통과했다.
- 적중: `Tools/Bin/Debug/SimLab.exe`는 `FormulaData` 17/85, BORK 최대 체력 10% 1회 온힛과 post-mitigation lifesteal, Q/W/E·R rank gate, same-seed hash `D9B579C1425033BB`를 모두 PASS했다.
- 미실측: 이번 문서 개편에서는 Client/Server build와 정상 서버 F5 visual timing을 다시 실행하지 않았다. 2026-07-16 상세 구현 보고서의 `/m:1` build PASS는 이전 실측으로 유지한다.

## 2. 판결

수정 반영. 현재 구현된 17명·85슬롯의 수치 소유 구조는 Data Driven 합격을 유지한다. Fiora/Zed의 미확정 신규 passive와 기타 미구현 제품 기능은 이 판결에 포함하지 않는다.

## 3. ⑤ 갱신

실측 후에도 대가는 “즉시 편집 속도보다 서버 권위·회귀 안전 우선”이다. 이 선택은 Debug overlay/draft가 release truth로 오인되거나, variant hook이 canonical query를 우회하거나, 혼합 피해 item을 단일 타입 request에 합산하거나, client visual timing이 server action lock을 바꾸는 순간 틀린다. 또한 SimLab 실행 산출물의 실제 위치는 `Tools/Bin/Debug/SimLab.exe`이므로 이후 계획·하네스가 다른 경로를 쓰면 검증 누락으로 판정한다.
