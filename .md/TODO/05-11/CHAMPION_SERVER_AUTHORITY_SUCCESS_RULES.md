# 챔피언 서버 권위 성공 규칙

작성일: 2026-05-11

목적: 이렐리아 서버 권위 Golden Slice를 성공시키면서 확인한 규칙, 삽질 원인, 튜닝 기준을 박제한다. 칼리스타, 제드, 나머지 챔피언으로 확장할 때 같은 문제를 반복하지 않기 위한 기준 문서다.

---

## 0. 현재 Golden Result

현재 성공 기준은 아래다.

```text
Irelia:
  우클릭 지면 -> 이동
  우클릭 적, BA 사거리 안 -> 이동 중지 후 서버 이벤트 기반 BA 애니/FX 재생
  우클릭 적, BA 사거리 밖 -> 가만히 멈추지 않고 target 쪽으로 chase/move
  BA 이후 Q/W/E/R 입력 가능. pending BA intent가 스킬을 막으면 안 됨
  Q UnitTarget -> caster-target 벡터로 dash
  W press -> stage1 hold, 커서 방향 회전
  W release -> stage2, 커서 방향 회전, release FX
  E -> blade/beam visual이 지형 위로 떠야 함
  R -> stale facing이 아니라 커서 방향

Kalista 1차 bridge:
  BA/Q action 중 우클릭 지면 -> passive dash 방향 pending
  network action 종료 -> 기존 Kalista recovery hook으로 passive dash 발동
```

이 문서는 "모든 챔피언 완료" 선언이 아니다. 이렐리아로 만든 성공 패턴이다. 모든 챔피언은 AI 진입 전 같은 게이트를 통과해야 한다.

---

## 0.5. 챔피언 이식 전 문서 검색 게이트

이미 client-only에서 한 번 튜닝된 챔피언은 코드부터 보지 않는다. 먼저 archive/TODO/AGENTS에 남아 있는 성공 규칙과 gotcha를 검색한다.

필수 순서:

```text
현재 로그 확인
-> 챔피언 이름 + 핵심 애니/FX 이름 + gotcha 키워드로 문서 검색
-> 기존 client-only 성공 규칙 확인
-> 서버 권위 구조에 어떤 규칙을 그대로 이식할지 비교
-> 그 다음 현재 코드 수정
```

공통 검색:

```powershell
rg -n "<Champion>|<core_anim>|<core_fx>|conditional|recoveryFrame|castFrame|gotcha" .md AGENTS.md Client -S
```

칼리스타처럼 이전 튜닝 이력이 있는 챔피언은 아래 검색을 먼저 실행한다.

```powershell
rg -n "Kalista|kalista_attack1_dash_0|kalista_spell1_dash_0|conditional|recoveryFrame|gotcha" .md AGENTS.md Client -S
```

왜 필요한가:

```text
현재 코드/로그만 보면 서버 권위 전환 중 생긴 새 버그처럼 보일 수 있다.
하지만 실제 원인이 "이전 client-only 성공 규칙을 잊고 stage2/NetAnimation 구조에 끼워맞춘 것"일 수 있다.
archive에 이미 박제된 챔피언별 애니/후속 모션 규칙은 서버 권위 이식 시에도 우선 보존한다.
```

이 게이트를 건너뛰면 같은 챔피언에서 이미 해결했던 문제를 다시 파게 된다.

---

## 1. 서버 권위 규칙

네트워크 권위 모드는 항상 이 흐름을 따른다.

```text
Client input
-> GameCommand
-> Server CommandExecutor 검증/실행
-> Server NetAnimation / EffectTrigger / Snapshot
-> Client EventApplier / SnapshotApplier
-> Client visual hook
```

최종 위치, 데미지, 쿨타임, stage, hit, death, objective state는 GameSim/server 소유다.

클라는 아래만 담당한다.

```text
입력 수집
Command 송신
snapshot 보간
animation cue 재생
FX cue 재생
UI / ImGui / debug trace
```

네트워크 권위 모드에서 아래 상황은 금지다.

```text
Client ApplyLocalPrediction이 gameplay/FX 실행
Server EffectTrigger도 같은 gameplay/FX 실행
```

이러면 FX 2번, animation 2번, action lock 꼬임, "왜 두 번 나갔지?" 문제가 다시 생긴다.

---

## 2. 데이터 원천 규칙

