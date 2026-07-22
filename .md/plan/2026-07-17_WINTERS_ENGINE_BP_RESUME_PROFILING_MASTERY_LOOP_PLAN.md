Session - Winters Engine을 BP로 인수하고, 버그 수정·구조 개선·프로파일링 결과를 영상·이력서·면접 답안으로 계속 환전한다.
좌표: W00/S00 운영 부트스트랩 → W/S 실전 좌표 방어 · 축 C4 수명·소유권 / C7 권위·정합 / C8 검증
관련: `.md/plan/2026-07-17_ENGINE_ACQUISITION_DEBUG_ROADMAP.md` · `.md/plan/2026-07-15_WINTERS_ENGINE_FULL_STACK_MASTERY_AND_RELEASE_GUIDE.md` · `.md/plan/2026-07-13_RESUME_DOMAIN_STORY_STARCRAFT_MINECRAFT_WINTERS.md` · `Plan/S00_SERVER_RUNTIME_ESSENCE_PLAN.md`

# 0. 결론

이번 목표는 엔진 문서를 전부 읽는 것이 아니다. 다음 루프를 반복해서 **Winters Engine을 수정 결과까지 예측할 수 있는 수준으로 인수하고, 그 이해를 외부 증거로 바꾸는 것**이다.

> 질문 선택 → 사전 답변 → BP로 실제 경로 관측 → 소유권 판정 → 버그/병목 최소 수정 → 빌드·런타임 검증 → 전후 영상 → 이력서 문장·HTML 카드 갱신

“완벽 이해”는 끝이 없는 표현이므로 다음 다섯 조건을 만족한 도메인을 `방어 완료`로 정의한다.

1. 코드를 열기 전에 입력·상태·출력·소유자를 예측할 수 있다.
2. Visual Studio Breakpoint(BP)로 실제 호출 경로와 수명을 끝까지 관통한다.
3. 예상과 실제가 달랐던 `divergence`를 한 줄 이상 남긴다.
4. 작은 변경을 넣기 전에 영향을 예측하고, 변경 후 자동 검증과 정상 F5로 증명한다.
5. 90초 안에 문제·순진한 해법·메커니즘·대가·검증을 소스 없이 설명한다.

여기서 BP는 새 Blueprint 기능이 아니라 **Visual Studio Breakpoint 기반 구조 인수**를 뜻한다. 기존 Blueprint 파이프라인은 현재 호출부 0건의 은퇴 대상으로 판정·반영된 별도 사안이다.

# 1. 현재 근거와 출발점

## 1.1 이미 확보된 것

- `BreakPoint/ladder_frame.xml`과 `.md/interview/cards/log.csv`에 8칸 사다리 관통 및 함수 BP 영구화 기록이 있다.
- W01은 `Entity.h`의 Create/Destroy/SwapRawState까지 관측했지만, divergence와 `m_Parent` 유도 설명이 미납이라 아직 진행 중이다.
- 클라이언트 디버거에서는 서버 권위 전투가 보이지 않는다는 S18/C7 경계를 직접 관측했다.
- 비에고 빙의는 pending command → 다음 틱 적용 → form/visual/skill champion 교체까지 관통했다.
- StaticScene, 구 Blueprint 경로, Engine의 고아 StatusEffectSystem은 호출부·소유권 근거로 은퇴시켰고 빌드까지 통과했다.
- `Tools/SimLab`은 결정론·GameSim 회귀의 핵심 장비지만 현재 로그에는 Zed passive/R probe와 키프레임 payload 관련 WIP 실패가 남아 있다. 다른 세션 변경을 덮지 말고 소유 세션과 조정한다.

## 1.2 HTML W/S의 실제 상태

- 실제 카드 ID는 W01/S01부터 시작한다. 문자 그대로의 W00 카드는 없다.
- 이 계획에서 `W00`은 엔진 인수 장비와 증거 계약을 준비하는 운영 부트스트랩이다.
- `S00`은 `Plan/S00_SERVER_RUNTIME_ESSENCE_PLAN.md`의 현재성 감사와 서버 런타임 수명 관통을 뜻한다.
- 카드 정본 후보는 `.md/interview/cards/data/deck_source_2026-07-16.json`이고, HTML은 생성 산출물이다.
- 생성기는 `.md/interview/cards/data/assemble_books.py`에 있으나 현재 `C:\Users\user\Downloads\...` 절대 경로가 박혀 있다. 경로 계약을 바로잡기 전에는 생성 HTML을 손으로 고쳐 정본과 갈라지게 하지 않는다.
- 현재 기본 source label은 W01~W22, S01~S20이며 심화 HTML에는 확장 카드가 있다. 첫 작업에서 정본 범위를 한 번 확정하고 그 뒤에는 같은 원장을 사용한다.

