# Engine DNA Hands-On Session 01

Session - 아직 전체가 흐릿한 상태에서 Winters, Unreal, Unity를 한 축으로 직접 보며 `World -> Object/Entity -> Command -> Simulation -> Snapshot -> Client Visual` 흐름을 잡는다.

## 0. 오늘 목표

오늘은 엔진 전체를 이해하려고 하지 않는다.

딱 하나만 잡는다.

```text
게임 안의 어떤 존재가
월드 안에 등록되고
입력/명령으로 상태가 바뀌고
서버가 진실을 만들고
클라이언트가 그 진실을 화면으로 보여준다.
```

이 한 줄이 잡히면 Unreal, Unity, Winters의 다른 개념들이 전부 붙기 시작한다.

오늘 외울 단어는 많지 않다.

```text
World
Object / Entity
Command
System
Snapshot
Presentation
Editor
```

## 1. 절대 길 잃지 않는 질문 4개

어떤 파일을 열어도 아래 4개만 물어본다.

```text
1. 이 파일은 상태를 소유하는가?
2. 이 파일은 상태를 바꾸는가?
3. 이 파일은 상태를 전송/직렬화하는가?
4. 이 파일은 상태를 보여주기만 하는가?
```

정답 예시:

```text
Shared/GameSim/Systems/Move/MoveSystem.cpp
  -> 상태를 바꾼다.

Shared/Schemas/Snapshot.fbs
  -> 상태를 전송하기 위한 계약이다.

Client/Private/Network/Client/SnapshotApplier.cpp
  -> 전송된 상태를 클라이언트 월드/표현에 적용한다.

Client/Private/Scene/Scene_InGameRender.cpp
  -> 상태를 화면으로 보여준다.
```

## 2. 오늘의 큰 그림

먼저 이 그림만 머리에 넣고 시작한다.

```text
User Input
  |
  v
Command
  |
  v
Server / GameSim
  |
  v
World State
  |
  v
Snapshot / Event
  |
  v
Client Visual
  |
  v
Render / Animation / FX / UI
```

Winters는 이 그림을 매우 직접적으로 가진다.

```text
Client Input -> GameCommand -> Server GameSim -> Snapshot/Event/FX Cue -> Client Visual
```

Unreal은 같은 본질을 Actor replication과 RPC로 가진다.

```text
Client Input -> Server RPC -> Server Actor State -> Replication -> Client Actor Visual
```

Unity는 기본 제공 구조만으로는 이 본질이 강제되지 않으므로 직접 설계해야 한다.

```text
Client MonoBehaviour -> Command Message -> Authoritative Server -> Snapshot -> Client Presentation
```

## 3. 준비

작업 루트:

```text
C:\Users\user\Desktop\Winters
```

Unreal 소스 루트:

```text
C:\Users\user\Desktop\UnrealEngine\UnrealEngine
```

Unity 설치:

```text
C:\Program Files\Unity\Hub\Editor\6000.0.79f1
```

PowerShell에서 확인:

```powershell
Test-Path C:\Users\user\Desktop\Winters\Shared\GameSim\Core\World\World.h
Test-Path C:\Users\user\Desktop\UnrealEngine\UnrealEngine\Engine\Source\Runtime\Engine\Classes\GameFramework\Actor.h
Test-Path "C:\Program Files\Unity\Hub\Editor\6000.0.79f1\Editor\Data"
```

세 개가 `True`면 시작 가능하다.

## 4. 실습 A - Unreal에서 Object와 World 보기

### 4.1 먼저 볼 파일

아래 순서로 연다.

```text
C:\Users\user\Desktop\UnrealEngine\UnrealEngine\Engine\Source\Runtime\CoreUObject\Public\UObject\Object.h
C:\Users\user\Desktop\UnrealEngine\UnrealEngine\Engine\Source\Runtime\Engine\Classes\GameFramework\Actor.h
C:\Users\user\Desktop\UnrealEngine\UnrealEngine\Engine\Source\Runtime\Engine\Classes\Engine\World.h
```

빠르게 찾기:

