# Sequencer 상세 설계 (.wseq)

> 작성: 2026-06-23. 대상: Codex 및 EldenRing 팀.
> 선행 문서: `17_UE5_GRADE_EDITOR_SUITE_MASTER.md`(2.3 Sequencer 매핑·게이트), `12_UE5_REFERENCE_DX12_RHI_EDITOR_BIG_PICTURE.md`(Phase G·게이트 G7), `06_FX_GRAPH_SEQUENCER_EDITOR.md`(Sequencer 섹션·트랙 표), `03_ELDEN_CLIENT_RUNTIME_ARCHITECTURE.md`(런타임 update 순서), `11_Ch11_Cinematics_Sequencer.md`(UE5 MovieScene 레퍼런스 정리).
> 이 문서는 위 06/11 문서의 Sequencer 스케치를 **실제 Winters 코드 근거 위에서 구현 가능한 형태로 확정**한다. 11문서의 `CSequence/ITrack/CSequencePlayer` 제안과 충돌하지 않고, 그 위에 `.wseq` 포맷·CPU 평가 모델·ImGui 패널을 구체화한다.

---

## 0. 한 줄 목표 + 시스템 경계

**한 줄 목표**: `.wseq`(또는 `.wseq.json`) 한 파일로 정의된 멀티트랙 타임라인을 `CSequencePlayer`가 시간축으로 평가해서, 에디터 뷰포트와 Elden 런타임에서 **동일하게** 카메라/애니/FX/사운드/가시성/시간배율 연출을 재생한다. 첫 완료기준은 **CameraTrack key 찍기 → scrub → play가 뷰포트에 보이는 것**.

**경계 (이 문서가 다루는 것)**:
- `.wseq` 포맷 스키마(JSON 초기 → binary 승격)와 7개 트랙 타입(Camera/Anim/Fx/Audio/Event/Visibility/TimeDilation).
- 런타임 평가기 `CSequencePlayer`(`Play/Stop/Tick/Seek/IsPlaying`)와 트랙 evaluator 계층, keyframe 보간/커브.
- ImGui `SequencerPanel`(타임라인, 트랙 add/remove, 키프레임 add/move/delete, scrub, save/load).
- `.wseq` → `CAssetStreamingSystem` 로드 → 런타임 적용 데이터 흐름.

**경계 밖 (다른 문서/시스템 소유)**:
- 판정/데미지/페이즈 전이 = Server GameSim(절대 침범 금지). EventTrack은 **후보 발행만**.
- FX 그래프 내부(Spawn→Init→Update→Render) = FX_NIAGARA 시스템. Sequencer는 `.wfx` handle을 **spawn 트리거만** 한다.
- 카메라 흔들림/충돌/lock-on의 게임플레이 로직 = ThirdPersonCamera 시스템. Sequencer는 **연출용 cinematic camera 평가값만** 산출.
- World Partition cell visibility = WORLD_PARTITION 시스템. VisibilityTrack은 **entity/DataLayer toggle 신호만** 보낸다.

---

## 1. UE5 실제 아키텍처 (깊이)

### 1.1 5대 구성요소와 책임 분리

UE5 Sequencer는 "에디터 UI"가 아니라 **데이터 모델(MovieScene) + 런타임 평가기(Player/Evaluation) + 에디터(Sequencer module)**의 3층 분리가 핵심이다.

| 계층 | 핵심 타입 | 위치 | 책임 |
|---|---|---|---|
| Asset/데이터 모델 | `UMovieSceneSequence`, `UMovieScene` | `Runtime/MovieScene` | 트랙/섹션/채널/바인딩의 순수 데이터. 에디터 없이도 존재 |
| 런타임 평가 | `UMovieSceneSequencePlayer`, Evaluation Template / ECS | `Runtime/MovieScene` | 시간 → 트랙 평가 → bound object에 값 적용 |
| 트랙 구현 | `UMovieScene*Track/Section` | `Runtime/MovieSceneTracks` | Transform/Audio/Event/CameraCut 등 도메인별 |
| 에디터 | `FSequencer`, `ISequencerTrackEditor` | `Editor/Sequencer` | 타임라인 UI, 키 편집. **런타임에 링크 안 됨** |

**철학 1 — 에디터/런타임 분리**: 게임 빌드에는 `Editor/Sequencer`가 들어가지 않는다. 런타임은 `UMovieScene` 데이터만 읽어 평가한다. 그래서 "에디터에서 만든 시퀀스 = 게임에서 그대로 재생"이 성립한다. Winters가 그대로 차용할 핵심.

### 1.2 Track / Section / Channel 계층

```text
MovieScene
 └─ MovieSceneBinding (어느 object에 적용?)
     └─ Track            (Transform / Audio / Camera / Event ...)
         └─ Section      (이 트랙이 활성인 시간 구간 [start,end])
             └─ Channel  (실제 키프레임 곡선: Location.X, Location.Y, FOV ...)
                 └─ Key  (FrameNumber + Value + Tangent/Interp)
```