## 1.3 현재 프로파일 캡처의 사실

`profiler.json`을 `Tools/Profiler/analyze_profiler_capture.py`로 분석한 현재 진단값이다.

| 항목 | 현재 값 | 판정 |
|---|---:|---|
| 스키마 | `WintersProfilerTimeline.v3` | 분석기 지원 |
| 프레임 / 시간 | 300 / 15.861초 | 진단에는 사용, 60초 acceptance에는 부족 |
| 해상도 | 1280×720 | 정상 acceptance 계약인 1920×1080 불충족 |
| Effective FPS | 18.85fps | 사용자가 본 16fps 체감과 같은 저성능 구간 |
| Frame median / p95 / p99 | 51.06 / 68.64 / 92.66ms | 60fps 16.67ms 예산 초과 |
| Render median / p95 | 38.40 / 51.45ms | 1차 CPU 병목 |
| Update median / p95 | 11.40 / 17.10ms | 2차 조사 대상 |
| GPU median / p95 | 10.77 / 24.14ms | median은 CPU보다 낮지만 tail은 예산 초과 |
| `Model::RenderWithMask` median | 33.82ms | 포괄 스코프, 하위 중복 합산 금지 |
| `Map::Render` median | 28.14ms | 정적 맵 렌더 체인 우선 조사 |
| `ModelRenderer::RenderStatic` median | 28.09ms | `Map::Render`의 중첩 경로로 추정 |
| `Model::RenderCombinedStatic` median | 28.04ms | 정적 모델 제출/순회 경로 핵심 후보 |
| 프로파일러 EndFrame p95 | 0.169ms | 병목의 주원인이 아님 |

이 캡처는 Map/Champion/Minion/UI와 서버 틱을 포함한 full scenario scope가 300프레임 모두 들어 있어 **원인 진단 자료로는 유효**하다. 다만 720p·15.9초라 최종 이력서 수치나 최적화 완료 증거로는 사용하지 않는다.

현재 근거로는 “미니언이 많아서 느리다”보다 **CPU의 정적 맵 모델 렌더/제출 경로가 프레임을 장악한다**는 가설이 먼저다. 포괄 스코프는 부모와 자식을 더하면 이중 계산되므로 `Render + Map + Static`을 합산하지 않는다.

## 1.4 이력서 증거 충돌

`.md/이력서/이력서_MASTER.md`에는 이미 `17.8ms → 9ms` 최적화 문장이 있다. 현재 정상 게임 캡처는 median 51.06ms이므로 다음 중 무엇인지 재검증하기 전에는 이 수치를 확정 성과로 재사용하지 않는다.

- 과거와 현재의 맵·해상도·로스터·카메라·빌드가 다른가.
- 과거 수치가 특정 스코프이고 현재 수치는 전체 프레임인가.
- 회귀가 생겼는가.
- 과거 측정이 짧거나 격리된 실험이었는가.

# 2. Decision Log

## 2.1 확정사항

1. **70% 바닥 / 30% 천장**으로 운영한다.
   - 바닥: 구조 인수, 버그 수정, 리팩터링, 프로파일링, 검증.
   - 천장: 이력서, 영상, 공개 가능한 기술 서사, 실제 지원.
2. 같은 도메인의 바닥 작업을 3세션 연속 했는데 이력서·영상·지원 산출물이 없으면 다음 심화를 중단하고 먼저 환전한다.
3. 한 시점의 WIP는 `인수 도메인 1개 + 버그/성능 슬라이스 1개 + 외부 산출물 1개`로 제한한다.
4. `Client Input → GameCommand → Server GameSim → Snapshot/Event → Client Visual` 권위 흐름을 흔들지 않는다.
5. 구조 변경은 학습을 위한 재작성으로 시작하지 않는다. 실제 중복 진실, 소유권 위반, 측정 병목, 반복 버그 중 하나가 증명될 때만 한다.
6. 성능 수치는 같은 시나리오의 before/after JSON과 영상이 모두 있을 때만 이력서에 쓴다.
7. HTML은 생성 결과다. 정본 JSON·생성기·로그를 갱신하고 재생성한다.
8. 정상 F5의 맵·챔피언·미니언·스냅샷·UI·FX를 숨겨 숫자를 만들지 않는다.

## 2.2 판단사항