```powershell
Select-String -LiteralPath C:\Users\user\Desktop\UnrealEngine\UnrealEngine\Engine\Source\Runtime\CoreUObject\Public\UObject\Object.h -Pattern "class UObject"
Select-String -LiteralPath C:\Users\user\Desktop\UnrealEngine\UnrealEngine\Engine\Source\Runtime\Engine\Classes\GameFramework\Actor.h -Pattern "class AActor :"
Select-String -LiteralPath C:\Users\user\Desktop\UnrealEngine\UnrealEngine\Engine\Source\Runtime\Engine\Classes\Engine\World.h -Pattern "class UWorld"
```

### 4.2 여기서 볼 것

`Object.h`에서 볼 것:

```text
UObject는 Unreal의 가장 기본 객체다.
일반 C++ 객체가 아니라 reflection, serialization, GC, editor exposure의 기반이다.
```

`Actor.h`에서 볼 것:

```text
AActor : public UObject

Actor는 World 안에 놓이거나 spawn되는 gameplay object다.
```

`World.h`에서 볼 것:

```text
UWorld는 active game world다.
World 안에 Actor, Level, Network, Physics, Timer 같은 runtime state가 모인다.
```

### 4.3 내가 직접 써볼 요약

아래 빈칸을 직접 채운다.

```text
Unreal에서 UObject는 ____________________ 이다.
Unreal에서 AActor는 ____________________ 이다.
Unreal에서 UWorld는 ____________________ 이다.
```

예상 답:

```text
UObject = 엔진이 인식하는 기본 객체
AActor = 월드에 배치/spawn되는 게임 객체
UWorld = 액터와 레벨이 살아 있는 실행 세계
```

## 5. 실습 B - Winters에서 Entity와 World 보기

### 5.1 먼저 볼 파일

아래 순서로 연다.

```text
C:\Users\user\Desktop\Winters\Shared\GameSim\Core\World\World.h
C:\Users\user\Desktop\Winters\Shared\GameSim\Core\Ecs\Entity.h
C:\Users\user\Desktop\Winters\Shared\GameSim\Core\Ecs\TransformComponent.h
C:\Users\user\Desktop\Winters\Shared\GameSim\Components\HealthComponent.h
```

빠르게 찾기:

```powershell
Get-Content -LiteralPath Shared\GameSim\Core\World\World.h -Encoding UTF8 -First 160
Get-Content -LiteralPath Shared\GameSim\Core\Ecs\Entity.h -Encoding UTF8 -First 160
Get-Content -LiteralPath Shared\GameSim\Core\Ecs\TransformComponent.h -Encoding UTF8 -First 160
Get-Content -LiteralPath Shared\GameSim\Components\HealthComponent.h -Encoding UTF8 -First 160
```

### 5.2 여기서 볼 것

Winters는 Unreal처럼 `AActor : UObject` 구조를 중심으로 하지 않는다.

Winters GameSim 쪽은 더 직접적인 ECS 사고방식에 가깝다.

```text
Entity
  정체성

Component
  상태 조각

System
  상태를 바꾸는 로직

World
  Entity + Component + System이 살아 있는 공간
```

비교:

```text
Unreal:
  UWorld 안에 AActor가 있고, Actor가 Component를 가진다.

Winters:
  GameSim World 안에 Entity가 있고, Entity에 Component가 붙고, System이 처리한다.
```

### 5.3 내가 직접 써볼 요약

```text
Winters에서 Entity는 ____________________ 이다.
Winters에서 Component는 ____________________ 이다.
Winters에서 System은 ____________________ 이다.
Winters에서 World는 ____________________ 이다.
```

예상 답:

```text
Entity = 시뮬레이션 안의 대상 ID
Component = HP, 위치, 이동 목표 같은 상태 조각
System = MoveSystem, DamageSystem처럼 상태를 바꾸는 로직
World = Entity와 Component를 담고 System이 읽고 쓰는 시뮬레이션 공간
```

## 6. 실습 C - Winters에서 Command 보기

### 6.1 먼저 볼 파일

