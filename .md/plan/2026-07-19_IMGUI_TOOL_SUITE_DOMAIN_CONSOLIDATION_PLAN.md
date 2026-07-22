Session - Winters 런타임 ImGui 도구를 F1 단일 허브와 도메인별 페이지로 통합하고 디자이너·기획자 인수인계 수준으로 정리
좌표: 신규 좌표 후보 · 축 C3 공유물 비일치, C5 예산·절차, C7 권위·통합
관련: 2026-07-19_IMGUI_TOOL_PRODUCT_DESIGN_GUIDE_PLAN.md / 2026-07-18_F4_BALANCE_TUNER_DAMAGE_RESPAWN_HUD_PLAN.md / WINTERS_LIVE_AUTHORING_CHRONOBREAK_ARCHITECTURE.md

## 1. 결정 기록

① 문제·제약: LoL 인게임 도구가 F1~F12, 숫자 7/8/9, M에 분산되고 F10/9는 최대 8개 창을 동시에 열며, 저장·세션 적용·서버 Hot Load가 같은 말로 노출된다. WFX 내부와 F7 진입은 이번 범위에서 변경하지 않는다.
② 추진·대안·실패: 최종 진입을 `F1 Winters Tools` 하나로 통합한다. F10 유지나 모든 기존 단축키 상시 호환은 기억 비용과 중복 렌더를 남기므로 최종안이 아니다.
③ 메커니즘: 단일 visibility owner와 도메인/page registry가 한 번에 한 페이지만 그린다. 각 페이지는 `대상 → 핵심 수치/관찰 → 정확한 저장·적용 행동 → 결과` 순서를 공유하되 권위와 persistence는 합치지 않는다.
④ 구조·권위: Hub는 routing/view/draft shell만 소유한다. Gameplay truth는 Server/GameSim, presentation은 Client, generic UI/profiler는 Engine, WFX document는 기존 WFX 도구가 계속 소유한다.
⑤ 대가·가역성: 한 번에 전부 갈아엎지 않고 5개 code slice와 1개 handoff slice로 parity를 잠근다. 필드·버튼·side effect 단위 대체 경로가 증명되지 않은 기능은 삭제하지 않으며, 각 slice는 별도 비평·빌드·수동 캡처 후 다음으로 간다.

## 1-1. 이번 문서의 범위와 성공 기준

이 문서는 **계획 전용 master plan**이다. 이번 세션에서 C++/JSON/project file을 수정하거나 빌드하지 않는다. 이 문서의 §2 code shape는 patch로 복사할 수 있는 implementation packet이 아니다. Slice 1~6은 각각 최신 active body를 다시 인용한 별도 dated implementation plan과 독립 비평을 만든 뒤에만 source edit를 시작하고, master `_RESULT.md`에는 각 slice 결과 링크를 누적한다.

성공 기준:

1. 사용자가 외워야 하는 일반 도구 진입키는 최종적으로 `F1` 하나다.
2. `F7 WFX Effect Tool`은 내부 UX, 파일, 저장 동작, 직접 진입키 모두 그대로다.
3. 한 시점에 Hub 한 창과 선택된 한 페이지만 보이며 F10처럼 여러 창이 폭발하지 않는다.
4. 모든 활성 도구가 도메인·사용자·권위·저장 위치·Release/Debug 조건 중 하나에 빠짐없이 매핑된다.
5. `Save`, `Apply`, `Reload`는 실제 동작과 일치하는 표준 동사로 바뀌고 비활성 사유가 버튼 옆에 직접 보인다.
6. 디자이너·기획자가 코드 설명 없이 2~3분 안내와 handbook만으로 Balance, AI, HUD, Visual, Replay 도구의 기본 작업을 수행한다.
7. 숫자 7/8/9는 인게임 도구에서 완전히 회수되고 inventory 1~6에는 영향이 없다.
8. WFX tool source와 기존 `.wfx`/manifest resource에는 의도하지 않은 diff가 없어야 한다.
9. active surface의 필드·버튼·관찰값·파일 write·server/client side effect는 parity ledger에서 정확히 한 대체 경로를 가진다. 동등 경로가 없으면 기존 기능을 유지한다.

범위에 포함:

- LoL Client 인게임 개발 도구와 이를 여는 Engine 전역 hotkey F3/F11/F12.
- 현재 도달 가능한 패널, 구현됐지만 진입이 끊긴 Minimap/Status/Combat/Map 패널, 상시 Perf HUD.
- 도메인 정보 구조, 공통 화면 계약, authority/persistence 표시, migration/cutover, handbook과 수동 visual QA.
- M으로 들어가는 LoL World Editor는 Hub에서 위치를 안내하되 이번 통합에서 scene/editor 내부는 변경하지 않는다.

범위에서 제외:

- `WfxEffectToolPanel.h/.cpp`, WFX document/runtime/save format, F7 동작 변경.
- 로그인·로딩·게임 선택·밴픽·게임 종료·MyInfo Replay Library 같은 제품 UI.
- EldenRingClient/EldenRingEditor/LoLEditor의 자체 shell 재설계. 공통 가이드만 후속 적용한다.
- 새로운 gameplay tuning 항목, Chrono 기능 복원, AI trace schema 확장, UI pipeline 재작성.
- F4 Balance backend의 진행 중 slider/Reload Draft/Hot Load 교정. 통합은 그 세션의 확정 surface를 소비한다.

## 1-2. 현재 코드 근거와 핫키 권위표

정적 조사 기준 핵심 owner:

- Client 도구 routing: `Client/Private/Scene/Scene_InGameImGui.cpp:38-182`
- Client visibility state: `Client/Public/Scene/Scene_InGame.h:265-273`
- Engine profiler/limiter/capture: `Engine/Private/Framework/CEngineApp.cpp:324-334`
- Camera mode: `Client/Private/DynamicCamera.cpp:32`

| 입력 | 현재 실제 동작 | 최종 위치 | 최종 키 정책 |
|---|---|---|---|
| F1 | Render Debug | `Diagnostics > Debug Draw`, `Visuals > Rendering` | **Winters Tools Hub toggle로 교체** |
| F2 | Follow/Free Camera | `Diagnostics > Camera` | 1개 migration slice 동안 redirect 안내 후 제거 |
| F3 | Profiler | `Diagnostics > Performance` | migration redirect 후 제거 |
| F4 | Balance | `Balance` | migration redirect 후 제거 |
| F5 | Attack Speed Lab | `Visuals > Basic Attack Timing` | migration redirect 후 제거 |
| F6 | Replay Control | `Replay & QA` | playback 강제 표시는 유지, 수동 shortcut은 제거 |
| F7 | WFX Effect Tool | 별도 protected tool | **현행 그대로 유지** |
| F8 | UI Manager | `UI & HUD` | migration redirect 후 제거 |
| F9 | AI Debug | `AI & Simulation` | migration redirect 후 제거 |
| F10 | legacy multi-window bundle | 여러 도메인으로 분해 | 모든 page parity 뒤 Slice 5에서 multi-window 제거/redirect |
| F11 | frame limiter toggle | `Diagnostics > Performance` | migration redirect 후 제거 |
| F12 | profiler JSON capture | `Diagnostics > Performance` | migration redirect 후 제거 |
| 숫자 7 | Model & Animation Lab | `Visuals > Model Preview` | Slice 5 cutover에서 alias 제거 |
| 숫자 8 | Attack Speed Lab alias | `Visuals > Basic Attack Timing` | Slice 5 cutover에서 alias 제거 |
| 숫자 9 | F10 alias | Hub | Slice 5 cutover에서 alias 제거 |
| M | legacy World Editor scene | `World & Navigation`에서 안내 | 이번 범위에서는 현행 유지 |