- 첫 성능 목표는 60fps 안정화, 즉 16.67ms 예산이다. 그 다음 120fps/300fps 실험 게이트로 올라간다.
- 현재 병목의 첫 번째 조사 대상은 `Map::Render → ModelRenderer::RenderStatic → Model::RenderCombinedStatic` 체인이다.
- 기존 분석기의 `gate.pass`는 target-fps 외에도 더 엄격한 hitch 조건을 포함한다. 60fps 안정화 단계와 저장소 최종 strict gate를 같은 의미로 부르지 않고, strict gate 계약 자체를 별도 감사한다.
- `S00_SERVER_RUNTIME_ESSENCE_PLAN.md`는 `C:\Users\tnest\...` 경로와 아직 존재하지 않는 `CServerRuntime` 추출안을 포함한 구 계획이다. 현재 `Server/Private/main.cpp`의 `ServerRuntimeOptions`와 실제 수명을 먼저 관측한 뒤 `수용 / 축소 / 폐기`를 판정한다.

## 2.3 미확정사항

- 지원 직무의 1순위가 엔진/클라이언트/서버 중 무엇인지.
- 첫 실제 지원 마감일과 회사 목록.
- `17.8ms → 9ms`의 과거 원본 캡처·시나리오.
- 카드 정본을 기본판 W01~W22/S01~S20으로 둘지 심화 확장판까지 한 번에 운영할지.

미확정사항은 진행을 막지 않는다. 우선 엔진/클라이언트형 이력서로 만들고, 서버 권위 역량을 차별화 근거로 유지한다.

# 3. 아키텍처 소유권 지도

| 계층 | 소유하는 진실 | BP로 답할 질문 | 변경 허용 조건 | 검증 |
|---|---|---|---|---|
| Engine | 프레임 루프, RHI, 렌더/리소스/UI 원시 기능 | 누가 만들고, 언제 갱신·파괴하며, Present까지 무엇이 흐르는가 | 제품 챔피언 규칙을 포함하지 않음 | Engine/Client 빌드 + 정상 F5 + GPU/CPU capture |
| Client | 입력, 카메라, 보간, 애니메이션/FX/UI 표현 | 서버 결과가 어떤 snapshot/event로 들어와 한 번만 재생되는가 | 권위 데미지·CC·판정 생성 금지 | 네트워크 F5 + 영상 + bounded trace |
| Shared/GameSim | 결정론 게임플레이 진실 | command가 어떤 시스템 순서로 상태를 바꾸는가 | Engine/DX/UI 의존 금지 | SimLab/replay/golden hash |
| Server | 세션, command 검증, GameRoom tick, snapshot/event 송신 | 어느 틱에 무엇을 수용·거부·방송하는가 | Client visual 의존 금지 | server harness + network smoke |
| Tools | importer, SimLab, harness, codegen, profiler analysis | 런타임 계약을 어떻게 재현·감사하는가 | 별도 gameplay truth 생성 금지 | tool unit test + 실제 runtime artifact 대조 |
| Resume/Video/Cards | 검증된 사실의 외부 표현 | 어떤 근거로 이 문장을 말할 수 있는가 | 확인 안 된 수치·과장 금지 | evidence ledger + closed-book defense |

# 4. 한 세션의 고정 루프

## 4.1 3시간 기준 시간 예산

| 구간 | 시간 | 산출물 |
|---|---:|---|
| 질문 선택·사전 답변 | 15분 | 코드 보기 전 예상 5줄 |
| BP 관통 | 45분 | 호출 사다리, owner, lifetime, divergence |
| 최소 수정 또는 병목 실험 | 45분 | 재현 가능한 한 슬라이스 |
| 자동 검증·정상 F5 | 21분 | build/test/capture 결과 |
| 이력서 환전 | 18분 | 수치·제약·행동·결과 bullet 후보 |
| 영상 환전 | 18분 | 15~30초 전후 증거 클립/스크립트 |
| 카드·지원 환전 | 18분 | 5칸 답안 또는 실제 지원 1건 |

바닥 126분(70%), 천장 54분(30%)이다. 빌드가 길어지면 기다리는 동안 이력서 문장과 영상 자막을 작성하되 검증 시간을 삭제하지 않는다.

## 4.2 세션 시작 계약

세션 시작 때 아래를 한 줄씩 고정한다.

- 오늘 좌표: 예) `W03 프레임 수명`.
- 실제 증상: 예) `정상 인게임 median 51ms`.
- 예상 owner: 예) `Engine generic render path`.
- 바꿀 수 있는 최대 범위: 파일/시스템 한 슬라이스.
- 자동 검증: build/SimLab/harness/profiler command.
- 천장 산출물: 이력서 bullet, 영상 클립, 카드 답안, 지원 중 하나.

## 4.3 BP 인수 기록 형식

각 관측은 `.md/interview/cards/log.csv`에 다음 정보를 남긴다.

- 질문/좌표와 축.
- 예상 call chain.
- 실제 BP chain.
- divergence.
- owner와 lifetime.
- 실패 시 보이는 로그/카운터/화면.
- 수정 전 예측.
- 검증 결과.
- 이력서·영상으로 환전했는지.

# 5. W00/S00 부트스트랩