```text
C:\Users\user\Desktop\Winters\Shared\Schemas\Command.fbs
C:\Users\user\Desktop\Winters\Shared\GameSim\Systems\CommandExecutor\ICommandExecutor.h
C:\Users\user\Desktop\Winters\Shared\GameSim\Systems\CommandExecutor\CommandExecutor.cpp
```

빠르게 찾기:

```powershell
Get-Content -LiteralPath Shared\Schemas\Command.fbs -Encoding UTF8
Select-String -LiteralPath Shared\GameSim\Systems\CommandExecutor\CommandExecutor.cpp -Pattern "ExecuteCommand|HandleMove|HandleCastSkill|HandleBasicAttack|BuildServerCommand"
```

### 6.2 여기서 볼 것

`Command.fbs`에서 볼 것:

```text
CommandKind
Move
CastSkill
BasicAttack
CommandPacket
CommandBatch
```

이 파일은 "클라이언트가 서버에게 무엇을 요청할 수 있는가"를 정의한다.

`CommandExecutor.cpp`에서 볼 것:

```text
CDefaultCommandExecutor::ExecuteCommand
HandleMove
HandleCastSkill
HandleBasicAttack
BuildServerCommand
```

여기서 중요한 본질:

```text
Command는 요청이다.
결과가 아니다.
```

예:

```text
Move command
  "나 저기로 가고 싶다."

Server GameSim
  "갈 수 있는지 검증한다."
  "길을 계산한다."
  "최종 위치/방향/상태를 바꾼다."

Snapshot
  "현재 진실은 이렇다."
```

### 6.3 절대 헷갈리면 안 되는 것

```text
Client click position != final position truth
Client skill button != damage truth
Client animation != action truth
```

Winters의 올바른 방향:

```text
Client는 의도를 보낸다.
Server/GameSim은 결과를 만든다.
Client는 결과를 보여준다.
```

## 7. 실습 D - Winters에서 System 보기

### 7.1 먼저 볼 파일

```text
C:\Users\user\Desktop\Winters\Shared\GameSim\Systems\Move\MoveSystem.h
C:\Users\user\Desktop\Winters\Shared\GameSim\Systems\Move\MoveSystem.cpp
C:\Users\user\Desktop\Winters\Shared\GameSim\Systems\Damage\DamagePipeline.h
C:\Users\user\Desktop\Winters\Shared\GameSim\Systems\Damage\DamagePipeline.cpp
```

빠르게 찾기:

```powershell
Select-String -LiteralPath Shared\GameSim\Systems\Move\MoveSystem.h -Pattern "Execute"
Select-String -LiteralPath Shared\GameSim\Systems\Move\MoveSystem.cpp -Pattern "Execute|Transform|MoveTarget|Velocity|TickContext"
Select-String -LiteralPath Shared\GameSim\Systems\Damage\DamagePipeline.cpp -Pattern "Apply|Damage|Health"
```

### 7.2 여기서 볼 것

System은 "상태를 바꾸는 함수 묶음"이다.

MoveSystem의 본질:

```text
현재 위치
이동 목표
속도
delta/tick
길/충돌/상태
  -> 다음 위치
```

DamagePipeline의 본질:

```text
공격/스킬 요청
방어력/계수/버프
  -> 최종 피해
  -> HP 변화
  -> 사망/이벤트
```

여기서 엔진식 사고:

```text
System은 화면을 그리려고 존재하지 않는다.
System은 truth state를 바꾸려고 존재한다.
```

## 8. 실습 E - Snapshot과 Event 보기

### 8.1 먼저 볼 파일

```text
C:\Users\user\Desktop\Winters\Shared\Schemas\Snapshot.fbs
C:\Users\user\Desktop\Winters\Shared\Schemas\Event.fbs
C:\Users\user\Desktop\Winters\Server\Public\Game\SnapshotBuilder.h
C:\Users\user\Desktop\Winters\Server\Private\Game\SnapshotBuilder.cpp
```

빠르게 찾기:

```powershell
Get-Content -LiteralPath Shared\Schemas\Snapshot.fbs -Encoding UTF8
Get-Content -LiteralPath Shared\Schemas\Event.fbs -Encoding UTF8
Select-String -LiteralPath Server\Private\Game\SnapshotBuilder.cpp -Pattern "EntitySnapshot|Snapshot|hp|mana|pos|yaw|stateFlags"
```

### 8.2 여기서 볼 것

`Snapshot.fbs`에서 볼 것:

```text
EntitySnapshot
Snapshot
posX / posY / posZ
yaw
hp
mana
stateFlags
skillCooldowns
```

Snapshot의 본질:

```text
서버가 현재 월드 진실을 클라이언트가 이해할 수 있는 형태로 압축한 것.
```

`Event.fbs`에서 볼 것:

```text
DamageEvent
ProjectileSpawnEvent
ProjectileHitEvent
ActionStartEvent
EffectTriggerEvent
EventPacket
```

Event의 본질:

```text
상태 그 자체보다는 "일어난 일"을 전달한다.
```

비교:

```text
Snapshot:
  지금 HP는 420이다.
  지금 위치는 x=10 z=20이다.

Event:
  방금 피해 80이 들어갔다.
  방금 스킬 액션이 시작됐다.
  방금 이펙트를 재생해야 한다.
```

## 9. 실습 F - Client가 Snapshot을 먹는 곳 보기

### 9.1 먼저 볼 파일

```text
C:\Users\user\Desktop\Winters\Client\Private\Scene\Scene_InGameNetwork.cpp
C:\Users\user\Desktop\Winters\Client\Public\Network\Client\SnapshotApplier.h
C:\Users\user\Desktop\Winters\Client\Private\Network\Client\SnapshotApplier.cpp
```

빠르게 찾기:

```powershell
Select-String -LiteralPath Client\Private\Scene\Scene_InGameNetwork.cpp -Pattern "OnSnapshot|SnapshotApplier|OnAuthoritativeSnapshot|EventApplier"
Select-String -LiteralPath Client\Private\Network\Client\SnapshotApplier.cpp -Pattern "OnSnapshot|Apply|EntitySnapshot|hp|mana|posX|yaw"
```

### 9.2 여기서 볼 것

이 파일들은 "클라이언트가 서버 진실을 받아서 자기 월드/표현에 적용하는 구간"이다.

여기서 핵심 질문:

```text
Client가 새 truth를 만드는가?
Server truth를 받아서 보여주는가?
```

정답:

```text
받아서 보여준다.
단, 입력 반응성을 위해 약한 예측과 보간은 할 수 있다.
```

Decoy:

```text
클라이언트에서 위치 보간을 하니까 클라이언트가 이동 truth를 소유한다.
```

Correct:

```text
보간은 표현이다.
최종 위치 truth는 서버 snapshot이다.
```

## 10. 실습 G - Client Visual 보기

### 10.1 먼저 볼 파일

```text
C:\Users\user\Desktop\Winters\Client\Private\Scene\Scene_InGameRender.cpp
C:\Users\user\Desktop\Winters\Client\Private\Scene\Scene_InGameNetwork.cpp
C:\Users\user\Desktop\Winters\Client\Private\Manager\Minion_Manager.cpp
C:\Users\user\Desktop\Winters\Client\Private\Manager\Structure_Manager.cpp
```

빠르게 찾기:

```powershell
Select-String -LiteralPath Client\Private\Scene\Scene_InGameRender.cpp -Pattern "RenderWorldSnapshot|AppendRenderSnapshotMeshes|Snapshot|Champion|Minion|Structure"
Select-String -LiteralPath Client\Private\Manager\Minion_Manager.cpp -Pattern "AppendRenderSnapshotMeshes|FaceMoveDirection|Snapshot|stateFlags"
```

### 10.2 여기서 볼 것

Render 쪽에서 중요한 본질:

```text
GameSim truth를 직접 만들지 않는다.
화면에 그릴 후보를 만든다.
```

Winters에서 렌더링 최적화 문서를 읽을 때도 이 기준이 중요하다.

```text
RenderWorldSnapshot
  = GPU에 보낼 시각화 후보 묶음

GameSim Snapshot
  = 서버가 만든 gameplay truth 묶음
```