- **Track**: 한 종류의 애니메이션 채널 묶음. 한 바인딩에 여러 트랙(Transform + Visibility).
- **Section**: 트랙이 활성인 **시간 구간**. 같은 트랙에 여러 섹션을 늘어놓고 blend(예: 두 Transform 섹션 crossfade). Section은 `EvalRange`, `RowIndex`(겹침 레이어), `BlendType`(Absolute/Additive/Relative)을 가진다.
- **Channel**: `FMovieSceneFloatChannel` 등. 내부에 `FRichCurve`(키 배열 + 보간/탄젠트). FOV 하나가 하나의 channel.
- **Key**: `{ FrameNumber, Value, InterpMode(Constant/Linear/Cubic), TangentMode, ArriveTangent, LeaveTangent }`.

**철학 2 — Section 기반 blend**: UE5가 단순 keyframe 배열이 아니라 Section 레이어를 둔 이유는 **여러 애니메이션 소스를 시간축에서 합성**하기 위함(Additive 카메라 흔들림을 base 카메라 위에 더하는 식). Winters는 초기엔 트랙당 단일 섹션(키 배열)로 단순화하고, blend가 필요한 시점에만 Section 레이어를 도입한다(과설계 방지).

### 1.3 Possessable vs Spawnable

- **Possessable**: 이미 레벨에 존재하는 액터를 시퀀스가 **점유(possess)**해 제어. `FMovieScenePossessable` + `FMovieSceneBinding`(GUID)으로 런타임에 실제 object를 resolve. 보스 컷신에서 "이미 스폰된 보스"를 카메라/애니로 조종할 때.
- **Spawnable**: 시퀀스가 **재생 중에만 생성**하는 액터(`FMovieSceneSpawnable`, 템플릿 보관). 시퀀스 끝나면 destroy. 인트로 전용 더미 액터 등.

**철학 3 — 바인딩 간접화**: 시퀀스는 구체 포인터가 아니라 **GUID binding**을 들고, 재생 시점에 `IMovieScenePlayer::ResolveBoundObjects()`로 실제 object를 찾는다. 같은 시퀀스를 다른 액터 인스턴스에 재사용 가능. Winters는 GUID 대신 **entity id / 논리 이름** 바인딩으로 매핑(P2에서 spawnable descriptor 추가).

### 1.4 Evaluation Template / ECS evaluation

UE 4.x는 `FMovieSceneEvaluationTemplate`(시퀀스를 컴파일한 평가 트랙 배열) → 시간마다 `Evaluate()`. UE5는 이를 **MovieScene ECS**로 재작성: 트랙을 entity/component로 분해하고, 시스템이 batched evaluate. 멀티스레드·blend 합성·성능에 유리.

**철학 4 — 그래프를 실행 계획으로 컴파일**: FX(Niagara)와 동일한 통찰. "에디터 데이터 모델"을 그대로 평가하지 않고 **평가 전용 표현으로 컴파일**한다. Winters 초기에는 CPU 단일스레드 직접 평가(template 없이)로 시작하고, 트랙 수가 커지면 "compiled eval plan"으로 승격(여기서도 데이터 모델 ≠ 실행 표현 원칙 유지).

### 1.5 Camera Cuts track / Sub-sequence / Shot track / Blending

- **Camera Cuts Track**: 특수 트랙. 시간 구간마다 "지금 활성 카메라가 누구"를 지정 → 컷 전환. 카메라 위치는 각 카메라의 Transform 트랙이, **누구를 볼지**는 Camera Cuts가.
- **Sub-sequence / Shot Track**: 시퀀스 안에 다른 시퀀스를 중첩. Shot track은 영화 "샷" 단위로 협업 분할. (Winters P3)
- **Blending**: 같은 property에 여러 섹션이 겹칠 때 BlendType + weight로 합성. Additive 카메라 셰이크가 대표.

**Winters 초기 결정**: Camera Cuts는 CameraTrack 내부의 `cut` key로 단순 표현(여러 카메라 바인딩 전환은 P2). Sub-sequence/Shot/full Blending은 P3. 첫 슬라이스는 **단일 cinematic camera Transform+FOV 평가**.

---

## 2. Winters 현재 구조 (실측 근거)

### 2.1 재사용 가능한 실제 코드

| 트랙 | 의존 런타임 (file:line) | 실제 호출 가능한 API |
|---|---|---|
| CameraTrack | `Engine/Public/Renderer/CCamera.h:8` `class WINTERS_ENGINE CCamera` | `SetPosition(const Vec3&)`(:22), `SetPerspective(fovY,aspect,near,far)`(:21), `Ready(eye,at,up,fov,...)`(:18), getter `GetEye/GetAt/GetUp`(:25-27), `GetViewProjection()`(:31). 내부 멤버 `m_vEye/m_vAt/m_vUp`(:44-46), `m_fFov`(:48) |
| AnimTrack | `Engine/Public/Renderer/ModelRenderer.h:22` `ModelRenderer` | `PlayAnimationByNameAdvanced(strKeyword,bLoop,bReverse,fPlaySpeed)`(:74), `HasAnimationByName`(:78), `GetAnimationDurationSecondsByName`(:79), `Update(f32_t)`(:70). 하위 `CAnimator`(Animator.h:9): `GetCurrentTime()`(:30), `SetPlaySpeed`(:42), `IsPlaying`(:28) |
| FxTrack | `Engine/Public/FX/FxAsset.h:17` `FxAssetHandle = RHIHandle<FxAssetTag>`, `eFxRenderType`(:19), `eFxAnchorType`(:29) | FX spawn은 FX_NIAGARA 시스템의 spawn API를 트리거(현재 spawn 진입점은 FX 시스템 소유) |
| AudioTrack | `Engine/Include/GameInstance.h:53-55` | `PlaySoundOn(key,eSoundChannel,vol)`(:53), `PlayEffect(key,vol)`(:54), `PlayBGM(key,vol)`(:55) → `CSound_Manager`(Sound_Manager.h:23) 포워딩 |
| EventTrack | — | gameplay callback **후보 큐**에 push만. 서버 GameSim이 판정 (신규) |
| VisibilityTrack | World Partition `DataLayerSystem` / entity visibility (신규·WORLD_PARTITION 소유) | toggle 신호 발행 |
| TimeDilationTrack | Scene tick dt 스케일 (Scene 레벨, 신규) | 평가값 = dt multiplier |
| 평가 진입 | `Engine/Include/GameInstance.h:48` `Change_Scene(...)` → Scene tick에서 `CSequencePlayer::Tick(dt)` 호출 | Scene update 순서(03문서) 안에 삽입 |

