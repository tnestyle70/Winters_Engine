# FX Graph, Sequencer, Editor

## 목표

WintersElden은 Unreal과 비교했을 때 부족해 보일 수 있는 툴링 영역을 직접 보강하는 포트폴리오 무대다.

핵심:

1. FX Node Graph
2. Sequencer
3. World Partition Editor
4. Boss Pattern Editor
5. Hitbox Timeline Editor
6. Asset Browser/Loader Debugger

## 전체 구조

```
WintersElden/Editor/
├── EldenEditorLayer
├── WorldPartitionEditor
├── FxGraphEditor
├── SequencerPanel
├── BossPatternEditor
├── HitboxTimelineEditor
├── AssetBrowser
└── PreviewViewport
```

엔진 공용으로 승격 가능한 것은 추후 `Engine/Public/Editor` 또는 `Engine/Public/FX`로 이동한다. 초기에는 `WintersElden`에 두어 빠르게 검증한다.

## FX Graph 목표

Unreal Niagara 전체를 한 번에 따라 하지 않는다.

첫 목표:

```
Spawn -> Initialize -> Update -> Render
```

노드 종류:

| Stage | Node |
|---|---|
| Spawn | Burst, Rate, SpawnOnEvent |
| Init | InitPosition, InitVelocity, InitColor, InitLifetime |
| Update | Age, Gravity, Drag, CurlNoise, SizeOverLife, ColorOverLife |
| Render | Billboard, Mesh, Ribbon |
| Event | OnHit, OnPhaseEnter, OnDeath |

## FX Asset

초기 JSON:

```json
{
  "name": "Boss_Stomp_Dust",
  "emitters": [
    {
      "name": "DustRing",
      "nodes": [],
      "edges": []
    }
  ],
  "preview": {
    "duration": 3.0,
    "loop": true
  }
}
```

중기:

```
.wfx
```

## FX Runtime

```
CFxGraph
  -> validation
  -> compiled CPU execution plan
  -> EmitterInstance
  -> ParticlePool
  -> FxRenderSystem
```

결정:

1. visual-only FX는 클라 로컬 시드 사용 가능
2. gameplay-affecting FX는 서버 event id와 seed를 받는다
3. raid telegraph FX는 서버 action state에서 파생한다

## Sequencer 목표

사용처:

1. 보스 등장 컷신
2. phase transition
3. 레이드 wipe/win 연출
4. tutorial camera flythrough
5. editor preview

트랙 종류:

| Track | 역할 |
|---|---|
| CameraTrack | 카메라 위치/회전/FOV |
| AnimTrack | actor animation |
| FxTrack | FX spawn |
| AudioTrack | sound event |
| EventTrack | gameplay/editor callback |
| VisibilityTrack | actor/layer visible toggle |
| TimeDilationTrack | 슬로우 연출 |

## Sequence Asset

초기 JSON:

```json
{
  "name": "BossIntro_01",
  "duration": 8.0,
  "tracks": [
    {
      "type": "Camera",
      "binding": "CinematicCamera",
      "keys": [
        { "time": 0.0, "position": [0, 3, -8], "rotation": [10, 0, 0], "fov": 55 },
        { "time": 8.0, "position": [0, 5, -4], "rotation": [15, 0, 0], "fov": 40 }
      ]
    }
  ]
}
```

중기:

```
.wseq
```

## Sequence Runtime

```cpp
class CSequencePlayer
{
public:
    void Play(CSequenceAssetHandle asset);
    void Stop();
    void Tick(f32_t dt);
    bool IsPlaying() const;
};
```

Sequence는 직접 gameplay 결과를 판정하지 않는다. 컷신/연출/트리거에 집중한다.

## Editor Panels

### WorldPartitionEditor

기능:

1. cell grid overlay
2. loaded/queued/unloaded cell 색상
3. streaming source radius
4. DataLayer toggles
5. selected cell entity list
6. force load/unload

### FxGraphEditor

기능:

1. node add/delete/connect
2. pin type validation
3. preview play/stop/restart
4. parameter inspector
5. save/load `.wfx.json`

### SequencerPanel

기능:

1. timeline
2. track add/remove
3. keyframe add/move/delete
4. scrub playhead
5. preview camera
6. save/load `.wseq.json`

### BossPatternEditor

기능:

1. phase graph 편집
2. action list
3. range/cooldown/weight 설정
4. telegraph FX 연결
5. hitbox timeline 연결

### HitboxTimelineEditor

기능:

1. animation clip 선택
2. time scrub
3. hitbox window 추가
4. hurtbox display
5. active frame 표시
6. damage/knockback/parry flags 편집

## 에디터 모드와 게임 모드

Elden Editor는 별도 exe가 아니라 `WintersElden.exe` 내부 scene으로 시작한다.

```
Scene_EldenEditor
  -> play mode
  -> simulate mode
  -> edit mode
```

장점:

1. 같은 런타임에서 즉시 검증
2. ImGui 기반 빠른 개발
3. asset hot reload와 연결 쉬움

## 저장 포맷 전략

초기:

```
.json
```

이유:

1. diff 가능
2. 빠른 수정
3. 에디터 안정화 전 schema 변경 쉬움

중기:

```
.wfx
.wseq
.wboss
.whitbox
```

후기:

```
Content.winters
```

## 구현 순서

| 단계 | 내용 | 완료 기준 |
|---|---|---|
| T0 | Editor scene | ImGui dockspace |
| T1 | Asset browser | resource tree 표시 |
| T2 | Hitbox timeline | attack clip에 box window 추가 |
| T3 | Boss pattern editor | action data 편집 |
| T4 | Sequencer panel | camera track preview |
| T5 | FX graph editor | burst billboard preview |
| T6 | save/load JSON | 재실행 후 복구 |
| T7 | runtime asset handles | editor -> runtime 연결 |
| T8 | binary export | `.wfx/.wseq` |

## 포트폴리오 포인트

이 문서의 기능들은 단순 게임 콘텐츠가 아니다.

면접에서 보여줄 수 있는 질문:

1. Unreal Sequencer 같은 툴을 직접 만들 때 runtime과 editor asset을 어떻게 나누는가
2. particle graph를 CPU/GPU 실행 계획으로 어떻게 바꾸는가
3. boss action data와 hitbox timeline을 어떻게 서버 권위 시뮬레이션에 연결하는가
4. 에디터에서 저장한 data가 runtime과 network에서 어떻게 사용되는가