추가 사실:

- F1과 F10/9를 동시에 열면 `CRenderDebugPanel::Render()`가 한 프레임에 두 번 호출될 수 있다.
- 숫자 7/8/9만 `WantCaptureKeyboard`를 존중하고 F1~F10/M은 그렇지 않아 입력 안전성도 불일치한다.
- F3/F11/F12는 Engine 전역이라 LoL 외 제품까지 영향을 준다. 최종 제거는 LoL Hub bridge를 먼저 만든 뒤 Engine 제품별 policy를 분리해야 한다.
- WFX의 유일한 직접 진입은 F7이다. 이번 계획은 이 경로를 보존한다.

## 1-3. 현재 패널 전수 분류와 처분

| 현재 surface | 현재 진입 | 최종 도메인/page | 처분 |
|---|---|---|---|
| Balance / `CChampionTuner` | F4, F10/9 | Balance / Champions·Skills·Minions·Towers | backend와 전체 편집 범위 유지, shell만 통합 |
| Attack Speed Lab | F5, 8 | Visuals / Basic Attack Timing | gameplay AS 관찰과 visual correction을 명시적으로 분리 |
| AI Debug | F9 | AI & Simulation / Overview·Runtime Tuning | observer와 mutation 구역 분리, typed server command 유지 |
| UI Manager | F8 | UI & HUD / Actor HUD·Health Bars·Cursor | Status/Minimap과 같은 도메인에 배치, persistence는 개별 표기 |
| Model & Animation Lab | 7 | Visuals / Model Preview | preview close/reset 계약 추가, BA/Skill timing과 충돌 제거 |
| Replay Control | F6 | Replay & QA | recording/playback 상태에 따라 같은 페이지 내용 전환 |
| Render Debug | F1, F10/9 | Visuals / Rendering + Diagnostics / Debug Draw | tuning과 debug draw를 분리, 중복 Render 제거 |
| Effect Tuner — Irelia | F10/9 | Visuals / Champion FX | live preset/test/clipboard/manifest/`.wfx` dump·load·spawn을 `Legacy Export & Preview` Advanced에 모두 보존; WFX 도구 내부는 변경하지 않음 |
| Skill Timing Tuner | F10/9 | Visuals / Skill Timing | gameplay cooldown과 visual timing을 명시적으로 구분 |
| Network Event Trace | F10/9 | Diagnostics / Network | observer로 통합, ring buffer owner 유지 |
| Camera + Camera Debug | F10/9, F2 | Diagnostics / Camera | 두 창을 한 페이지로 합치고 mode action도 여기 배치 |
| Minion Tuner | F10/9 | World & Navigation / Legacy Local Preview | local-only 경고와 함께 모든 기존 control을 Advanced에 보존; M editor와 field/action parity가 증명되기 전 삭제 금지 |
| Profiler + Perf HUD | F3/F12, 상시 | Diagnostics / Performance | 한 page owner, compact HUD opt-in |
| Frame limiter | F11 | Diagnostics / Performance | 상태와 target FPS를 보이는 명시적 control로 전환 |
| Structure Tuner | 진입 없음 | Balance / Towers > Live Session QA | replicated live summary, turret/inhibitor/nexus batch override, Low HP, Restore를 모두 보존; canonical F4 Towers와 역할을 구분 |
| Minimap Layout | 진입 없음 | UI & HUD / Minimap | active runtime owner와 연결 후 정식 진입 제공 |
| Status Panel Layout | 진입 없음 | UI & HUD / Status Panel | local developer persistence를 명확히 표시하고 정식 진입 제공 |
| Combat Debug | 사실상 진입 없음 | Diagnostics / Combat | 일반 관찰만 통합, Sylas 전용 test는 Advanced로 격리 |
| Map Rotation Tuner | 사실상 진입 없음 | World & Navigation / Map Preview | local preview임을 표시하고 저장 기능으로 오인하지 않게 함 |
| CScene_Editor | M | World & Navigation / World Editor 안내 | full-scene workflow 유지, Hub에 설명/진입 위치만 제공 |
| WFX Effect Tool | F7 | Protected separate tool | **파일·UI·저장·키 전부 변경하지 않음** |

활성 기능으로 계산하지 않을 것:

- `AIDebugPanel.cpp`, `ChampionTuner.cpp`, `UI_Manager.cpp` 안의 `#if 0` 레거시 all-in-one 구현.
- 비활성 Chrono Decision Timeline, 옛 Practice Lab, 옛 Lua/raw HUD editor.
- 위 코드는 이번 기능 통합에서 복원하거나 삭제하지 않는다. disabled body 정리는 별도 사용자 승인과 별도 cleanup plan에서만 진행한다.

### 1-3-A. 기능 축소 방지 parity ledger

Slice 0/1 시작 전에 모든 active surface를 `control ID / label / input·output / owner / side effect / persistence / replacement page / proof` 단위로 추출한다. 최종 ledger에서 `replacement page` 또는 `explicitly retained standalone`이 비어 있는 row가 하나라도 있으면 해당 wrapper/file/caller를 삭제할 수 없다. 사용자가 별도로 승인하지 않은 deprecation은 금지한다.

현재 코드에서 이미 확인된 고위험 ledger:

| Source surface | 필드·행동·관찰 | side effect/owner | 최종 대체 경로 | 삭제 gate |
|---|---|---|---|---|
| Structure Tuner | Turret/Inhibitor/Nexus Max HP, Turret AD | typed practice structure override / Server | Balance > Towers > Live Session QA | 4개 override 결과 snapshot 확인 |
| Structure Tuner | Apply To Server | 4개 batch command | 같은 page `Apply Session` | command accept + HP/AD 관찰 |
| Structure Tuner | Low HP Preset | 300/400/550 HP batch apply | 같은 page `Low HP QA Preset` Advanced | nexus 파괴 QA parity |
| Structure Tuner | Restore Defaults | clear server overrides | 같은 page `Reset Session` | definition 값 복귀 확인 |
| Structure Tuner | replicated live summary | kind별 alive/dead/min/max HP observer | 같은 page read-only summary | table parity screenshot |
| Minion Tuner | Enabled, Spawn Interval, Visual Scale, Reset Scale | legacy Client minion manager/session | World > Legacy Local Minion Preview | local-only badge + live visual parity |
| Minion Tuner | Spawn Wave, Clear All | legacy Client ECS mutation | 같은 page Advanced | server-authoritative room에서 차단, local preview smoke |
| Minion Tuner | Reset Defaults, 6 lane waypoint lists, add/edit | Client manager + Stage persistence owner | 같은 page Advanced 또는 M editor | 필드·roundtrip 동등성 증명 전 원본 유지 |
| Effect Tuner | Irelia preset, material/atlas/mesh fields, Q/W/E/R tuning | Client FX live state | Visuals > Champion FX | 모든 slider/combo parity screenshot |
| Effect Tuner | Spawn Test | Client FX preview spawn | 같은 page `Spawn Preview` | preset별 spawn smoke |
| Effect Tuner | Save Preset Clipboard | clipboard source export | 같은 page Advanced | clipboard text equality |
| Effect Tuner | Dump EFX-0 Manifest | legacy manifest file write | 같은 page Advanced | path/content roundtrip |
| Effect Tuner | Dump Current `.wfx` | legacy WFX file write | 같은 page Advanced | output hash/schema/load parity |
| Effect Tuner | Load `.wfx` + Spawn | WFX asset load + Client preview | 같은 page Advanced | load error/success and spawn parity |