근거 확인: `CCamera`는 `SetPosition`/`SetPerspective`로 위치·FOV를 직접 세팅 가능(에디터/디버그용 FPS 입력 경로와 분리됨). `ModelRenderer::PlayAnimationByNameAdvanced`는 loop/reverse/speed를 받아 AnimTrack key 적용에 그대로 쓸 수 있다. `CGameInstance`가 사운드 3종을 이미 포워딩한다.

### 2.2 미구현 (이 시스템이 신규 작성)

- `CSequencePlayer`, `CSequenceAsset`, 트랙 evaluator 계층 (`grep CSequencePlayer` → `.md`만 hit, 코드 0). 11문서에 헤더 스케치만 존재.
- `.wseq`/`.wseq.json` 로더·파서·시리얼라이저.
- `SequencerPanel`(ImGui). 06문서에 기능 목록만.
- EventTrack 후보 큐 ↔ Server GameSim 연결(이 문서는 client 측 후보 발행까지만; 서버 수신은 GameSim 소유).
- VisibilityTrack/TimeDilationTrack 적용 hook(World Partition·Scene 연동 포인트).

### 2.3 11문서 스케치와의 관계

11문서(`11_Ch11_Cinematics_Sequencer.md:172-210`)는 `CSequence/ITrack/CSequencePlayer`를 Engine/Public/Cinematic에 두는 안을 제안. 본 문서는 그 골격을 따르되:
- 17문서 게이트에 맞춰 **CameraTrack 먼저** 좁게 증명(11문서 Stage1은 transform/event 동시 시작 — 본 문서는 카메라 단일 슬라이스로 더 좁힘).
- `FrameTime/FrameRate`(UE식 60000 tick resolution)는 초기 과설계로 보고, 초기에는 `f64_t timeSec` + displayRate를 메타로만 둠. binary 승격 시 tick 정수화 검토.

---

## 3. Winters 설계

### 3.1 (a) 포맷 스키마

#### 3.1.1 JSON 초기 (`.wseq.json`)

06문서 예시를 7트랙으로 확장한 권위 스키마. 첫 슬라이스는 `Camera` 트랙만 있어도 로드/재생 가능해야 한다(부분 스키마 허용).

```json
{
  "format": "wseq",
  "version": 1,
  "name": "BossIntro_01",
  "durationSec": 8.0,
  "displayRate": 60,
  "loop": false,
  "tracks": [
    {
      "type": "Camera",
      "name": "CinematicCamera",
      "binding": "cinematic_cam",
      "keys": [
        { "time": 0.0, "pos": [0,3,-8], "rotEuler": [10,0,0], "fov": 55.0, "interp": "cubic", "cut": false },
        { "time": 4.0, "pos": [2,4,-6], "rotEuler": [12,15,0], "fov": 48.0, "interp": "cubic" },
        { "time": 8.0, "pos": [0,5,-4], "rotEuler": [15,0,0], "fov": 40.0, "interp": "linear" }
      ]
    },
    {
      "type": "Anim",
      "name": "BossRoar",
      "binding": "entity:boss_0",
      "keys": [
        { "time": 1.0, "anim": "roar", "loop": false, "reverse": false, "speed": 1.0 }
      ]
    },
    {
      "type": "Fx",
      "name": "DustBurst",
      "binding": "entity:boss_0",
      "keys": [
        { "time": 1.2, "wfx": "Boss_Stomp_Dust", "anchor": "Bone:foot_l", "oneShot": true }
      ]
    },
    {
      "type": "Audio",
      "name": "RoarSfx",
      "keys": [
        { "time": 1.0, "sound": "Boss/Roar.wav", "channel": "Effect", "volume": 1.0 }
      ]
    },
    {
      "type": "Event",
      "name": "GameplayCandidates",
      "keys": [
        { "time": 2.0, "event": "boss_intro_done", "payload": "phase=1" }
      ]
    },
    {
      "type": "Visibility",
      "name": "HideHUD",
      "binding": "layer:HUD",
      "keys": [
        { "time": 0.0, "visible": false },
        { "time": 8.0, "visible": true }
      ]
    },
    {
      "type": "TimeDilation",
      "name": "SlowMo",
      "keys": [
        { "time": 3.0, "scale": 0.3, "interp": "linear" },
        { "time": 3.5, "scale": 1.0, "interp": "linear" }
      ]
    }
  ]
}
```

#### 3.1.2 필드 정의