현재는 아직 같은 값이 여러 파일에 있다. 챔피언을 만질 때는 반드시 아래 파일을 같이 확인한다.

```text
Client/Private/GameObject/SkillTable.cpp
Client/Private/GameObject/Champion/<Champion>/<Champion>_Registration.cpp
Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp
Shared/GameSim/Definitions/ChampionRuntimeDefaults.h
```

동기화해야 하는 값:

```text
target mode
range
cooldown
lock/action duration
cast/recovery frame
stage count
stage2 target mode
stage window
anim play speed
stage2 play speed
transition idle/run animation
transition duration
attack range
move speed
```

검증용 fast timing은 테스트가 명시적으로 필요할 때만 켠다. 기본값으로 켜두면 `0.2s` 같은 검증값이 기존 튜닝을 덮어서 "서버 디폴트로 박힌 것 같은" 버그가 다시 나온다.

장기 목표:

```text
ChampionData.json 또는 ChampionData.lua
-> 한 번 파싱/codegen
-> Client SkillRegistry
-> Server ChampionRuntimeDefaults
```

그 전까지는 수동 수정 시 최소 세 파일 동기화 작업으로 취급한다.

---

## 3. NetAnimation stage 규칙

`NetAnimationComponent::flags`는 stage를 상위 비트에 저장한다.

```cpp
anim.flags = static_cast<u16_t>(static_cast<u16_t>(sanitizedStage) << 12);
```

의미:

```text
stage 1 -> 0x1000
stage 2 -> 0x2000
decode  -> flags >> 12
```

이전의 잘못된 판정:

```cpp
flags >= 2
```

이건 틀렸다. stage가 작은 정수로 들어있는 게 아니라 상위 nibble에 packed 되어 있다.

정답:

```cpp
const u16_t stage = static_cast<u16_t>(flags >> 12);
```

이 문제가 이렐리아 W stage1/stage2가 이상하게 보였던 핵심 원인 중 하나였다.

---

## 4. 이동과 액션 규칙

서버가 skill 또는 basic attack을 accept하면 현재 MoveTarget을 반드시 지운다.

```text
skill accept -> ClearMoveTarget(issuer)
basic attack accept -> ClearMoveTarget(issuer)
```

이유:

```text
이동 목표가 남아 있음
-> 서버가 공격/스킬 중에도 계속 이동시킴
-> 클라는 공격 애니를 보지만 snapshot은 locomotion을 계속 밀어냄
-> 멈추지 않음, 공격 안 됨, 다음 스킬이 막힘 같은 조작감 붕괴 발생
```

반대 방향도 필요하다.

```text
우클릭 지면
-> client pending attack intent 정리
-> Move command 송신
```

Q/W/E/R 입력도 command 생성 전에 pending network BA intent를 정리해야 한다. 안 그러면 BA intent가 다음 스킬을 먹거나 지연시킨다.

주요 파일:

```text
Shared/GameSim/Systems/CommandExecutor.cpp
Client/Private/Scene/InGameCombatInputBridge.cpp
Client/Private/Scene/InGameSkillDispatchBridge.cpp
Client/Private/Scene/InGamePlayerControlBridge.cpp
```

---

## 5. 기본 공격 규칙

우클릭 의도는 아래처럼 나뉜다.

```text
커서 아래 적이 있고 effective BA range 안
-> BasicAttack command

커서 아래 적이 있지만 effective BA range 밖
-> target 방향 chase/move. 절대 가만히 멈추면 안 됨

적이 없거나 지면
-> Move command
```

effective BA range는 단순 skill row 숫자만이 아니다. 실린더 피킹과 챔피언/타겟 반지름 감각이 포함되어야 한다.

```text
effective range = 튜닝된 champion attack range + 실전 target/caster radius 보정
```

현재 이렐리아 Golden 값:

```text
BA range: 1.5f
```

성공 로그:

```text
[BA] network attack intent target=...
[Command] client send basic-attack sid=... myNet=... seq=... targetNet=...
[Command] basic-attack accept issuer=... target=... seq=... damage=... cooldown=...
[ModelRenderer] Playing (loop=false): skinned_mesh_irelia_attack_01
[EventApplier] fx cue champion=... slot=0 stage=1 ...
```

accept 직후 반복 우클릭 때문에 `cooldown` reject가 나오는 것은 정상일 수 있다. 그것만으로 버그가 아니다.

버그 패턴:

```text
[BA] network attack intent target=...
client send basic-attack 없음
```

