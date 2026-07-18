Session - S041 · Winters Extreme Profiling → Verified Optimization → Resume/Notion Evidence Pipeline
좌표: Winters 정상 F5 LoL DX11 클라이언트 · 축: C1 측정 신뢰성 → C2 병목 귀속 → C3 실제 최적화 → C4 외부 증거화
관련: `.md/plan/2026-07-17_WINTERS_ENGINE_BP_RESUME_PROFILING_MASTERY_LOOP_PLAN.md` · `.md/plan/2026-07-17_S040_SESSION_HANDOVER.md` · Notion `홍건보 / 포트폴리오`

# Winters 극한 프로파일링·실최적화·이력서/Notion 파이프라인

> 상태: **실행 계획 확정 / 코드 최적화 미착수 / 현행 성능 주장은 증거 재검증 전까지 격리**
> 목표: CPU·GPU·SIMD·DrawCall 중 실제 병목을 재현 가능한 절차로 좁히고, 한 번에 한 가설만 최적화한 뒤, 수치·캡처·영상·이력서 문장을 같은 증거 묶음으로 남긴다.
> 원칙: 정상 F5의 맵·로스터·미니언·스냅샷·챔피언·UI·FX를 끄지 않는다. 실험실 격리는 원인 증명용 보조 실험일 뿐 최종 수치가 아니다.

---

## §1 결정 로그

① 현행 `profiler.json`은 1280×720·300프레임·15.861초·Debug이며 Frame p50/p95/p99=`51.0615/68.6443/92.6615ms`, Render=`38.3984/51.4529ms`, Update=`11.4020/17.0965ms`, GPU=`10.7655/24.1383ms`, `Map::Render` p50=`28.1427ms`, `Model::RenderCombinedStatic` p50=`28.0442ms`; 정상 F5 스코프는 있으나 1080p·60초·최적화 빌드 게이트를 통과하지 못했다.
② 현재 `WINTERS_PROFILING`은 Debug에서만 정의되고 Debug 최적화는 Disabled이며 DX11 debug layer도 활성화된다. Release는 최적화되지만 계측이 제거되므로 현재 수치로 코드 병목을 확정하면 관측 오염을 병목으로 오인할 수 있다.
③ 먼저 별도 출력 경로의 **최적화된 Profile 빌드**와 probe-effect 비교를 만든 뒤, 1080p·60초 이상·3회 반복·동일 시나리오에서 PresentMon + JSON + CPU sampler + GPU capture를 계층적으로 사용한다.
④ `Mesh::DrawCalls`는 혼합 legacy/RHI 경로 전체를 세지 못하므로 외부 API 캡처 전까지 “전체 드로우콜”로 쓰지 않는다. D3D11 pipeline statistics는 작업량 증거이고 GPU 시간 증거가 아니다.
⑤ `17.8ms → 9ms`, `16ms=90%`, `9.54ms/~94 draw`는 원본 캡처·시나리오·빌드 계약을 다시 통과하기 전까지 **historical/unverified**로 강등한다. 중단 조건은 재현 불가, 계측 오버헤드 p95 >3% 또는 0.5ms, 시각/게임플레이 회귀, 3회 중앙값 개선 <3%다.

---

## §2 반영해야 하는 코드와 산출물

### 2-0. 완료의 정의

이 계획은 다음 다섯 묶음이 모두 있어야 완료다.

1. **측정 신뢰성**: optimized Profile 빌드, 명시적 시나리오 manifest, 60초 이상 3회 반복, 계측 오버헤드 비교.
2. **병목 귀속**: CPU/GPU/sync/draw/SIMD/memory 중 주 병목과 보조 병목을 외부 도구로 교차 검증.
3. **실제 최적화**: 한 가설당 독립 변경, A/B/A 또는 최소 Before 3회·After 3회, 절대 ms와 비율을 함께 기록.
4. **눈 검증**: 동일 카메라 Before/After 영상, GPU frame 캡처, 픽셀·애니메이션·게임플레이 체크리스트 통과.
5. **환전 산출물**: 결과 문서, 원본 JSON/CSV/trace, 30~60초 영상/GIF, Notion case study, 이력서 1~2줄, 면접 답변.

계측 인프라만 추가하거나 스코프 개수를 늘린 것은 최적화 완료로 세지 않는다.

### 2-1. 현행 코드 증거와 판결

