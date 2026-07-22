# Winters ImGui Tool Product Design Guide

작성일: 2026-07-19
상태: 모든 ImGui 도구의 즉시 적용 리뷰 기준. 신규·변경 도구는 필수 준수하며, 기존 도구는 별도 전수 감사 권한이 없는 한 해당 도구를 수정할 때 이관한다.
모토: **단순함은 궁극의 정교함이다.**

## 1. 단순함의 정의

Winters에서 단순함은 기능을 덜 제공하는 것이 아니다. 사용자가 하려는 일을 완결하는 기능은 모두 보존하고, 그 일과 관계없는 화면 요소·중복 경로·내부 구현 용어·불필요한 행동만 제거하는 것이다.

```text
기능 범위는 완전하게
화면 구조는 최소하게
권위 흐름은 하나로
결과 확인은 즉시
```

- “본질만”은 필수 필드 삭제가 아니다. 필수 필드로 가는 경로를 선명하게 만드는 것이다.
- 한 창은 한 문장으로 설명되는 사용자 작업 하나를 우선한다.
- 기본 화면은 가장 자주 하는 작업만 보여준다. 진단 정보와 드문 옵션은 필요할 때만 연다.
- 복잡한 도메인은 복잡할 수 있다. WFX 그래프처럼 작업 자체가 복잡한 경우에도 도메인 복잡성만 남기고 운영 잡음을 제거한다.
- 빌드 통과는 UI 완료가 아니다. 실제 화면에서 목적, 계층, 조작, 피드백이 확인되어야 한다.

## 2. 구현 전 사용자 작업 계약

ImGui 코드를 쓰기 전에 계획서에 아래 계약을 먼저 채운다. 첫 문장을 쓰지 못하면 구현을 시작하지 않는다.

```text
사용자 작업: [누가] [무엇을] [어떤 결과가 될 때까지] 한다.
대상 범위: [선택 가능한 객체 전체]
필수 데이터: [반드시 보고/편집해야 하는 값]
핵심 행동: [유형별 핵심 행동, toolbar 명령, 또는 해당 없음]
제외: [이번 도구가 하지 않는 것]
권위/저장: [Draft owner -> persist owner -> apply owner -> 확인 신호]
완료 증거: [수동 화면 경로 + 동작 결과]
```

그 다음 필수 데이터 범위표를 만든다. 사용자 요구의 모든 명사는 정확히 한 카테고리와 한 편집 경로에 배치한다. `Current` 하나만 노출하거나 임의의 일부 필드만 남기지 않는다. 범위 축소가 필요하면 사용자 승인 없이는 진행하지 않는다.

## 3. 도구 유형을 먼저 고른다

### Tuner

값을 선택·편집·저장·적용하는 도구다. 기본 구조는 `대상 선택 -> 카테고리 -> 필드 -> 적용 -> 상태`다. F4 Balance, HUD Layout Tuner가 여기에 해당한다.

### Debug Observer

상태를 관측하고 원인을 좁히는 도구다. 요약과 핵심 신호를 먼저 보여주고 raw ECS/네트워크/로그는 `Diagnostics` 뒤에 둔다. 관측 패널에 mutation 버튼을 섞지 않는다.

### Workflow Editor

그래프·타임라인·에셋처럼 화면 자체가 작업 공간인 도구다. 도메인 도구막대, 작업 캔버스, 선택 항목 속성, 결과 상태만 유지한다. WFX의 유효한 복잡성은 여기에 속한다.

한 창이 두 유형을 동시에 수행하려 하면 창이나 명확한 모드로 분리한다. 관측과 변경, 일반 작업과 내부 진단을 한 기본 화면에 섞지 않는다.

### 유형별 적용표

| 계약 | Tuner | Debug Observer | Workflow Editor |
|---|---|---|---|
| 범위표·사용자 작업 | 필수 | 관측 신호 범위로 필수 | 작업/캔버스 명령 범위로 필수 |
| Mutation action | Primary 1개 중심 | 없어도 됨. 개입이 필요하면 Tuner로 분리 | undo/redo/save/preview 등 도메인 필수 명령 보존 |
| Draft/Persist/Apply/Ack | 해당 authority mode에 맞게 필수 | 비해당. source/freshness/empty/error 상태 필수 | asset owner의 dirty/save/reload/revision 계약 필수 |
| 행동 예산 | routine Primary 1개, Secondary 최대 1개 | 핵심 시각화 토글만 기본, raw 진단은 접음 | 현재 모드의 commit/export를 하나만 강조하되 canvas 명령 수를 억지로 줄이지 않음 |
| 수동 시각 QA | 편집 성공·실패 모두 | 정상·empty·error/freshness | 편집·undo/redo·save/reload |

