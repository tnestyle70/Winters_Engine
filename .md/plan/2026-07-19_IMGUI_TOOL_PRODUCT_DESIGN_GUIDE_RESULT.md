Session - Claude와 Codex 공통 ImGui 제품 설계 가이드 반영 결과
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성, C8 검증이 병목
관련: 2026-07-19_IMGUI_TOOL_PRODUCT_DESIGN_GUIDE_PLAN.md

## 1. 예측 vs 실측

- 적중: `AGENTS.md`, `CLAUDE.md`, Compass, UI Pipeline이 각각 정확히 1회 단일 권위 가이드를 참조한다. 가이드 전문은 최종 `ACCEPT` 받은 PLAN 전문과 일치하고 Markdown fence 8개가 균형이며 `git diff --check`가 통과했다.
- 빗나감/교정: 최초 계획은 Tuner의 Primary/Secondary·Draft/Ack 계약을 Observer와 Workflow Editor에도 보편 적용해 단순화 명목의 기능 삭제를 다시 유발할 수 있었다. 서브 에이전트 최초 `REJECT`, 재비평 `REJECT` 뒤 유형별 적용표, 세 authority mode, 상태 모델, 실제 predicate 기반 비활성 이유, destructive reload, slider 조작/validation 범위 분리를 반영해 최종 `ACCEPT`를 받았다.
- 추가 실측: F4 `Reload Draft`는 disk JSON으로 현재 Draft를 덮고 dirty 확인이 없으며, `Save & Hot Load`는 `_DEBUG`와 authoritative scene/serializer/network connection이 필요하다. 사용자가 Release 실행이었다고 확인했으므로 Release 차단은 정상 정책이며 제한 해제 대상이 아니다.
- 미검증: 이 세션은 문서 가이드 반영이라 새 ImGui 화면이나 수동 캡처를 만들지 않았다. 첫 후속 ImGui 변경은 가이드의 executable/scene/shortcut·해상도/DPI·성공/실패 캡처 게이트를 실제 적용해야 한다.

## 2. 판결

수정 반영. 상세 규칙은 `.md/architecture/WINTERS_IMGUI_TOOL_DESIGN_GUIDE.md` 한 곳이 소유하고 루트 문서는 필수 읽기 링크만 가진다. 다른 세션의 F4 작업은 Release 정책을 유지한 채 Debug Hot Load 왕복, reload 문구/dirty 확인, 근거 있는 field별 slider 범위와 실제 화면을 검증해야 한다.

## 3. ⑤ 갱신

모든 ImGui 변경에 같은 버튼 수를 강제하면 오히려 기능을 훼손한다. 비용은 유형 분류·작업 계약·수동 캡처가 추가되는 것이며, 이 비용은 빌드만으로 UX 완료를 오판하지 않기 위해 유지한다. 이번 세션은 공통 규칙을 실제 승인/반려 사례로 환전한 바닥 70%·천장 30% 작업이다.