| 증거 | 현행 사실 | 이번 계획의 판결 |
|---|---|---|
| `Engine/Include/ProfilerAPI.h` | 한 스코프가 Tracy zone과 자체 CPU scope를 함께 기록 | 내부 JSON은 회귀 게이트, Tracy/ETW는 상세 귀속에 사용 |
| `Engine/Private/Core/Profiler/CPUProfiler.cpp` | `PushScope`는 fiber-local stack이지만 `PopScope`는 완료 스코프마다 전역 mutex로 병합 | “수집 전체가 lock-free” 주장은 부정확. Profile/Clean Release 오버헤드부터 측정 |
| `Engine/Public/Core/Profiler/ProfilerTypes.h` | event/counter/stat/history cap이 있고 raw tree cap은 별도 | dropped/truncated=0을 모든 합격 캡처의 필수 조건으로 유지 |
| `Engine/Private/Manager/Profiler/ProfilerOverlay.cpp` | F3 overlay, F12 JSON, schema v3, async timeline save | acceptance 동안 overlay는 닫고 F12만 사용. 저장 직전/직후 프레임은 분석 구간에서 제외 검토 |
| `Engine/Private/Framework/CEngineApp.cpp` | Frame/Update/Render/LimiterWait와 cadence/wall gauges 기록 | 전체 프레임과 CPU phase의 기준 clock으로 사용 |
| `Engine/Private/RHI/DX11/CDX11Device.cpp` | 4-slot nonblocking whole-frame timestamp/disjoint query | GPU 전체 시간의 1차 신호. pass별 GPU 병목은 외부 GPU profiler로 확정 |
| `Engine/Include/Engine.vcxproj`, `Client/Include/Client.vcxproj` | Debug만 profiling, Release만 optimization | 최우선 구현 슬라이스는 별도 output의 optimized Profile 계약 |
| `Engine/Private/Resource/Model.cpp` | static combined range가 인접 동일 텍스처만 병합, bind 변경 뒤 range draw | 현재 Debug 캡처의 첫 **가설 후보**이지 아직 최적화 확정 대상은 아님 |
| `Engine/Private/Renderer/ModelRenderer.cpp` | shader/pipeline/CB/AO bind와 combined static 경로가 CPU render에 포함 | Profile baseline에서도 상위면 state/draw submission을 외부 도구로 분해 |
| `Tools/Profiler/analyze_profiler_capture.py` | 1080p/60초/정상 스코프/GPU/server gate가 있으나 hitch threshold가 120Hz로 고정 | 60Hz 합격과 120Hz ceiling을 분리하는 정책 수정 필요 |
| `.md/build/2026-07-13_S019_PROFILER_TIMELINE_ANALYSIS.json` | main menu/empty scene·0.245ms median·server tick 없음 | 정상 F5 성능 증거로 사용 금지 |

### 2-2. 소유권 경계

```text
Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual

Shared/GameSim     : authoritative gameplay truth, 렌더/프로파일러 의존 금지
Server             : 30Hz tick·snapshot/event·AI command 생산, Client visual 의존 금지
Engine             : profiler/RHI/resource/render primitives와 공용 성능 카운터
Client             : 정상 F5 scenario·camera·UI/FX 재생과 제품별 render scopes
Tools/Profiler     : 캡처 분석·게이트·보고서 생성; 런타임 truth를 변경하지 않음
Evidence/Notion    : 결과를 소비할 뿐 코드의 성능 truth를 정의하지 않음
```

성능 수치를 좋게 만들기 위해 정상 F5의 로스터·맵·미니언·서버 스냅샷·챔피언·UI·FX를 끄지 않는다. 서버 tick 성능과 클라이언트 frame 성능은 같은 실행에서 함께 보되, 서로 다른 clock/budget으로 판정한다.

#### 2-2-1. 동시 진행 중인 Claude 세션 대조

2026-07-17 17:19 KST 기준 Claude Desktop 세션 `53cd1bba-7cf3-4fb1-847e-184f2520fd14`도 같은 사용자 요청을 받고 5개 read-only 감사를 병렬 실행 중이었다. 관찰 당시 branch 표시는 `codex/2026-07-16-replay-backend-worktree`였고, 최종 계획/소스 수정은 아직 기록되지 않았다.

| Claude 감사 축 | 확인한 결론 | 이 계획에 반영한 위치 |
|---|---|---|
| profiler/build | Debug-only profiling, Release-only optimization, per-scope `PopScope` mutex, GPU whole-frame only | §2-1, §2-3 |
| render/draw | legacy `DX11Buffer` + RHI command-list 두 funnel과 scattered raw draw, real instancing 없음, state bind 다수 | §2-5-4, §2-6 |
| SIMD/hot-loop | first-party intrinsics·`/arch:AVX2` 없음, DirectXMath 경계 load/store, animation/FX/culling/server AI가 정적 후보 | §2-5-6, §2-6 |
| reproducibility | network client/server, `--banpick-smoke`, WRPL replay, SimLab/soak 경로 존재; client timed auto-exit 없음 | §2-4 |
| prior docs/resume | `17.8→9ms`, `9.54ms/~94 draw`, 이론 300~650 FPS 문서가 공존하고 근거 수준이 섞임 | §2-9 |

교차 세션 규칙:

- Claude 감사 결과는 후보와 탐색 지도를 넓히는 입력이지 runtime 판결이 아니다.
- Claude가 이후 별도 계획서를 만들면 두 문서를 그대로 병행 실행하지 않는다. 이 문서의 measurement/evidence 계약을 canonical로 두고, Claude 문서의 더 정확한 코드 anchor만 병합한다.
- `vcxproj`, analyzer, profiler core, render funnel은 한 세션만 소유한다. 다른 세션은 그 사이 read-only review와 결과 대조만 한다.
- 코드 착수 직전 Claude 세션의 최종 출력과 `git diff`를 다시 읽어 같은 파일 동시 편집을 막는다.

### 2-3. Phase 0 — 측정 장비를 먼저 교정한다

#### 2-3-1. Optimized Profile 빌드 계약

필수 속성:

- C/C++ 최적화는 Release와 동일(`/O2`, 인라이닝, LTCG 여부 포함).
- `_DEBUG`와 D3D11 debug layer는 비활성.
- `WINTERS_PROFILING`과 Tracy zone은 활성.
- PDB와 함수/라인 심볼은 보존.
- normal Release와 다른 `OutDir/IntDir`을 사용해 배포 바이너리를 덮어쓰지 않음.
- Engine/Client/Server/Shared 및 SDK 복사 규칙이 한 configuration 이름으로 일치.
- 빌드 산출물에 commit, configuration, compiler flags를 manifest로 남김.

