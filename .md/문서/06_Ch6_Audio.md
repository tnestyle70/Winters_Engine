# Ch6. Audio (3D Spatial / DSP Graph / Wwise/FMOD Interop)

> Winters 현재: `Engine/External/FMOD` 런타임 사용, `Engine/Public/Sound/`에 간단한 wrapper.
> 레퍼런스: `UnrealEngine/Engine/Source/Runtime/AudioMixer/Public/`, `Engine/Plugins/Runtime/Metasound/`.

---

## 1. 기초 원리 — 오디오는 신호 그래프다

옛날: `PlaySound2D("explosion.wav")`. 끝.

문제:
- 동굴에서 폭발 → reverb는?
- 폭발 100개 동시 → mixer 폭주
- 거리에 따른 attenuation, doppler shift
- 캐릭터 발자국 surface별로 다르게
- 게임플레이 신호(쿨다운 ready)와 환경음 동시
- 음악이 게임 상태(전투/평화)에 맞춰 동적으로

해결책: **소스 → DSP 노드 그래프 → 출력 device**.

```text
Source 1 (explosion) ─┐
Source 2 (footstep)  ─┼──→ SFX Submix ──┐
Source 3 (footstep)  ─┘                 ├─→ Reverb Send ──┐
                                        │                 ├─→ Master Submix → Output
Source 4 (BGM)       ─→ Music Submix ───┤                 │
Source 5 (voice)     ─→ Voice Submix ───┘                 │
                            EQ ─────────────────────────────┘
```

각 노드는 **버퍼 단위 신호처리**. 매 ms 수십~수백 sample씩 흘러간다.

---

## 2. 핵심 — UE5 AudioMixer 6대 구성

### 2.1 AudioDevice

플랫폼 오디오 API와의 진입점. WASAPI / CoreAudio / OpenSL / XAudio2 등을 wrap.

`Source/Runtime/AudioMixer/Public/AudioMixerDevice.h:36~50`:

```cpp
namespace Audio
{
    class FMixerSourceManager;
    class FMixerSourceVoice;
    class FMixerSubmix;
    class FAudioFormatSettings;
    class FAudioRenderScheduler;

    typedef TSharedPtr<FMixerSubmix, ESPMode::ThreadSafe> FMixerSubmixPtr;

    struct FAudioThreadTimingData
    {
        // Sample-accurate timing 정보. 음악 cue를 박자에 정확히 맞추는 데 사용
    };
}
```

### 2.2 Source / Voice

재생 중인 소리 1개 = 1 Voice. 동시 voice 수에 제한 (보통 64~256).

```text
Voice {
    SoundWave*       data
    float            volume, pitch
    Vec3             worldPosition
    AttenuationCurve curve
    SubmixSend[]     sendTargets    // 어느 submix에 보낼지
    bool             isLooping
}
```

### 2.3 Submix (믹스 버스)

여러 source의 출력을 합쳐 효과를 거는 노드.

```text
Master Submix
├── SFX Submix
│   ├── Reverb (DSP)
│   └── Compressor
├── Music Submix
│   ├── EQ
│   └── Sidechain (전투 시 음악 -3dB)
├── Voice Submix
└── UI Submix    (3D 안 함, 항상 풀볼륨)
```

### 2.4 DSP Effect Chain

각 submix에 다중 effect 직렬 연결. UE5 기본 제공:
- Reverb (convolution / algorithmic)
- EQ (parametric, multi-band)
- Compressor / Limiter
- Delay / Chorus / Flanger / Distortion
- Sidechain Ducker

### 2.5 Metasound (DSP Graph)

UE5 5.0+: **데이터 자산으로 DSP 노드 그래프를 만든다**. 옛 SoundCue를 대체.

`UnrealEngine/Engine/Plugins/Runtime/Metasound/Source/MetasoundEngine/`.

```text
Metasound asset (예: gun_shot):
   Input(GunType: int) ──┐
                         ├── BranchByEnum ──→ WaveAsset ──→ Gain ──┐
   Input(Distance) ──────┘                                         ├── 출력
                                              LowPass(cutoff=f(distance))
```

런타임에서:
```cpp
USoundBase* Sound = GunShotMetasound;
UGameplayStatics::PlaySoundAtLocation(World, Sound, Location, ...,
    /* paramSet: */ {{"GunType", 2}, {"Distance", 25.f}});
```

→ 한 asset이 데이터로 분기. 코드 변경 없이 사운드 디자이너가 편집.

### 2.6 Audio Spatializer

3D 위치 → 좌/우 채널 + HRTF (binaural).
- Default: ITD/ILD 기반
- Steam Audio / Resonance Audio: HRTF + occlusion + reverb
- Wwise: 자체 spatializer

---

## 3. 심화

### 3.1 Occlusion / Obstruction

벽 너머의 소리가 막혀야 자연스럽다.
- **Occlusion**: 발사 지점 → 청취자 raycast. 막히면 LowPass cutoff 낮춤.
- **Obstruction**: 청취자 정면 차단 (low) vs 측면 차단 (mid).
- **Portal**: 방 안의 소리가 문 통해 새어 나옴. acoustics volume + portal mesh.

UE5: `Source/Runtime/AudioMixer/Public/AudioMixerSubmix.h`의 reverb send + `Plugins/Runtime/Steam Audio`.

### 3.2 Music Adaptive

게임 상태에 따라 음악 트랙 전환. 자연스러운 transition을 위해:
- **Stinger**: 짧은 cue (전투 시작 0.5초 fanfare)
- **Vertical layering**: 같은 곡, 트랙별 (drum + bass + lead) volume 조절
- **Horizontal switching**: 트랙 끝나는 박자에 다른 곡 시작

