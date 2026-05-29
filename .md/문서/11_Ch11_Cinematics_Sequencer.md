# Ch11. Cinematics / Sequencer / Camera

> Winters 현재: `Client/Public/DynamicCamera.h` 단일. 시네마틱 트랙/시퀀서 **없음.**
> 레퍼런스: `UnrealEngine/Engine/Source/Runtime/MovieScene/Public/`, `Runtime/MovieSceneTracks/Public/`, `Source/Editor/Sequencer/Private/`.

---

## 1. 기초 원리 — 컷씬, 보스 패턴, 카메라는 같은 문제

다음 4가지는 본질적으로 같은 시스템이 필요하다:
1. **컷씬**: 영화처럼 카메라/캐릭터를 시간축으로 제어
2. **보스 페이즈**: HP 70%에서 카메라 zoom + 음악 변화 + 어둠 + 이펙트
3. **시간 기반 메뉴**: 메인 메뉴 인트로 동영상
4. **카메라 흔들림**: 폭발 시 shake + 색조 + 모션 블러

각 시스템 따로 짜면 4종의 시간축 시스템이 생기고 디자이너가 4번 배워야 함.

**Sequencer의 통찰**: 시간축에 트랙(Track)을 쌓고, 각 트랙이 특정 property(transform, color, audio, event)를 시간 함수로 평가. 컷씬도 보스 패턴도 메뉴 인트로도 같은 데이터.

---

## 2. 핵심 — UE5 MovieScene 5대 구성

### 2.1 MovieScene (Asset)

`Source/Runtime/MovieScene/Public/MovieScene.h`. Sequencer asset의 데이터 모델.

```cpp
class UMovieScene : public UMovieSceneSignedObject
{
public:
    UPROPERTY()
    TArray<FMovieSceneBinding> ObjectBindings;        // 영향 받는 actor binding

    UPROPERTY()
    TArray<FMovieSceneSpawnable> Spawnables;          // 시퀀스 동안만 spawn

    UPROPERTY()
    TArray<FMovieScenePossessable> Possessables;      // 기존 actor 점유

    FFrameRate TickResolution;     // 60000fps tick 등 fixed resolution
    FFrameRate DisplayRate;        // 30/60 등 display fps
};
```

### 2.2 Track / Section / Channel

```text
MovieScene "Boss_Phase2_Intro" (10초)
├── Possessable: BossActor (참조)
│   ├── TransformTrack
│   │   ├── Section [0~3s]: Float channels (X,Y,Z,Roll,Pitch,Yaw, 각각 키프레임)
│   │   └── Section [3~10s]: ...
│   └── AnimationTrack
│       └── Section: AnimMontage_BossIntro
├── Possessable: CinematicCamera
│   ├── TransformTrack ...
│   ├── CameraSettingsTrack (FoV, Aperture)
│   └── PostProcessTrack (Vignette, Saturation)
├── AudioTrack
│   └── Section [0~10s]: Soundcue_BossMusic
└── EventTrack
    ├── Event @ t=1.5s: TriggerLight
    ├── Event @ t=5.0s: ApplyDamage(player, 10)
    └── Event @ t=9.0s: EndPhase
```

`Track`: 한 종류의 영향 (transform, audio, event...).
`Section`: track 안의 시간 구간 (여러 section 합성 = additive / override / blend).
`Channel`: section 안의 1차 데이터 (X 채널, Y 채널, color R 채널 각각 키프레임 리스트).

UE5는 채널 종류만 30+: float, bool, byte, integer, string, enum, audio trigger, ...

### 2.3 Sequence Player

```cpp
class UMovieSceneSequencePlayer
{
public:
    void Play();
    void Pause();
    void Stop();
    void SetPlaybackPosition(const FFrameTime& InFrameTime);
    void SetPlayRate(float InPlayRate);

    // Trigger 콜백
    DECLARE_MULTICAST_DELEGATE(FOnMovieSceneSequencePlayerEvent);
    FOnMovieSceneSequencePlayerEvent OnPlay, OnPause, OnFinished;
};
```