Balance, AI, UI, Attack Speed, Skill Timing, Model, Render, Network, Camera, Replay, Profiler도 같은 ledger를 작성한다. 위 표는 위험 기능의 선결 예시이지 전체 ledger를 대신하지 않는다.

## 1-4. 최종 정보 구조

```text
Winters Tools [F1]
├─ DESIGN
│  ├─ Balance
│  │  ├─ Champions
│  │  ├─ Skills
│  │  ├─ Minions
│  │  └─ Towers
│  │     ├─ Definition Draft
│  │     └─ Live Session QA
│  ├─ AI & Simulation
│  │  ├─ Overview
│  │  └─ Runtime Tuning
│  ├─ UI & HUD
│  │  ├─ Actor HUD
│  │  ├─ Health Bars
│  │  ├─ Status Panel
│  │  ├─ Minimap
│  │  └─ Cursor
│  ├─ Visuals & Animation
│  │  ├─ Rendering
│  │  ├─ Basic Attack Timing
│  │  ├─ Skill Timing
│  │  ├─ Model Preview
│  │  └─ Champion FX
│  ├─ World & Navigation
│  │  ├─ Map Preview
│  │  ├─ Navigation Diagnostics
│  │  ├─ Legacy Local Minion Preview
│  │  └─ World Editor Info (M)
│  └─ Replay & QA
│     ├─ Recording
│     └─ Playback / Chrono
├─ DIAGNOSTICS
│  ├─ Combat
│  ├─ Debug Draw
│  ├─ Network
│  ├─ Performance
│  └─ Camera
└─ PROTECTED SEPARATE TOOL
   └─ WFX Effect Tool [F7] — unchanged
```

이 구조에서 Balance의 `Attack Speed`는 **base/growth game data**, Visuals의 `Basic Attack Timing`은 **실효 공속 검증과 visual cadence/correction**이다. 같은 이름의 필드를 복제하지 않고 서로의 위치를 링크한다.

단일 창 원칙의 명시적 예외는 두 개뿐이다.

1. WFX는 사용자 지시대로 F7 별도 창을 유지한다.
2. replay playback/stop-pending transport는 현재 게임 하단 고정 제어의 안전성을 유지하기 위해 Hub와 독립된 필수 overlay로 남긴다. Hub의 Replay page는 recording과 상세 설명/상태 owner이며, playback transport가 Hub page를 강제로 빼앗거나 Hub를 재개방하지 않는다.

## 1-5. 공통 화면 계약

```text
┌ Winters Tools [F1] ───────────────────────────────────────────────┐
│ DEBUG CLIENT | SERVER AUTHORITY: CONNECTED | TOOL REVISION 42    │
├─────────────────────┬─────────────────────────────────────────────┤
│ DESIGN              │ Balance > Skills                            │
│  Balance            │ 챔피언 스킬 피해·계수·쿨다운을 조정합니다. │
│  AI & Simulation    │ Scope: Canonical JSON + Debug Server        │
│  UI & HUD           │                                             │
│  Visuals/Animation  │ [Champion ▼] [Slot Q/W/E/R]                 │
│  World/Navigation   │                                             │
│  Replay & QA        │ 핵심 편집 내용                              │
│ DIAGNOSTICS         │                                             │
│  Combat             │                                             │
│  Debug Draw         │                                             │
│  Network            │                                             │
│  Performance        │                                             │
│  Camera             │                                             │
│ ─────────────────── │ [Reload Draft] [Save & Hot Load]           │
│ WFX [F7] separate   │ Status: Saved and acknowledged by server   │
└─────────────────────┴─────────────────────────────────────────────┘
```

모든 page header는 다음 세 가지를 한 줄로 보여 준다.

- 무엇을 바꾸는가: 예) `Champion base/growth stats and skill formulas`
- 언제 쓰는가: 예) `Debug authoritative room balance pass`
- 반영 범위: `Session only`, `Local file`, `Canonical source`, `Debug server acknowledged`, `Release build required`

공통 동사 사전:

| 표준 label | 정확한 의미 | 금지되는 오해 |
|---|---|---|
| Reload Draft | 외부 source에서 다시 읽고 미저장 편집을 폐기 | 런타임 적용 아님 |
| Save Draft | 파일만 저장 | 서버/현재 entity 적용 아님 |
| Apply Session | 현재 세션 override만 변경 | canonical 저장 아님 |
| Save & Hot Load | 원자 저장 후 authority reload와 revision ack까지 확인 | Release에서 사용 불가 |
| Reset Session | 세션 override 해제 | source 파일 삭제/초기화 아님 |
| Restore Defaults | 정의된 default를 draft에 적재 | 자동 저장/적용 아님 |

공통 상태/안전 규칙:

1. 환경 badge는 `DEBUG/RELEASE`, connection, host/authority, source dirty, server ack를 항상 같은 위치에 표시한다.
2. 비활성 버튼은 tooltip에 숨기지 않고 바로 옆에 정확한 사유를 쓴다. 예: `Release Client: save is allowed only through cook + Release build`.
3. dirty 상태에서 Reload/close/page switch가 데이터를 버리면 같은 확인 dialog를 사용한다.
4. 슬라이더는 실용 범위를 제공하되 직접 입력과 validation 범위를 분리한다. 필수 수치를 제거해 단순해 보이게 만들지 않는다.
5. 기본 화면에는 한 job의 핵심 action 최대 2개만 노출한다. 진단 raw rows와 특수 reset/test는 `Advanced` 아래로 내린다.
6. page switch는 이전 preview/session mutation의 close/reset 계약을 실행한다. gameplay server override는 명시적 Reset 전까지 남는지 header에 표시한다.
7. Hub는 gameplay 값을 직접 쓰지 않는다. 기존 typed command, JSON writer, renderer/UI owner API를 호출하고 결과를 표시한다.

Balance의 실제 반영 흐름은 축약하지 않고 다음 순서로 표시한다.

```text
Client draft/editor
  → atomic canonical JSON writer (3 files)
  → Debug host reload command
  → Server/GameSim runtime truth
  → snapshot toolRevision acknowledgement
```

따라서 Release에서는 `Save & Hot Load`를 단순히 “권한 없음”으로 표시하지 않는다. 현재 build가 Release라 Debug host reload/ack 계약을 실행할 수 없고, canonical 변경을 제품에 넣으려면 data cook + Release build가 필요하다는 이유를 같은 자리에서 보여 준다.

## 1-6. 권위·저장 매트릭스

| 도메인/page | 진실 owner | 저장 위치/수명 | 적용/확인 |
|---|---|---|---|
| Balance | Client draft/writer → 3 canonical JSON → Server/GameSim truth | 세 canonical JSON | Debug host reload command + snapshot `toolRevision` ack; Release는 cook/build 필요 |
| AI Overview | Server snapshot | 저장 없음 | read-only snapshot |
| AI Runtime Tuning | Server AI policy override | room/session | typed command accept/reject |
| Actor HUD | Engine UI runtime + layout JSON | Actor HUD layout JSON | local reload/render proof |
| Health Bars/Cursor | Engine UI runtime | session unless owner gains persistence | immediate local apply, explicit `Session only` |
| Status Panel | Engine UI | `%LOCALAPPDATA%` developer layout | local save/reload 표기 |
| Minimap | Client presentation | 현재 memory-only | `Session only` until atomic owner exists |
| Basic Attack Timing | Server effective AS + Client visual correction | practice tuning JSON + session | 두 상태를 별도 row로 관찰 |
| Skill Timing | Client visual registry | practice visual draft | local visual reload, gameplay cooldown 아님 |
| Model Preview | Client animator/renderer instance | session | close/reset 후 원상복구 확인 |
| Champion FX | Client legacy preset/test | session/legacy manifest as applicable | generic `.wfx` writer는 제거하고 WFX로 안내 |
| World/Map Preview | Client preview | session | server stage truth가 아님을 표시 |
| World Editor | editor scene/workspace | Stage/NavGrid | 이번 통합에서는 M workflow 유지 |
| Replay | Server recording / Client playback | replay artifact | stop/save ack 또는 local playback state |
| Network/Combat/Debug Draw | Client observer | session/ring buffer | read-only 또는 local overlay |
| Performance | Engine profiler | profile JSON | capture path와 성공/실패 표시 |
| Camera | Client camera | session | immediate local state |
| WFX | 기존 WFX document | 기존 WFX path | 기존 F7 도구 계약 그대로 |

