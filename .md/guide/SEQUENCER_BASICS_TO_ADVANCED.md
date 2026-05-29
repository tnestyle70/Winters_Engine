# Sequencer Basics To Advanced

이 문서는 Elden Ring 준비용으로 Winters에 붙일 Sequencer를 이해하기 위한 개념 설명이다. 구현 계획은 `.md/plan/engine/2026-05-21_ELDEN_GAMEPLAY_SEQUENCER_MERGE_PLAN.md`를 따른다.

## 1. Sequencer가 해결하는 문제

Sequencer는 "시간에 따라 무엇을 언제 바꿀 것인가"를 한 시스템으로 다루는 장치다.

예를 들어 보스가 포효한다면 실제로는 여러 일이 동시에 일어난다.

- 0.0초: 보스 포효 애니메이션 시작
- 0.2초: 카메라 흔들림 시작
- 0.4초: 바닥 마법진 FX 생성
- 1.2초: 충격파 FX 생성
- 1.3초: 플레이어가 범위 안이면 피해 적용
- 2.0초: 보스 AI를 다음 패턴으로 전환

이걸 각각의 시스템에 흩어 쓰면 보스 코드, FX 코드, 카메라 코드, 데미지 코드가 서로 시간을 맞추려고 꼬인다. Sequencer는 이 전체를 하나의 timeline으로 묶는다.

핵심 한 줄:

```text
Sequencer = 시간축 + 트랙 + 키/이벤트 + 런타임 바인딩 + 평가기
```

## 2. 가장 작은 모델

가장 작은 Sequencer는 다음 네 가지면 된다.

```text
Sequence
  Track
    Cue / Key / Section
SequencePlayer
Binding
```

`Sequence`는 전체 타임라인 asset이다. "Boss_Phase2_Intro" 같은 이름을 가진다.

`Track`은 한 종류의 일을 담당한다. AnimationTrack, FxTrack, CameraTrack, DamageTrack처럼 나눈다.

`Cue`는 특정 시간에 한 번 발생하는 이벤트다. 예를 들어 `t=1.3s Damage 120` 같은 것.

`Key`는 시간에 따른 값이다. 예를 들어 `t=0 FoV 60`, `t=1 FoV 40`이면 중간값을 보간한다.

`Binding`은 sequence 안의 추상 이름을 실제 런타임 객체에 연결하는 것이다. Sequence asset은 "Boss"라고만 알고, 실행할 때 "이번 방의 Margit entity"에 연결한다.

## 3. Tick 평가 원리

런타임은 매 tick마다 현재 시간을 계산하고, 모든 track을 평가한다.

```text
previousTime -> currentTime
각 Track 평가
  Continuous track: currentTime의 값을 계산해서 적용
  Event track: previousTime과 currentTime 사이에 지나간 cue를 한 번만 fire
```

여기서 중요한 것은 event cue가 한 번만 실행되어야 한다는 점이다. 프레임 드랍으로 `0.9s -> 1.4s`로 건너뛰어도 `1.2s` cue는 실행되어야 하고, 다음 tick에서 다시 실행되면 안 된다.

Winters의 서버 GameSim에서는 floating second보다 fixed tick이 안전하다.

```text
30 ticks/sec
0.5 sec = 15 ticks
1.3 sec = 39 ticks
```

서버, 리플레이, 봇 AI가 같은 결과를 내야 하므로 gameplay sequencer는 tick 기반으로 시작하는 편이 좋다.

## 4. Track 종류

기초 track:

- `AnimationTrack`: 특정 시점에 `NetAnimationComponent`를 갱신한다.
- `FxTrack`: `EffectTrigger` cue를 내보낸다.
- `DamageTrack`: 서버에서 `DamageRequest`를 만든다.
- `TransformTrack`: entity 위치/회전/스케일을 시간 함수로 바꾼다.
- `CameraTrack`: 카메라 위치, FoV, shake, target을 제어한다.

심화 track:

- `MontageTrack`: 애니메이션 section, blend, notify를 가진다.
- `AudioTrack`: 음악, one-shot sound, volume curve를 가진다.
- `SubSequenceTrack`: 큰 컷씬을 작은 sequence 여러 개로 조립한다.
- `DirectorTrack`: 어떤 카메라를 화면에 쓸지 결정한다.
- `SpawnTrack`: sequence 동안만 존재하는 actor/FX를 만든다.
- `StateTrack`: input lock, AI pause, boss phase flag 같은 상태를 제어한다.

Winters의 첫 병합은 `Animation / FX / Damage`까지만 가는 것이 맞다. 이 셋은 이미 서버 권위 이벤트 파이프라인이 있으므로 새 시스템이 기존 흐름을 우회하지 않는다.

## 5. Binding: 재사용성의 핵심

나쁜 방식:

```text
BossIntro sequence가 entity 1234를 직접 참조
```

좋은 방식:

```text
BossIntro sequence는 "Boss", "Player", "ArenaCenter" binding만 참조
런타임에서 Boss -> 현재 보스 entity로 연결
```

이렇게 해야 같은 sequence를 여러 보스, 여러 스테이지, 리플레이, 테스트 scene에서 재사용할 수 있다.

Elden boss 패턴은 binding이 특히 중요하다.

```text
Boss = 현재 패턴을 실행하는 보스
Target = lock-on 대상 플레이어
Weapon = 오른손 무기 socket
ArenaCenter = 페이즈 전환 연출 중심점
```

## 6. Server Authority에서의 Sequencer

멀티플레이 gameplay sequence는 서버가 진실을 가져야 한다.

```text
Client Input -> GameCommand -> Server GameSim -> Sequencer Tick -> Snapshot/Event -> Client Visual
```

서버가 해야 하는 일:

- 패턴 시작 여부 결정
- 데미지, hit 판정, 상태 변화 적용
- 어떤 animation/effect cue가 발생했는지 event로 송신
- sequence 시작 tick과 cue 실행 순서를 결정

클라가 해야 하는 일:

- 서버 event를 받아 animation/FX/sound/camera를 재생
- 이미 받은 cue를 중복 재생하지 않기
- 화면 보간, camera shake, post-process 같은 presentation 처리

이 원칙 때문에 DamageTrack은 클라에서 직접 HP를 깎으면 안 된다. 클라는 damage event를 받아 UI 숫자와 피격 연출을 보여준다.

## 7. Client Deterministic vs Server Replicated

두 모델이 있다.

`Client deterministic`:

```text
서버: Sequence A를 tick 1000에 시작하라고 broadcast
클라: 로컬 asset Sequence A를 직접 재생
```

장점은 네트워크 비용이 작고, 컷씬/메뉴/순수 연출에 좋다. 단점은 모든 클라가 같은 asset을 가지고 있어야 하고 gameplay truth에는 부적합하다.

`Server replicated`:

```text
서버: 매 cue 또는 중요한 상태를 event/snapshot으로 보냄
클라: 받은 것만 재생
```

장점은 서버 권위와 리플레이에 강하다. 단점은 이벤트 설계가 더 필요하다.

Winters의 현재 우선순위는 server authoritative gameplay이므로, Elden 준비용 첫 단계는 server replicated cue 방식이 맞다.

## 8. Elden Boss Pattern으로 보는 예시

보스가 검을 들어 올리고, 1초 뒤 지면을 내려찍는 패턴:

```text
Sequence: Boss_HeavySlam
Source: Boss
Target: Player

Cue @ 0 ticks
  Animation: SkillR / heavy_slam_start

Cue @ 8 ticks
  Effect: SwordTrail.Start attached to Weapon

Cue @ 24 ticks
  Effect: GroundTelegraph at Target predicted position

Cue @ 36 ticks
  Effect: SlamImpact at Boss forward point

Cue @ 36 ticks
  Damage: 180 physical to players inside hit shape

Cue @ 48 ticks
  Animation: Idle or recovery
```