단순한 3줄 Observer도 사용자 작업 한 문장, source/freshness, empty/error, 실제 화면 확인은 지킨다. Draft나 Primary action처럼 적용되지 않는 항목은 `해당 없음`으로 기록하며 가짜 버튼이나 가짜 ack를 만들지 않는다.

## 4. 기본 화면 구조

Tuner의 기본 와이어프레임은 다음 순서를 기준으로 한다.

```text
┌─ Tool name ─────────────────────────────────────┐
│ [Target ▼]   [Category tabs]                    │
│                                                 │
│ Label / unit                    Editable value  │
│ Label / unit                    Editable value  │
│                                                 │
│ [Primary Action] [Secondary]   One-line status │
└─────────────────────────────────────────────────┘
```

1. 제목은 구현 컴포넌트명이 아니라 사용자 작업을 말한다.
2. 대상과 카테고리는 본문보다 먼저 둔다. 현재 선택은 항상 보인다.
3. 본문에는 현재 카테고리의 필수 값만 한 번씩 보여준다.
4. 하단에는 Primary action 1개, 일상적인 Secondary action 최대 1개, 한 줄 상태만 둔다.
5. 긴 연결 설명, 권위 설명, command sequence, netId, raw path는 기본 화면에서 제거하고 필요하면 tooltip 또는 Diagnostics에 둔다.
6. 본문만 스크롤하고 대상 선택과 action/status는 가능한 한 고정한다.

Debug Observer는 `한 줄 요약 -> 핵심 상태 -> 시각화 토글 -> Diagnostics` 순서, Workflow Editor는 `도구막대 -> 캔버스 -> 선택 속성 -> 상태` 순서를 쓴다.

## 5. 행동 예산과 Progressive Disclosure

- Tuner의 routine mutation 흐름은 Primary action 하나를 중심으로 한다. 예: `Save & Hot Load`.
- Tuner의 일상적인 Secondary action은 최대 하나다. 예: `Reload Draft`.
- Debug Observer는 mutation Primary가 없어도 된다. 핵심 관측 토글만 기본 화면에 두고 개입 행동이 필요하면 별도 Tuner로 분리한다.
- Workflow Editor는 undo/redo/save/preview와 도메인 canvas 명령을 보존한다. 현재 모드의 commit/export 한 개만 시각적으로 강조하며, 행동 예산을 이유로 필수 편집 명령을 삭제하지 않는다.
- 서로 반대인 두 버튼보다 단일 toggle/checkbox가 명확하면 하나로 합친다.
- 같은 값을 수정하는 표, quick override, live override를 동시에 두지 않는다. 편집 경로는 하나다.
- 파괴적·드문 행동은 기본 action bar에서 분리하고 확인 절차를 둔다.
- 고급 옵션은 사용 빈도가 낮고 기본 작업 없이도 안전할 때만 접는다. 필수 필드를 `Advanced`에 숨기지 않는다.
- “All” 카테고리는 다른 카테고리를 그대로 중복하면 만들지 않는다.
- 내부 상태를 보여주기 위한 버튼을 사용자의 핵심 행동보다 앞에 두지 않는다.

## 6. 데이터 범위와 컨트롤 선택

- 선택 목록은 canonical registry/data에서 만든다. 손으로 적은 목록은 canonical source와 parity gate가 있을 때만 허용한다.
- 사용자가 전체 roster/object family 편집을 요청하면 현재 플레이어나 현재 선택 객체로 범위를 축소하지 않는다.
- 필드 라벨은 사용자 용어를 쓴다. 원시 멤버명은 tooltip/Diagnostics에만 둔다.
- 수치에는 단위와 의미를 붙인다. 비율은 `1.0 = 100%`인지 `%` 입력인지 한 방식으로 통일한다.
- 사용자가 slider 조작을 요청하면 `InputFloat`를 그대로 남기지 않는다. `SliderFloat`/`SliderScalar`와 보이는 최소·최대·단위를 제공하고 Ctrl+클릭 직접 입력 가능 여부를 함께 검증한다.
- UI 조작 범위와 데이터 안전 검증 범위는 다르다. 예를 들어 `-1,000,000~1,000,000` validation guard를 slider 범위로 재사용하지 않는다. canonical 데이터 분포와 도메인 의도로 field별 조작 범위를 정하고, 근거가 없으면 임의 범위를 만들지 말고 `CONFIRM_NEEDED`로 둔다.
- 넓은 양수 범위는 logarithmic slider나 단계/범위 전환을 검토한다. `DragFloat`는 slider 요구를 몰래 대체하지 않으며 사용자가 허용할 때만 쓴다.
- Q/W/E/R처럼 랭크 수가 다른 데이터는 실제 랭크 수를 보존한다. scalar를 임의로 전체 랭크에 복제했다면 그 규칙을 화면과 저장 계약에 명시한다.
- 각 행은 `PushID` 또는 `##stable_id`로 안정적인 ImGui ID를 갖는다. 보이는 라벨에 내부 ID를 노출하지 않는다.
- 초기 창 크기는 `ImGuiCond_FirstUseEver`로 제공하고, 작은 해상도에서도 action/status가 잘리지 않게 body child 영역을 설계한다.