## 5.1 W00 — 엔진 인수 장비 완성

목표는 기존 `ladder_frame.xml`을 재현 가능한 엔진 관측 장비로 승격하는 것이다.

1. Debug x64 정상 F5에서 심볼·working directory·런타임 리소스가 `Client/Bin/Resource`를 보는지 확인한다.
2. 프레임 사다리의 BP를 다시 밟는다.
   - 엔진 진입/초기화.
   - GameInstance tick.
   - SceneManager update/render.
   - InGame scene update/render.
   - DX11 Present.
3. 엔티티 사다리에서 Create → alive check → Destroy → slot reuse/generation mismatch를 관측한다.
4. 조건부 BP로 틱 전체를 멈추지 않는다. 템플릿·인라인은 실제 호출부 line BP로 대체한다.
5. 60~90초 화면 녹화로 “입력 한 번이 프레임과 엔티티 상태를 통과하는 과정”을 설명한다.

완료 조건:

- 다른 날에도 XML을 불러와 같은 BP가 적중한다.
- W01의 `m_Parent`/generation/divergence 미납을 닫는다.
- BP 없이 프레임과 엔티티 수명을 화이트보드에 재구성한다.

## 5.2 S00 — 서버 런타임 현재성 감사

1. `Server/Private/main.cpp`에서 옵션 파싱 → socket/service 초기화 → room/runtime 시작 → loop → shutdown 수명을 BP로 관통한다.
2. `Plan/S00_SERVER_RUNTIME_ESSENCE_PLAN.md`의 가정과 현재 코드를 표로 대조한다.
3. `CServerRuntime` 추출이 현재도 필요한지 다음 기준으로 판정한다.
   - main이 테스트 불가능한 수명과 상태를 과도하게 소유하는가.
   - Start/Stop 대칭과 실패 rollback이 분명한가.
   - harness가 실제 서버 runtime을 재사용해야 하는가.
   - 새 wrapper가 두 번째 runtime truth를 만드는가.
4. 판정이 `수용`일 때만 별도 구현 계획을 작성한다. 그 계획에는 기존 main 경로의 삭제/축소 범위가 반드시 들어간다.

완료 조건:

- 서버를 띄우지 않고도 수명 순서와 실패 지점을 설명한다.
- 실제 BP로 그 설명을 확인한다.
- 구 S00 계획에 `수용 / 축소 / 폐기` 판정을 남긴다.
- 구조 변경 없이 끝나는 것이 더 옳다면 그 결론도 완료로 인정한다.

# 6. 핵심 도메인 인수 순서

기존 acquisition roadmap의 네 생애주기를 유지하되, 매 단계에 버그/영상/이력서 환전을 붙인다.

## 6.1 트랙 A — 엔티티의 일생

대상 좌표: W01 · W02 · W14.

- 생성, handle/index/generation, component attach, parent/child, destroy, slot reuse를 관통한다.
- stale handle, raw pointer, deferred destroy가 어떤 실패로 보이는지 관측한다.
- 버그 후보는 lifetime trace로 재현한 것만 수정한다.
- 외부 산출물: “댕글링 참조를 generation mismatch라는 관측 가능한 실패로 바꾼 설계” 1개.

## 6.2 트랙 B — 프레임의 일생

대상 좌표: W03 · W13 · W07 · W08 · W11.

- Win32 message pump → Engine update → scene update → render submission → RHI → Present를 관통한다.
- update/render 경계, render-world snapshot, resource lifetime, frame pacing을 설명한다.
- 현재 `profiler.json`의 정적 맵 경로를 이 트랙의 실전 문제로 사용한다.
- 외부 산출물: before/after profiler JSON, 30초 성능 영상, 이력서 bullet 1개.

## 6.3 트랙 C — 명령의 일생

대상 좌표: S01 · S03 · S04 · S07 · S08 · S18.

- Client input → GameCommand → Server/GameRoom → Shared/GameSim → Snapshot/Event → Client apply/visual을 한 기술로 끝까지 관통한다.
- 스킬 데미지·CC·쿨다운은 Shared/Server에서만 진실을 만든다.
- Client visual cue가 snapshot/event를 소비해 한 번만 재생되는지 확인한다.
- SimLab in-process 관측을 우선하고, network F5는 통합 확인에 사용한다.
- 외부 산출물: packet/tick 타임라인 영상과 서버 권위 bullet 1개.

## 6.4 트랙 D — 정의값의 일생

대상 좌표: W12 · W15 · S13.

- authoring JSON/definition → validation → pack/codegen → GameSim consumption → Client visual mapping을 관통한다.
- 하드코딩 상수가 데이터를 덮는 이중 진실을 찾는다.
- save/restore/keyframe과 definition hash가 값 보존을 증명하는지 확인한다.
- 외부 산출물: 데이터 주도 전환 전후 검증 사례 1개.