## 1-7. Hotkey migration과 최종 cutover

### Phase A — page별 원자 migration

- Slice 1은 Hub shell과 Render Debug content extraction을 같이 넣어 F1을 원자적으로 Hub로 바꾼다. 지금 F1이 collider/champion debug draw를 강제로 켜는 부수효과는 폐기하고 Hub visibility와 debug draw enable state를 분리한다.
- Slice 1에서는 F4/F5/F6/F8/F9/F10, 숫자 7/8/9, 기존 replay renderer를 그대로 유지한다. 빈 Hub page나 아직 추출되지 않은 method로 redirect하지 않는다.
- 이후 각 Slice는 `content extraction + lifecycle parity + 해당 shortcut redirect`를 하나의 변경으로 묶는다.
  - Slice 2: F4 Balance, F8 UI & HUD, F9 AI.
  - Slice 3: F5 Attack Speed, F6 Replay recording/detail. playback/stop-pending transport는 같은 Slice에서 별도 overlay로 분리한다.
  - Slice 4: F2 Camera, LoL ApplicationOwned F3/F11/F12.
- F10 multi-window redirect/제거와 숫자 7/8/9 제거는 모든 대응 ledger row가 Hub에서 reachable인 Slice 5 cutover까지 미룬다.
- redirect된 page 상단에는 한 번만 `Moved to F1 > ...` 안내를 표시한다.
- F7은 모든 Slice에서 redirect하지 않고 기존 WFX 창을 그대로 toggle한다.

입력 안전 정책:

- F1과 F7은 명시적 global tool toggle로 유지한다. F7의 현재 동작은 바꾸지 않는다.
- migration redirect, 숫자 7/8/9의 기존 동작과 최종 제거 확인, M scene 전환, F2 camera action은 `ImGui::GetIO().WantCaptureKeyboard`를 존중한다.
- M은 이번에 제거하지 않지만 Hub의 `World Editor Info` page에서 별도 scene/dirty save/복귀 방식을 설명한다.

### Phase B — Engine product shortcut policy를 먼저 도입

현재 F3/F11/F12는 `CEngineApp::Run()`이 모든 제품에서 Scene/App ImGui보다 먼저 직접 처리한다. 따라서 LoL Hub redirect를 먼저 추가하거나 global handler만 삭제하면 안 된다.

1. `EngineConfig`에 `EngineGlobal`(기본) / `ApplicationOwned` developer shortcut policy를 추가한다.
2. Client main만 `ApplicationOwned`를 선택한다. EldenRingClient, EldenRingEditor, LoLEditor는 자체 대체 진입과 runtime smoke가 완료될 때까지 `EngineGlobal`을 유지한다.
3. F11의 loop-local `bLimitFrameRate`를 `CEngineApp` 소유 state/API로 승격하고, profiler capture도 end-of-frame request/ack API로 노출한다.
4. Engine handler skip, GameInstance bridge, LoL Hub 연결을 Slice 4의 한 원자 변경으로 묶는다. 부분 merge를 금지한다.
5. F2는 DynamicCamera action을 Camera page로 옮기고 입력 capture/preview reset smoke가 통과한 같은 slice에서만 제거한다.

### Phase C — 최종안

- 일반 도구 hotkey는 F1 하나다.
- WFX는 F7 그대로다.
- M World Editor는 이번 범위에서 그대로이며 Hub에 위치와 위험(별도 scene, dirty save)을 설명한다.
- F2/F3/F4/F5/F6/F8/F9/F10/F11/F12와 숫자 7/8/9의 도구 alias는 제거된다.
- replay playback/stop pending transport overlay는 hotkey와 Hub visibility에 무관하게 유지한다.
- LoL의 F3/F11/F12 제거는 `ApplicationOwned` policy와 Hub bridge가 동시에 활성화된 뒤에만 완료된다. 다른 제품의 기존 global route는 제품별 대체 진입이 생기기 전까지 보존한다.

## 1-8. 구현 slice와 30% ceiling budget

이번 주제는 ImGui 바닥 공사만 반복한 세 번째 연속 slice다. 따라서 구현 예산의 30%를 실제 사용자 산출물로 고정한다.

| Slice | core code | QA/feedback | 결과 | 독립 종료 조건 |
|---|---:|---:|---|---|
| 1. Inventory freeze + Hub/Render vertical slice | 13% | 2% | F1 Hub, registry, Render content extraction, debug state 분리 | F1 기능 parity, 다른 shortcut 현행 유지, F7 diff 없음 |
| 2. Designer core | 18% | 2% | Balance, AI, UI & HUD page 통합 + F4/F8/F9 redirect | 권위/persistence badge, core jobs parity |
| 3. Visual/World/Replay | 18% | 2% | AS/Skill/Model/FX, World, Replay 통합 + F5/F6 redirect | preview reset, transport overlay, local/authority 경계 확인 |
| 4. Diagnostics + Engine bridge | 13% | 2% | Render split, Combat/Network/Profiler/Camera/limiter | F1~F12 routing 충돌 제거 |
| 5. Cleanup/cutover | 8% | 2% | F10과 숫자 7/8/9 최종 cutover, legacy alias·중복 wrapper 정리 | 모든 ledger row reachable, dead caller와 duplicate Begin 0건 |
| 6. Ceiling output | 0% | **20%** | handbook, annotated screenshots, 2~3분 walkthrough | 비구현자 1명이 기본 작업 성공 |

합계는 core code 70% + 각 slice QA/feedback 10% + handoff output 20% = 100%다. ceiling 30% 산출물은 `.md/build/imgui-tools/<slice>/`의 before/after/failure screenshot, 각 slice QA log, 최종 handbook, walkthrough script, 비구현자 수행 기록으로 감사 가능하게 남긴다. 코드 정리만으로 완료 처리하지 않는다.

## 2. 반영해야 하는 코드 — master work breakdown

중요: 이 절은 직접 적용 가능한 implementation packet이 아니다. 현재 `Scene_InGameImGui.cpp`, `Scene_InGame.h`, `AIDebugPanel.cpp`, `ChampionTuner.cpp`, `UI_Manager.cpp`는 다른 세션에서 수정 중이다. 각 Slice plan은 첫 source edit 전에 최신 active body와 project/build target을 재스캔하고, `.md/계획서작성규칙.md`가 요구하는 정확한 기존 anchor·complete replacement block·새 파일 전체 본문을 별도로 작성한다. `CSomePanel`, 생략 인자, 포괄적 `CONFIRM_NEEDED`를 그대로 구현 근거로 사용할 수 없다.

### 2-1. `C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h`

기존 코드:

```cpp
bool_t m_bShowAIDebug = false;
bool_t m_bShowUITuner = false;
bool_t m_bShowWfxEffectTool = false;
bool_t m_bShowModelAnimPanel = false;
bool_t m_bShowAttackSpeedLab = false;
bool_t m_bShowBalanceTuner = false;
bool_t m_bShowReplayControl = false;
bool_t m_bShowLegacyInGameDebug = false;
bool_t m_bShowRenderDebug = false;
```

Slice 1 target state contract. Domain은 page metadata에서 유도하며 별도 mutable state로 중복 저장하지 않는다.

```cpp
enum class EWintersToolPage : uint8_t
{
    BalanceChampions,
    BalanceSkills,
    BalanceMinions,
    BalanceTowerDefinitions,
    BalanceTowerLiveSession,
    AIOverview,
    AIRuntimeTuning,
    UIActorHud,
    UIHealthBars,
    UIStatusPanel,
    UIMinimap,
    UICursor,
    VisualRendering,
    VisualBasicAttackTiming,
    VisualSkillTiming,
    VisualModelPreview,
    VisualChampionFx,
    WorldMapPreview,
    WorldNavigation,
    WorldLegacyMinionPreview,
    WorldEditorInfo,
    ReplayControl,
    DiagnosticCombat,
    DiagnosticDebugDraw,
    DiagnosticNetwork,
    DiagnosticPerformance,
    DiagnosticCamera,
};

bool_t m_bShowWintersTools = false;
EWintersToolPage m_eWintersToolPage = EWintersToolPage::BalanceChampions;
bool_t m_bShowWfxEffectTool = false;
bool_t m_bEnableDebugDraw = false;
```

`m_bReplayPlaybackMode`와 `m_bReplayStopRequested`는 기존 lifecycle 위치에서 그대로 유지한다. `m_bShowRenderDebug`는 단순 삭제하지 않는다. 현재 `DebugDrawSystem.cpp:265`의 master enable이므로 `m_bEnableDebugDraw`, `IsDebugDrawEnabled()`, `SetDebugDrawEnabled()`로 의미를 분리하고 caller를 함께 이동한다. F1로 Hub를 열 때 collider/champion flag를 강제 true로 바꾸지 않는다.

`m_bShowCombatDebug`, `m_bShowMapTuner`와 전용 legacy visibility state는 field/action parity와 wrapper caller 0이 확인된 Slice 5에서만 제거한다. 중간 slice에서는 redirect용으로만 남겨도 되지만 새 기능이 이 bool을 읽게 하지 않는다.

### 2-2. `C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameImGui.cpp`

anchor: `void CScene_InGame::OnImGui()` 전체 hotkey dispatch와 F10 multi-window block.

현재 active 입력 구조의 정확한 API는 다음과 같다.

```cpp
if (input.IsKeyPressed(VK_F9))
    m_bShowAIDebug = !m_bShowAIDebug;
// ... F8/F7/F4/F5/F6/F10/7/8/9/F1 ...

if (!m_bShowLegacyInGameDebug)
    return;

UI::CCombatDebugPanel::Render(m_World, this);
UI::CMapTunerPanel::Render(this);
// 여러 독립 창을 연속 Render
```

Slice 1 target routing contract. 생략된 F4/F5/F6/F8/F9/F10/7/8/9 handler와 기존 replay call은 이 Slice에서 그대로 남는다.

```cpp
if (input.IsKeyPressed(VK_F1))
    m_bShowWintersTools = !m_bShowWintersTools;

if (input.IsKeyPressed(VK_F7))
    m_bShowWfxEffectTool = !m_bShowWfxEffectTool;

HandleMigratedToolRedirects(); // Slice 1에서는 아직 비어 있음

if (m_bShowWintersTools)
    RenderWintersToolsHub();

if (m_bShowWfxEffectTool)
    UI::CWfxEffectToolPanel::Render(this);

if (m_bReplayPlaybackMode || m_bShowReplayControl || m_bReplayStopRequested)
    DrawReplayControlPanel(); // Slice 3 split 전까지 기존 call 유지
```

추가할 private method contract:

```cpp
void HandleMigratedToolRedirects();
void OpenWintersToolPage(EWintersToolPage ePage, const char* szMovedFromShortcut = nullptr);
void RenderWintersToolsHub();
void RenderWintersToolNavigation();
void RenderWintersToolPage();
void RenderWintersToolEnvironmentBadge() const;
```

Slice 1은 `CRenderDebugPanel` content만 Hub에 실제로 연결한다. 각 후속 Slice가 page parity를 완료할 때에만 대응 redirect를 `HandleMigratedToolRedirects()`에 추가한다. Slice 3에서 replay를 분리한 뒤 `DrawReplayPlaybackTransportOverlay()` contract를 추가한다.

최종 Hub는 `ImGui::Begin("Winters Tools", ...)`를 정확히 한 번 호출한다. page renderer는 독립 `ImGui::Begin`을 호출하지 않고 현재 window 안에 content만 그린다. WFX와 필수 replay playback transport만 별도 기존 창이다.

`CONFIRM_NEEDED`: implementation slice 1 시작 시 진행 중 F4 변경을 포함한 최신 `OnImGui()` 전체를 다시 인용하고, redirect 기간을 정확히 1개 merge slice로 고정한 complete replacement block을 작성한다.

### 2-3. Client panel public/private 파일군

대상:

```text
Client/Public/UI/AIDebugPanel.h
Client/Private/UI/AIDebugPanel.cpp
Client/Public/UI/AttackSpeedLab.h
Client/Private/UI/AttackSpeedLab.cpp
Client/Public/UI/ChampionTuner.h
Client/Private/UI/ChampionTuner.cpp
Client/Public/UI/CombatDebugPanel.h
Client/Private/UI/CombatDebugPanel.cpp
Client/Public/UI/EffectTuner.h
Client/Private/UI/EffectTuner.cpp
Client/Public/UI/MapTunerPanel.h
Client/Private/UI/MapTunerPanel.cpp
Client/Public/UI/ModelAnimPanel.h
Client/Private/UI/ModelAnimPanel.cpp
Client/Public/UI/RenderDebug.h
Client/Private/UI/RenderDebug.cpp
Client/Public/UI/SkillTimingPanel.h
Client/Private/UI/SkillTimingPanel.cpp
Client/Public/UI/StructureTunerPanel.h
Client/Private/UI/StructureTunerPanel.cpp
```

각 Slice plan은 active panel의 실제 `Render(...)` signature와 `Begin/End` body를 인용한 뒤 root window shell과 embeddable content를 분리한다. 임의의 공통 signature로 억지 통일하지 않는다. 기존 standalone wrapper는 migration 동안 유지하고, Hub 외 caller 0과 lifecycle parity가 확인된 뒤에만 제거한다. extraction commit은 수치·command·writer 동작을 바꾸지 않는 mechanical change로 먼저 하고 UX 변경은 다음 commit으로 분리한다.

특수 분리:

- `CRenderDebugPanel`: `DrawRenderingContent()`와 `DrawDebugDrawContent()`로 나눈다.
- `CAIDebugPanel`: `DrawOverviewContent()`와 `DrawRuntimeTuningContent()`로 나눈다.
- `CAttackSpeedLab`: server effective AS 영역과 client visual correction 영역을 구분한다.
- `CEffectTuner`: Irelia live/test뿐 아니라 clipboard, manifest, `.wfx` dump/load/spawn을 `Legacy Export & Preview` Advanced에 그대로 보존한다. WFX tool source는 변경하지 않는다.
- `CStructureTunerPanel`: replicated summary와 live session QA control을 `Balance > Towers > Live Session QA` content로 제공한다. canonical definition editor와 다른 authority임을 header에 표시한다.

