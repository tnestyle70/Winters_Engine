Session - F4 저장값을 보존한 채 canonical authoring 계약과 공개 빌드를 닫았다
좌표: 없음 · 축: C7 권위와 정합성, C8 검증이 병목
관련: 2026-07-22_F4_AUTHORING_AUTHORITY_AND_PUBLIC_PUSH_PLAN.md

## 1. 예측 vs 실측

- 적중: F4가 저장하는 canonical 4개 파일 전체를 원본 작업 폴더와 JSON 구조로 비교해 모두 일치했다. 실제 복구 범위는 `SkillEffectGameplayDefs.json`의 5개 배열, 15개 scalar leaf였고 다른 canonical 의미 변화는 0건이었다.
- 적중: current source로 recook한 definition pack hash는 `0xBEC4359E`이며 두 generator `--check`, F4/BasicAttack/WFX 회귀, RHI truth gate, Go tests, Shared boundary와 Debug x64 전체 빌드가 모두 통과했다.
- 빗나감 교정: 초안은 일부 ranked 값만 보호했으나 독립 비평에서 다른 F4-editable exact fixture와 4파일 전체 동일성 검사가 빠진 것을 발견했다. 모든 editable exact fixture를 schema·domain·rank shape·canonical/generated parity 검사로 전환하고 전체 JSON 비교를 추가한 뒤 P0/P1 0으로 구현했다.

## 2. 판결

- 수정 반영. 현재 F4 canonical 값은 유지하고 파생물만 recook한다. 과거 PLAN/RESULT와 generated 파일은 값 권위가 아니며, 생성물 freshness 실패로 source를 되돌리지 않는 규칙을 모든 세션 시작 문서와 도메인 문서에 고정했다.

## 3. ⑤ 갱신

- exact 밸런스 숫자를 기본 회귀 테스트에서 제거했으므로 임의 수치 변화 자체는 테스트가 막지 않는다. 이 선택은 사용자 승인·원본/작업본 전체 JSON 비교·git diff 리뷰가 변경 승인 경계일 때 유효하며, 사용자가 특정 baseline을 명시적으로 동결하면 그때만 별도 exact-value 계약을 추가해야 한다.
- 실제 F4 창에서 다시 저장하는 수동 UX smoke는 하지 않았다. 자동 검증은 파일 저장 결과, codegen, 서버 pack 소비 계약과 전체 빌드를 닫았지만, ImGui 조작감 자체가 변경되는 작업에서는 별도 수동 smoke가 필요하다.