## 6.5 트랙 E — 자원·FX·UI의 일생

- `Client/Bin/Resource` 기준 texture/wmesh/wfx 로드와 cache/lifetime을 관통한다.
- 서버 cue → 클라이언트 visual → atlas/UI/FX playback을 추적한다.
- 피오라·제드·사일러스 같은 챔피언 작업은 이 트랙에서 “표현”만 다루고 판정·데미지는 트랙 C에 둔다.
- 외부 산출물: skill cue가 권위 상태와 표현으로 분리된 before/after 영상.

## 6.6 트랙 F — 검증·툴의 일생

- SimLab seed/replay/hash, harness, schema/codegen, profiler timeline을 관통한다.
- instrumentation coverage와 실제 최적화 성과를 구분한다.
- 실패하는 probe는 숨기지 않고 owner와 회귀 범위를 기록한다.
- 외부 산출물: “주장을 기계 검증으로 바꾼 방법” bullet 1개.

# 7. HTML W/S 문제 해결 방식

## 7.1 카드 한 장의 종료 조건

카드는 정답을 읽었다고 끝나지 않는다. 다음을 모두 만족해야 `방어 성공`이다.

1. **문제·제약·숫자**: 무엇을 어떤 예산 안에서 해결했는가.
2. **순진한 해법의 실패**: 왜 쉬운 해법이 틀렸는가.
3. **메커니즘**: 실제 타입·함수·데이터 흐름과 BP 근거.
4. **수렴/발산 대조**: 다른 엔진·이전 프로젝트와 무엇이 같고 다른가.
5. **대가·언제 틀리나**: 무엇을 팔았고 어느 조건에서 재설계해야 하는가.
6. 실제 divergence 한 줄.
7. closed-book 90초 설명 성공.
8. 변경이 있었다면 build/test/runtime 증거.

## 7.2 정본 갱신 순서

1. `.md/interview/cards/log.csv`에 관측과 판정을 먼저 남긴다.
2. 정본 JSON의 해당 카드에 실제 코드 근거와 최신 상태를 반영한다.
3. `assemble_books.py`의 입출력 경로를 repo-relative/argument 기반으로 만드는 별도 소규모 계획을 먼저 검증한다.
4. HTML을 재생성한다.
5. 생성 전후 diff에서 해당 카드 외 대규모 drift가 없는지 확인한다.
6. 브라우저에서 카드 접힘·검색·스타일·한글을 확인한다.

## 7.3 우선순위

1. W00 장비 → W01 완료.
2. S00 감사 → S01 30Hz 고정 틱.
3. 현재 성능 병목과 직결된 프레임 좌표.
4. 권위 파이프라인과 직결된 명령 좌표.
5. 정의값·직렬화 좌표.
6. 나머지 카드는 30분 단위 light acquisition 후 심화 필요 여부를 정한다.

# 8. 프로파일링·병목 제거 계획

## 8.1 캡처 계약

최적화 전후 캡처는 다음을 고정한다.

- Debug/Release 여부와 commit SHA.
- 1920×1080, normal F5 network gameplay.
- 같은 맵, 로스터, 봇 수, 카메라 위치·동선, UI/FX 설정.
- 60초 이상, VSync off, limiter 설정 기록.
- Map/Champion/Minion/UI/server tick scope 존재.
- dropped scope/counter/raw event 0.
- GPU source-frame join 유효.

진단 명령:

```powershell
python Tools/Profiler/analyze_profiler_capture.py profiler.json --target-fps 60 --minimum-duration-sec 60 --top 30
python Tools/Profiler/audit_update_render_scopes.py
python -m unittest Tools/Profiler/test_analyze_profiler_capture.py
```

두 번째 명령은 계측 커버리지만 확인한다. scope가 생겼다는 이유로 최적화 완료라고 판정하지 않는다.

## 8.2 현재 병목의 첫 조사 순서

1. `Map::DrawFrustumCulled`의 입력 개수, culled 개수, 제출 개수를 같은 프레임에서 확인한다.
2. `Model::RenderCombinedStatic`이 프레임마다 같은 static data를 재조립·재순회하는지 확인한다.
3. material/subset별 shader, texture, constant buffer, input layout, sampler bind가 중복되는지 확인한다.
4. combined/static 경로가 실제 batch를 만드는지, 이름만 combined이고 draw/state change가 그대로인지 확인한다.
5. occlusion/frustum 결과가 render submission에서 다시 무시되지 않는지 확인한다.
6. `Model::RenderWithMask` 약 40 calls/frame의 호출자·entity kind 분포를 확인한다.
7. 그 뒤에 skinned/minion/champion 비용을 분리한다. 보이는 미니언 수만으로 원인을 단정하지 않는다.
8. CPU 제출 시간을 줄인 뒤 같은 장면에서 GPU p95 24.14ms tail을 별도로 조사한다.

