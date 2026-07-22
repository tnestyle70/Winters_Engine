# 2026-07-19 챔피언 월드 레벨 + 레벨업 FX + 미니맵 UI 튜너 RESULT

관련 계획: 2026-07-19_CHAMPION_WORLD_LEVEL_FX_MINIMAP_UI_TUNER_PLAN.md

```text
1. 예측 vs 실측:
- 모든 가시 챔피언 월드 체력바 왼쪽 레벨 전달·렌더 배선 → 코드/빌드 기준 적중. authoritative `Champion.level`을 `UIWorldHealthBarDesc.iLevel`로 전달하고 fallback/RHI overlay 모두 동일 helper로 한 번 그리며, F8에서 X/Y/font scale과 reset을 제공한다.
- FoW/은신 적 비노출 → 코드 경계 기준 적중. 월드 바 호출은 Debug reveal을 명시적으로 금지하고, 추가 보강으로 visibility component가 아직 없는 적도 fail-closed 처리한다. 기본 호출의 기존 fail-open 동작은 보존했으며 이 델타도 독립 재비평 residual P0 0/P1 0을 통과했다.
- 레벨업 FX → 코드/빌드 기준 적중. full snapshot 적용 tick이 바뀐 post-apply LateUpdate에서만 delta를 감지하고, 최초/rebase 관측은 baseline-only다. 보이는 생존 아군에 한해 `attachTo=NULL_ENTITY`, snapshot 지면 위치, 축소 recall 크기, lifetime 1.0초로 `Recall.Channel`을 1회 생성하며 carried/Yone soul은 제외한다.
- 미니맵/UI Manager → 코드/빌드 기준 적중. 기본 `ViewportHeightRatio`는 0.39이고 F8 UI tuner가 `UI Manager - Minimap`과 champion level X/Y/font slider를 함께 호출하며 변경 projection을 VisionSystem에 반영한다. 구조물 아이콘의 normalized 좌표는 의도대로 수정하지 않았다.
- Debug x64 전체 솔루션 빌드 → 수정 후 적중. 첫 빌드가 level FX 경로의 `YoneSoulPresentationTag` include 누락을 검출해 보강했고, 최종 `/t:Build /m:1 /nr:false`는 오류 0, 기존 EngineSDK DLL-interface C4251/C4275 경고 135로 통과했다. `git diff --check`와 `SimLab --gamefeel-only`도 exit 0이다.
- normal F5 시각 QA와 캡처 3종 → NOT_RUN. 서버·백엔드·5v5 다중 클라이언트 실행 없이 실제 폰트 겹침/클리핑, 0.39 패널 체감 정렬, FX의 지면 고정과 정확한 1초 종료를 눈으로 확인했다고 주장하지 않는다. 공식 최소 해상도/DPI도 계속 CONFIRM_NEEDED다.
2. 판결: 수정 반영 — 레벨/UI/FX/minimap 배선과 기본값은 유지하고, 빌드가 찾은 Yone tag include 및 strict visibility의 missing-component fail-closed를 보강했다. 컴파일·정적 계약은 닫혔고 시각 합격 판정만 수동 F5 영역으로 남는다.
3. ⑤ 갱신: 0.39 확대는 사용자가 선택한 panel-enlargement 종결안이지 탑/바텀 구조물 icon-to-PNG 정규화 오차의 수학적 수정이 아니다. 레벨 폰트 위치·배율은 해상도/DPI에 따라 클리핑될 수 있고, FX는 snapshot tick 단위 delta이므로 한 snapshot에서 여러 레벨이 뛰어도 효과는 1회다. 이 셋이 수용 불가능하면 다음 변경은 좌표 보정·DPI 계약·레벨별 event cue로 범위가 커진다.
```
