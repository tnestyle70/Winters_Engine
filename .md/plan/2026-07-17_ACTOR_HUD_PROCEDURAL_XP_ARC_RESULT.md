## 1. 예측 vs 실측

- 적중: runtime layout의 XP 요소는 1개이며 `shape=ringArc`, `arc=[38,-96,8.5]`이다. `sprite`와 `clip` 속성, atlas의 fill/track 두 sprite 정의는 제거됐다.
- 적중: 정적 좌표 실측은 0% 시작점 `(268.79,159.79)`, 50% 종점 `(281.29,99.80)`, 100% 종점 `(252.40,48.55)`다. 하단→우측→상단 순으로 채워진다.
- 적중: 실제 로드 상수, fallback 상수, 튜너 저장 경로, 실패 로그가 모두 `hud_irelia_layout.json`을 가리킨다. fallback도 `portrait.face → portrait.frame → ringArc` 순서다.
- 적중: HUD Layout 튜너에 기본 OFF XP preview와 비율 slider, 원호 rect/start/sweep/thickness/start·end color 편집을 연결했다. gameplay `ActorHUDState.XpRatio`는 변경하지 않았다.
- 미실측: 사용자 요청에 따라 Engine/Client 빌드와 인게임 XP 0/50/100% 시각 검증은 실행하지 않았다.

## 2. 판결

수정 반영 — 독립된 보라 PNG를 직사각형으로 잘라 맞추는 기존 경로를 폐기하고, `CUIRenderer::DrawRingArc`의 타원 링 메시를 HUD 기준 좌표에서 직접 생성하도록 전환했다. JSON 2개 파싱, XP layout 계약, 끝점 수식, 대상 파일 trailing whitespace와 scoped `git diff --check`는 통과했다. 런타임 최종 판결은 사용자 빌드의 0/50/100% 캡처 전까지 보류한다.

## 3. ⑤ 갱신

- 실제 비용은 XP 원호 1개당 48구간·288정점이며 기존 UI 동적 버퍼 한도 65,536정점 안이다.
- `Client/Bin/Resource/UI`는 ignored runtime output이므로 로컬 JSON 반영은 git diff에 잡히지 않는다. 동일 procedural fallback을 tracked Engine 코드에 함께 둬 JSON 유실 시 구형 PNG 경로로 회귀하지 않게 했다.
- 빈 홈이 인게임에서 타원 호가 아닌 자유곡선으로 확인되거나 레벨 구슬의 authored 가림이 procedural arc와 충돌하면 이 선택은 틀린다. 그 경우 width/height 재튜닝으로 돌아가지 않고 프레임과 동일 UV 공간의 mask 셰이더로 교체한다.