## 8.3 최적화 실험 규칙

한 번에 가설 하나만 검증한다.

| 항목 | 기록 내용 |
|---|---|
| 가설 | 예: static model subset state bind 중복이 CPU render를 지배한다 |
| 예측 | Render median/p95, draw calls, state changes가 얼마나 내려갈지 |
| 변경 | 최소 코드 경로와 제거되는 중복 |
| 동일성 | 픽셀/영상, roster/map/UI/FX, GameSim hash가 유지되는지 |
| 결과 | before/after median·p95·p99·GPU·calls |
| 판정 | 수용, 원복, 추가 계측 |

새 renderer/cache/update loop를 옆에 하나 더 만드는 방식은 금지한다. 기존 경로를 조이고, 새 경로가 필요하면 기존 경로의 삭제 시점과 owner를 먼저 적는다.

## 8.4 단계별 성능 완료 조건

### P0 — 재현 완료

- 1080p 60초 정상 F5 캡처 확보.
- 현재 720p 캡처와 병목 순서가 같은지 확인.
- `17.8ms → 9ms` 과거 주장과 시나리오 차이 판정.

### P1 — 첫 병목 제거

- 같은 시나리오에서 Render p95 20% 이상 감소 또는 해당 가설 기각.
- visual/gameplay 동일성 유지.
- 전후 JSON·영상·commit을 묶음으로 보존.

### P2 — 60fps 안정화

- CPU frame p95 ≤ 16.67ms.
- GPU p95 ≤ 16.67ms.
- p99 ≤ 20.83ms, median에 충분한 headroom.
- dropped record 0, 정상 시스템 전부 활성.

### P3 — 상위 gate

- 120fps/300fps 목표는 P2 이후 별도 시나리오와 예산으로 수행한다.
- 기존 analyzer의 strict hitch/pass 정책을 먼저 감사하고, 통과를 위해 임계값을 몰래 낮추지 않는다.

# 9. 버그 수정과 구조 개선의 결합 규칙

## 9.1 버그 슬라이스

1. 영상 또는 deterministic harness로 증상을 고정한다.
2. 권위 owner를 판정한다.
3. 필요한 경우 Debug-only overlay나 bounded `OutputDebugStringA/W` trace를 추가한다.
4. BP로 실제 잘못된 상태가 처음 생기는 지점을 찾는다.
5. 최소 수정한다.
6. 가장 좁은 자동 검증 → 관련 target build → 정상 F5 순서로 넓힌다.
7. before/after 영상을 같은 카메라와 입력으로 찍는다.
8. 원인과 수정이 이력서 가치가 있으면 evidence ledger에 올린다.

## 9.2 구조 개선 트리거

다음 중 하나가 없으면 구조를 바꾸지 않는다.

- 같은 소유권 혼동으로 버그가 2회 이상 발생.
- 호출부 0/중복 truth/은퇴 경로가 전수 조사로 확인됨.
- profiler가 특정 중복 경로를 병목으로 지목함.
- 계층 위반이 build/lint에서 재현됨.
- 테스트 불가능한 lifecycle이 실제 회귀를 숨김.

구조 계획에는 반드시 다음이 들어간다.

- 현재 owner와 새 owner.
- 유지할 계약과 바뀌는 계약.
- 새 경로와 함께 삭제할 옛 경로.
- dependency 방향.
- 데이터 migration/compatibility.
- 자동 검증과 normal F5 증거.
- rollback 기준.

# 10. 이력서·영상 환전 파이프라인

## 10.1 이력서 단일 원장

- 제출 원장은 `.md/이력서/이력서_MASTER.md`로 유지한다.
- 장문 근거는 `.md/이력서/weapons/04_Winters_LOL.md`와 관련 도메인 문서에 둔다.
- 이력서 본문에는 프로젝트당 가장 강한 4~6개 bullet만 남긴다.
- 지원 직무별로 순서와 상단 요약만 바꾸고 사실 원장은 복제하지 않는다.

bullet 형식:

> `[제약/규모]에서 [문제]를 [구조적 행동]으로 해결해 [같은 조건의 수치/검증 결과]를 만들었다. 대신 [대가/한계]가 있다.`

증거 ledger의 최소 열:

| Claim | Code/Commit | Test command | Before/After JSON | Video | 한계 | 상태 |
|---|---|---|---|---|---|---|

상태는 `초안 / 코드 확인 / 측정 확인 / 영상 확인 / 제출 가능` 다섯 단계로 둔다.

## 10.2 영상 계층

### 레벨 1 — 원시 증거 클립

