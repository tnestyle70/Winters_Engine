# Winters Essence Boundary Code Concept

`00_ESSENCE_BOUNDARY_REFACTOR.md`를 코드에 반영할 때 남길 최소 계약이다.

세부 구현 세션은 이 문서를 그대로 코드로 옮기지 않는다. 대상 h/cpp/vcxproj를 다시 inspect한 뒤 기존 코드, 교체 코드, 삭제 범위 단위로 쪼갠다.

## 본질

```text
기획/디자인 의도 -> Data/Editor/Tools contract
gameplay truth -> Shared/GameSim + Server authority
presentation -> Client/EldenRingClient
runtime primitive -> Engine
outside-match state -> Services
output graph -> Build/Generated
```

한 파일이 두 이유로 바뀌면 파일을 나눈다.

## 개념 반영

### 1. 권위

```text
Client Input -> GameCommand -> Server GameSim -> Snapshot/Event/Cue -> Client Visual
```

HP, 피해, 쿨타임, 승패, 투사체 판정은 Shared/GameSim 또는 Server가 만든다. Client는 입력, 약한 예측, 보간, 애니메이션, FX, UI, 디버그만 맡는다.

### 2. 데이터

```text
ChampionGameData  -> stat, skill, cooldown, damage, cost, rule
ChampionVisualDef -> mesh, texture, material, animation, FX, UI, yaw/offset
```

기획 값과 디자인 값은 같은 이름을 공유해도 다른 truth다. C++ fallback table은 migration path일 수는 있어도 최종 truth가 아니다.

### 3. 엔진

Engine은 제품 이름을 몰라야 한다.

남길 것: window, frame loop, RHI, renderer, resource, ECS primitive, generic query, UI drawing primitive, audio, diagnostics.

뺄 것: LoL/Elden/champion/minion/turret/skill/HUD/shop/kill feed policy, GameSim truth, Server/Client product dependency.

### 4. 산출물

`EngineSDK`, `Bin`, `out`, log, capture, profiler output은 편집 원본이 아니다. 원본을 고치고 산출물을 다시 만든다.

## 코드 반영

| 영역 | 남길 코드 | 제거할 방향 |
|---|---|---|
| `Engine/` | 제품 중립 primitive | 제품 정책, GameSim/Client/Server dependency, 새 `ID3D11*` public 노출 |
| `Shared/GameSim/` | deterministic component/system/schema/replay contract | Engine, Client, UI, Renderer, ImGui, DX include |
| `Server/` | command intake, GameSim tick, snapshot/event/cue emission, bot command generation | client visual 판정, truth component 직접 조작 bot |
| `Client/` | input send, prediction, interpolation, view-state, animation/FX/UI playback | authoritative gameplay 결과 생성 |
| `EldenRingClient/` | action RPG runtime presentation | LoL Client 내부에 섞인 Elden 의미, editor save workflow |
| `EldenRingEditor/` | authoring workflow, validator, cooked output | runtime gameplay truth, renderer backend policy |
| `Data/` | gameplay data와 visual data의 분리된 원본 | validator 없는 runtime 연결, C++ fallback과 다른 truth |
| `Tools/` | cook, convert, codegen, validation, SimLab, replay/golden evaluation | normal F5 runtime 대체 경로 |
| `Services/` | account, profile, matchmaking, shop backend state | in-match gameplay truth, client visual state |
| Build graph | target/source/dependency 소유권 반영 | build 통과용 역방향 dependency |

## 우선순위

1. Generated output 격리: `EngineSDK/inc`, `Bin`, `out`, log, capture, profiler output을 수정 원본에서 제외한다.
2. Engine 제품 의미 제거: 제품 이름과 gameplay policy를 실제 소유자로 내린다.
3. GameSim 독립: Engine gameplay header re-export와 visual asset/timing 의존을 끊는다.
4. Server authority 정리: bot은 command producer로 두고 smoke/lab branch는 명시적 flag로 격리한다.
5. Client presentation 분리: `Scene_InGame` 계열을 bootstrap, network bridge, prediction, presentation, debug로 나눈다.
6. Data contract 분리: gameplay data와 visual data를 validator가 있는 별도 contract로 둔다.
7. UI/FX/Asset pipeline 분리: Engine primitive, Product view-state, Server cue, Client playback, Data preset을 분리한다.
8. Elden runtime/editor 분리: runtime presentation은 `EldenRingClient`, authoring workflow는 `EldenRingEditor` 또는 Tools가 소유한다.
9. Build graph 잠금: 정리된 owner 기준으로 `.sln`, `.vcxproj`, CMake target source list를 맞춘다.

## 완료 판정

아래 질문에 모두 "아니오"라고 답할 수 있어야 한다.

- Engine이 제품 이름을 알아야 빌드되는가?
- Shared/GameSim이 Engine, Client, UI, Renderer, ImGui, DX를 include하는가?
- Server가 client visual 성공을 authority 검증으로 착각하는가?
- Client가 서버 event 없이 gameplay 결과를 만드는가?
- Data 없이 C++ fallback table이 최종 truth가 되는가?
- Editor/Tools가 normal F5 runtime을 우회하는가?
- Services가 in-match gameplay truth를 소유하는가?
- generated output을 직접 편집해서 문제를 해결하는가?

## 세부 세션 규칙

- 실제 대상 파일을 먼저 inspect한다.
- 기존 코드 기준의 삭제, 추가, 교체 범위만 적는다.
- 새 파일은 전체 본문을 적는다.
- `EngineSDK/inc`는 수정 대상에 넣지 않는다.
- 검증에는 `git diff --check`와 해당 target build를 넣는다.
- Engine public header 변경이면 `UpdateLib.bat` 또는 SDK sync 필요 여부를 남긴다.