**구현 전 확인 필요:** `Winters.sln`의 모든 프로젝트 구성, `Server/Shared/Tools` 프로젝트의 configuration matrix, `UpdateLib.bat`의 Debug/Release 분기, Profile용 Bin/Lib/PDB 출력 경로를 전수 확인한 뒤 별도 v2 코드 계획을 쓴다. 일부 프로젝트만 Profile이면 ABI/CRT/PDB가 섞이므로 즉시 중단한다.

#### 2-3-2. Probe-effect 게이트

같은 commit과 scenario에서 다음 두 빌드를 비교한다.

| 빌드 | 최적화 | 자체 프로파일러/Tracy | 용도 |
|---|---:|---:|---|
| Clean Release | ON | OFF | 실행 성능의 외부 기준 |
| Profile | ON | ON | 내부 scope/counter와 trace |

합격 조건:

- PresentMon FrameTime p95 차이: `max(3%, 0.5ms)` 이내.
- 평균/median만 통과하고 p95/p99가 악화되면 실패.
- Profile 캡처의 `Profiler::ScopeCalls`, `CounterCalls`, `GaugeCalls`, `PreviousEndFrameUs`를 보존.
- `PreviousEndFrameUs`는 현재 매-scope `PopScope` mutex 비용 전체를 포함하지 않으므로 “프로파일러 총 오버헤드”로 쓰지 않음.
- 실패하면 먼저 scope density와 `PopScope` 병합 구조를 계측/재설계한다. 게임 코드를 최적화하지 않는다.

#### 2-3-3. 분석기 게이트 교정

`Tools/Profiler/analyze_profiler_capture.py`의 목표는 두 개로 분리한다.

- **Floor / 60Hz 제출 게이트**: Frame p95 ≤16.67ms, p99 ≤20.83ms, GPU p95 ≤16.67ms.
- **Ceiling / 120Hz headroom 게이트**: Frame p95 ≤8.33ms, p99 ≤10.00ms, GPU p95 ≤8.33ms.

현행처럼 `--target-fps 60`에도 모든 frame을 8.33ms hitch 기준으로 검사하지 않는다. target별 budget을 계산하고, regression threshold와 target threshold를 별도 필드로 출력한다. 기존 v1/v2/v3 fixture 호환 테스트를 유지한다.

### 2-4. Phase 1 — 재현 가능한 정상 F5 baseline

#### 2-4-1. 고정 시나리오 계약

`profile_run_manifest.json`에 아래를 반드시 기록한다.

| 범주 | 고정값/기록값 |
|---|---|
| 해상도 | 1920×1080, window mode 명시 |
| 시간 | warm-up 30초 후 측정 60초 이상 |
| 반복 | Before 3회, After 3회; 실행 순서와 중앙값 run 명시 |
| frame control | F11 limiter OFF, Present VSync OFF, foreground |
| scene | 정상 network F5, 동일 map/roster/bot defaults |
| camera | spawn → 고정 waypoint/camera yaw-pitch-distance 또는 replay |
| gameplay | 동일 입력/replay seed, 미니언·챔피언·UI·FX 유지 |
| hardware | CPU/GPU/RAM, driver, OS build, 전원 모드, monitor Hz |
| build | commit, dirty flag, compiler, configuration, EXE/DLL hashes |
| capture | JSON, PresentMon CSV, CPU trace, GPU frame capture 파일명 |
| environment | debugger/overlay off, recording on/off를 별도 run으로 구분 |

원본 수치용 run과 영상 녹화 run은 분리한다. 화면 녹화 오버헤드가 있는 결과를 성능 truth로 쓰지 않고, 같은 commit의 시각 증거로만 연결한다.

#### 2-4-1A. 재현 경로의 등급

Claude 정적 감사에서 다음 실행 경로가 확인됐다. 서로 목적이 다르므로 수치를 섞지 않는다.

| 등급 | 경로 | 용도 | 최종 이력서 수치 |
|---|---|---|---|
| A | 정상 Server + Client network F5 | 제품 전체 부하와 authoritative flow acceptance | **가능** |
| B | WRPL replay playback | 동일 snapshot/event stream의 render workload 반복·A/B 진단 | 보조 근거만 가능 |
| C | `--banpick-smoke --smoke-full-ingame --smoke-full-map ...` | 자동 진입·smoke·장면 준비 | 정상 시스템 유지가 증명될 때만 보조 |
| D | SimLab/soak/headless/lab hotkey | GameSim 결정성·서버 tick·개별 기능 검증 | client frame 대표 수치로 사용 금지 |

WRPL은 world-state stream은 반복하지만 playback이 wall-clock `dt`를 사용하므로 frame work가 bit-identical하지 않다. 고정 `--fps` 분포 비교용이며, 최종 acceptance는 uncapped normal network F5로 다시 통과한다. 현재 client에는 timed auto-exit flag가 없으므로 외부 watchdog을 쓰거나 별도 최소 구현을 검토하되, 강제 종료 시 JSON flush/trace 저장 완료를 확인한다.

#### 2-4-2. 같은 장면을 눈으로 재현하는 방법

1. replay 또는 고정 입력 script가 있으면 seed·tick·camera keyframe을 manifest에 기록한다.
2. 없으면 최소한 spawn 후 대기 시간, 이동 경로, 카메라 transform, 관찰 구간을 영상 첫 5초에 overlay로 노출한다.
3. Before/After는 같은 frame/tick을 나란히 배치한다.
4. vegetation, shadows, particles, champion/minion counts, UI, damage/FX cue를 체크리스트로 비교한다.
5. RenderDoc은 draw/state/texture/pixel correctness 확인에 사용하고 GPU 시간의 유일한 근거로 쓰지 않는다.