- 버그 before 5~10초.
- profiler/BP/overlay 근거 5~10초.
- after 5~10초.
- 파일명 또는 메모에 날짜·commit·scenario를 남긴다.

### 레벨 2 — 도메인 설명 클립

- 30~60초.
- 화면: 게임 → BP/프로파일러 → 구조 그림 → 결과.
- 자막은 문제·수치·행동·결과만 쓴다.

### 레벨 3 — 제출 영상

- `.md/이력서/video/2분영상_스크립트.md`를 원장으로 사용한다.
- Winters 30초에는 정상 게임, 서버 권위 한 장면, profiler before/after를 우선한다.
- 현재 근거가 없는 `17.8ms→9ms` 자막은 재현 전까지 확정하지 않는다.

## 10.3 이해를 외부 산출물로 바꾸는 최소 규칙

- BP 사다리 1개 → 구조 그림 또는 카드 1장.
- 버그 수정 1개 → before/after 클립 1개.
- profiler 실험 1개 → 수치표와 이력서 bullet 후보 1개.
- 구조 변경 1개 → ADR형 판단 기록과 면접 답변 1개.
- 일주일 → 제출 가능한 이력서 PDF/문서 갱신과 실제 지원 1건 이상.

# 11. 4주 실행 일정과 외부 마감

지원 공고가 아직 확정되지 않았으므로 아래 날짜를 강제 외부 마감으로 사용한다. 더 이른 실제 공고가 생기면 그 날짜로 당긴다.

## 0차 — 2026-07-17 ~ 2026-07-19

- W00 BP 장비 재현 및 W01 미납 종료.
- S00 현재성 감사 착수, server main lifecycle 사다리 작성.
- 1080p 60초 정상 F5 profiler capture 준비.
- 이력서 기존 성능 claim을 `재검증 중`으로 분류.
- 원시 증거 클립 1개.
- 결과: resume evidence ledger v0와 첫 90초 closed-book 영상.

## 1차 — 2026-07-20 ~ 2026-07-24

- 프레임 트랙 W03/W13 중심 인수.
- 정적 맵 render 병목 가설 1개를 수용 또는 기각.
- before/after JSON과 30초 성능 영상.
- `.md/이력서/이력서_MASTER.md` v1 제출본 정리.
- **2026-07-24까지 실제 외부 지원 또는 제3자 검토 1건**을 발생시킨다.

## 2차 — 2026-07-25 ~ 2026-07-31

- 명령 트랙 S01/S03/S04/S18 관통.
- 한 champion skill을 input부터 visual까지 완주.
- SimLab WIP 실패의 owner와 범위를 정리하고 담당 세션과 합의.
- 서버 권위 60초 설명 영상과 이력서 bullet 완성.
- 성능 case study v1 공개 가능 상태.

## 3차 — 2026-08-01 ~ 2026-08-07

- 정의값 W12/W15/S13 및 resource/FX/UI 트랙 관통.
- HTML 정본·생성기 경로 정리 후 완료 카드 재생성.
- 구조 개선 후보를 `수용 / 보류 / 폐기`로 판정.
- 2분 포트폴리오 영상 rough cut.

## 4차 — 2026-08-08 ~ 2026-08-14

- 남은 핵심 W/S 카드 closed-book 방어.
- 60fps P2 달성 여부 판정. 미달이면 정확한 병목과 다음 예산을 공개한다.
- 이력서 직무 변형본, 2분 영상, Winters 기술 서사 최종화.
- 모의면접 2회, 실제 지원 누적 3건 이상.

# 12. 검증 매트릭스

| 변경 유형 | 가장 좁은 검증 | 통합 검증 | 육안/증거 |
|---|---|---|---|
| 문서/카드 | JSON parse, generator diff | HTML 브라우저 확인 | 90초 closed-book |
| Engine/RHI/Render | 관련 unit/tool + target build | `Winters.sln` Debug x64 | normal F5 + profiler + 영상 |
| Shared/GameSim | 관련 probe | SimLab same-seed/replay | server/client authority trace |
| Server/network | server harness | normal network F5 | packet/tick timeline |
| Client visual/FX/UI | cue mapping 검사 | Client+Server F5 | before/after 영상 |
| 데이터/에셋 | converter/audit | data-driven pipeline | runtime resource path 확인 |
| 성능 | analyzer tests | 1080p 60초 capture | 같은 장면의 전후 영상 |

공통 명령:

```powershell
git diff --check
msbuild Winters.sln /m /p:Configuration=Debug /p:Platform=x64
powershell -ExecutionPolicy Bypass -File Tools/Harness/Check-SharedBoundary.ps1
Tools\Bin\Debug\SimLab.exe 1800 42
python -m unittest Tools/Profiler/test_analyze_profiler_capture.py
python Tools/Profiler/analyze_profiler_capture.py profiler.json --target-fps 60 --minimum-duration-sec 60 --top 30
```