의미:

```text
client target resolve / netID mapping / pending intent gate 중 하나가 실패
```

버그 패턴:

```text
[Command] basic-attack reject reason=out-of-range
champion이 chase/move하지 않고 멈춤
```

의미:

```text
out-of-range attack intent가 chase/move fallback으로 내려가지 않음
```

---

## 6. 스킬 stage 규칙

2-stage 스킬은 client가 stage를 명시해서 서버에 보낸다.

```text
press   -> slot=N stage=1
release -> slot=N stage=2
```

서버는 stage window가 열려 있지 않은 stage2를 reject해야 한다.

```text
stage1 window 없음 + stage2 요청 -> reject reason=stage2-window
```

이게 없으면 "W press가 곧바로 W release처럼 처리되는" 문제가 다시 생긴다.

이렐리아 W Golden 동작:

```text
W press:
  client sends slot=2 stage=1 targetNet=self
  server accepts reason=ok
  client plays spell2 hold animation
  champion rotates toward cursor

W release:
  client sends slot=2 stage=2 direction=cursor
  server accepts reason=stage2
  client plays spell2_2
  W release FX spawns in cursor direction
```

성공 로그:

```text
[Command] client send cast-skill ... slot=2 stage=1 targetNet=...
[Command] cast-skill accept reason=ok champion=1 issuer=... slot=2 ...
[IreliaSim] W hold caster=...

[Command] client send cast-skill ... slot=2 stage=2 targetNet=0 dir=(...)
[Command] cast-skill accept reason=stage2 champion=1 issuer=... slot=2 ...
[IreliaSim] W release caster=...
[WRel] Spawn owner=...
```

---

## 7. 방향 벡터 규칙

방향은 스킬의 실제 target mode에서 와야 한다. stale facing을 재사용하면 안 된다.

예시:

```text
Irelia Q:
  UnitTarget
  dash direction = caster -> target vector

Irelia W release:
  Direction
  direction = cursor ray ground point - caster position

Irelia R:
  Direction
  direction = cursor direction

GroundTarget skills:
  ground hit position 사용
```

버그 증상:

```text
Q가 순간이동처럼 느껴짐
R이 커서 방향이 아니라 현재 바라보는 방향으로 나감
W release FX 방향이 틀림
```

의미:

```text
command builder 또는 server hook이 잘못된 vector source를 사용 중
```

---

## 8. FX 규칙

네트워크 권위 모드의 FX 시작점은 서버 `EffectTrigger` 하나다.

```text
Server CommandExecutor / champion sim
-> ReplicatedEvent::EffectTrigger
-> Client EventApplier
-> CVisualHookRegistry
-> champion visual hook
```

visual hook은 model/PNG/projectile/ribbon/beam 생성을 담당한다. 최종 damage/hit truth는 담당하지 않는다.

성공 로그:

```text
[EventApplier] fx cue champion=... sourceNet=... targetNet=... effectID=... slot=... stage=... tick=...
```

로그는 있는데 visual이 없으면 아래를 본다.

```text
1. visual hook 미등록
2. effectID가 champion/slot과 다르게 매핑
3. asset path 오류
4. mesh/texture는 로드됐지만 scale/offset/y-lift 오류
5. hook 내부가 아직 legacy local-only 상태
```

visual이 두 번 보이면 아래를 의심한다.

```text
local prediction과 server event가 동시에 spawn
```

---

## 9. 애니메이션 전환 규칙

서버 action timing이 메인 애니메이션을 구동한다. 클라는 action duration 종료 뒤 idle/run을 복구한다.

```text
NetAnimation action active
-> skill/BA anim 1회 재생
-> action duration 종료
-> optional transition anim
-> movement state 기준 idle 또는 run
```

이렐리아 주요 transition:

```text
spell1_to_idle
spell1_into_runbase
spell2_to_run
spell3_to_idle
spell3_run
spell4_to_idle
spell4_to_run
```

버그 패턴:

```text
skill anim이 계속 반복됨
```

가능 원인:

```text
network action state가 끝나지 않음
actionSeq 변화 없음
duration decode/apply 실패
client transition이 bActionActive를 clear하지 않음
```

버그 패턴:

```text
skill anim이 즉시 idle/run으로 잘림
```

가능 원인:

```text
lockDuration/action duration이 너무 짧음
fast timing override가 켜져 있음
recovery 전에 transition이 발동함
```

---