### 2-5. Phase 2 — 원인을 좁히는 결정 트리

```text
PresentMon frame delivery가 나쁜가?
├─ 아니오 -> 체감 문제는 simulation cadence/input/network/animation pacing 분기
└─ 예
   ├─ CPU app frame ≈ frame time, GPU가 충분히 짧음 -> CPU/sync 병목
   │  ├─ Update 상위 -> gameplay/animation/ECS/server-client work 분석
   │  ├─ Render CPU 상위 -> draw submission/state/resource/driver 분석
   │  └─ Limiter/Present/ready time 상위 -> pacing/OS/lock/context-switch 분석
   ├─ GPU busy ≈ frame time, CPU submit이 짧음 -> GPU 병목
   │  ├─ vertex/VS workload 상위 -> mesh/skin/vertex fetch/SIMD와 분리
   │  ├─ raster/PS 상위 -> overdraw/resolution/shadow/post/particle 분석
   │  └─ bandwidth/occupancy 상위 -> texture/RT/format/shader 분석
   ├─ CPU와 GPU 모두 김 -> independent 또는 pipeline-coupled 병목, 큰 쪽부터 한 축씩
   └─ 둘 다 짧은데 display가 김 -> VSync/queue/present/display/recording 외부 요인
```

#### 2-5-1. Layer A — 프레임 전달/페이싱

도구와 질문:

- **PresentMon**: CPU start/duration, GPU duration, display latency와 dropped/present mode를 본다.
- 자체 JSON: `Frame`, `LimiterWait`, `Frame::CadenceUs`, `Frame::WallElapsedUs`, `Frame::TargetUs`, `Frame::PresentVSync`를 대조한다.
- 평균 FPS보다 p50/p95/p99, 1% low, longest 10 frames를 먼저 본다.
- 16.67ms 근처 톱니면 limiter/VSync, 주기적 큰 spike면 streaming/GC/alloc/network cadence, 지속적인 긴 frame이면 CPU/GPU throughput을 의심한다.

#### 2-5-2. Layer B — CPU coarse attribution

내부 JSON의 필수 scope/counter:

```text
Frame
  PumpMessages
  Timer
  ServiceTick
  Update
    Scene_InGame::OnUpdate
  Render
    Scene_InGame::OnRender
    Map::Render
    Champion::Render
    Minion::Render
    UIOverlay::Render
  LimiterWait

Server::Tick / Server tick span
Profiler::ScopeCalls / CounterCalls / GaugeCalls / PreviousEndFrameUs
```

- 부모와 자식 inclusive 시간을 더하지 않는다.
- top parent를 고른 뒤에만 하위 scope를 추가한다.
- 현재 110개 update/render 함수의 heuristic scope coverage 24.66%는 감사 정보이지 acceptance gate가 아니다.
- 캡처마다 required scope coverage ≥95%, GPU valid coverage ≥90%, server tick coverage ≥90%, dropped/truncated=0을 요구한다.
- 현행 Server/GameSim 프로젝트에는 `WINTERS_PROFILING`이 없어 client JSON의 server tick은 snapshot/cadence envelope이지 **서버 CPU 함수 귀속**이 아니다. 서버 CPU 최적화는 별도 optimized server profiling 또는 ETW process capture 없이는 주장하지 않는다.

#### 2-5-3. Layer C — CPU fine attribution

CPU 병목일 때만 순서대로 사용한다.

1. Visual Studio CPU Usage 또는 ETW/WPA sampling으로 Release/Profile call tree의 self time을 확인.
2. Tracy timeline으로 main/worker thread의 겹침, idle, lock, frame spike를 확인.
3. WPA CPU Usage(Precise)/Concurrency Visualizer로 context switch, ready time, synchronization을 확인.
4. native memory profiler/ETW allocation은 sampling이 allocation hot path를 가리킬 때만 사용.
5. 함수 이름이 아니라 **self time + 호출 횟수 + 입력 크기**를 함께 기록한다.

CPU Render가 크고 GPU가 짧다면 GPU 최적화가 아니라 driver/API submission, texture resolve, state binding, draw loop, debug layer 유무를 먼저 조사한다.

#### 2-5-4. Layer D — DrawCall과 submission

현재 내부 카운터의 역할:

| 카운터 | 의미 | 제한 |
|---|---|---|
| `Mesh::DrawCalls` | `DX11Buffer` 계열 일부 draw | UI/PostFx/Plane/Fog/RHI direct path 전체가 아님 |
| `Mesh::SubmittedIndices` | 해당 경로가 제출한 index volume | 실제 shaded pixel/overdraw를 말하지 않음 |
| `Model::CombinedDrawCalls` | combined static range draw | 전체 API draw가 아님 |
| `Model::MaterialBinds` | combined path texture/material change | sampler/CB/pipeline 전체 state change가 아님 |
| `RHI::SceneDrawCalls` | RHI scene path draw | 현행 `RHISceneOnly=0` 혼합 경로의 canonical total이 아님 |

따라서 “전체 draw N회”는 Visual Studio GPU Usage, Nsight Graphics 또는 RenderDoc event list의 API draw count로 확정한다. 장기적으로 canonical RHI/device boundary counter가 필요하면 direct draw 경로를 전수 감사한 별도 구현 계획을 만든다. 카운터를 맞추기 위해 렌더러를 이중화하지 않는다.