모든 세션에서 전 솔루션 빌드를 무조건 먼저 돌리지는 않는다. 가장 좁은 검증으로 빨리 실패시키고, 구조·공유 계약을 건드렸을 때 전 솔루션으로 넓힌다.

# 13. 병렬 작업과 충돌 방지

- 현재 작업 중인 다른 챔피언/제드 세션의 파일을 먼저 확인하고 같은 파일을 동시에 수정하지 않는다.
- lane을 `Engine render`, `Shared/GameSim`, `Client champion visual`, `Docs/Resume`로 나눈다.
- 공유 헤더·schema·SimLab main·project file은 단일 owner가 잡을 때만 수정한다.
- 다른 세션의 dirty change는 사용자 소유로 간주하고 원복·정리하지 않는다.
- 충돌 가능성이 있으면 구현 대신 BP 관측, profiler 분석, 이력서/영상 작업으로 전환한다.
- 병렬 세션이 끝날 때는 변경 파일, 검증, 미검증, 후속 owner를 handoff에 남긴다.

# 14. 실패 방지 규칙

1. 코드를 읽기만 하고 log/card/video가 없으면 인수로 세지 않는다.
2. BP 미적중은 즉시 dead code 판정이 아니라, 심볼·경로·조건을 확인한 뒤 호출부 0 근거와 함께 판정한다.
3. profiler 부모·자식 inclusive scope를 합산하지 않는다.
4. 짧은 720p 캡처를 이력서 성과 수치로 쓰지 않는다.
5. 성능을 위해 정상 시스템을 숨기지 않는다.
6. Client visual에서 권위 데미지·CC를 만들지 않는다.
7. Shared/GameSim에 Engine/DX/UI 타입을 넣지 않는다.
8. instrumentation 추가만으로 병목을 해결했다고 쓰지 않는다.
9. 생성 HTML을 직접 고쳐 정본과 갈라지게 하지 않는다.
10. 구조 개선이 새 병렬 경로를 만들면 기존 경로 삭제 계획 없이는 진행하지 않는다.
11. `CONFIRM_NEEDED`와 실제 미검증을 이력서에서 확정형으로 바꾸지 않는다.
12. 세 번 연속 깊게 팠다면 네 번째 세션은 반드시 이력서·영상·지원으로 환전한다.

# 15. 바로 시작할 첫 실행 큐

## 세션 1 — W00/W01 닫기

- `ladder_frame.xml` 로드.
- W01 Create/Destroy/slot reuse/generation mismatch 재현.
- `m_Parent`와 divergence 답변 완료.
- `log.csv` 갱신.
- 90초 설명 녹화.

## 세션 2 — 프로파일 1080p 재현

- 현재 `profiler.json` 시나리오를 기록.
- 같은 normal F5를 1080p 60초로 재캡처.
- CPU/GPU/top inclusive scopes 대조.
- static map render 가설을 첫 child investigation으로 고정.
- 기존 이력서 수치의 유효 범위 판정.

## 세션 3 — S00/S01 관통

- server main lifecycle BP.
- 30Hz fixed tick과 command processing 관측.
- 구 S00 계획을 현재 코드와 대조.
- extraction 여부 판정.
- 서버 권위 60초 설명 클립.

## 세션 4 — 첫 병목 수정과 환전

- static render 가설 하나만 실험.
- 관련 build → normal F5 → profiler before/after.
- 결과가 좋아도 나빠도 가설 판정을 남긴다.
- 이력서 bullet 후보와 30초 영상 생성.

# 16. Verification / Handoff

이 계획 자체의 완료 조건은 다음과 같다.

- 각 도메인 작업이 BP·수정·검증·외부 산출물의 동일 루프를 따른다.
- 70:30 예산이 주간 결과에서 실제로 보인다.
- W00/S00은 부트스트랩으로 닫히고 실제 카드는 W01/S01부터 방어된다.
- profiler 작업은 `profiler.json`의 현재 수치와 1080p 60초 후속 캡처를 같은 기준으로 비교한다.
- 이력서 claim마다 코드, test, capture, video, 한계가 연결된다.
- 구조 변경마다 owner·삭제 경로·검증·rollback이 있다.
- 2026-07-24 첫 외부 제출/검토, 2026-08-14 4주 결과물이라는 외부 마감이 유지된다.

각 구현 세션의 handoff에는 아래 여섯 줄을 남긴다.

1. 관통한 좌표와 BP chain.
2. 예상과 달랐던 사실.
3. 변경 파일과 owner.
4. 통과한 자동 검증과 normal F5 범위.
5. 남은 미검증·충돌 위험.
6. 만들어진 이력서 문장·영상·카드·지원 산출물.
