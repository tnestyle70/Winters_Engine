# 프로파일링 원인 좁히기 플레이북 (Winters 실전판)

프레임이 느릴 때 "어디가 느린가"를 **CPU/GPU → 패스 → 시스템 → 루프 → 명령어** 순으로 좁혀 내려가는 표준 절차.
모든 단계는 Winters의 실제 계측 지점(파일 앵커)과 짝지어져 있다 — 이론 문서가 아니라 운전 매뉴얼이다.

관련 정본: 측정 수치의 원장은 `.md/plan/performance/2026-06-12_ENGINE_FULL_OPTIMIZATION_MASTER_PLAN.md` §0,
프로파일러 이론은 `.md/논문/05_Profiler_프로파일러.md`, 판별 요약은 `.md/interview/graphics-dx11.md` §13.

---

## 0. 철칙 — 측정 전 3가지 고정

숫자가 없으면 문제가 아니라 취향이다. 그리고 숫자는 **조건이 고정된 숫자**만 유효하다.

| 고정 대상 | Winters에서의 방법 | 함정 |
|---|---|---|
| **빌드 구성** | Profile 실측 = 최적화 + `WINTERS_PROFILING` (마스터 계획서 §2-1의 Release 계측 활성 이후). | Debug 수치는 이력서/비교에 사용 금지 — 디버그 STL·무최적화라 병목 순위 자체가 달라진다. 기존 17.8→9ms, 9.54ms 계열은 전부 Debug 캡처였다. |
| **시나리오** | WRPL 리플레이 재생 (`CScene_InGame(replayPath)`, 네트워크 펌프 완전 차단 — `Client/Private/Scene/Scene_InGame.cpp:150-154`). 같은 .wrpl = 같은 틱 스트림 = 같은 월드 상태. | 라이브 매치는 봇 AI·네트워크 타이밍이 매 실행 다르다. 리플레이도 **프레임 단위**로는 wall-clock dt 의존이므로, 비교는 항상 "같은 리플레이 + 같은 캡처 프레임 수 + 통계(median/p95/p99)"로 한다. |
| **프레임 리미터/VSync** | 병목 측정은 `--uncapped --no-vsync`. 히치/안정성 측정은 `--fps=60`. | 리미터 ON 상태의 Frame ms는 LimiterWait를 포함한다 — `Frame::LimiterActive` 게이지(CEngineApp.cpp)로 캡처 조건을 항상 기록. |

관찰자 효과: 프로파일러 자신도 비용이다. 자기 측정 카운터(`Profiler::PreHistoryMergeUs`,
`Profiler::PreviousEndFrameUs` — `Engine/Private/Core/Profiler/CPUProfiler.cpp:233-253`)가 프레임 예산의
1%를 넘으면 그 캡처는 무효 (`Tools/Profiler/analyze_profiler_capture.py` 게이트).

---

## 1단계 — CPU 바운드 vs GPU 바운드

**질문**: 프레임 시간을 결정하는 쪽이 어느 프로세서인가?

**계측**: F3 오버레이(또는 F12 JSON 캡처)에서 두 값을 비교.
- CPU 프레임: `Frame` 스코프 (ms) — `CEngineApp::Run`의 최상위 스코프
- GPU 프레임: `GPU::FrameUs` 게이지 — 프레임 전체를 감싼 D3D11 타임스탬프 disjoint 쿼리, 4슬롯 링 + non-blocking readback (`Engine/Private/RHI/DX11/CDX11Device.cpp:907-958`)

**판정**:
```text
CPU ms ≫ GPU ms  → CPU 바운드 → 2단계-A (스코프 트리)
GPU ms ≈ CPU ms 또는 GPU가 지배 → GPU 바운드 → 2단계-B (패스 분해)
둘 다 예산 안인데 프레임이 김 → Present/DWM/리미터 — Render::EndFrame 스코프와 Frame::LimiterActive 확인
```

Winters 실측 전례: 드로우콜 ~94 / 인덱스 ~47만에서 GPU sub-ms → "완전 CPU 바운드 확정"
(2026-06-11 캡처, ENGINE_FULL_OPTIMIZATION_MASTER_PLAN §0). 씬이 커지기 전까지 이 엔진의 기본 가설은 CPU 바운드다.

---

## 2단계-A — CPU 바운드: 어느 시스템, 어느 스레드인가

**1) 스코프 트리로 시스템 좁히기**
- F3 오버레이: EMA 평활 상위 스코프 테이블 (48행, `Engine/Private/Manager/Profiler/ProfilerOverlay.cpp:404-440`)
- F12: `Profiles/profiler_<timestamp>.json` 캡처 → `python Tools/Profiler/analyze_profiler_capture.py`로 median/p95/p99/히치 분석
- 계층은 raw events(depth 포함)로 본다. 상위 155개 스코프가 이미 심겨 있다 (프레임 루프 `CEngineApp.cpp:305-382`, 렌더 패스 `Scene_InGameRender.cpp` 25곳 등)