## 10. 이렐리아 튜닝 규칙

성공한 이렐리아 규칙:

```text
BA:
  range 1.5f
  accepted BA clears movement
  skill input clears pending BA intent

Q:
  UnitTarget
  server dash uses caster-target vector
  transition returns to idle/run

W:
  stage1 press hold
  stage2 release only on key release
  rotate toward cursor on both hold/release
  release FX uses cursor direction

E:
  stage1 blade placed at ground target
  stage2 second blade/beam
  visual must be lifted above terrain

R:
  cursor direction
  transition to idle/run after action
```

이렐리아 smoke target:

```text
Red Sylas dummy
필요 시 collider/debug visible
실린더 기준 BA target 가능
Q/R direction을 dummy/cursor 기준으로 검증
```

---

## 11. 칼리스타 튜닝 규칙

칼리스타는 passive dash 루프가 핵심이다.

```text
BA/Q action 시작
BA/Q action 중 우클릭으로 passive dash 방향 저장
recovery/action end에서 pending dash 소비
```

현재 1차 bridge:

```text
Client/Private/Scene/InGameCombatInputBridge.cpp
  network mode 우클릭에서 TryQueueLocalPassiveDashFromCursor 호출

Client/Private/Scene/InGameChampionStateBridge.cpp
  Kalista만 queue 가능
  network action state가 BasicAttack 또는 SkillQ일 때 dash window 열림
  TriggerNetworkPassiveDashFromAction이 Kalista::OnRecoveryFrame_PassiveDash 호출

Client/Private/Scene/Scene_InGame.cpp
  network action end에서 Kalista passive dash bridge 호출
```

성공 로그:

```text
[KalistaPassive] pending dir=(...) slot=0
[KalistaPassive] dash dir=(...) slot=0 anim=kalista_attack1_dash_0

[KalistaPassive] pending dir=(...) slot=1
[KalistaPassive] dash dir=(...) slot=1 anim=kalista_spell1_dash_0
```

중요:

```text
현재 bridge는 client-side recovery bridge다.
snapshot 보정 때문에 튀거나 되돌아오면 passive dash를 server GameSim으로 올려야 한다.
```

최종 칼리스타 구조:

```text
server receives/derives passive dash direction
server sim moves Kalista
snapshot replicates final position
client only animates/interpolates
```

client-only 성공 gotcha:

```text
conditional 후속 애니 = recoveryFrame hook에서 직접 PlayAnimationByName 호출
```

칼리스타 passive dash 같은 "1타 후 conditional 후속 모션"은 SkillDef stage2 시스템에 끼워맞추지 않는다.

```text
잘못된 방향:
  BA stage1 -> dash를 stage2처럼 처리
  NetAnimation stage2 / SkillDef stage2에 passive dash 애니를 넣음

문제:
  stage2는 사용자 입력 1타/2타, cooldown, mana, castFrame, target mode가 별도로 있는 스킬용이다.
  passive dash는 action recovery에 붙는 conditional 후속 모션이다.

정답:
  recoveryFrame/action end hook 안에서 slot 분기
  slot=0 -> PlayAnimationByName("kalista_attack1_dash_0", false)
  slot=1 -> PlayAnimationByName("kalista_spell1_dash_0", false)
  dash 중 자동 idle/run 교체는 passive dash active guard로 막는다.
```

이번 서버 권위 이식에서 확인한 추가 판정 기준:

```text
로그에 dash_0가 actionSeq당 1회만 찍히면 애니메이터 자동 반복 문제가 아니다.
Q -> spell1_dash_0가 깔끔하고 BA -> attack1_dash_0만 이상하면 passive dash 파이프라인보다 BA 전용 dash clip/transition을 먼저 의심한다.
old binary/source 로그는 slot=0 anim=kalista_attack1_dash_0로 남을 수 있으므로, 최신 빌드에서는 기대 anim 이름부터 확인한다.
```

---

## 12. 미니언 규칙

AI 전에 미니언은 반드시 검증되어야 한다. AI는 미니언 상태를 먹고 판단한다.

필수 동작:

```text
loading barrier 이후 spawn
server waypoint 기준 이동
lane formation이 옆으로 퍼지지 않고 전진 방향 유지
separation/spatial correction은 겹침을 막되 wave를 옆으로 돌리면 안 됨
attack state에서 actionSeq 증가
client는 actionSeq 변화로 attack animation 재생
death animation 1회
stale/remove로 dead visual 제거
```