D3D11 `D3D11_QUERY_DATA_PIPELINE_STATISTICS`는 IA vertices/primitives, VS/PS invocations 같은 **작업량 변화**를 증명하는 보조 계측으로만 쓴다. 쿼리 범위가 GPU pipeline을 stall하지 않는 nonblocking readback인지 검증하기 전 상시 삽입하지 않는다.

#### 2-5-5. Layer E — GPU

1. 자체 `GPU::FrameUs`로 whole-frame GPU 병목 여부를 먼저 판단한다.
2. Visual Studio GPU Usage 또는 NVIDIA Nsight Graphics로 top event/pass, CPU-GPU sync, draw별 duration을 확인한다.
3. Nsight GPU Trace는 지원 GPU에서 shader/pixel/ROP/memory unit utilization을 확인할 때 사용한다.
4. GPU 병목이 확정된 pass에만 marker/per-pass timestamp를 추가한다.
5. 해상도 1080p→720p 축소 시 GPU 시간이 비례해 줄면 pixel/bandwidth 계열, 거의 안 줄면 geometry/CPU/sync 계열이라는 보조 실험을 한다. 단 최종 결과는 1080p 정상 F5만 인정한다.

GPU 최적화 후보는 overdraw, shadow/post target, shader invocation, texture bandwidth, skinning/vertex fetch, state change 순으로 **캡처가 지목한 것만** 연다.

#### 2-5-6. Layer F — SIMD

SIMD는 CPU sampler가 연산 hot loop를 지목한 뒤에만 연다.

1. Profile/Release의 `/Qvec-report:2`로 vectorized/non-vectorized loop와 이유를 저장한다.
2. compiler explorer가 아니라 실제 빌드 PDB/disassembly에서 SIMD instruction과 loop body를 확인한다.
3. data layout, alignment, aliasing, branch, trip count, gather/scatter 비용을 기록한다.
4. scalar와 vector path를 같은 입력에서 benchmark하고 output hash/epsilon을 비교한다.
5. GameSim 결정성에 닿는 float 연산은 300-tick hash/replay를 통과하기 전 채택하지 않는다.
6. 개선은 hot loop 자체뿐 아니라 전체 Frame/Update p95에도 나타나야 한다. 전체 개선 <3%면 이력서 대표 수치로 승격하지 않는다.

`/Qvec-report:1`은 vectorized loop만, `:2`는 vectorized되지 않은 loop와 이유까지 보여준다. 보고서만으로 속도를 증명하지 않고 time과 결과 동일성을 함께 요구한다.

Claude 정적 감사 기준으로 first-party `_mm_*`/`__m128` 직접 intrinsics와 `/arch:AVX2` 설정은 없고, `Vec3` 계열 scalar storage와 DirectXMath transient compute의 load/store 경계가 주 형태다. animation pose, transform, clip-space bounds, FX billboard, server AI가 후보로 보이지만 이는 occurrence/code-shape 지도일 뿐 성능 순위가 아니다. sampler가 지목하지 않은 loop를 SIMD로 재작성하지 않는다.

#### 2-5-7. Layer G — memory/cache/allocation

- CPU sampling에서 allocator/memcpy/cache-sensitive loop가 상위일 때만 조사한다.
- allocation count/bytes/frame, working set, page fault, cache miss를 서로 다른 지표로 구분한다.
- pooling/arena/SoA는 “그럴듯한 구조”가 아니라 call count·miss·bytes가 지목할 때만 적용한다.
- 새 cache/renderer/data owner를 만들기 전 기존 중복 경로의 삭제/통합 경계를 먼저 제시한다.

### 2-6. Phase 3 — 첫 병목 후보와 실험 순서

현행 Debug 캡처의 관찰:

```text
Map::MeshCount                       1080
Model::ClipBypassSubmeshes           1080
Mesh::DrawCalls                       432   (partial)
Mesh::SubmittedIndices            2,054,427
Model::VisibleMeshCalls              1,118
Model::CombinedDrawCalls               394
Model::MaterialBinds                   258
Model::ClipVisibleSubmeshes             18
Model::ClipRejectedSubmeshes              0
```

현재 가설은 “GPU shader가 아니라 Debug/D3D11 debug layer가 포함된 static map CPU submission이 Render를 지배한다”이다. **Profile baseline이 같은 순위를 재현하기 전에는 판결이 아니다.**

Claude 감사가 추가로 찾은 FX per-billboard draw/texture-key construction, off-screen animation pose, clip AABB 8-corner transform, server `CollectSorted`/blocker allocation, ECS type lookup도 정적 후보일 뿐이다. Profile baseline의 top self-time과 runtime cardinality가 먼저다. 서로 다른 후보를 한 변경에 묶지 않는다.

Profile에서도 `Map::Render`/`Model::RenderCombinedStatic`가 Frame p95의 30% 이상이고 GPU p95가 CPU Render p95보다 충분히 짧을 때 다음 순서로 실험한다.

1. 외부 GPU event list로 actual draw/state count와 top duration을 확정.
2. CPU sampler로 `texture resolve → state bind → RenderIndexRange/DrawIndexed` self time을 분리.
3. combined range의 material adjacency와 동일 material 비연속 range 수를 offline report로 산출.
4. 기존 combined path 안에서 중복 texture resolve/bind 또는 불필요 draw만 제거.
5. 같은 visible set/index count/pixel output에서 draw/material bind/CPU Render p95 변화를 비교.

금지:

- 정상 F5 맵을 숨겨 수치 만들기.
- 두 번째 static renderer/cache/data owner 추가.
- GPU 병목 증거 없이 LOD/해상도/quality를 낮추기.
- draw count만 줄고 Frame p95가 개선되지 않은 변경을 “최적화 성공”으로 기록.