매 tick player가 현재 frame을 모든 track에 broadcast → 각 track이 자기 property 갱신.

### 2.4 Binding Resolver

Sequence asset은 추상 binding (예: "Boss"). 런타임에 어느 actor를 가리킬지 분리.

```cpp
// LevelSequenceActor가 spawn 시
LevelSeq.BindingOverrides[BossBindingID] = WorldBossActor;
SequencePlayer.Play();
// 이제 "Boss" track이 WorldBossActor에 작용
```

같은 컷씬 asset을 던전마다 다른 보스로 재사용 가능.

### 2.5 Camera System

`Cine Camera Actor`: 영화용. focal length, aperture, focus distance, sensor size.
`Camera Modifier`: 흔들림, FOV blend, post-process override.
`Player Camera Manager`: 카메라들의 우선순위 stack.

---

## 3. 심화

### 3.1 Sub-sequence / Shot Track

긴 컷씬은 sub-sequence들의 트랙으로 분해. 각 sub-sequence는 개별 asset.

```text
Cinematic_MainStory
├── Shot 1: Intro_City  (sub-sequence asset)
├── Shot 2: Dialogue    (sub-sequence asset)
└── Shot 3: BossReveal  (sub-sequence asset)
```

`MovieSceneCinematicShotTrack`. 디자이너 협업에 필수 (shot 별로 다른 사람이 작업).

### 3.2 Spawnable vs Possessable

- **Spawnable**: 시퀀스 시작 시 spawn, 끝나면 destroy (컷씬 전용 NPC, 이펙트)
- **Possessable**: 월드에 이미 있는 actor를 점유 (플레이어 캐릭, 보스)

### 3.3 Sequencer Editor

`Source/Editor/Sequencer/`. Track 추가, 키프레임 편집, 곡선 편집(Curve Editor), spawnable 박스 추가.

비디오 편집 툴(Premiere)과 비슷한 UX. 디자이너/연출가가 직접 사용.

### 3.4 Server-side Cinematic

멀티 게임에서 컷씬을 모든 클라가 같은 시점에 봐야 한다. 두 가지 모델:
- **Server-replicated**: 서버 sequence player가 진실, 클라들이 따라옴
- **Client-deterministic**: 서버는 "play" event만 broadcast, 각 클라가 같은 sequence asset 재생

LoL 챔프 처형 모션 = client-deterministic.
GTA6 미션 컷씬 = server-replicated (호스트).

### 3.5 LiveLink / Mocap

영화 캡처 데이터를 실시간 sequencer에 stream. 게임플레이 모션도 같은 시스템.

UE5: `Source/Runtime/LiveLink*`.

### 3.6 Camera Director / Take Recorder

Take Recorder: gameplay를 녹화 → MovieScene으로 export. 리플레이/마케팅 영상 제작에 사용.

---

## 4. Winters 매핑

### 4.1 현재 상태

`Client/Public/DynamicCamera.h`: 플레이어 추적 카메라 1개. shake / FOV change / cinematic camera 없음.

컷씬/보스 페이즈/메뉴 인트로 없음.

### 4.2 Ch11 추가 헤더 (제안)