패널별 lifecycle/embedding 계약:

| Surface | activation/deactivation | embedded signature 주의 | standalone owner 종료 gate |
|---|---|---|---|
| Balance | draft는 page 이동 시 유지, dirty close/reload만 confirm | Scene pointer + active category; writer/ack state 유지 | 진행 중 F4 교정 parity 후 |
| AI | server session override는 page 이동으로 자동 reset하지 않음 | World + Scene, overview/tuning 분리 | typed command result parity |
| Attack Speed | 기존 server override와 visual correction 수명을 ledger에 기록 | Scene, 두 authority section 분리 | JSON reload/apply parity |
| Model/Animation | `OnPageDeactivated`에서 preview/mesh/animation override 복구 | Scene + selected preview instance | Attack Speed 동시 사용 충돌 0 |
| Skill Timing | visual draft는 유지, runtime preview만 명시적 reset | Scene/registry owner | gameplay cooldown과 label 혼동 0 |
| Render | root-only window sizing 제거; `Rendering`/`Debug Draw` 분리 | Scene; Hub visibility와 debug enable 분리 | duplicate Begin 0 |
| Combat/Map | 내부 show flag를 content가 다시 gate하지 않음 | World/Scene 또는 Scene | 접근 가능·close parity |
| Structure | session QA state 보존 | World + Scene | 별도 window caller 0, 파일은 유지 가능 |
| Minimap | 기존 `bool_t DrawTunerImGui(bool_t, MinimapProjection&)` 반환 계약 유지 | projection output을 Hub가 소비 | runtime projection smoke |
| Network | ring buffer와 Enabled/Clear state 유지 | singleton content, unique ImGui ID namespace | row/clear parity |
| Profiler | Engine standalone `DrawOverlay()`와 embedded content를 동시에 그리지 않음 | Engine owner, root-only SetNextWindow 제거 | product policy/visibility parity |
| Replay | playback transport는 별도 필수 overlay; Hub는 recording/detail | Scene + replay state | playback close/page-switch smoke |

모든 embedded page는 Hub가 부여한 고유 `PushID(pageId)` namespace 안에서 그린다. `SetNextWindow*`, `Begin/End`, standalone open bool은 root wrapper만 소유한다.

`CONFIRM_NEEDED`: 각 implementation slice plan은 active `Begin/End` body와 static state를 완전히 인용하고 `OnPageActivated/OnPageDeactivated`, dirty/preview/session 수명을 확정한 complete code를 작성한다. `#if 0` body를 active contract로 사용하지 않는다.

### 2-4. `C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp` 및 Engine UI bridge

anchor:

```cpp
void CUI_Manager::OnImGui_StatusPanelLayoutTuner()
void CUI_Manager::OnImGui_Tuner()
```

분리할 content API:

```cpp
void CUI_Manager::DrawActorHudToolContent();
void CUI_Manager::DrawHealthBarToolContent();
void CUI_Manager::DrawStatusPanelToolContent();
void CUI_Manager::DrawCursorToolContent();
```

Client Hub가 Engine private type을 include하지 않도록 기존 `CGameInstance` forwarding layer를 통해 page별 content를 호출한다. 현재 standalone `OnImGui_Tuner()`는 migration wrapper로만 남긴 뒤 Slice 5에서 제거한다.

Minimap은 `Client/Private/UI/MinimapPanel.cpp`의 `DrawTunerImGui()`를 `UI & HUD > Minimap`에 연결한다. runtime minimap draw/update owner는 바꾸지 않는다.

`CONFIRM_NEEDED`: `CGameInstance`의 최신 public forwarding API와 EngineSDK header export 범위를 implementation slice 2에서 확인한다. 새 public Engine API가 필요하면 Client type, GameSim type, DX type을 signature에 노출하지 않는 complete header/body를 그 slice plan에 기입한다.

### 2-5. Performance와 Engine global hotkey bridge

대상:

```text
Engine/Private/Framework/CEngineApp.cpp
Engine/Public/Framework/CEngineApp.h
Engine/Include/EngineConfig.h
Engine/Public/Manager/Profiler/ProfilerOverlay.h
Engine/Private/Manager/Profiler/ProfilerOverlay.cpp
Engine/Private/GameInstance.cpp
Engine/Include/GameInstance.h
Client/Private/main.cpp
```

현재 anchor:

```cpp
if (CInput::Get().IsKeyPressed(VK_F3))
    CGameInstance::Get()->Profiler_Toggle();
if (CInput::Get().IsKeyPressed(VK_F12))
{
    bSaveProfilerJson = true;
    bOpenProfilerOverlayAfterSave = true;
}
if (m_uTargetFPS > 0u && CInput::Get().IsKeyPressed(VK_F11))
    bLimitFrameRate = !bLimitFrameRate;
```

Slice 4 target ownership contract:

```cpp
enum class eDeveloperShortcutPolicy : u8_t
{
    EngineGlobal,
    ApplicationOwned,
};

struct SPerformanceToolState
{
    bool_t bProfilerAvailable = false;
    bool_t bFrameLimiterEnabled = false;
    bool_t bCompactHudVisible = false;
    u32_t uTargetFps = 0u;
};

SPerformanceToolState GetPerformanceToolState() const;
void DrawProfilerToolContent();
bool_t SetFrameLimiterEnabled(bool_t bEnabled);
bool_t RequestProfilerTimelineCapture();
void SetCompactPerformanceHudVisible(bool_t bVisible);
```

`EngineConfig` 기본값은 `EngineGlobal`이다. Client main만 `ApplicationOwned`로 설정한다. LoL Client Hub는 `Engine/Include/GameInstance.h`의 forwarding API만 호출하고 성공 path 또는 실패 사유를 page footer에 표시한다. F11의 현재 loop-local `bLimitFrameRate`는 `CEngineApp` member로 승격해야 Hub가 상태를 읽고 안전하게 바꿀 수 있다.

`CEngineApp::DebugRender()`의 항상 표시되는 `##PerfOverlay`도 같은 owner로 편입한다. 최종 기본값을 제품별로 보존한 채 LoL에서는 `Diagnostics > Performance > Compact HUD`로 표시 여부를 제어한다. Profiler standalone `DrawOverlay()`와 Hub embedded content가 동시에 렌더되지 않도록 product policy와 visibility owner를 한 변경에서 전환한다.

Engine handler skip + member state/API + GameInstance bridge + Client Hub route는 하나의 원자 Slice 4로 묶는다. Engine global F3/F11/F12를 다른 제품에서 제거하지 않는다. EldenRingClient, EldenRingEditor, LoLEditor는 각자 대체 진입과 runtime smoke가 생기기 전까지 `EngineGlobal`을 유지한다.

`CONFIRM_NEEDED`: 위 API 이름과 반환 status/path 타입은 Slice 4 implementation plan이 기존 `Profiler_SaveJson(const char*, ...)` convention과 end-of-frame capture path builder를 정확히 인용한 뒤 complete header/body로 확정한다. profiler compile gate가 VS/CMake에서 다른 차이를 숨기기 위해 LoL Client가 profiler internals를 직접 include하지 않는다.

### 2-6. Camera, Network, Minion, Replay content owner

대상:

```text
Client/Private/DynamicCamera.cpp
Client/Public/DynamicCamera.h
Client/Private/Network/Client/NetworkEventTrace.cpp
Client/Public/Network/Client/NetworkEventTrace.h
Client/Private/Manager/Minion_Manager.cpp
Client/Public/Manager/Minion_Manager.h
Client/Private/Scene/Scene_InGameImGui.cpp
```