### 2-7. Phase 4 — 한 가설 한 변경 실험 규격

각 최적화는 아래 카드 한 장으로 관리한다.

```text
Experiment ID:
Commit / dirty state:
Scenario manifest hash:
Hypothesis:
Expected metric and direction:
Owned files:
Before run IDs (3):
After run IDs (3):
CPU p50/p95/p99:
GPU p50/p95/p99:
Present/display p50/p95/p99 and 1% low:
Actual API draws / material binds / submitted work:
Server tick p95/p99:
Profiler overhead:
Visual/replay/hash verification:
Result: ACCEPT / REJECT / INCONCLUSIVE
Rollback commit:
Resume claim state:
```

판정 규칙:

- Before/After의 중앙값 run을 대표로 쓰되 세 run 전체를 보존.
- 개선량은 `old_ms - new_ms`, `percentage`, FPS 환산을 함께 기록. FPS만 단독으로 쓰지 않음.
- p50이 좋아지고 p95/p99가 나빠지면 실패.
- CPU 개선이 GPU bound 전환을 만들었으면 성공으로 기록하되 새 critical path를 명시.
- visual/hash/replay 중 해당 도메인 검증이 실패하면 성능 개선과 무관하게 reject.
- A/B/A에서 원복 시 수치가 복귀하지 않으면 환경 노이즈 또는 비독립 실험으로 판정.

### 2-8. Phase 5 — 눈으로 검증하는 증거 묶음

한 최적화의 최소 증거:

1. 동일 tick/camera의 Before/After 1920×1080 PNG.
2. 30~60초 split-screen 영상 또는 두 동기화 영상.
3. frame-time plot(p50/p95/p99, 1% low, longest 10 frames 표시).
4. CPU call tree 또는 Tracy timeline screenshot.
5. GPU event/pass screenshot과 actual draw count.
6. 정상 F5 체크리스트: map, roster, minion, snapshot, champion, UI, FX, animation, camera.
7. 성능 변경이 visual quality를 의도적으로 바꿨다면 품질 trade-off를 별도 표기. 숨기지 않음.

영상 overlay는 다음만 보여준다.

```text
commit / build / resolution
Frame p95 | CPU Update p95 | CPU Render p95 | GPU p95
1% low | actual draw calls
scenario ID / run ID
```

### 2-9. Phase 6 — Evidence ledger → Notion → 이력서

#### 2-9-1. 단일 진실 원장

각 주장은 다음 상태만 가진다.

```text
초안 -> 코드확인 -> 측정확인 -> 영상확인 -> 제출가능
           \-> 반증/폐기
```

`제출가능`의 필수 링크:

- result 문서의 표/실험 ID
- Before/After 원본 JSON과 PresentMon CSV
- CPU/GPU capture 경로
- 영상/GIF URL 또는 파일
- 관련 commit/PR
- 재현 command와 scenario manifest hash

#### 2-9-2. 현행 주장 격리 목록

아래 문구는 즉시 삭제 대상으로 단정하지 않고, 새 result가 나오기 전 **historical/unverified** 배지를 붙인다.

| 위치 | 현행 주장 | 현재 결함 | 승격 조건 |
|---|---|---|---|
| `.md/이력서/이력서_MASTER.md:31` | 16ms=90%, 17.8→9ms | 원본 capture/build/scenario 미연결 | 같은 증거 묶음 또는 새 verified result |
| `.md/이력서/weapons/04_Winters_LOL.md:14,34` | 자체 profiler로 병목 확정·17.8→9 | 코드 폴더만 근거로 제시 | run/result/commit 링크 |
| `.md/이력서/포트폴리오_MASTER_설계.md:48,68` | 대표 수치/확보됨 | 제출용 대표 수치로 과승격 | `제출가능` ledger 상태 |
| `.md/이력서/HANDOVER_2026-07-13_노션_V1_수정.md:31` | 검증된 수치 | 현행 캡처와 대조 불가 | evidence ID 추가 |
| `.md/이력서/면접/12_PERFORMANCE_MEASUREMENT.md` | Release 상시 계측, lock-free 수집, 9.54ms/~94 draw | vcxproj/PopScope/partial counter와 불일치 | 코드 사실 교정 + 새 result |
| Notion `홍건보 / 포트폴리오` | 17.8→9ms 스토리 | 원본 증거 없음 | child lab page의 verified experiment 링크 |

#### 2-9-3. Notion 구조

기존 포트폴리오 본문을 바로 덮어쓰지 않는다. 하위 페이지 `Winters Engine — Extreme Profiling & Optimization Lab`을 증거 원장으로 만들고 다음 DB형 섹션을 둔다.

```text
1. Current Truth / 측정 계약
2. Baseline Runs
3. Bottleneck Decision
4. Experiments (ACCEPT/REJECT/INCONCLUSIVE)
5. Visual Evidence
6. Resume-ready Claims
7. Open Risks / Next Run
```

Notion에는 요약과 링크만 두고 원본 JSON/trace의 canonical copy는 repository evidence 폴더 또는 release artifact에 둔다. Notion의 수치는 result 문서의 experiment ID를 반드시 역참조한다.

2026-07-17 연결 상태:

- Lab page 생성·재조회 완료: <https://app.notion.com/p/3a0b8c3c75e28155850ce8505c6e5cad>
- `Performance Experiments` data source: `collection://56df5a93-a57b-49fb-bfdb-45b605a2f282`
- 상태: `Draft / Code Verified / Measured / Visual Verified / Resume Ready / Rejected / Inconclusive`
- 필드: scenario/commit, Before·After p95, delta, CPU/GPU p95, actual draws, evidence URL, result, run date.
- 첫 원장 행 `LEGACY-DEBUG-720P — invalid baseline`을 `Inconclusive`로 기록해 68.6443ms Frame p95와 24.1383ms GPU p95가 제출 수치로 오인되지 않게 했다.
- 기존 포트폴리오 본문의 `17.8→9ms` 문구는 원본 증거를 찾거나 새 verified result가 나오기 전까지 수정/재인용하지 않는다.

#### 2-9-4. 이력서 문장 생성 규칙

문장 템플릿:

```text
[병목]을 [도구/지표]로 재현·귀속하고, [변경]으로
[Frame 또는 해당 phase] p95를 A ms → B ms(C%) 개선했다.
동일 1080p 정상 F5 시나리오 60초×3회와 [시각/해시/리플레이]로 회귀가 없음을 검증했다.
```

좋은 문장은 “프로파일러를 만들었다”에서 끝나지 않고 병목·판단·변경·절대 수치·재현 조건·회귀 검증을 한 줄에 연결한다. 단, 확인하지 않은 `~FPS`, `90%`, “전체 DrawCall”을 자동 생성하지 않는다.

### 2-10. 저장 구조와 파일 계약

실행 시 다음 구조를 사용한다. 날짜와 experiment ID는 고정한다.

```text
.md/build/profiling/<YYYY-MM-DD>/<experiment-id>/
  manifest.json
  before/
    run-01_profiler.json
    run-01_presentmon.csv
    run-01_cpu.etl-or-diagsession
    run-01_gpu-capture
  after/
    ...
  images/
  video/
  summary.json

.md/plan/<YYYY-MM-DD>_<TOPIC>_PLAN.md
.md/plan/<YYYY-MM-DD>_<TOPIC>_RESULT.md
```

대형 trace/video는 Git에 무조건 넣지 않는다. 용량·LFS 정책 확인 후 artifact URL과 SHA-256을 manifest에 기록한다. JSON/CSV와 작은 PNG는 diff 가능한 범위만 버전 관리한다.

### 2-11. 70:30 실행 예산과 외부 마감

| 예산 | 비율 | 작업 | 완료 증거 |
|---|---:|---|---|
| Floor | 70% | Profile build, analyzer gate, baseline, bottleneck capture, 1개 실최적화, 회귀 검증 | result + raw evidence + green build/tests |
| Ceiling | 30% | split video/GIF, Notion case study, 이력서/면접 문장, 외부 리뷰 | 공개/공유 가능한 링크와 피드백 |

마감:

- **2026-07-20**: Profile build/probe-effect gate와 baseline manifest 확정.
- **2026-07-22**: 1080p 60초×3 baseline + CPU/GPU/draw 병목 판결.
- **2026-07-24**: 첫 ACCEPT 최적화 result와 Before/After 영상 초안, 제3자 1명에게 공유.
- **2026-07-27**: Notion/이력서/면접 문장 `제출가능` 승격 또는 “아직 없음”으로 정직하게 마감.

3세션 연속으로 계측/문서만 하고 ACCEPT 최적화가 없으면 ceiling 작업을 멈추고, 가장 큰 verified hotspot 한 개의 코드를 끝까지 줄인다. 반대로 최적화 수치만 쌓고 영상/이력서 환전이 없으면 다음 deep dive를 열지 않는다.

### 2-12. 공식 도구 근거

- Visual Studio CPU Usage는 실제 성능 분석에 Release build를 권장하며 call tree/caller-callee를 제공한다: <https://learn.microsoft.com/en-us/visualstudio/profiling/cpu-usage?view=vs-2022>
- WPA CPU Analysis는 sampled CPU와 precise context-switch 분석을 구분한다: <https://learn.microsoft.com/en-us/windows-hardware/test/wpt/cpu-analysis>
- Visual Studio GPU Usage는 Direct3D CPU/GPU bottleneck 분석을 제공한다: <https://learn.microsoft.com/en-us/visualstudio/profiling/gpu-usage?view=vs-2022>
- PresentMon은 CPU/GPU/display duration과 latency를 ETW 기반으로 수집한다: <https://github.com/GameTechDev/PresentMon>
- NVIDIA Nsight Graphics/DX11과 GPU Trace: <https://docs.nvidia.com/nsight-graphics/> · <https://docs.nvidia.com/nsight-graphics/UserGuide/gpu-trace-overview.html>
- MSVC auto-vectorizer report `/Qvec-report`: <https://learn.microsoft.com/en-us/cpp/build/reference/qvec-report-auto-vectorizer-reporting-level?view=msvc-170>
- D3D11 pipeline statistics query 항목: <https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_query_data_pipeline_statistics>

---

## §3 검증

### 3-1. 예측값

1. Optimized Profile baseline에서는 Debug baseline의 `Map::Render ≈28ms`가 크게 줄거나 순위가 바뀔 수 있다. 바뀌면 Debug/debug-layer 오염이 컸다는 판결이다.
2. Profile에서도 CPU Render p95가 GPU p95보다 크고 static model이 상위면 draw/state/resource submission이 첫 실최적화 후보로 남는다.
3. 60Hz analyzer에서 8.33ms 초과 frame이 있다고 무조건 실패하던 현행 strict gate는 target-aware 수정 후 16.67/20.83ms 정책으로 분리된다.
4. 현행 `Mesh::DrawCalls=432`와 외부 actual API draw count는 다를 가능성이 높다. 차이는 counter coverage 부채로 기록한다.
5. 첫 ACCEPT 변경은 Frame p95 또는 top phase p95를 3% 이상 줄이고, 정상 F5 visual/replay/server contract를 유지해야 한다.

