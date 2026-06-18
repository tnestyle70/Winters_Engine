Session - 프레임 드랍 제거와 롤급 조작감 개선 인덱스

1. 반영해야 하는 코드

1-1. 세션 인덱스

기존 코드:
```text
없음. 이 문서는 성능/조작감 개선 세션의 진입점이다.
```

아래에 추가:
```text
최종 목표:
- 실전 F5 흐름에서 60 FPS 이상을 안정적으로 유지한다.
- 120/144 FPS에서도 프레임 타임 변동이 플레이 감각을 망치지 않게 한다.
- 300 FPS는 최종 보장치가 아니라 CPU/GPU 병목을 드러내는 압박 테스트로 사용한다.
- 서버 권위 흐름은 유지한다.
  Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual
- 클라이언트는 입력 즉시 반응, 약한 예측, 보간, UI/FX/카메라 표현만 담당한다.

현재 관측:
- 초기 병목은 FxBeam, 모델 로드, 스케줄러 오버헤드, 미니언 애니메이션, UI/ImGui가 섞여 있었다.
- FxBeam 배치, 스케줄러 실행 계획 캐시, 미니언 애니메이션 예산화, 프로파일러 Freeze/Sample 이후 단일 10ms 병목은 줄었다.
- 현재는 Update 10~15ms, Render 6~9ms, UIOverlay 1~2ms, Minion::AnimUpdate 스파이크가 프레임 드랍의 주된 후보이다.
- "부드러운 조작감"은 평균 FPS보다 입력 반응, 프레임 페이싱, 보간/예측/보정의 일관성에 더 민감하다.

세션 순서:
S01 - 프레임 페이싱과 프로파일러 기준선 고정
문서: .md/plan/performance/01_FRAME_PACING_PROFILER_BASELINE_PLAN.md
목표: VSync/targetFPS/무제한 모드를 명령행에서 고정하고, CPU 프레임과 limiter 상태를 매번 같은 조건으로 비교한다.

S02 - Update 스파이크와 애니메이션 예산 정리
문서: .md/plan/performance/02_UPDATE_SPIKE_BUDGET_PLAN.md
목표: Minion::AnimUpdate, Scheduler, Vision, Spatial 계열을 프레임 예산형으로 만들고 Update 16.67ms 초과를 먼저 제거한다.

S03 - UI/ImGui/RHI HUD 비용 절감
문서: .md/plan/performance/03_UI_RENDER_IMGUI_COST_PLAN.md
목표: 게임 HUD는 RHI 경로에서 배치하고, ImGui는 디버그/프로파일러 영역으로 제한한다.

S04 - 입력 반응과 카메라 감각 개선
문서: .md/plan/performance/04_INPUT_CAMERA_FEEL_PLAN.md
목표: 우클릭 이동/공격 입력이 같은 프레임에 시각 피드백을 내고, 카메라는 서버 보정/스냅을 숨길 정도로 부드럽게 따라간다.

S05 - 클라이언트 예측과 서버 스냅샷 보정
문서: .md/plan/performance/05_CLIENT_PREDICTION_RECONCILIATION_PLAN.md
목표: 이동 명령을 예측 버퍼에 기록하고, lastAckedCommandSeq 기준으로 서버 위치와 부드럽게 수렴한다.

S06 - 랙 보정과 플레이 감각 QA 루프
문서: .md/plan/performance/06_LAG_COMPENSATION_AND_FEEL_QA_PLAN.md
목표: CommandBatch timestamp, 서버 수신 시간, rewindTicks, LagCompensation을 실제 판정과 디버그 지표로 연결한다.

S07 - Client visual AOI로 화면 밖 update/render 후보 줄이기
문서: .md/plan/performance/07_CLIENT_VISUAL_AOI_PLAN.md
목표: 서버 권위 계산은 유지하고, 클라이언트 visual update/render/interpolation 후보만 카메라 AOI 기준으로 줄여 `Scene_InGame::OnUpdate`, `Minion::AnimUpdate`, `Champion::AnimUpdate`, champion render 비용을 낮춘다.

S08 - 미니언 웨이브 스폰 hitch와 visual binding 비용 제거
문서: .md/plan/performance/08_MINION_SPAWN_HITCH_VISUAL_BINDING_PLAN.md
목표: 서버 웨이브 생성은 유지하고, 클라이언트 미니언 renderer/Animator 생성만 프레임 예산형 queue, model prewarm, renderer pool로 분산해 첫 웨이브 순간 프레임 드랍을 제거한다.

프레임 예산:
- 60 FPS: 16.67ms
- 120 FPS: 8.33ms
- 144 FPS: 6.94ms
- 300 FPS: 3.33ms

우선 합격선:
- 프로파일러 Overlay가 열린 상태에서도 입력 조작이 가능해야 한다.
- 프로파일러 Overlay를 닫은 상태에서 평균 60 FPS 이상을 먼저 고정한다.
- 일반 전투 화면에서 Frame p95가 16.67ms를 넘지 않는다.
- 우클릭 이동 시 로컬 플레이어 회전/이동 애니메이션 반응이 1 프레임 이내에 보인다.
- 서버 스냅샷 수신 시 로컬 플레이어가 순간이동하거나 반 바퀴 뒤집히지 않는다.
```

2. 검증

```text
공통 검증:
- git diff --check
- Engine/Include/Engine.vcxproj Debug x64 빌드
- Client/Include/Client.vcxproj Debug x64 빌드 또는 해당 cpp 단위 컴파일
- 필요 시 Server/Include/Server.vcxproj Debug x64 빌드
- Client/Bin/Resource 기준 런타임 리소스만 사용
- Engine 빌드 후 SDK 헤더/라이브러리 동기화 여부 확인

프로파일러 검증:
- F3 Profiler Live/Freeze/Sample 동작 확인
- Overlay Open/Closed 각각 Frame, Update, Render, UIOverlay, Minion::AnimUpdate 캡처
- 99개 내외 엔티티, 전투 FX 있음, 미니언 웨이브 있음 기준으로 캡처

플레이 감각 검증:
- 우클릭 연타, 공격 클릭, 스킬 입력, 카메라 추적, 서버 스냅샷 수신 중 끊김 확인
- localhost 네트워크와 지연/패킷 흔들림 테스트를 분리해서 기록
```