Wwise interactive music이 이 패턴 전문. UE5 Metasound로도 가능.

### 3.3 Voice Bank / Localization

```text
voice/
   en_us/
      character_intro.wav  → key="char_intro" (DataTable)
   ko_kr/
      character_intro.wav
```

Ch15 Data pipeline의 localization과 묶임.

### 3.4 Wwise / FMOD interop

상용 미들웨어는 own runtime이 있고 엔진은 callback만.

```cpp
// Wwise 흐름
AK::SoundEngine::PostEvent("Play_Gunshot", GameObjectID);
AK::SoundEngine::SetRTPCValue("Distance", 25.f, GameObjectID);
```

장점: tooling 1급, 디자이너 자율성, 플랫폼 mixer 검증됨.
단점: 라이선스 비쌈, 빌드 통합 복잡.

Winters는 현재 **FMOD**. 라이브 서비스 가면 Wwise 검토.

### 3.5 Concurrency / Voice Stealing

물리적 voice 수 제한(64) 초과 시 어떤 voice를 죽일까?
- Priority + Distance + Age 가중치
- "Important" flag 박힌 voice는 보호 (보스 대사)

---

## 4. Winters 매핑

### 4.1 현재 상태

`Engine/Public/Sound/`에 FMOD wrapper만. 단일 channel 단위 재생. submix/DSP/spatializer 없음.

### 4.2 Ch6 추가 헤더 (제안)

```cpp
// Engine/Public/Audio/AudioEngine.h
class WINTERS_ENGINE CAudioEngine
{
public:
    void Initialize(const AudioEngineDesc& desc);
    void Shutdown();
    void Tick(f32_t dt);

    AudioVoiceHandle PlayOneShot(SoundAssetID asset, const PlayParams& params);
    AudioVoiceHandle PlayAttached(SoundAssetID asset, EntityID owner, const PlayParams& params);
    void Stop(AudioVoiceHandle voice, f32_t fadeOut = 0.f);

    SubmixHandle CreateSubmix(const char* name, SubmixHandle parent);
    void AddEffect(SubmixHandle submix, std::unique_ptr<IAudioEffect> effect);

    void SetListener(const Vec3& pos, const Quat& rot, const Vec3& vel);
private:
    std::unique_ptr<IAudioBackend> m_backend;   // FMOD / Wwise / 자체
};

struct PlayParams
{
    Vec3   worldPosition;
    f32_t  volume = 1.f;
    f32_t  pitch  = 1.f;
    bool_t spatialize = true;
    SubmixHandle submix = SubmixHandle::Default;
    // metasound style 파라미터
    std::unordered_map<std::string, AudioParamValue> params;
};

// Shared/GameSim/Systems/AudioCueSystem.h
// 서버 actionSeq → 클라 audio cue 매핑 (single source of truth)
class CAudioCueSystem : public ISystem
{
public:
    void OnGameplayEvent(const GameplayEventCue& cue);  // 서버에서 도착
};
```

### 4.3 Ch4 Animation Notify와의 합류

```text
montage.notify [t=0.3, type=AudioCue, payload="sword_hit"]
  ↓ 클라가 montage 재생 중 hit notify를 받음
  ↓ CAudioEngine.PlayAttached("sword_hit", owner, params)
```

이게 single source of truth. `Ch4 + Ch6 + Ch8`이 같은 데이터(montage notify / GameplayCue)를 공유.

### 4.4 Bot AI 불변식 재확인

- AI는 사운드를 모른다. AI는 `GameCommand("CastQ")`만 발행. 서버가 그 결과로 actionSeq broadcast → 클라가 사운드 재생.
- **AI perception**(소리로 적 인지)은 별개. server-side로 `HearStimulus`를 ECS event로 publish. 실제 audio 재생과는 다른 시스템(Ch9 Perception).

### 4.5 단계별

```text
Ch6-Stage1  CAudioEngine + Voice + Submix 기본 (FMOD 위에 wrap)
Ch6-Stage2  3D spatialize + attenuation curve
Ch6-Stage3  Submix + DSP effect (reverb, EQ)
Ch6-Stage4  Occlusion raycast (Ch5 raycast 사용)
Ch6-Stage5  Audio cue data (.wsound) + 데이터 분기 (Metasound 등가)
Ch6-Stage6  Music adaptive layer
Ch6-Stage7  Voice bank + localization (Ch15와 묶임)
Ch6-Stage8  Wwise 검토 (라이브 서비스 진입 시)
```

### 4.6 게임별 적용

| 게임 | 필요 Stage |
|------|-----------|
| LoL (현재) | Stage 1~3 (3D + submix + reverb로 충분) |
| 로아 | Stage 1~5 + adaptive music |
| 엘든링 | Stage 1~7 + voice bank + dialogue |
| GTA6 | Stage 1~8 + Wwise + radio + crowd chatter |

---

## 5. 검증 명령

```powershell
.\Client\Bin\Debug-DX12\WintersGame.exe --audio-trace --audio-show-voices

# 기대 로그
# [Audio] device init: WASAPI 48000Hz 2ch buffer=1024
# [Audio] submix tree: Master → [SFX, Music, Voice, UI]
# [Audio] voice spawn: sword_hit @ (12,0,34), spatialize=true, distance=8.3m
# [Audio] occlusion: ray hit, lowpass cutoff=2500Hz
```

---

## 6. 다음 챕터로

Ch6 Stage 5까지 가야 **Ch11 Sequencer**의 audio track이 같은 데이터(metasound)를 쓴다. Ch9 AI Perception은 audio 시스템과 독립적으로 hearing stimulus만 발행.