MVP에서는 hit shape 전체를 아직 넣지 않고, target entity damage cue로 시작할 수 있다. 다음 단계에서 AreaDamageTrack, ShapeQuery, LagCompensation을 붙인다.

## 9. Keyframe과 Curve

Event cue는 한 번 fire한다. Curve는 값을 계속 평가한다.

```text
Camera FoV curve
t=0.0  FoV 60
t=0.3  FoV 42
t=1.0  FoV 55
```

보간 방식:

- Linear: 일정 속도
- Step: 값이 순간 변경
- Cubic: 부드러운 ease in/out
- Custom curve: 디자이너가 직접 조절

Elden식 보스 연출에서는 camera/FoV/post-process가 curve가 되고, damage/FX/animation notify는 event cue가 된다.

## 10. Section과 Montage

긴 animation 하나를 그냥 재생하는 것보다 section을 나누면 조작성이 좋아진다.

```text
Boss_Attack_Montage
  Section: Windup
  Section: Strike
  Section: Recovery
```

Notify는 animation 안의 event다.

```text
Notify @ Strike frame 12: SpawnImpactFx
Notify @ Strike frame 13: ApplyDamage
```

Server authority에서는 notify도 두 갈래로 나눠야 한다.

- Visual notify: 클라 FX/sound
- Gameplay notify: 서버 damage/hit/state

처음에는 gameplay notify를 sequence cue로 직접 표현하고, 나중에 MontageTrack과 통합하는 쪽이 안전하다.

## 11. Sub-sequence와 Shot

큰 컷씬은 하나의 긴 timeline으로 만들면 유지보수가 힘들다.

```text
BossIntro_Main
  Shot 1: GateOpen
  Shot 2: BossReveal
  Shot 3: PlayerReaction
  Shot 4: CombatStart
```

Sub-sequence는 협업 단위다. 한 명은 camera shot, 한 명은 FX, 한 명은 animation을 맡을 수 있다.

Winters에서는 MVP 후순위다. 먼저 server cue sequencer가 안정된 뒤 들어가는 것이 좋다.

## 12. Editor가 필요한 이유

Sequencer는 코드로만 만들면 금방 한계가 온다.

디자이너가 원하는 일:

- timeline에서 key를 드래그
- animation preview
- FX timing 조절
- camera curve 조절
- event 중복 fire 확인
- sequence를 저장하고 hot reload

하지만 editor를 먼저 만들면 runtime 진실이 흐려질 수 있다. Winters는 runtime core를 먼저 만들고, 그 다음 JSON/.wseq loader, 마지막에 ImGui timeline editor로 가는 순서가 안전하다.

## 13. Winters 단계 제안

```text
Stage 1: Server GameplaySequencerSystem
  Animation / Effect / Damage cue

Stage 2: Boss pattern trigger
  boss AI 또는 debug command가 sequence 시작

Stage 3: Area/shape damage
  capsule, sphere, cone, ground AoE

Stage 4: Camera cue
  client presentation event, local camera modifier stack

Stage 5: Asset loader
  JSON or .wseq cooked binary

Stage 6: Editor preview
  ImGui timeline, key edit, hot reload

Stage 7: Montage/sub-sequence
  animation notify, shot track, director track
```

## 14. 흔한 실패

- 클라에서 damage를 먼저 적용한다.
- effect를 로컬 skill hook과 network event 양쪽에서 재생한다.
- sequence event가 frame drop 때 누락된다.
- event cue가 매 tick 반복 실행된다.
- binding 없이 entity id를 asset에 박아 재사용을 막는다.
- editor부터 시작해서 server authority 흐름을 나중에 억지로 붙인다.
- boss pattern과 cinematic을 별도 시스템으로 만들어 시간축 코드가 두 벌이 된다.

Winters에서는 기존 규칙대로 서버가 gameplay truth를 만들고, 클라는 visual path에서 한 번만 재생하는 구조를 유지해야 한다.