| 필드 | 타입 | 의미 |
|---|---|---|
| `format`/`version` | string/int | 포맷 식별·마이그레이션 게이트 |
| `durationSec` | f64 | 시퀀스 총 길이. play head 0 → duration |
| `displayRate` | int | 에디터 스냅용 fps(60). 평가는 연속 time 사용 |
| `loop` | bool | 끝에서 0으로 wrap 여부 |
| `tracks[].type` | enum | `Camera/Anim/Fx/Audio/Event/Visibility/TimeDilation` |
| `tracks[].binding` | string | `cinematic_cam` / `entity:<name>` / `layer:<name>` / 빈값(글로벌) |
| `keys[].time` | f64 | 트랙 로컬 시간(초). 정렬 보장은 로더가 |
| `keys[].interp` | enum | `constant/linear/cubic`(연속 채널 전용: Camera/TimeDilation) |
| Camera key | `pos[3]`,`rotEuler[3]`(deg),`fov`,`cut` | cut=true면 보간 없이 즉시 전환(컷 키) |
| Anim key | `anim`(키워드),`loop`,`reverse`,`speed` | `ModelRenderer::PlayAnimationByNameAdvanced`로 직매핑 |
| Fx key | `wfx`,`anchor`(`World`/`Bone:<n>`/`Entity`),`oneShot` | FX 시스템 spawn 트리거. presentation only |
| Audio key | `sound`(키),`channel`(eSoundChannel),`volume` | `CGameInstance::PlaySoundOn/PlayEffect` |
| Event key | `event`(id),`payload` | **후보 큐로만**. 판정 금지 |
| Visibility key | `visible`(bool) | step 트랙(보간 없음) |
| TimeDilation key | `scale`,`interp` | dt multiplier. 연속 보간 |

#### 3.1.3 binary 승격 (`.wseq`)

JSON 안정화 후 동일 의미를 binary로. 헤더(`magic 'WSEQ'`, version, durationSec, displayRate, trackCount) → 트랙별 (type u8, binding string, keyCount, key blob). key는 트랙 type별 고정 레이아웃(time f64 + payload). 이유: JSON은 diff/수정 용이, binary는 로드/배포. 두 경로 모두 같은 `CSequenceAsset`로 역직렬화(로더만 분기).

### 3.2 (b) 런타임 클래스 계층 (C++ 시그니처)

```text
CSequenceAsset            // 순수 데이터 모델 (.wseq.json/.wseq 역직렬화 결과)
 └─ SeqTrack (variant 7종)
     └─ SeqKey[]

CSequencePlayer           // 시간 → 트랙 평가 (UE Sequence Player 대응)
 ├─ ISeqTrackEvaluator    // 트랙 type별 평가 전략
 │   ├─ CameraTrackEvaluator
 │   ├─ AnimTrackEvaluator
 │   ├─ FxTrackEvaluator
 │   ├─ AudioTrackEvaluator
 │   ├─ EventTrackEvaluator
 │   ├─ VisibilityTrackEvaluator
 │   └─ TimeDilationTrackEvaluator
 └─ SeqBindingResolver    // binding 문자열 → 런타임 대상 (CCamera*/ModelRenderer*/...)

SeqEventSink              // EventTrack 후보 발행 인터페이스 (서버로 가는 후보만)
```

위치: 엔진 공용 승격 대상이지만 06문서 지침대로 **초기엔 EldenRingClient/Editor 측에 두고 검증** → 안정화 후 `Engine/Public/Cinematic/`로 승격. Engine→Client 의존 역전 금지(평가기가 ModelRenderer/CCamera 같은 Engine public 타입만 참조하도록; Client 구체 타입은 binding resolver 뒤로 숨김).

### 3.3 (c) 에디터 패널 (ImGui) — `SequencerPanel`

06문서 기능 목록 구체화:

```text
┌ SequencerPanel ───────────────────────────────────────────┐
│ [Open .wseq] [Save] [New Track ▾]   ▶Play ■Stop  loop[ ]   │
│ duration: 8.0s   displayRate: 60   time: 3.42s             │
├──────────┬────────────────────────────────────────────────┤
│ Tracks   │  0     1     2     3     4   ... timeline ruler  │
│ Camera   │  ◆-----◆-----------◆-----------------◆           │  ← 키 = ◆, 선택 드래그로 이동
│ Anim     │        ◆                                          │
│ Fx       │          ◆                                        │
│ Audio    │        ◆                                          │
│ Event    │              ◆                                    │
│ Visibility│ ◆-----------------------------------◆            │
│ TimeDilation│              ◆--◆                              │
│          │              ▮ playhead (scrub 드래그)            │
├──────────┴────────────────────────────────────────────────┤
│ Key Inspector: time[3.42] pos[..] rot[..] fov[48] interp[▾]│
└────────────────────────────────────────────────────────────┘
```

기능: (1) 트랙 add/remove, (2) 빈 트랙 클릭으로 키 add, 키 드래그로 move, Del로 delete, (3) playhead 드래그=scrub(즉시 평가→뷰포트 반영), (4) Play/Stop, (5) 선택 키 인스펙터 편집, (6) save/load `.wseq.json`. **완료기준은 CameraTrack에서 key 찍고 scrub하면 뷰포트 카메라가 그 값으로 움직이는 것.**

---

## 4. 데이터 흐름 (presentation / truth 경계)