## 7. Draft, Save, Hot Load의 상태와 권위 모드

Mutation 도구는 최소한 다음 상태를 구분한다.

| 상태 | 의미 | 기본 UI 행동 |
|---|---|---|
| `clean` | Draft와 source/applied가 같음 | 저장 비활성 가능 |
| `dirty` | 저장되지 않은 유효 변경 | Primary 활성, reload/discard 시 확인 |
| `invalid` | 검증 실패 | 적용 비활성, 원인 표시 |
| `stale` | 로드 뒤 source가 외부 변경됨 | 덮어쓰기 금지, 다시 로드 안내 |
| `saving/applying` | 트랜잭션 진행 중 | 중복 실행 비활성 |
| `ack-timeout/offline/read-only` | owner 확인 불가 또는 변경 불가 | truth 성공 표시 금지, 재시도/다음 행동 안내 |
| `succeeded/failed` | 확인된 최종 결과 | revision 또는 실패/롤백 원인 한 줄 표시 |

`Reload Draft`나 창 닫기가 dirty 입력을 버리면 확인한다. 실패 뒤에는 컨트롤을 안전한 상태로 다시 활성화하고, 부분 저장/부분 적용은 성공으로 표시하지 않는다.

권위 흐름은 도구 목적에 맞는 아래 세 모드 중 하나를 계획서에 선언한다.

### A. Gameplay practice overlay

세션 한정 override다. `Draft -> typed command -> Server/GameSim -> snapshot/revision ack`를 따르며 canonical 파일을 바꾸지 않는다.

### B. Canonical gameplay authoring

`Canonical source -> Draft -> validate -> atomic persist -> cook/SimLab/build`가 기준이다. Debug server reload가 지원되면 저장 뒤 별도 authoritative reload/ack로 관측할 수 있지만 Client가 canonical gameplay truth를 직접 만들지 않는다.

Canonical gameplay 수치의 최신 저장값은 사용자가 승인한 authoring truth다. 회귀 테스트는 기본적으로 schema·domain·rank shape·canonical/generated parity를 고정하고, 사용자가 명시적으로 동결하지 않은 editable exact value를 고정하거나 과거 문서 값으로 복원하지 않는다.

### C. Presentation / asset authoring

UI layout, WFX, material 같은 owning Client/Editor asset을 저장하고 해당 owner의 reload/revision으로 확인한다. 의미 없는 Server/GameSim command나 가짜 ack를 만들지 않는다.

`Save & Hot Load`는 지원되는 Tuner에서 사용자가 이해하는 하나의 행동 예시다. 내부적으로 검증, stale-source 검사, 원자적 저장, 올바른 owner 적용, revision 확인 순서를 지킨다. Release에서 지원하지 않으면 비활성 이유와 필요한 cook/build 행동을 한 줄로 말한다.

## 8. 코드 소유권과 구조

- ImGui는 Client/Tools의 render·interaction shell이다. Shared/GameSim과 Server는 ImGui/DX/UI 타입을 include하지 않는다.
- panel render 함수는 월드 truth를 즉석에서 탐색하지 않는다. Client bridge가 만든 view state와 draft/action interface를 받는다.
- panel render에서 매 프레임 disk JSON을 파싱하거나 full-world discovery를 수행하지 않는다. 관측 수집 주기와 캐시는 bridge/view-state owner가 소유한다.
- gameplay 결과는 `Client input -> typed command -> Server/GameSim -> Snapshot/Event/Ack -> Client status` 흐름을 지킨다.
- Scene의 `OnImGui`는 단축키와 panel dispatch만 소유한다. 큰 도구 상태, JSON transaction, validation을 Scene 함수에 쌓지 않는다.
- 공용 Engine ImGui helper는 제품 의미를 모른다. 챔피언, 미니언, AI 같은 의미는 Client 제품 도구가 소유한다.
- 관측용 상태와 변경용 draft를 다른 타입/이름으로 구분한다. `current`, `temp`, `override`만으로 수명을 숨기지 않는다.