계약:

- `CDynamicCamera::OnImGui()`는 `DrawToolContent()`로 분리하고 Scene의 작은 `Camera` window 내용을 같은 page로 이동한다.
- `CNetworkEventTrace`는 기존 256-entry ring buffer를 유지하고 `DrawToolContent()`만 제공한다.
- Replay recording/detail은 Hub page로 옮기지만 현재 하단 고정 playback/stop-pending transport는 별도 필수 overlay로 유지한다. Hub close/page switch가 transport를 숨기거나 page를 강제로 빼앗지 않는다.
- `CMinion_Manager::OnImGui_Tuner()`의 모든 기존 field/action은 `Legacy Local Minion Preview` Advanced에 보존한다. direct local spawn/clear는 정상 authoritative room에서 비활성화하고 정확한 이유를 표시한다. waypoint를 M World Editor로 넘기는 것은 6-lane add/edit/reset/save roundtrip parity가 증명된 후 별도 승인으로만 한다.

`CONFIRM_NEEDED`: Minion page의 local-preview 가능 조건과 현재 authoritative-room disabled 조건을 Slice 3에서 exact code로 확정한다. 실제 사용자 owner가 아직 불명확하더라도 기능 삭제 근거로 사용하지 않는다.

### 2-7. cleanup 대상

Slice 5에서만 수행:

```text
field/action parity가 완료된 Scene_InGame.h의 legacy window visibility bool
Scene_InGameImGui.cpp의 F10 multi-window path와 숫자 7/8/9 alias
중복 Camera mini window
중복 RenderDebug standalone caller
Hub 외 caller가 0이고 parity가 완료된 standalone Begin/End wrapper
```

Structure Tuner source, Effect Tuner writer, Minion Tuner 기능, 비활성 `#if 0` body는 이번 cleanup에서 삭제하지 않는다. 이들은 별도 사용자 승인과 별도 cleanup plan 대상이다. 삭제 전에는 parity ledger 완료, `rg` caller 0, active side-effect parity, user-visible screenshot을 모두 확인한다. WFX protected files는 cleanup 대상이 아니다.

### 2-8. 디자이너·기획자 handbook

새 문서 후보:

```text
.md/guide/WINTERS_TOOLS_DESIGNER_HANDBOOK.md
```

`CONFIRM_NEEDED`: 새 파일의 완전한 본문은 구현된 최종 label과 screenshot path가 확정된 Slice 6 plan에 작성한다. 최소 목차는 다음과 같다.

```text
1. 30초 시작: Debug Client + authoritative room + F1
2. 화면 읽는 법: environment / authority / persistence / result
3. Balance: 챔피언 스탯과 QWER를 Save & Hot Load
4. AI: 관찰과 세션 튜닝 구분
5. UI & HUD: 저장되는 항목과 session-only 항목
6. Visuals: gameplay cooldown과 visual timing 구분
7. World: preview와 authoritative stage 구분
8. Replay & QA
9. Diagnostics
10. WFX는 F7 별도 도구라는 안내
11. Release에서 막히는 이유와 cook/build workflow
12. 실패 사유별 해결 표
```

## 3. 검증

### 3-1. 먼저 적는 예측

- F1만 누르면 Hub 한 창이 열리고 default는 `Balance > Champions`다.
- 최종 Slice 5 cutover 뒤 F10 또는 숫자 9를 눌러도 여러 창이 동시에 열리지 않는다.
- migration 기간의 F4는 Hub의 `Balance` page를 열고 `Moved to F1 > Balance`를 한 번 보여 준다.
- 최종 cutover 뒤 LoL Client의 F2/F3/F4/F5/F6/F8/F9/F10/F11/F12와 숫자 7/8/9는 도구 창을 직접 열거나 gameplay state를 바꾸지 않는다. `EngineGlobal`을 유지하는 다른 제품의 F3/F11/F12는 기존 동작을 보존한다.
- replay playback/stop-pending transport는 Hub를 닫거나 다른 page를 선택해도 계속 보이고, stop 완료 후에만 사라진다.
- F1로 Hub를 열어도 debug draw master/collider/champion state는 바뀌지 않는다.
- F7은 전후 동일한 WFX 창, 저장 경로, preview 동작을 가진다.
- Balance Save & Hot Load는 Debug authority에서만 동작하고 Release에서는 같은 위치에 이유가 보인다.
- AI Force/Tuning은 서버 command 결과가 돌아오기 전 성공으로 표시되지 않는다.
- Model/Animation page를 닫거나 이동하면 preview override가 정의된 계약대로 복구된다.
- Profiler capture는 성공 path를 표시하고 실패 시 이유를 표시한다.
- WFX tool source와 기존 `.wfx`/manifest resource의 의도하지 않은 diff는 0이다.

### 3-2. 정적 inventory와 계약 검사

```powershell
rg -n "VK_F[0-9]+|DIK_F[0-9]+|IsKeyPressed\('[M789]'\)|IsKeyDown\(VK_F" Client Engine LoLEditor EldenRingClient EldenRingEditor
rg -n "ImGui::Begin\(" Client/Private Engine/Private | rg "Tuner|Debug|Lab|Profiler|Camera|Balance|Replay|Tools"
rg -n "m_bShowAIDebug|m_bShowUITuner|m_bShowLegacyInGameDebug|m_bShowRenderDebug|m_bShowAttackSpeedLab|m_bShowModelAnimPanel" Client
rg -n "CStructureTunerPanel|OnImGui_StatusPanelLayoutTuner|DrawTunerImGui" Client Engine
rg -n "Save|Reload|Apply|Hot Load|Reset" Client/Private/UI Engine/Private/Manager/UI
git diff -- Client/Public/UI/WfxEffectToolPanel.h Client/Private/UI/WfxEffectToolPanel.cpp
git diff -- Data/LoL/FX/Manifest ':(glob)Data/LoL/FX/**/*.wfx'
git diff --check
```

예상 결과:

- 최종 shortcut owner는 LoL F1 Hub, F7 WFX, M Editor와 명시적 제품별 Engine policy뿐이다.
- 독립 legacy **window** visibility bool과 F10 multi-window call은 0건이다. debug draw enable과 replay lifecycle state는 별도 의미로 남는다.
- WFX tool/resource의 의도하지 않은 diff는 비어 있다.
- orphan panel은 Hub page에 도달하거나 명시적으로 retained standalone이며, 사용자 승인 없이 삭제된 기능은 0건이다.

### 3-3. 자동 테스트와 빌드

구현 세션에서 다음 contract test를 추가한다.

```text
PASS final shortcut map has exactly one general hub key and protected F7 WFX
PASS numeric 7/8/9 tool aliases are absent
PASS every active tool page declares domain, audience, authority, persistence, and availability
PASS no page renderer calls ImGui::Begin when embedded in Hub
PASS F10 redirect cannot render multiple pages in one frame
PASS F1 hub toggle does not mutate debug-draw subflags
PASS playback/stop-pending transport survives hub close and page switch
PASS EngineGlobal products retain F3/F11/F12 while LoL ApplicationOwned routes them to Hub
PASS M and migration keys do not trigger while ImGui captures keyboard
PASS Release mutation controls expose a non-empty disabled reason
PASS Balance backend contract remains unchanged
PASS every parity-ledger row has a replacement or retained owner and proof
PASS Structure live summary/overrides/presets, Minion local controls, and Effect legacy writers retain parity
PASS WFX protected tool/resources and F7 route remain unchanged
```