```text
[SequencerPanel] 편집
   → .wseq.json (또는 .wseq) 저장
        → CAssetStreamingSystem (handle/state 로드, 에디터 우회 금지)
             → CSequenceAsset (역직렬화)
                  → CSequencePlayer::Play()
                       → Tick(dt): time 진행 → 각 트랙 evaluate
                            ├ Camera   → SeqBindingResolver → CCamera::SetPosition/SetPerspective   [presentation]
                            ├ Anim     → ModelRenderer::PlayAnimationByNameAdvanced                  [presentation]
                            ├ Fx       → FX 시스템 spawn 트리거 (.wfx handle)                        [presentation]
                            ├ Audio    → CGameInstance::PlaySoundOn/PlayEffect                       [presentation]
                            ├ Visibility→ entity/DataLayer toggle 신호                               [presentation]
                            ├ TimeDilation→ Scene dt multiplier                                      [presentation]
                            └ Event    → SeqEventSink.PushCandidate(event,payload)  ──────┐         [후보만]
                                                                                          ▼
                                                                          Server GameSim (판정 권위)  [truth]
                                                                                          │
                                                                          Snapshot/Event/FX Cue → Client
```

**불변식**:
1. 에디터가 만든 `.wseq`는 항상 `CAssetStreamingSystem`을 거쳐 로드(에디터 전용 경로 금지). 같은 데이터를 EldenRingClient 런타임이 그대로 로드.
2. **Sequence는 gameplay truth를 직접 판정하지 않는다.** EventTrack은 후보(`boss_intro_done` 등)만 발행하고, hitbox/damage/phase 전이는 Server GameSim 권위(17문서 2.3, 11문서 4.3, 03문서 Boss/Raid). 즉 EventTrack의 `ApplyDamage`류는 금지 — 오직 트리거 후보.
3. 네트워크 컷신: 서버가 "시퀀스 X 재생" 이벤트를 보내면 클라들이 같은 `.wseq`를 동시 재생(11문서 Stage6). 동기화 권위는 서버, 재생은 presentation.

---

## 5. 구현 순서 (S0~S6, 완료기준 + 게이트)

전제(17문서 4절·12문서 게이트): RHI G2(텍스처+라이트+스태틱메시)와 에디터 셸 G3가 선행. **G2/G3 전에는 Sequencer 패널을 크게 벌리지 않는다.** Sequencer 자체 게이트는 **G7(camera track scrub/play)**.

| 단계 | 내용 | 완료기준 | 게이트 |
|---|---|---|---|
| **S0** | `CSequenceAsset` + `.wseq.json` 로더(Camera 트랙만 파싱) + `CSequencePlayer::Play/Tick/Stop/IsPlaying` 골격. binding resolver는 단일 cinematic CCamera만 | JSON 1개 로드 → `Tick`이 time 진행 → CameraTrackEvaluator가 보간된 pos/fov를 `CCamera::SetPosition/SetPerspective`에 적용. 로그로 평가값 확인 | (선행 G3) |
| **S1** | `SequencerPanel` 최소: 타임라인 ruler + CameraTrack 키 표시 + playhead scrub | 패널에서 playhead 드래그 → 뷰포트 cinematic 카메라가 키 사이 보간으로 이동 | — |
| **S2** | 키 편집: Camera key add/move/delete + 인스펙터(pos/rot/fov/interp) + Play/Stop + save/load `.wseq.json` | 키 찍고 저장 → 재실행 로드 → 동일 재생. **Sequencer G7 통과** | **G7** |
| **S3** | AnimTrack + AudioTrack (가장 직매핑되는 두 트랙) | Anim key가 `PlayAnimationByNameAdvanced` 호출, Audio key가 `PlaySoundOn` 호출. 컷신에서 보스 roar 애니+사운드 동기 | (G7 이후) |
| **S4** | FxTrack(.wfx handle spawn 트리거) + VisibilityTrack(toggle 신호) | Fx key가 FX 시스템 spawn 1회, Visibility key가 HUD/엔티티 toggle. **FX 연결은 G6(WFX bake) 통과 후에만** | (G6 의존) |
| **S5** | TimeDilationTrack(dt scale) + EventTrack(후보 큐) | 슬로우모션 구간에서 Scene dt가 scale됨. Event key가 `SeqEventSink`에 후보 push(판정 없음) | — |
| **S6** | `.wseq` binary 승격(로더 분기) + 서버 트리거 동기 재생 hook(후보 인터페이스) | binary/JSON 동일 재생. 서버 "play seq" 이벤트로 클라 재생(판정은 서버) | (G9 서버권위 의존) |

**게이트 막힘 대응**: G7(camera scrub/play) 막히면 AnimTrack 이후(S3+) 전부 중단하고 카메라 평가/보간만 고친다. FX 연결(S4)은 G6(WFX bake) 전엔 시작 금지 — `.wfx` handle이 없으면 FxTrack은 "ref only"로 둔다.

---

## 6. 코드 스켈레톤 (Winters 타입: f32_t/f64_t/u32_t/Vec3/Mat4)