이 둘을 이름이 비슷하다고 섞으면 안 된다.

## 11. 실습 H - Unreal과 Winters를 같은 칸에 놓기

이제 매핑한다.

| 질문 | Unreal | Winters |
|---|---|---|
| 실행 세계는? | UWorld | Shared/GameSim CWorld, Client scene world |
| 게임 객체는? | AActor/APawn/ACharacter | Entity + Components |
| 상태 조각은? | UActorComponent, UPROPERTY | Component structs |
| 명령은? | Server RPC, input action | Command.fbs, GameCommand |
| 서버 권위는? | GameMode/server Actor state | Server + Shared/GameSim |
| 복제는? | Actor replication | Snapshot.fbs/Event.fbs |
| 표현은? | client replicated Actor, Niagara, UMG | Client visual, FX, UI, render snapshot |
| 데이터는? | DataAsset/DataTable | Definitions/cooked packs |

여기까지 오면 중요한 감각이 생긴다.

```text
Unreal 이름을 몰라도 Winters 칸으로 번역할 수 있다.
Winters 구조가 막혀도 Unreal 칸으로 비유할 수 있다.
Unity도 같은 표에 넣을 수 있다.
```

## 12. 실습 I - Unity를 같은 칸에 놓기

Unity는 지금 실제 프로젝트 소스가 아니라 설치 구조만 확인한 상태다.

그래도 개념 매핑은 가능하다.

| 질문 | Unity |
|---|---|
| 실행 세계는? | Scene |
| 게임 객체는? | GameObject |
| 상태/행동 조각은? | Component / MonoBehaviour |
| 데이터는? | ScriptableObject |
| 에셋 정체성은? | .meta GUID |
| 패키지는? | Packages/manifest.json, package.json |
| 에디터 도구는? | EditorWindow, CustomEditor |
| 서버 권위는? | 기본 제공으로 강제되지 않음. 직접 설계 필요 |

Unity에서 LoL 모작을 만들 때 가장 중요한 말:

```text
MonoBehaviour Update가 server truth가 되면 안 된다.
```

Unity client는 이렇게 생각한다.

```text
Input -> Command Message -> Server -> Snapshot/Event -> Apply to GameObject Visual
```

## 13. 오늘의 30분 루트

시간이 많지 않으면 아래만 한다.

```text
00-05분:
  이 문서 0~2번 읽기

05-10분:
  Unreal Actor.h, World.h에서 AActor/UWorld 선언만 찾기

10-15분:
  Winters World.h, Entity.h, TransformComponent.h 보기

15-20분:
  Command.fbs, Snapshot.fbs 보기

20-25분:
  CommandExecutor.cpp에서 ExecuteCommand switch 보기

25-30분:
  아래 5문장 직접 쓰기
```

직접 쓸 5문장:

```text
1. Unreal에서 AActor는 무엇이다.
2. Winters에서 Entity는 무엇이다.
3. Command는 결과가 아니라 무엇이다.
4. Snapshot은 무엇이다.
5. Client Visual은 truth가 아니라 무엇이다.
```

예상 답:

```text
1. Unreal에서 AActor는 UWorld 안에 배치되거나 spawn되는 게임 객체다.
2. Winters에서 Entity는 GameSim World 안의 시뮬레이션 대상 ID다.
3. Command는 결과가 아니라 클라이언트/AI가 서버에 보내는 의도다.
4. Snapshot은 서버가 만든 현재 gameplay truth의 전송 형태다.
5. Client Visual은 truth가 아니라 서버 상태를 보여주는 표현이다.
```

## 14. 오늘의 90분 루트

조금 더 할 수 있으면 아래 순서다.

```text
1. Unreal UObject -> Actor -> World 보기
2. Winters World -> Entity -> Component 보기
3. Command.fbs -> CommandExecutor.cpp 보기
4. MoveSystem.cpp 보기
5. Snapshot.fbs -> SnapshotBuilder.cpp 보기
6. SnapshotApplier.cpp 보기
7. Scene_InGameRender.cpp 보기
8. 표를 직접 다시 작성하기
```