빌드 명령:

```powershell
MSBuild Engine/Include/Engine.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
MSBuild Engine/Include/Engine.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1 /nr:false /v:minimal
MSBuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
MSBuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1 /nr:false /v:minimal
```

Engine global key policy 변경이 들어가는 Slice 4에서는 CMake products를 실제 target으로 추가:

```powershell
MSBuild EldenRingClient/Include/EldenRingClient.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
cmake --preset vs2022
cmake --build out/build/vs2022 --config Debug --target WintersLoLEditor WintersEldenRingEditor WintersElden
```

Client/Engine은 현재 VS project, LoLEditor/EldenRingEditor는 CMake target이며 EldenRingClient는 양쪽에 존재한다. Slice 4는 실제 configure 성공 후 위 명령을 갱신하며 경로를 추측하지 않는다.

### 3-4. 수동 visual QA

지원 최소 해상도와 대표 DPI를 먼저 확정한 뒤 다음 캡처를 남긴다.

1. Hub default, 가장 긴 Balance page, AI empty/connected, UI & HUD, Performance success/failure.
2. 16:9 최소 해상도에서 HUD, shop, minimap, skill bar를 가리지 않는 위치/크기.
3. Release Client에서 mutation이 disabled이고 이유가 보이는 화면.
4. Dirty draft에서 page switch/Reload/close 확인 dialog.
5. F10/9를 눌러도 창이 폭발하지 않는 화면/trace.
6. playback 중 Hub close/page switch, stop-pending, stop 완료 transport lifecycle.
7. F7 WFX 전후 동일 화면과 기존 save/preview smoke.
8. LoL, EldenRingClient, EldenRingEditor, LoLEditor 각각 F3/F11/F12 runtime smoke와 policy 기록.
9. 비구현자 1명이 handbook만 보고 다음을 수행:
   - Irelia Base AD draft 변경 후 Hot Load 결과 확인
   - Q damage/cooldown 위치 찾기
   - Minion Max Health 위치 찾기
   - UI health-bar offset 위치 찾기
   - AI observer와 AI session tuning의 차이 설명
   - profiler capture path 찾기

완료 조건은 “버튼이 있다”가 아니라 작업 성공과 결과 확인이다.

### 3-5. 미검증과 확인 필요

- 현재 다른 세션이 F4 Balance, AI, UI Manager source를 수정 중이므로 구현 시작 시 active body 재스캔 필요.
- Engine F3/F11/F12 product policy는 각 제품 runtime smoke 전에는 완료 주장 불가. 다른 제품의 `EngineGlobal` route를 먼저 끊지 않는다.
- Minion local preview의 실제 사용자 owner는 아직 확인되지 않았다. 기본 page가 아닌 Advanced에 보존하며 owner 부재를 삭제 근거로 사용하지 않는다.
- 한국어 한 줄 설명의 font/glyph coverage와 최소 해상도/DPI는 실행 캡처로 확인해야 한다.
- World Editor M은 이번에 내부 통합하지 않으므로 별도 scene의 dirty/save/escape 문제는 `World Editor Info`에 명시하고 근본 해결은 후속 계획 대상으로 둔다.

## 4. 서브 에이전트 비평

비평 주체: `plan_critic`, `plan_critic_fast` read-only sub-agents. 실제 코드 inventory와 초안을 대조했으며 파일은 수정하지 않았다.

| 등급 | 지적 | 처분 | 반영 |
|---|---|---|---|
| P0 | Engine F3/F11/F12는 global pre-scene handler라 LoL redirect/단순 제거가 불가능하고 다른 제품을 끊음 | **수용** | §1-7 Phase B에 `EngineGlobal/ApplicationOwned` product policy, loop-local limiter state 승격, 원자 Slice 4, 4개 제품 runtime smoke 추가 |
| P0 | Effect/Minion/Structure의 실제 필드·writer·preset을 중복으로 보고 삭제할 수 있음 | **수용** | §1-3-A parity ledger, Advanced 보존, 승인 없는 deprecation 금지, §2-7 삭제 범위 축소 |
| P1 | replay forced transport와 one-window Hub가 충돌하고 Hub 재개방/page 탈취 위험 | **수용** | playback/stop-pending transport를 두 번째 명시적 별도 overlay 예외로 유지, close/page-switch test 추가 |
| P1 | 일괄 `DrawToolContent()` 추출은 lifecycle, profiler 이중 draw, Minimap output, ImGui ID를 닫지 못함 | **수용** | §2-3에 panel별 activation/deactivation, signature 주의, `PushID`, root-only window API, wrapper 종료 gate 추가 |
| P1 | 초안 code packet이 실제 API/field/path/build system과 불일치 | **수용** | 문서를 master plan으로 명확히 낮추고 slice별 exact implementation plan을 의무화; `m_bShowBalanceTuner`, `VK_F*`, `Engine/Include/GameInstance.h`, CMake target 수정 |
| P1 | `World Editor Info` page와 keyboard capture policy가 빠짐 | **수용** | page enum/IA 추가, M/redirect/F2 capture 정책과 dirty 위험 설명 추가 |
| P1 | `m_bShowRenderDebug`는 window flag이자 실제 DebugDraw master라 단순 삭제 불가 | **수용** | `m_bEnableDebugDraw` 의미 분리, getter/caller 이동, F1의 collider/champion 강제-on 부수효과 폐기와 회귀 test 추가 |
| P2 | Balance authority/persistence가 writer→JSON→server→ack 순서를 숨김 | **수용** | §1-5/1-6에 실제 5단계 흐름과 Release cook/build 이유 추가 |
| P2 | 30% ceiling이 20%+암묵적 10%라 감사하기 어려움 | **수용** | §1-8을 core 70 + slice QA 10 + handoff 20으로 재산정하고 산출물 경로/기록 gate 추가 |
| P2 | 상시 Perf HUD와 WFX/manifest resource 보호 검사가 구현 계약에서 약함 | **수용** | §2-5 Compact HUD owner/API, §3 source+resource diff와 Effect writer parity test 추가 |
| 최종 재비평 P1 | Slice 1에서 아직 추출되지 않은 F4~F10/7~9를 먼저 redirect하면 중간 build의 진입이 끊기고 replay method도 미구현 | **수용** | §1-7/1-8/2-2를 page별 원자 migration으로 변경. Slice 1 replay 조건은 기존 `playback || showControl || stopPending`을 정확히 유지하고, F4/F8/F9는 Slice 2, F5/F6는 Slice 3, F2/F3/F11/F12는 Slice 4, F10/7/8/9는 Slice 5에서 parity와 함께 전환 |

최종 재비평 판정: **ACCEPT — 미해결 P0/P1 없음.** 단, 이 판정은 master plan에 대한 승인이다. 각 Slice implementation plan은 최신 코드로 다시 독립 비평을 받아야 하며 그 전에는 source edit를 시작하지 않는다.

## 5. 인수인계

- 이 master plan은 한 번의 big-bang patch 승인서가 아니다. Slice 1~6 각각 이 문서를 resume/update하고 독립 비평을 받은 뒤 구현한다.
- 첫 구현 세션은 다른 세션의 F4/UI/AI 작업 종료 여부를 확인하고 current diff를 보존한다.
- 각 Slice는 code diff, contract test, build, visual capture, `_RESULT.md`를 함께 닫는다.
- 최종 전달 문장은 `F1로 열고, 왼쪽 도메인을 고르고, 상단 Scope를 확인하고, 하단의 정확한 action과 결과를 본다. WFX만 F7 별도다.`로 설명 가능해야 한다.