```cpp
// ── CSequenceAsset.h ─────────────────────────────────────────────
#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"
#include <string>
#include <vector>

enum class eSeqTrackType : u8_t
{
    Camera, Anim, Fx, Audio, Event, Visibility, TimeDilation
};

enum class eSeqInterp : u8_t { Constant, Linear, Cubic };

// 트랙별 key payload (union 대신 type별 vector 보유 — 단순/디버그 용이)
struct SeqCameraKey   { f64_t time; Vec3 pos; Vec3 rotEulerDeg; f32_t fov; eSeqInterp interp; bool_t cut; };
struct SeqAnimKey     { f64_t time; std::string anim; bool_t loop; bool_t reverse; f32_t speed; };
struct SeqFxKey       { f64_t time; std::string wfx; std::string anchor; bool_t oneShot; };
struct SeqAudioKey    { f64_t time; std::string sound; u32_t channel; f32_t volume; };
struct SeqEventKey    { f64_t time; std::string event; std::string payload; };
struct SeqVisKey      { f64_t time; bool_t visible; };
struct SeqDilationKey { f64_t time; f32_t scale; eSeqInterp interp; };

struct SeqTrack
{
    eSeqTrackType eType;
    std::string   strName;
    std::string   strBinding;          // "cinematic_cam" | "entity:boss_0" | "layer:HUD" | ""

    std::vector<SeqCameraKey>   cameraKeys;
    std::vector<SeqAnimKey>     animKeys;
    std::vector<SeqFxKey>       fxKeys;
    std::vector<SeqAudioKey>    audioKeys;
    std::vector<SeqEventKey>    eventKeys;
    std::vector<SeqVisKey>      visKeys;
    std::vector<SeqDilationKey> dilationKeys;
};

class CSequenceAsset
{
public:
    static bool_t LoadFromJson(const std::string& strPath, CSequenceAsset& out); // S0
    static bool_t LoadFromBinary(const std::string& strPath, CSequenceAsset& out); // S6
    bool_t SaveToJson(const std::string& strPath) const;                          // S2

    std::string             strName;
    f64_t                   durationSec = 0.0;
    u32_t                   displayRate = 60;
    bool_t                  bLoop = false;
    std::vector<SeqTrack>   tracks;
};

// ── SeqBindingResolver.h ─────────────────────────────────────────
// binding 문자열 → 런타임 대상. Engine public 타입만 노출(의존 역전 방지).
class CCamera;          // Engine/Public/Renderer/CCamera.h
class ModelRenderer;    // Engine/Public/Renderer/ModelRenderer.h

class ISeqBindingResolver
{
public:
    virtual ~ISeqBindingResolver() = default;
    virtual CCamera*       ResolveCamera(const std::string& binding) = 0;
    virtual ModelRenderer* ResolveModel (const std::string& binding) = 0;
    virtual void           SetLayerVisible(const std::string& layer, bool_t bVisible) = 0;
    virtual void           SetTimeDilation(f32_t scale) = 0;
};

// ── SeqEventSink.h ───────────────────────────────────────────────
// EventTrack 후보 발행 — 판정 금지, 서버로 가는 후보만.
class ISeqEventSink
{
public:
    virtual ~ISeqEventSink() = default;
    virtual void PushCandidate(const std::string& event, const std::string& payload) = 0;
};

// ── CSequencePlayer.h ────────────────────────────────────────────
class CSequencePlayer
{
public:
    void Play(const CSequenceAsset* pAsset,
              ISeqBindingResolver* pResolver,
              ISeqEventSink* pEventSink /*nullable*/);
    void Stop();
    void Tick(f32_t dt);          // 03문서 update 순서의 FX 직전에 삽입
    void Seek(f64_t timeSec);     // 에디터 scrub
    bool_t IsPlaying() const { return m_bPlaying; }
    f64_t  GetTime() const { return m_dTime; }

private:
    void EvaluateCamera(const SeqTrack& t);       // pos/fov 보간 → CCamera
    void EvaluateAnim(const SeqTrack& t);         // key 발화 → ModelRenderer::PlayAnimationByNameAdvanced
    void EvaluateAudio(const SeqTrack& t);        // key 발화 → PlaySoundOn/PlayEffect
    void EvaluateFx(const SeqTrack& t);           // key 발화 → FX spawn 트리거
    void EvaluateEvent(const SeqTrack& t);        // key 발화 → SeqEventSink.PushCandidate
    void EvaluateVisibility(const SeqTrack& t);   // step 평가 → SetLayerVisible
    void EvaluateTimeDilation(const SeqTrack& t); // 보간 → SetTimeDilation

    // 연속 채널 보간 (Camera/TimeDilation). discrete 트랙은 prevTime<key.time<=curTime 발화.
    static f32_t SampleScalar(f64_t t, f64_t t0, f64_t t1, f32_t v0, f32_t v1, eSeqInterp e);
    static Vec3  SampleVec3  (f64_t t, f64_t t0, f64_t t1, const Vec3& a, const Vec3& b, eSeqInterp e);

    const CSequenceAsset*  m_pAsset = nullptr;
    ISeqBindingResolver*   m_pResolver = nullptr;
    ISeqEventSink*         m_pEventSink = nullptr;
    f64_t                  m_dTime = 0.0;
    f64_t                  m_dPrevTime = 0.0;   // discrete 키 발화 edge 감지
    bool_t                 m_bPlaying = false;
};
```