### 3-2. 현행 캡처 재현 명령

```powershell
python Tools/Profiler/analyze_profiler_capture.py profiler.json --target-fps 60 --minimum-duration-sec 60 --top 30
python -m unittest Tools/Profiler/test_analyze_profiler_capture.py
python Tools/Profiler/audit_update_render_scopes.py
```

현행 예상:

- analyzer: `scenarioValid=false`, `gate.pass=false` — 720p·15.861초·Debug이므로 정상.
- analyzer unit tests: 4 tests pass.
- scope audit: exit 0, heuristic direct-scope coverage 약 24.66%; acceptance 결과로 해석 금지.

### 3-3. 구현 후 필수 검증 명령

```powershell
msbuild Winters.sln /m /p:Configuration=Profile /p:Platform=x64
python -m unittest Tools/Profiler/test_analyze_profiler_capture.py
python Tools/Profiler/analyze_profiler_capture.py <capture.json> --target-fps 60 --minimum-duration-sec 60 --top 30
git diff --check
```

`Configuration=Profile`은 아직 존재하지 않으므로 Phase 0 구현 전에는 실행하지 않는다. 실제 빌드 명령은 solution/project 전수 확인 후 v2 코드 계획에서 확정한다.

### 3-4. acceptance 체크리스트

- [ ] Profile/Clean Release가 동일 commit·동일 scenario이고 probe-effect gate 통과.
- [ ] 1920×1080, warm-up 30초, capture ≥60초, Before/After 각 3회.
- [ ] limiter off, Present VSync off, debugger/overlay off.
- [ ] 정상 map/roster/minion/snapshot/champion/UI/FX 스코프와 server tick 존재.
- [ ] required scope ≥95%, GPU/server coverage ≥90%, dropped/truncated=0.
- [ ] CPU/GPU/present actual bottleneck을 서로 다른 두 도구가 같은 방향으로 지목.
- [ ] actual API draw와 partial in-engine counters를 구분.
- [ ] 한 가설 한 변경, A/B/A 또는 3×3 반복, p95/p99 악화 없음.
- [ ] visual/replay/hash 검증 통과.
- [ ] result 문서·원본 evidence·영상·Notion·resume claim이 같은 experiment ID 참조.

### 3-5. 미검증

- 현행 하드웨어/드라이버/전원/monitor 환경과 RTX 4060 가정은 새 manifest에서 재확인 필요.
- PresentMon, Nsight Graphics, RenderDoc, WPA/VS Profiler의 설치·실행 권한은 미확인.
- 정상 F5를 자동으로 같은 camera/input/tick에 고정할 replay/script 경로는 미확인.
- Profile configuration의 모든 프로젝트/SDK/CRT/PDB 호환은 미확인.
- 이전 `17.8→9ms`, `9.54ms/~94 draw`, `16ms=90%` 원본 캡처는 미발견.
- D3D11 pipeline statistics와 per-pass timestamp의 probe effect는 미측정.
- Notion 원본 JSON/trace 저장 용량과 외부 공유 정책은 미확인.

### 3-6. 확인 필요

1. **Profile build 상세 계획:** solution 전 프로젝트와 `UpdateLib.bat`를 읽고 complete code block/anchor를 가진 별도 v2 plan을 작성한다.
2. **Scenario automation:** replay/camera/input 고정 경로를 조사하고, 없으면 최소 deterministic performance route의 소유 위치를 정한다.
3. **Evidence storage:** 대형 `.etl`, Nsight/RenderDoc capture, video를 Git LFS·GitHub Release·Drive 중 어디에 둘지 사용자가 확정한다.
4. **Hardware:** 실제 CPU/GPU/RAM/driver/monitor refresh/power mode를 첫 manifest에서 기록한다.

### 3-7. 결과 문서 이름

구현과 측정이 끝난 뒤 같은 이름의 결과 문서를 만든다.

```text
.md/plan/2026-07-17_EXTREME_PROFILING_OPTIMIZATION_RESUME_NOTION_PIPELINE_RESULT.md
```

RESULT에는 계획 재서술 대신 experiment별 Before/After 표, rejected hypothesis, code diff/commit, build/test, visual evidence, Notion URL, 최종 이력서 문장만 남긴다.

---

## 다음 세션 첫 슬라이스

1. `Winters.sln`과 모든 `.vcxproj`, `UpdateLib.bat`의 configuration/output/CRT/PDB를 전수 표로 만든다.
2. 별도 `Profile|x64` 또는 동등한 opt-in 방식 중 normal Release를 오염하지 않는 최소 변경을 결정한다.
3. Profile/Clean Release probe-effect 측정부터 통과한다.
4. 그 뒤에만 1080p 정상 F5 60초×3 baseline을 캡처한다.
5. baseline의 가장 큰 verified hotspot 하나를 다음 코드 계획의 유일한 최적화 대상으로 고른다.

**Handoff:** 이번 문서는 최적화 코드를 넣는 계획이 아니라, 거짓 수치 없이 실제 병목을 찾아 최적화하고 외부 산출물로 환전하는 실행 계약이다. 현재 가장 유력한 가설은 static map CPU submission이지만 Debug 오염 때문에 미확정이다. 첫 판결은 Profile/Clean Release 교정 없이는 내리지 않는다.