**2) 스레드/파이버 타임라인은 Tracy로**
- `WINTERS_PROFILE_SCOPE`는 Tracy 존을 동시에 기록한다 (`Engine/Include/ProfilerAPI.h:51-53`, TRACY_ON_DEMAND — 뷰어 접속 시에만 수집)
- JobSystem 워커의 유휴/스틸 패턴, `WaitForCounter` 정체, 병렬 배치가 실제로 도는지(`Scheduler::ParallelBatches` 카운터)는 플랫 테이블로는 안 보인다 — 타임라인 필수
- 스레드 이름은 MainThread + JobSystem 워커만 지정돼 있음 (`CEngineApp.cpp:287`, `JobSystem.cpp:614`)

**3) 카운터로 "왜 많은가"를 잡는다**
시간(스코프)과 규모(카운터)를 항상 짝으로 읽는다. 시간만 보면 "느리다"까지만 알고, 규모를 보면 "왜"가 나온다.
- 예: `Minion::RenderVisible` vs `Minion::RenderMeshCount`, `Fx::Drawn` vs `Fx::CullSkipped`, `Anim::UpdateCalls`
- 전례: Anim::UpdateCalls=107 발견 → 정적 엔티티가 전부 애님 갱신 중 → bAnimated 플래그 (17.8→9ms의 실체)

---

## 2단계-B — GPU 바운드: 어느 패스, 어느 드로우인가

**1) 패스별 GPU 시간** (마스터 계획서 §2-3의 per-pass 타임스탬프 이후)
- 패스 경계는 `CScene_InGame::OnRender`의 순서 그대로: NormalPass 프리패스 → SSAO(컴퓨트 2 Dispatch) → Map → Champions → Structures/Jungle/Minions/Props → 컨택트 섀도우 → 투명 → FoW → FX → PostFx(풀스크린 최대 4) → HUD/ImGui (`Client/Private/Scene/Scene_InGameRender.cpp:330-750`)
- 어느 패스가 GPU 예산을 먹는지 나오면, 그 패스의 성격으로 다시 분기: 지오메트리(정점) / 픽셀(오버드로우·해상도) / 컴퓨트(SSAO)

**2) 드로우콜·상태 변경 규모**
- 통합 카운터(마스터 계획서 §2-2): 드로우 깔때기는 정확히 두 곳 — 레거시 `DX11Buffer::DrawIndexed/DrawIndexedRange` (`Engine/Private/RHI/DX11/DX11Buffer.cpp:82-104`)와 RHI `CDX11FrameCommandList::Draw/DrawIndexed` (`CDX11Device.cpp:662-679`) — 나머지는 소수의 원시 사이트
- 상태 변경은 현재 전부 무조건 재바인드다: `DX11Shader::Bind`/`DX11Pipeline::Bind`/`CTexture::Bind`/RHI `SetPipeline`(저장만 하고 비교 안 함 — `CDX11Device.cpp:538-629`). 드로우콜이 적어도 상태 변경이 많으면 CPU(드라이버) 비용으로 되돌아온다 — 이 경우 실은 CPU 바운드의 변종이다.

**3) 셰이더/대역폭 수준 — 외부 도구로 넘어가는 지점**
- RenderDoc: 프레임 캡처 → 패스/드로우별 GPU 시간, 오버드로우 시각화. 마커가 없으면 캡처가 무라벨이므로 `ID3DUserDefinedAnnotation` 패스 마커(계획서 §2-3에 포함)가 선행돼야 한다.
- 판별 휴리스틱: 해상도를 낮춰서 프레임이 비례 개선되면 픽셀 바운드, 그대로면 지오메트리/CPU. arithmetic intensity(FLOP/byte)가 낮은 셰이더는 ALU 최적화 무의미 — 대역폭 바운드 (`.md/interview/rendering/02_gpu_pipeline_hardware.md:228`).

---

## 3단계 — 루프 내부: 알고리즘 vs 메모리 vs 호출 오버헤드 vs SIMD

시스템 하나로 좁혀졌으면, 그 시스템이 느린 **종류**를 판별한다. 순서대로 검사:

**1) 알고리즘 형태 (가장 먼저, 가장 싸게)**
- 루프 경계가 O(N²)/O(N·K)인가? 매 틱/프레임 재계산인가, 캐시 가능한가?
- Winters 확정 사례: 미니언 depenetration의 per-minion blockers 벡터 할당 + O(N·K) 그리드 질의 (`Server/Private/Game/GameRoomUnitAI.cpp:1126-1156`), `BuildClipVisibilityMask` 매 프레임 109회 전수 AABB (`Engine/Private/Resource/Model.cpp:741-811`), 가시성 게이트 없는 전 인스턴스 애님 포즈 평가 (`Engine/Private/Renderer/ModelRenderer.cpp:864-870`)