**평가 규칙 메모**:
- 연속 채널(Camera pos/fov, TimeDilation): 현재 time을 둘러싼 두 키를 찾아 `interp`로 보간. cut 키는 보간 무시 즉시 적용.
- discrete 트랙(Anim/Fx/Audio/Event): `m_dPrevTime < key.time <= m_dTime`인 키를 이번 Tick에 **edge로 1회 발화**(Animator의 `HasFramePassed` 패턴과 동일 사상, Animator.h:36 참조). Seek(scrub) 시에는 재발화 정책을 별도(에디터 미리듣기는 발화 억제 옵션).
- Visibility: step. 현재 time 이하 마지막 키의 visible 적용.

---

## 7. 검증 · 리스크

### 7.1 빌드 타겟별 (17문서·12문서 검증 기본값)

```powershell
# diff 위생
git diff --check

# Engine 변경 (CSequence*를 Engine/Public/Cinematic으로 승격한 경우)
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' Winters.sln /t:Engine /m /p:Configuration=Debug /p:Platform=x64 /v:minimal

# Editor 패널 변경
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' Winters.sln /t:EldenRingEditor /m /p:Configuration=Debug-DX12 /p:Platform=x64 /v:minimal

# Elden 런타임 재생 검증
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' Winters.sln /t:EldenRingClient /m /p:Configuration=Debug-DX12 /p:Platform=x64 /v:minimal

# LoL 영향 (visual smoke 유지)
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' Winters.sln /t:Client /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
```
Engine public header(Cinematic 승격) 변경 시 `UpdateLib.bat`로 SDK sync 확인. `/utf-8` 누락 시 한글 주석 CP949 오판(메모리 규칙) 주의 — 새 vcxproj 항목에 `/utf-8` 포함 확인.

### 7.2 게이트 막힐 때

| 막힌 게이트 | 대응 |
|---|---|
| G3(에디터 셸) 미통과 | Sequencer 패널 착수 금지. RHI/도크스페이스부터 |
| G7(camera scrub/play) 미통과 | S3+ 전면 중단. 보간/binding resolver만 고침. cut 키·연속 보간 단순화 |
| G6(WFX bake) 미통과 | FxTrack(S4)을 ref-only로 두고 spawn 미연결 |
| G9(서버 권위) 미통과 | EventTrack은 후보 push까지만, 네트워크 동기 재생(S6) 보류 |

### 7.3 과설계 방지 (Karpathy 가드레일)

- UE5 `FrameTime/60000 tick`, Evaluation Template, ECS evaluation, Spawnable, Sub-sequence/Shot, full Blending은 **전부 초기 제외**. `f64_t timeSec` + 트랙당 단일 키 배열 + 직접 평가로 시작.
- Section 레이어/BlendType는 "additive 카메라 셰이크가 실제로 필요해질 때"만 도입.
- 트랙 type 7개를 한 번에 만들지 말고 Camera→Anim/Audio→Fx/Vis→Dilation/Event 순서로 게이트 통과마다 추가.
- EventTrack에 판정 로직을 넣고 싶은 유혹 차단 — 후보 발행이 전부.

---

## 8. Codex 요구사항 프롬프트 (복붙용)