각 단계에서 한 줄씩만 남긴다.

```text
이 파일은 무엇을 소유/변환/전송/표현하는가?
```

## 15. 막힐 때 보는 해석법

### 15.1 코드가 너무 길 때

긴 파일을 읽을 때는 함수 전체를 이해하려 하지 않는다.

먼저 이것만 찾는다.

```text
입력 파라미터
읽는 component
쓰는 component
호출하는 다음 함수
return 또는 event
```

예:

```text
HandleMove(CWorld& world, const TickContext& tc, const GameCommand& cmd)
```

이 시그니처만 봐도 꽤 많은 것이 보인다.

```text
CWorld
  바꿀 대상 world

TickContext
  현재 simulation tick

GameCommand
  외부에서 들어온 의도
```

즉 본질은:

```text
현재 tick에서 command를 world state로 반영한다.
```

### 15.2 모르는 타입이 나올 때

모르는 타입이 나오면 바로 전체 구현을 파지 않는다.

먼저 이름을 분류한다.

```text
Component인가?
System인가?
Definition인가?
Schema인가?
Visual인가?
Manager인가?
```

예:

```text
MoveTargetComponent
  Component다. 이동 목표 상태 조각이다.

MoveSystem
  System이다. 이동 목표와 현재 위치를 읽어 다음 위치를 만든다.

SnapshotBuilder
  Schema output을 만든다. server world를 network payload로 바꾼다.

SnapshotApplier
  network payload를 client world/visual state에 적용한다.
```

## 16. 다음 세션 예고

Session 02에서는 하나를 더 깊게 본다.

추천 주제:

```text
right-click 이동 하나가
Client input
CommandSerializer
Server CommandIngress
CommandExecutor::HandleMove
MoveSystem
SnapshotBuilder
SnapshotApplier
Client interpolation/render
까지 어떻게 이동하는가
```

이것 하나를 끝까지 추적하면 엔진 감각이 확 열린다.

## 17. 완료 기준

오늘 완료 기준은 낮다.

아래만 말할 수 있으면 성공이다.

```text
게임 엔진은 화면 그리기 이전에
World 안의 Object/Entity 상태를
입력과 시간에 따라 바꾸고
그 결과를 저장/전송/표현하는 시스템이다.
```

그리고 Winters 기준으로는 이렇게 말할 수 있으면 된다.

```text
Client는 의도를 보내고,
Server/GameSim은 진실을 만들고,
Snapshot/Event는 진실과 사건을 전달하고,
Client Visual은 그것을 보여준다.
```

## 18. Verification / Handoff

이 문서는 코드 변경이 아니라 학습 실습 시트다.

확인한 로컬 기준:

```text
C:\Users\user\Desktop\Winters\Shared\GameSim\Core\World\World.h
C:\Users\user\Desktop\UnrealEngine\UnrealEngine\Engine\Source\Runtime\Engine\Classes\GameFramework\Actor.h
C:\Program Files\Unity\Hub\Editor\6000.0.79f1\Editor\Data
```

다음에 이어갈 때 열 문서:

```text
C:\Users\user\Desktop\Winters\.md\plan\2026-07-10_ENGINE_SOURCE_DNA_UNREAL_UNITY_WINTERS_LOL_GUIDE.md
C:\Users\user\Desktop\Winters\.md\plan\2026-07-10_ENGINE_DNA_HANDS_ON_SESSION_01.md
```

다음 세션 추천 파일:

```text
C:\Users\user\Desktop\Winters\Client\Private\Network\Client\CommandSerializer.cpp
C:\Users\user\Desktop\Winters\Server\Private\Game\CommandIngress.cpp
C:\Users\user\Desktop\Winters\Shared\GameSim\Systems\CommandExecutor\CommandExecutor.cpp
C:\Users\user\Desktop\Winters\Shared\GameSim\Systems\Move\MoveSystem.cpp
C:\Users\user\Desktop\Winters\Server\Private\Game\SnapshotBuilder.cpp
C:\Users\user\Desktop\Winters\Client\Private\Network\Client\SnapshotApplier.cpp
```