**2) 은닉 할당·문자열·해시 (스코프에 안 잡히는 상수 비용)**
- 프레임당 힙 할당, 문자열 생성, 해시 조회가 루프 안에 있는가?
- 확정 사례: FxSystem이 파티클마다 매 프레임 `std::wstring` 키 생성 + 해시맵 find (`Client/Private/GameObject/FX/FxSystem.cpp:374-385`), ECS `GetComponent`가 매번 `unordered_map<type_index>::find` (`Engine/Public/ECS/...` — 미니언당 틱당 6+회)

**3) 메모리 바운드 판별 (IPC)**
- 스코프 시간은 큰데 위 두 가지가 아니면 캐시 미스를 의심한다. 판별: **IPC < 1이면 메모리 바운드** — VTune(무료) microarchitecture 분석 또는 Tracy 샘플링으로 확인 (`.md/interview/computer-architecture.md:612-616`)
- 데이터 배치 확인: AoS 포인터 체이싱(예: 스냅샷 보간의 `unordered_map` 순회 — `Scene_InGameNetwork.cpp:944-984`) vs SoA(예: ParticlePool은 이미 SoA — `Engine/Private/FX/ParticlePool.cpp:3-52`)

**4) SIMD 활용도 (마지막 — 위 3개가 해결된 뒤에만)**
- Winters 수학은 스칼라 Vec3(2,900+ 사용처)이고 XMVECTOR는 경계 변환용(~117곳)이다. `Mat4::operator*`는 호출마다 Load/Store 왕복 (`Engine/Include/WintersMath.h:247-311`). `/arch` 미지정 = SSE2 기본.
- SIMD 전환은 "핫루프가 확정되고, 메모리 레이아웃이 SoA로 정리된 뒤"에만 의미 있다 — 순서를 건너뛰고 SIMD부터 하면 대역폭 바운드 루프에서 0% 개선이 나온다 (roofline).

---

## 4단계 — 수정 → 재측정 → 게이트 → 기록

1. **수정 전** 베이스라인 캡처를 `Profiles/`에 보존 (파일명에 시나리오·구성 포함)
2. 한 번에 한 가지만 수정 — 두 최적화를 겹치면 어느 쪽 효과인지 영영 모른다
3. **같은 리플레이·같은 조건** 재캡처 → `analyze_profiler_capture.py` 통계 비교 (median 우선, p99로 히치 회귀 확인)
4. 판정 기록: 대상 계획서의 `_RESULT` 문서에 예측 vs 실측 (빗나간 예측이 최우선 기록 — `.md/계획서작성규칙.md`)
5. 수치는 "측정 방법 1문장"과 세트로만 이력서 원장(`.md/interview/resume.md` §4 표)에 올린다

---

## 도구 → 단계 매핑

| 단계 | 1차 도구 (내장) | 2차 도구 (외부) |
|---|---|---|
| CPU/GPU 판별 | F3 오버레이 (`Frame` vs `GPU::FrameUs`) | — |
| CPU 시스템 좁히기 | F12 JSON + analyze_profiler_capture.py | Tracy 타임라인 (스레드/파이버) |
| CPU 명령어/캐시 | — | VTune microarchitecture (IPC, 캐시미스), Tracy 샘플링 |
| GPU 패스 좁히기 | per-pass 타임스탬프 (§2-3 이후) | RenderDoc (마커 필요) |
| GPU 셰이더/오버드로우 | — | RenderDoc 픽셀 히스토리, NSight |
| 드로우/상태 규모 | 통합 Draw/Bind 카운터 (§2-2 이후) | RenderDoc API 통계 |
| 회귀 게이트 | analyze_profiler_capture.py + SimLab 골든 해시 | — |

## Winters 고유 함정

- **EMA vs 순간값**: 오버레이 테이블은 EMA(α=0.25) 평활 — 히치는 EMA에 묻힌다. 히치 분석은 반드시 JSON 타임라인 + p99/max로.
- **스코프 캡**: raw 이벤트 1024/frame, 스코프 stat 256/frame — 초과분은 dropped 카운터에 기록되고 조용히 유실된다. 캡처의 `droppedScopeStats/droppedRawEvents`가 0이 아니면 그 프레임 트리는 불완전하다 (analyze 게이트가 잡는다).
- **PopScope 비용**: 전역 mutex + O(n) strcmp (`CPUProfiler.cpp:367-392`) — per-draw/per-entity 같은 고밀도 계측을 심기 전에 프로파일러 자기 오버헤드부터 재라.
- **서버는 계측 0**: Server/GameSim vcxproj에 `WINTERS_PROFILING` 자체가 없다. 서버 틱 예산(p99 < 33.3ms)은 별도 하니스(`Tools/Harness/RunGameRoomBotMatchSoak.ps1`) 수치로만 존재.
- **DWM/창모드**: 캡 해제 측정에서 Present 이후 비용(~0.8ms대)은 엔진 밖이다. 650FPS 이상 논의는 전체화면 독점 없이 무의미 (6-12 플랜 이론 상한 분석).
- **이론 수치 발화 금지**: 300~650 FPS 사다리는 이론 추정치 — 실측 전엔 어디에도 "달성"으로 쓰지 않는다 (`.md/이력서/면접/12_PERFORMANCE_MEASUREMENT.md:198`).