```cpp
// Engine/Public/Cinematic/Sequence.h
class WINTERS_ENGINE CSequence
{
public:
    void AddTrack(std::unique_ptr<ITrack> track);
    void Evaluate(FrameTime frame, SequenceContext& ctx);

    FrameRate tickResolution = 60000;
    FrameRate displayRate    = 60;
    FrameTime startFrame, endFrame;
};

// Engine/Public/Cinematic/Track.h
class ITrack
{
public:
    virtual ~ITrack() = default;
    virtual void Evaluate(FrameTime frame, SequenceContext& ctx) = 0;
};

class WINTERS_ENGINE CTransformTrack : public ITrack { /* X/Y/Z/Rot 채널 */ };
class WINTERS_ENGINE CAudioTrack     : public ITrack { /* sound cue + volume */ };
class WINTERS_ENGINE CEventTrack     : public ITrack { /* discrete event 트리거 */ };
class WINTERS_ENGINE CCameraTrack    : public ITrack { /* FoV, focus, post-process */ };
class WINTERS_ENGINE CMontageTrack   : public ITrack { /* AnimMontage section */ };

// Engine/Public/Cinematic/SequencePlayer.h
class WINTERS_ENGINE CSequencePlayer
{
public:
    void Play (SequenceRef seq, const BindingMap& bindings);
    void Pause();
    void Stop();
    void Seek(FrameTime t);

    using EventCb = std::function<void()>;
    void OnFinished(EventCb cb);
};

// Engine/Public/Cinematic/CameraModifier.h
class ICameraModifier
{
public:
    virtual ~ICameraModifier() = default;
    virtual void Modify(CameraState& inOut, f32_t dt) = 0;
};
class CCameraShake     : public ICameraModifier { ... };
class CCameraFovBlend  : public ICameraModifier { ... };
class CCameraVignette  : public ICameraModifier { ... };
```

### 4.3 Bot AI / 서버 권위와의 관계

- **Client cinematic**: 클라 전용 (메인 메뉴 인트로, 메뉴 전환). 서버 무관.
- **Gameplay cinematic** (보스 페이즈, 처형 모션): 서버 trigger → 클라들이 같은 sequence 재생.
- AI는 cinematic 동안 input 차단 / 강제 모션 (sequence가 possessable 통해 actor 점유).
- **Damage는 여전히 서버 권위**. cinematic event track의 ApplyDamage는 서버에서만 실행.

### 4.4 Ch4 / Ch6 / Ch8 / Ch10과의 합류

```text
Cinematic Sequencer
   ├── TransformTrack       ─→ entity transform (Server snapshot 통해)
   ├── MontageTrack         ─→ Ch4 AnimMontage (notify에서 fx/sound 자동)
   ├── AudioTrack           ─→ Ch6 AudioEngine
   ├── EventTrack           ─→ Ch8 GAS ability trigger / cue
   ├── UI Track             ─→ Ch10 widget animation
   └── CameraTrack          ─→ DynamicCamera + modifier stack
```

→ Sequencer = 위 모든 시스템의 시간축 컨트롤러. 단일 데이터.

### 4.5 단계별

```text
Ch11-Stage1  CSequence + Track + Channel 기본 (transform / event)
Ch11-Stage2  SequencePlayer + 키프레임 보간 (linear / curve)
Ch11-Stage3  Camera modifier stack (shake, FOV, vignette)
Ch11-Stage4  Audio track (Ch6 연동)
Ch11-Stage5  Montage track (Ch4 연동)
Ch11-Stage6  Server-replicated sequence (멀티 게임 컷씬 동기화)
Ch11-Stage7  Sub-sequence / Shot track
Ch11-Stage8  Take Recorder (gameplay → sequence export)
Ch11-Stage9  Sequencer Editor (Ch12와 묶임)
```

### 4.6 게임별 적용

| 게임 | 필요 Stage |
|------|-----------|
| LoL (현재) | Stage 1, 3, 5 (처형 모션) |
| 로아 | Stage 1~6 (보스 페이즈, 컷씬) |
| 엘든링 | Stage 1~7 (보스 인트로, 엔딩) |
| GTA6 | Stage 1~9 + LiveLink + Mocap pipeline |

---

## 5. 검증 명령

```powershell
.\Client\Bin\Debug-DX12\WintersGame.exe --cine-debug

# 기대 로그
# [Cine] sequence loaded: Boss_Phase2.seq (3 actors, 5 tracks, duration=10s)
# [Cine] play started @ frame 0, rate=60fps
# [Cine] camera modifier active: Shake(amp=0.3, freq=12Hz)
# [Cine] event fired @ t=5.0s: ApplyDamage(player_0, 10) → server authoritative
```

---

## 6. 다음 챕터로

Ch11 Stage 9 (Sequencer Editor)는 Ch12에서 같이 짜야 한다. Ch4/Ch6/Ch8/Ch10이 모두 1차 도입 후에 Ch11 Stage 4+가 의미 있다.