## 9. 문구와 피드백

- 화면 문구는 짧고 행동 중심으로 쓴다: `Saved and applied (rev 42)`처럼 결과와 revision을 한 줄로 표현한다.
- 사용자가 해결할 수 없는 내부 설명을 상단에 상시 노출하지 않는다.
- 실패 문구는 무엇이 실패했고 다음에 무엇을 해야 하는지 말한다.
- 색은 의미가 있을 때만 쓴다: 성공, 경고, 실패. 계층을 색상만으로 표현하지 않는다.
- 도움말은 긴 본문 대신 짧은 tooltip을 쓴다. 핵심 사용법은 도움말 없이도 보여야 한다.

### 비활성·재로드 컨트롤 계약

- 비활성 Primary 옆에는 **현재 차단 predicate와 같은 source**에서 만든 한 줄 이유와 사용자가 취할 다음 행동을 표시한다. generic `Cannot apply`나 서로 다른 원인을 `Connect as host` 하나로 뭉치지 않는다.
- Debug build, authoritative scene, serializer, network connection, room-host permission처럼 조건이 여럿이면 최초 실패 이유를 구분한다. Client가 검사하지 않는 서버 host 권한을 Client-side 비활성 이유로 단정하지 않는다.
- 저장은 가능하지만 Hot Load만 불가능한 상태처럼 결합 행동의 capability가 갈리면, 유효한 저장까지 함께 막지 않는다. Primary/Secondary 예산 안에서 `Save`, `Save & Hot Load` 또는 명확한 fallback을 설계한다. Release 정책이 저장과 Hot Load 모두를 의도적으로 금지하면 제한을 우회하지 말고 현재 모드와 Debug/cook/build 다음 행동을 설명한다.
- `Reload Draft`처럼 내부 수명 용어만 쓰지 않는다. 실제로 disk/source를 다시 읽고 dirty 입력을 버린다면 clean 상태는 `Reload from Disk`, dirty 상태는 `Discard Changes & Reload`처럼 결과를 드러내며 확인한다.
- tooltip만으로 비활성 이유를 숨기지 않는다. disabled control은 hover가 불안정할 수 있으므로 기본 화면의 인접 status를 사용한다.

## 10. 금지 사례

- 필수 데이터는 줄였는데 연결 상태, authority 문장, sequence 번호, enable/disable 버튼은 남은 화면.
- 같은 값을 `Override Table`과 `Live Balance`에서 두 번 수정하는 화면.
- 전체 챔피언 편집 도구가 `Current champion`만 지원하는 화면.
- 모든 멤버를 소유권·빈도 구분 없이 세로로 나열한 화면.
- `Load JSON`, `Save JSON`, `Apply`, `Reload Server`, `Enable Session`을 각각 기본 버튼으로 노출해 사용자 트랜잭션을 구현 단계로 분해한 화면.
- 읽기 전용 AI Debug에 개입 버튼과 raw ECS dump를 기본 노출한 화면.
- 빌드만 통과하고 작은 화면, HUD 겹침, 잘림, 첫 행동의 명확성을 확인하지 않은 완료 판정.
- 단순화를 이유로 사용자가 요청한 대상, 필드, 랭크, 저장 의미를 삭제하는 것.

## 11. 도메인별 기준 예시

### Balance Tuner

`Champion/Skills/Minions/Towers`처럼 사용자의 대상 분류를 그대로 쓰고, 대상 선택과 필드 편집을 한 경로로 제공한다. Hot Load 가능 시 기본 행동은 `Save & Hot Load`다. source 재로드는 clean/dirty에 따라 `Reload from Disk` 또는 `Discard Changes & Reload`로 의미를 드러낸다. Debug/authority/serializer/network/host 조건은 실제 검사 owner별로 분리해 차단 이유를 말한다. 서버 연결의 raw 세부 정보는 실패 진단이 필요할 때만 연다.

### AI Debug

기본 화면은 선택 AI의 현재 목표, 상태, 핵심 의사결정 이유, 경로/전투 신호를 요약한다. perception dump, raw score table, command history는 Diagnostics에 둔다. AI 변경 도구가 필요하면 Observer와 Tuner를 분리한다.

### UI Manager / Layout Tuner

기본 화면은 widget 선택, position/size/anchor, `Save`, `Reload`에 집중한다. texture handle, atlas internals, renderer state는 Inspector/Diagnostics로 분리한다. 실제 HUD와 동시에 보며 이동 결과를 확인할 수 있어야 한다.

### WFX Editor