성공 로그:

```text
[GameRoom] First minion wave scheduled tick=...
[GameRoom] Spawn minion wave #...
[MinionVisual] bind network entity=... type=... team=...
[EventApplier] animation play netID=... animID=10 seq=...
[ModelRenderer] Playing (loop=false): minion_*_attack*
[ModelRenderer] Playing (loop=false): minion_*_death
[SnapshotApplier] remove stale minion netID=... entity=...
```

버그 패턴:

```text
minion이 공격 중에도 run animation 유지
```

가능 원인:

```text
server attack actionSeq 미복제
client가 minion NetAnimation action 무시
locomotion idle/run이 one-shot attack을 즉시 덮어씀
```

---

## 13. 검증 게이트

이 순서대로 검증한다. 여기 통과 전 AI로 넘어가지 않는다.

```text
Gate 1: Server + Client build pass
Gate 2: 1 client Irelia + red Sylas dummy
Gate 3: Irelia BA/move/Q/W/E/R pass
Gate 4: Kalista BA/Q/passive dash first pass
Gate 5: Zed Q/W/E visual parity
Gate 6: Minion movement/attack/death/remove pass
Gate 7: 3-client smoke
Gate 8: 위 통과 후 AI command generator
```

이 게이트는 클라 단독 테스트로는 충분하지 않다. 서버를 띄우고 snapshot/event가 흐르는 상태에서만 서버 권위 버그가 드러난다.

---

## 14. 디버그 체크리스트

문제가 생기면 렌더 화면보다 로그를 먼저 본다.

```text
1. client가 command를 보냈는가?
   [Command] client send ...

2. server가 accept/reject 했는가?
   [Command] ... accept/reject reason=...

3. server animation이 내려왔는가?
   [EventApplier] animation play ...

4. server FX가 내려왔는가?
   [EventApplier] fx cue ...

5. client visual target이 bind 됐는가?
   [SnapshotApplier] bind ...
   [MinionVisual] bind ...

6. local bridge가 막거나 덮었는가?
   [SkillDispatch] rejected ...
   [BA] network attack intent ...
   [NetworkEndTransition] ...
```

렌더 화면은 마지막 결과다. 원인은 command/server/snapshot/event 중간 로그에 있다.

---

## 15. 먼저 볼 파일

Server/GameSim:

```text
Shared/GameSim/Systems/CommandExecutor.cpp
Shared/GameSim/Systems/MoveSystem.cpp
Shared/GameSim/Systems/SkillCooldownSystem.cpp
Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp
Shared/GameSim/Components/NetAnimationComponent.h
Server/Private/Game/GameRoom.cpp
Server/Private/Game/SnapshotBuilder.cpp
```

Client network/gameplay:

```text
Client/Private/Network/Client/CommandSerializer.cpp
Client/Private/Network/Client/EventApplier.cpp
Client/Private/Network/Client/SnapshotApplier.cpp
Client/Private/Scene/InGameCombatInputBridge.cpp
Client/Private/Scene/InGameSkillDispatchBridge.cpp
Client/Private/Scene/InGamePlayerControlBridge.cpp
Client/Private/Scene/InGameChampionStateBridge.cpp
Client/Private/Scene/Scene_InGame.cpp
```

Champion:

```text
Client/Private/GameObject/SkillTable.cpp
Client/Private/GameObject/Champion/Irelia/Irelia_Registration.cpp
Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp
Client/Private/GameObject/Champion/Kalista/Kalista_Registration.cpp
Client/Private/GameObject/Champion/Kalista/Kalista_Skills.cpp
Client/Private/GameObject/Champion/Zed/Zed_Registration.cpp
Client/Private/GameObject/Champion/Zed/Zed_Skills.cpp
```

---

## 16. 회귀 금지 목록

다시는 아래를 넣지 않는다.

```text
flags >= 2 stage decode
fast timing default enabled
skill/BA accept 후 stale MoveTarget 유지
pending BA intent가 Q/W/E/R 입력 차단
client local prediction과 server FX가 같은 효과 double spawn
W stage2가 stage1 window 없이 accept
사거리 밖 우클릭이 chase/move가 아니라 freeze
attack range가 실린더/radius 감각을 무시
visual hook 등록만 하고 실제 asset spawn 없음
minion locomotion이 attack animation을 즉시 덮어씀
```

이 목록은 실제로 이미 시간을 크게 쓴 문제들이다.