```text
SYSTEM=SEQUENCER

너는 Winters 엔진에 UE5 Sequencer급 타임라인 시스템을 구축하는 시니어 엔진/툴 엔지니어다.
목표: .wseq(Camera/Anim/Fx/Audio/Event/Visibility/TimeDilation 7트랙) 한 파일을
CSequencePlayer가 시간축 평가해서 에디터 뷰포트와 Elden 런타임에서 동일하게 연출을 재생한다.
첫 완료기준 = CameraTrack key 찍기 → scrub → play가 뷰포트에서 보이는 것.

[절대 원칙 — 위반 시 작업 무효]
1. UE5 Sequencer는 reference depot일 뿐. 코드 복사/모듈 링크/MovieScene object model 이식 금지.
   개념(Track/Section/Channel/Key, Possessable/Spawnable, evaluation 분리)만 Winters식 재구성.
2. "에디터 화면 먼저 크게" 금지. runtime contract(.wseq + CSequencePlayer) 먼저 작게 증명 →
   패널이 그 contract를 편집. 모든 단계 완료기준 = ".wseq/JSON seed → runtime preview".
3. .wseq는 CAssetStreamingSystem 거쳐 로드(에디터 우회 금지). 같은 데이터를 EldenRingClient가 그대로 로드.
4. Sequence는 gameplay truth를 직접 판정하지 않는다. EventTrack은 후보만 발행, 판정/damage/phase는
   Server GameSim 권위. presentation(camera/anim/fx/audio/vis/dilation)/truth(event 판정) 분리.
5. Engine→Client 의존 역전 금지. Client/Public·Shared에 DX11/DX12 concrete type 노출 금지.
   binding은 ISeqBindingResolver 뒤로 숨겨 Engine public 타입(CCamera/ModelRenderer)만 평가기에 노출.
6. normal F5 LoL runtime을 우회·은폐 금지. LoL DX11 visual smoke 계속 검증.

[환경]
- 저장소: C:/Users/tnest/Desktop/Winters
- 런타임 타입(검증됨):
  · Engine/Public/Renderer/CCamera.h: CCamera::SetPosition(Vec3), SetPerspective(fovY,aspect,near,far),
    Ready(...), GetEye/GetAt/GetUp, m_vEye/m_vAt/m_vUp/m_fFov
  · Engine/Public/Renderer/ModelRenderer.h: PlayAnimationByNameAdvanced(name,loop,reverse,speed),
    HasAnimationByName, GetAnimationDurationSecondsByName, Update(f32_t); 하위 Engine/Public/Resource/Animator.h
    CAnimator::GetCurrentTime/SetPlaySpeed/IsPlaying/HasFramePassed
  · Engine/Include/GameInstance.h:53-55: PlaySoundOn(key,eSoundChannel,vol)/PlayEffect/PlayBGM
  · Engine/Public/FX/FxAsset.h: FxAssetHandle, eFxRenderType, eFxAnchorType (FX spawn 진입점은 FX_NIAGARA 소유)
- 미구현(이 시스템 신규): CSequenceAsset, CSequencePlayer, 트랙 evaluator, .wseq 로더/세이버, SequencerPanel(ImGui)

[먼저 읽을 문서 — 순서대로]
1. .md/EldenRing/18_SEQUENCER_DESIGN_WSEQ.md   ← 이 설계(스키마·클래스·순서·게이트·스켈레톤)
2. .md/EldenRing/17_UE5_GRADE_EDITOR_SUITE_MASTER.md (2.3 Sequencer, 4절 순서/게이트)
3. .md/EldenRing/12_UE5_REFERENCE_DX12_RHI_EDITOR_BIG_PICTURE.md (Phase G, 게이트 G7)
4. .md/EldenRing/06_FX_GRAPH_SEQUENCER_EDITOR.md (Sequencer 섹션·트랙표)
5. .md/EldenRing/03_ELDEN_CLIENT_RUNTIME_ARCHITECTURE.md (runtime update 순서)
6. .md/문서/11_Ch11_Cinematics_Sequencer.md (UE5 MovieScene 레퍼런스) + CLAUDE.md/.claude/gotchas.md

[작업 범위 — 18문서 5절 S0~S6]
- S0: CSequenceAsset + .wseq.json 로더(Camera) + CSequencePlayer::Play/Tick/Stop/IsPlaying 골격.
- S1: SequencerPanel 최소(타임라인 ruler + Camera 키 + playhead scrub).
- S2: Camera key add/move/delete + 인스펙터 + Play/Stop + save/load .wseq.json → 게이트 G7.
- S3: AnimTrack + AudioTrack (직매핑).  S4: FxTrack(G6 후) + VisibilityTrack.
- S5: TimeDilationTrack + EventTrack(후보 큐).  S6: .wseq binary + 서버 트리거 동기(G9 의존).
- 평가 규칙: 연속 채널(Camera/Dilation) 보간, discrete(Anim/Fx/Audio/Event) prevTime<key<=curTime edge 1회 발화,
  Visibility step, cut 키 즉시 전환. scrub 시 discrete 재발화 억제 옵션.

[작업 루프 — 게이트 통과까지]
1. 선행 게이트 확인: RHI G2 + 에디터 셸 G3 됐는지. 안 됐으면 패널 확장 멈추고 선행부터.
2. runtime contract 먼저: CSequenceAsset(.wseq.json) + CSequencePlayer(Camera 평가)부터 구현·로그 검증.
3. 패널이 그 contract 편집: SequencerPanel 추가. 완료기준은 항상 "편집→저장→런타임 preview" 왕복.
4. 게이트 검증: 18문서 5절. G7(camera scrub/play) 전에는 S3+ 금지.
5. presentation/truth 구분: EventTrack은 후보만(서버 판정). camera/anim/fx/audio/vis/dilation은 presentation.
6. 막히면 사유 분류(서버 권위/의존 역전/게이트 미통과) 보고하고 나머지는 계속.

[빌드 검증]
- Engine(승격 시): MSBuild Winters.sln /t:Engine /p:Configuration=Debug /p:Platform=x64
- Editor: /t:EldenRingEditor /p:Configuration=Debug-DX12
- Elden: /t:EldenRingClient /p:Configuration=Debug-DX12
- LoL 영향: /t:Client (visual smoke 유지).  git diff --check.
- Engine public header 변경 시 UpdateLib.bat SDK sync. 새 vcxproj 항목 /utf-8 포함 확인.

[완료 기준 — SEQUENCER]
- camera track key/scrub/play + .wseq.json 저장→재실행 로드→동일 재생(G7).
- Anim/Audio key 발화가 ModelRenderer/PlaySoundOn 호출, Fx key가 FX spawn 1회, Visibility/Dilation 적용.
- EventTrack은 후보만 push(판정 없음). 네트워크 동기 재생은 서버 트리거(판정 서버).

[금지사항]
- UE FrameTime(60000 tick)/Evaluation Template/ECS evaluation/Spawnable/Sub-sequence/Shot/full Blending
  초기 도입 금지(과설계). f64_t timeSec + 트랙당 단일 키 배열 + 직접 평가로 시작.
- EventTrack에 판정/damage/phase 로직 삽입 금지. Client 구체 타입을 평가기에 직접 노출 금지.
- FX 연결은 G6(WFX bake) 전 금지(FxTrack ref-only).

[시작]
지금: (1) 위 문서 읽고, (2) 선행 게이트(G2/G3) 충족과 CCamera/ModelRenderer/Sound 현재 API 상태 집계 보고,
(3) S0(CSequenceAsset + CSequencePlayer Camera 평가)부터 구현하라. 막히면 사유 분류 보고하고 나머지는 계속.
```