그래프/타임라인/속성은 작업의 본질이므로 유지한다. 단, 동일 속성의 중복 편집기, 상시 raw resource 진단, 작업과 무관한 전역 버튼은 제거한다. “단순함”을 노드 기능 삭제로 해석하지 않는다.

## 12. 계획서와 서브 에이전트 비평 게이트

ImGui 기능을 추가·변경하는 dated PLAN에는 다음이 반드시 있어야 한다.

1. §2의 사용자 작업 계약.
2. 필수 데이터 범위표와 제외 범위.
3. 기본 화면 ASCII wireframe.
4. Primary/Secondary action 예산.
5. view/draft/persist/apply/ack owner.
6. 기존 패널에서 삭제·통합되는 중복 경로.
7. 실제 화면 검증 경로와 예상 캡처.

독립 서브 에이전트 비평은 기술 안전성뿐 아니라 아래 질문에 답해야 한다.

1. 사용자 요청의 대상·필드·행동이 하나라도 빠졌는가?
2. 내부 구현 정보가 사용자 작업보다 먼저 보이는가?
3. 같은 값을 바꾸는 경로가 둘 이상인가?
4. Client tool과 Server/GameSim 권위 경계가 맞는가?
5. 처음 보는 사용자가 10초 안에 첫 행동을 찾을 수 있는가?

주 에이전트는 각 지적을 수용/기각/보류하고 이유를 PLAN에 기록한다. “더 단순하게”라는 비평은 어떤 필수 기능을 보존하고 어떤 화면 잡음을 제거하는지 구체화되지 않으면 수용하지 않는다.

미해결 P0/P1은 구현을 차단한다. P2는 근거와 후속 조건을 기록하고 보류할 수 있다. P0/P1 때문에 계획의 범위·권위·화면 계약이 실질적으로 바뀌면 같은 비평 주체 또는 다른 독립 서브 에이전트가 수정본을 다시 확인해야 한다.

## 13. 완료 게이트

아래 7개 중 하나라도 실패하면 ImGui 작업은 완료가 아니다.

1. **목적**: 창을 보고 10초 안에 사용자 작업과 첫 행동을 말할 수 있다.
2. **범위**: 요청된 대상·필드·랭크·행동이 범위표와 UI에 모두 존재한다.
3. **계층**: §4의 유형별 계층(Tuner, Observer, Workflow Editor)이 화면에서 보인다.
4. **행동 경제**: 유형별 적용표를 지키며 중복 mutation 경로가 없다. Tuner의 action 예산을 Observer/Workflow에 기계 적용하지 않는다.
5. **권위/정합성**: 선언한 authority mode의 source/draft/apply/revision 상태가 구분되고 올바른 owner를 통과한다.
6. **자동 검증**: build, data/schema/contract test, dirty diff check가 통과한다.
7. **수동 시각 검증**: 유형별 적용표를 따른다. Tuner는 편집·적용 성공/실패, Observer는 normal/empty/error/freshness, Workflow Editor는 편집·undo/redo·save/reload를 실제 실행 환경에서 확인하고 잘림/HUD 겹침/스크롤/핵심 action을 캡처한다.

수동 시각 검증을 하지 못했다면 결과서에 `미검증`으로 남긴다. 빌드 통과만으로 “깔끔하다”, “직관적이다”, “화면 검증 완료”라고 쓰지 않는다.

각 ImGui PLAN은 수동 검증의 기준 executable/scene/shortcut, 지원 최소 해상도와 DPI, 성공 상태와 최소 1개 실패·empty 상태의 캡처 경로, RESULT에 남길 artifact 파일명을 미리 쓴다. 해당 제품에 공식 최소 해상도/DPI가 정해지지 않았다면 임의 숫자를 만들지 말고 `CONFIRM_NEEDED`로 남긴다.

## 14. 구현 순서

```text
1. 사용자 작업 계약과 범위표를 쓴다.
2. 기존 화면의 필수/중복/진단 요소를 분류한다.
3. ASCII wireframe과 action 예산을 계획서에 고정한다.
4. 독립 서브 에이전트가 목표 충실도와 권위 경계를 비평한다.
5. 한 편집 경로와 최소 shell로 구현한다.
6. 자동 검증 뒤 실제 실행 화면에서 10초 테스트와 전체 작업을 수행한다.
7. RESULT에 캡처 경로, 미검증, 남은 진단 경로를 기록한다.
```

이 가이드의 성공 기준은 패널이 작아지는 것이 아니다. 사용자가 원한 모든 작업을 더 적은 판단과 더 적은 클릭으로 안전하게 끝내는 것이다.
