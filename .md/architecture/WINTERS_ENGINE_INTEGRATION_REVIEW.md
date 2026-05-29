# Winters Engine 통합 구조 리뷰

작성일: 2026-05-17
범위: Engine, Client, Server, Shared, Services, Tools, build/deploy 연결 구조

이 문서는 Winters를 장기적으로 확장 가능한 엔진/게임/서비스 구조로 정리하기 위한 세션별 감사 로그다. 코드가 항상 문서보다 우선하며, 이 문서는 실제 저장소 구조와 빌드 파일에서 확인한 사실을 기준으로 한다.

## 고정 원칙

1. Gameplay truth는 Server와 Shared/GameSim이 소유한다.
2. Client는 입력, 약한 예측, 보간, 렌더링, UI, FX, 디버그를 담당한다.
3. Engine은 가능하면 게임 독립 런타임이어야 한다.
4. Shared는 Client/Server 양쪽에서 쓰이는 계약과 deterministic gameplay core를 담는다.
5. Services는 계정, 상점, 결제, 프로필, 매치메이킹 같은 비실시간 백엔드를 담당하고, 실시간 판정 서버가 되어서는 안 된다.
6. Tools와 CI/CD는 반복 가능한 빌드, 코드 생성, asset cook, 검증을 자동화해야 한다.

## Session 1 - 전체 구조와 연결

### 현재 큰 구조

```text
Winters/
  Engine/      - 런타임 DLL, RHI, ECS, 리소스, UI, 사운드, 툴 일부
  EngineSDK/   - Engine public headers/lib 복사본, Client/Server 소비
  Client/      - WintersGame.exe, scenes, champion visuals, network client, backend HTTP clients
  Server/      - WintersServer.exe, IOCP, GameRoom, authoritative tick loop
  Shared/      - GameSim, schemas, replay/network shared contracts
  Services/    - Go backend services
  Tools/       - asset converter, DX12 smoke host
  Shaders/     - runtime shader assets
```

현재 메인 빌드는 `Winters.sln`과 Visual Studio `vcxproj` 중심이다. CMake/Ninja는 메인 스파인이 아니며, 추후 도입한다면 별도의 build spine으로 설계해야 한다.

### 솔루션 프로젝트

```text
Engine              Engine/Include/Engine.vcxproj
Server              Server/Include/Server.vcxproj
Client              Client/Include/Client.vcxproj
WintersAssetConverter Tools/WintersAssetConverter/WintersAssetConverter.vcxproj
DX12SmokeHost       Tools/DX12SmokeHost/DX12SmokeHost.vcxproj
```

확인된 참조:

- `Server.vcxproj`는 `Engine.vcxproj`를 ProjectReference로 가진다.
- `DX12SmokeHost.vcxproj`는 `Engine.vcxproj`를 ProjectReference로 가진다.
- `Client`는 솔루션 dependency로 Engine을 먼저 빌드하게 되어 있지만, 실제 연결은 `EngineSDK/lib`와 `EngineSDK/inc`를 통해 이루어진다.
- `Client`는 PreBuild에서 `UpdateLib.bat`를 호출한다.
- `Engine`은 PostBuild에서 `UpdateLib.bat`를 호출한다.

### 현재 런타임 흐름

Client 실행:

```text
Client/main.cpp
  -> CGameApp
  -> WintersRun(app, EngineConfig)
  -> CGameInstance::Initialize_Engine
  -> CEngineApp::Initialize
  -> CGameApp::OnInit
  -> SceneManager loop
```

Server 실행:

```text
Server/main.cpp
  -> CGameRoom::Create
  -> CPacketDispatcher::RegisterRoom
  -> CGameRoom::Start
  -> CIOCPCore::Start
  -> GameRoom tick thread
```

GameRoom tick 핵심:

```text
Tick
  -> Phase_DrainCommands
  -> Phase_ServerBotAI
  -> Phase_ExecuteCommands
  -> Phase_SimulationSystems
       -> StatusEffectSystem
       -> champion GameSim Tick
       -> Minion/Turret/Projectile/Death
  -> Phase_BroadcastEvents
  -> Phase_BroadcastSnapshot
```

이 흐름은 서버 권위 원칙과 맞다. 특히 `Client Input -> Command -> Server GameSim -> Snapshot/Event -> Client Visual` 방향이 코드에도 나타난다.

### 현재 좋은 점

1. `Shared/GameSim`이 물리적으로 분리되어 있고, Server가 그 코드를 직접 컴파일한다.
2. Client도 `Shared/GameSim`을 참조하지만, 네트워크 권위 모드에서는 Snapshot/Event 적용 쪽으로 점점 이동 중이다.
3. FlatBuffers schema가 `Shared/Schemas`에 있어 command/event/snapshot 계약 위치가 명확하다.
4. `UpdateLib.bat`가 Engine 공개 API와 런타임 DLL 배포를 한 곳에서 처리한다.
5. Services가 Go 서비스로 분리되어 있어 실시간 게임 서버와 계정/상점/매치메이킹 서버가 섞이지 않는다.
6. Server `GameRoom`은 tick phase가 명시적으로 나뉘어 있어 이후 프로파일링/검증 포인트를 세우기 쉽다.

### 구조 리스크

#### 1. Engine이 게임 규칙을 직접 알고 있다

확인된 예:

```text
Engine/Private/Manager/UI/UI_Manager.cpp
  -> Shared/GameSim/Components/GoldComponent.h
  -> Shared/GameSim/Components/InventoryComponent.h
  -> Shared/GameSim/Components/SkillRankComponent.h
  -> Shared/GameSim/Components/StatComponent.h
  -> Shared/GameSim/Definitions/ItemDef.h
```

이 구조는 당장은 HUD 구현이 빠르지만, 장기적으로 Engine이 MOBA GameSim 데이터 구조에 묶인다. Engine을 다른 게임이나 툴에서 재사용하려면 UI runtime과 game-specific HUD adapter를 분리해야 한다.

권장 방향:

```text
Engine UI runtime
  - text, rect, sprite, atlas, font, Lua VM, input focus

Client/Game UI adapter
  - Gold/Inventory/SkillRank/StatComponent 읽기
  - ChampionHUDState 생성
  - Engine UI에 순수 표시 데이터 전달
```

#### 2. EngineSDK 복사 방식이 공개 API 경계를 흐릴 수 있다

`UpdateLib.bat`는 `Engine/Include/*.h`와 `Engine/Public` 전체를 `EngineSDK/inc`로 복사한 뒤 `_Manager.h`를 삭제한다. 이 방식은 단순하지만, 공개되어야 하는 헤더와 내부용 헤더가 `Public` 아래에서 섞이면 SDK 경계가 계속 흐려질 수 있다.

권장 방향:

```text
Engine/Include      - SDK에 노출되는 stable API
Engine/Public       - Engine 내부 모듈 간 public header
Engine/Private      - 구현
EngineSDK/inc       - Engine/Include 중심으로 최소화
```

단기적으로는 전체 이동보다 SDK export manifest를 두는 편이 안전하다.

#### 3. Client가 Engine에 의존하는 방식이 이중적이다

Client는 솔루션 dependency로 Engine을 먼저 빌드하고, 동시에 `UpdateLib.bat`로 복사된 `EngineSDK`를 include/lib 경로로 쓴다. 동작은 하지만 소스 경계와 배포 경계가 섞여 있다.

장기 방향은 둘 중 하나로 명확히 해야 한다.

```text
Option A: SDK 소비 모델
  Client/Server는 EngineSDK만 본다.
  Engine source include 금지.

Option B: monorepo source model
  Client/Server는 Engine project reference와 source include를 명시적으로 쓴다.
  SDK는 external sample/demo용 산출물로만 둔다.
```

현재 Winters에는 Option A가 더 어울린다. 이유는 Engine DLL 경계를 이미 세우고 있고, Client가 `WintersRun`, `CGameInstance`, SDK lib를 통해 붙기 때문이다.

#### 4. GameRoom이 너무 많은 책임을 가진다

`CGameRoom`은 lobby, session mapping, command queue, server world, minion wave, stage loading, projectile, death/respawn, snapshot/event broadcast, replay까지 갖고 있다. 서버 권위의 중심점으로는 맞지만, 150 챔피언과 더 큰 운영 구조로 가면 `GameRoom`은 orchestration만 하고 실제 시스템은 더 잘게 나뉘어야 한다.

권장 방향:

```text
GameRoom
  - tick ownership
  - phase order
  - session to entity mapping
  - world lifetime

GameSim systems
  - status, movement, combat, projectile, death, respawn, minion, turret

Network replication
  - event serializer
  - snapshot builder
  - AOI/delta/reliability

Match/lobby
  - lobby slots, ready, bot edit policy, game start
```

#### 5. Backend endpoint가 Client 코드에 하드코딩되어 있다

확인된 예:

```text
AUTH_SERVICE_URL    = http://127.0.0.1:8081
MATCH_SERVICE_URL   = http://127.0.0.1:8083
PROFILE_SERVICE_URL = http://127.0.0.1:8084
SHOP_SERVICE_URL    = http://127.0.0.1:8086
Game server         = 127.0.0.1:9000
```

개발 단계에서는 괜찮지만, 런처/환경/배포를 생각하면 config file 또는 build profile로 이동해야 한다.

#### 6. 현재 문서와 코드 사이에 인코딩/활성 문서 문제가 있다

일부 `.md/architecture` 문서는 내용이 깨져 보이며, 실제 활성 아키텍처 문서는 제한적이다. 반면 `.md/plan/*`에는 큰 계획이 많이 흩어져 있다. 앞으로는 “긴 계획 문서”보다 현재 코드 기준의 짧은 활성 문서를 유지해야 한다.

## 목표 의존성 방향

권장 목표:

```text
Services
  <- Client HTTP clients

Engine
  <- Client
  <- Server
  <- Tools

Shared
  <- Client
  <- Server
  <- Services schema consumers

Shared/GameSim
  -> Engine ECS/types only
  -> no Client include
  -> no Server include

Server
  -> Shared/GameSim
  -> Shared/Schemas
  -> EngineSDK or Engine public API

Client
  -> EngineSDK
  -> Shared/Schemas
  -> Shared/GameSim for prediction/query/visual mapping only
```

금지하고 싶은 방향:

```text
Engine -> Client
Engine -> Server
Engine -> game-specific Shared/GameSim rules
Shared/GameSim -> Client
Shared/GameSim -> Server
Services -> Client/Server source code
```

현재 `Shared/GameSim -> Client/Server` 방향은 발견되지 않았고 좋다. 현재 가장 큰 위반 후보는 `Engine -> Shared/GameSim`이다.

## Session 1 결론

Winters의 큰 뼈대는 맞다. 특히 Server authoritative GameSim, Shared schema, Go backend services, Tools 분리는 좋은 방향이다. 다만 “엔진 독립성”과 “게임 전용 구현”이 아직 같은 DLL 내부에서 섞여 있다.

가장 먼저 정리할 순서는 다음이 좋다.

1. Engine UI에서 MOBA GameSim 직접 include 제거
2. SDK export 경계 명시
3. GameRoom 책임 분해 기준 문서화
4. Client backend/game server endpoint config화
5. CMake/Ninja 도입 전 현재 vcxproj 빌드 그래프를 manifest화

## 다음 세션 진입점

Session 2는 `Shared/GameSim`과 `Server/GameRoom`을 기준으로 본다.

확인할 질문:

1. StatusEffect, Buff, SkillState, Projectile, DamagePipeline의 책임이 겹치지 않는가?
2. 챔피언별 GameSim 파일이 늘어날 때 CommandExecutor와 GameRoom이 계속 비대해지는가?
3. Client prediction과 Server truth의 경계가 코드에서 강제되는가?
4. 상태 이상, 은신, 지정 불가, 스턴, 슬로우, 변신, 그림자, 카운터 같은 능력을 primitive 조합으로 받을 수 있는가?
5. Snapshot/Event가 gameplay state와 visual cue를 충분히 분리하는가?

## Session 2 - Server GameSim, StatusControl, Skill Primitive

### 현재 상태

StatusControl의 방향은 맞다. 이미 `StatusEffectComponent`, `StatusEffectApplyDesc`, `GameplayStateComponent`, `GameplayStateQuery`가 있고, `StatusEffectSystem`이 매 tick 효과 시간을 줄인 뒤 `GameplayStateComponent`를 다시 빌드한다.

중요한 분리도 시작되어 있다.

```text
Buff          - 공격력, 방어력, 이동속도 배율 같은 숫자/스탯 변화
StatusEffect  - 스턴, 둔화, 무장해제, 은신, 지정 불가 같은 규칙 상태
GameplayState - 여러 StatusEffect를 합친 현재 판정용 bitset
SnapshotState - 클라이언트 표시/애니메이션/렌더에 필요한 압축 state flag
Event/Cue     - 스킬 시전, 이펙트, 투사체 생성/피격 같은 1회성 재생 신호
```

이 구분은 150명 챔피언을 받치기 위한 기본 방향으로 적절하다. 라이엇 내부 구현은 공개적으로 확정할 수 없지만, 대규모 챔피언 게임은 보통 “챔피언별 코드가 직접 서로를 검사하는 방식”보다 “공통 판정 primitive와 데이터/스크립트/훅 조합”으로 간다. Winters도 이 방향으로 가야 한다.

### 좋은 기반

1. `StatusEffectSystem`이 generic status container를 가지고 있다.
2. `GameplayStateQuery`가 `CanMove`, `CanAttack`, `CanCast`, `CanBeSeenBy`, `CanBeTargetedBy`, `CanReceiveProjectileHit`를 제공한다.
3. `BuffSystem`은 numeric modifier 중심이라 StatusEffect와 역할이 다르다.
4. `DamagePipeline`은 피해 계산의 중앙 통로가 되어 있다.
5. `ReplicatedEventComponent`는 SkillCast, EffectTrigger, ProjectileSpawn, ProjectileHit, Damage를 이미 하나의 event primitive로 묶고 있다.
6. `SkillProjectileComponent`는 서버 소유 투사체의 출발점으로 쓸 수 있다.

### 핵심 위험

#### 1. Query가 아직 판정 소비처에 충분히 연결되어 있지 않다

검색 기준으로 `GameplayStateQuery`는 존재하지만 `CommandExecutor`, `MoveSystem`, `AttackChaseSystem`, `Projectile`, `BotLaneAI`의 주요 판정에서 아직 충분히 소비되지 않는다.

필수 연결 방향:

```text
Move command / MoveSystem        -> CanMove
BasicAttack / AttackChase        -> CanAttack, CanBeTargetedBy
CastSkill                        -> CanCast, CanBeTargetedBy
Projectile hit                   -> CanReceiveProjectileHit
Bot target scan                  -> CanBeSeenBy, CanBeTargetedBy
SnapshotBuilder                  -> GameplayState -> SnapshotState 변환
Client visual                    -> SnapshotState/Event만 소비
```

지금 구조에서 StatusEffect를 붙이기만 하면 “컴포넌트에는 상태가 있는데 실제 룰은 막히지 않는” 버그가 날 수 있다. 따라서 다음 구현 1순위는 챔피언 스킬 추가가 아니라 Query 소비처 연결이다.

#### 2. Viego E가 공통 은신이 아니라 Viego 전용 상태로 snapshot에 직접 연결되어 있다

현재 `ViegoGameSim`은 E에서 `ViegoSimComponent.bMistActive`를 켜고, `SnapshotBuilder`가 이 컴포넌트를 직접 읽어서 `kSnapshotStateInvisibleFlag`를 세운다.

이 방식은 Viego 하나만 볼 때는 빠르지만, Senna 같은 다중 은신, Akali 장막, Twitch 은신, camouflage/true sight/ally visibility 같은 변형으로 확장하면 금방 무너진다.

목표 구조:

```text
Viego E
  -> Area/Aura primitive 생성
  -> 범위 안 caster에게 StatusEffectApplyDesc{ ViegoMist, Invisible }
  -> StatusEffectSystem이 GameplayStateInvisibleFlag 빌드
  -> AI/target/projectile/command가 GameplayStateQuery로 판정
  -> SnapshotBuilder가 GameplayState를 SnapshotState로 변환
  -> Client는 snapshot/event로 투명도, mist FX, screen haze 재생
```

`ViegoSimComponent`는 Viego 고유 타이머와 스킬 문맥만 보관하고, 보편 판정인 은신 자체는 StatusControl에 맡겨야 한다.

#### 3. CommandExecutor가 챔피언 지식 허브가 되어가고 있다

`CommandExecutor` 안에 primary hook id, 서버 투사체 여부, 서버 소유 스킬 보정, 즉시 스킬 처리, 기본 공격 특수 처리 등이 챔피언별 branch로 늘고 있다. 이 파일이 계속 커지면 150명 챔피언에서 가장 먼저 깨지는 곳이 된다.

목표 역할:

```text
CommandExecutor
  - command 유효성 검사
  - cooldown/stage/animation/event 공통 처리
  - GameplayStateQuery gate
  - ChampionSkillRegistry 호출

ChampionSkillModule
  - champion/slot별 execute 함수
  - projectile/area/status/damage/dash/summon primitive 조합
  - champion-local state tick
```

단기적으로는 모든 것을 한 번에 갈아엎지 말고, Viego/Ashe를 넣으면서 새 primitive를 붙이고 기존 branch를 줄이는 식으로 이동한다.

#### 4. Projectile에 OnHitEffect가 없다

Ashe W 둔화, Ashe R 스턴, Ezreal Q on-hit, Yasuo tornado airborne 같은 효과는 projectile이 맞은 뒤 공통적으로 후속 효과를 적용해야 한다. 현재는 `SkillProjectileComponent`가 damage 중심이고, `GameRoom::Phase_ServerProjectiles`에서 Yasuo tornado만 특수 처리한다.

목표 구조:

```text
SkillProjectileComponent
  - source/team/kind/skill/rank/position/motion
  - damage desc
  - onHit status desc 목록
  - onHit event/cue id
  - pierce/bounce/destroy policy
```

이렇게 되면 Ashe W는 `OnHit: slow`, Ashe R은 `OnHit: stun`, 이후 Varus/Sejuani/Brand 같은 효과도 같은 통로를 탄다.

#### 5. Area/Aura primitive가 필요하다

Viego E, Senna E, 장판형 도트, 오라 버프, zone reveal은 “한 번 맞고 끝나는 투사체”가 아니다. 일정 시간 동안 영역을 유지하고, 들어오고/나가는 entity에 상태를 부여하거나 제거해야 한다.

목표 구조:

```text
AreaComponent
  - owner/team/shape/duration
  - affects allies/enemies/self
  - enter/inside/exit effect policy
  - vision/reveal/mist/screen cue metadata

AreaSystem
  - deterministic entity scan
  - status/buff/damage application
  - replicated event/cue emission
```

Viego E는 이 primitive 검증용으로 가장 좋다. 장판, caster 은신, screen haze, enemy AI 감지 불가가 모두 들어 있기 때문이다.

#### 6. Bot AI가 visibility/targetability를 아직 모른다

`BotLaneAISystem`은 `FindEnemyChampion`, `EmitBasicAttackCommand`, `EmitSkillCommand` 흐름에서 HP/거리/팀 중심으로 판단한다. 은신/지정 불가/스턴/무장해제가 들어오면 AI도 반드시 같은 Query를 통과해야 한다.

원칙:

```text
AI는 직접 상태 컴포넌트를 해석하지 않는다.
AI는 GameplayStateQuery만 묻는다.
AI가 만든 command도 CommandExecutor에서 다시 검증된다.
```

이 이중 방어가 서버 권위 구조의 안전장치다.

### 다음 구현 순서

1. StatusControl Query 소비처 연결
2. SnapshotBuilder를 Viego 전용 은신이 아니라 GameplayState 기반으로 변경
3. Viego E를 StatusEffect + Area/Aura 기반으로 재구현
4. Projectile OnHitEffect 추가
5. Ashe W/R을 OnHitEffect로 검증
6. ChampionSkillRegistry/Module로 CommandExecutor branch를 단계적으로 축소
7. Viego Passive와 Sylas는 Spellbook/FormOverride primitive가 들어온 뒤 연결

### Session 2 결론

현재 뼈대는 방향이 맞다. 하지만 “데이터 구조가 있다”와 “게임 규칙이 그 구조를 항상 통과한다”는 다르다. Winters가 150명 챔피언을 목표로 한다면 다음 코딩 세션은 비에고 스킬을 바로 예쁘게 만드는 것보다, StatusControl Query를 명령/이동/공격/투사체/AI/snapshot에 먼저 연결하는 것이 맞다. 그 다음 Viego E와 Ashe W/R을 검증 케이스로 삼으면 구조와 기능을 동시에 잡을 수 있다.

## Session 3 - Backend Services, Matchmaking, Security Boundary

### 현재 상태

`Services`는 Go 기반 마이크로서비스 묶음이다.

```text
auth          - 회원가입, 로그인, refresh token
matchmaking   - Redis queue, MMR 기반 매칭, Kafka match event
profile       - 프로필/전적 조회, match event 소비
leaderboard   - Redis leaderboard, match event 소비
payment       - 결제 검증 gateway, coin 지급, Kafka payment event
shop          - 상점/구매/인벤토리
```

기술 스택은 현재 목표에 잘 맞는다.

```text
PostgreSQL - 영속 계정/전적/결제/인벤토리
Redis      - matchmaking queue, refresh token, leaderboard cache
Kafka      - match/payment/player event fanout
chi        - lightweight HTTP service
JWT        - client auth token
bcrypt     - password hashing
```

`go test ./...` 기준으로 전체 패키지 컴파일은 통과했다. 단, 현재 테스트 파일은 없다.

### 좋은 기반

1. 실시간 전투 서버와 비실시간 서비스가 분리되어 있다.
2. Auth/Profile/Shop/Payment/Matchmaking/Leaderboard가 별도 프로세스로 나뉘어 책임이 명확하다.
3. Refresh token을 Redis에 저장하고 refresh 시 기존 token을 삭제하는 구조라 최소한의 revoke 경로가 있다.
4. 결제는 `PaymentGateway` 인터페이스를 두고 mock gateway를 등록하는 구조라 실제 PG 연동으로 교체하기 쉽다.
5. Match/Profile/Leaderboard는 Kafka event를 통해 느슨하게 연결된다.

### 핵심 위험

#### 1. 운영 secret/config 경계가 아직 개발용이다

`.env.example`에는 개발용 DB 비밀번호와 JWT secret fallback이 들어 있고, `pkg/config`도 같은 fallback을 가진다. 실제 운영에서는 fallback secret으로 프로세스가 뜨면 안 된다.

권장 원칙:

```text
local/dev
  - .env.example 허용
  - docker-compose 개발 secret 허용

staging/prod
  - JWT secret, DB password, Redis password 필수
  - fallback secret 금지
  - sslmode/host/auth mode 환경별 분리
```

현재 `.env`는 git tracked가 아니고 `.env.example`만 tracked라 기본 정책은 괜찮다. 다만 `docker-compose.yml`과 `Makefile`에는 개발 비밀번호가 직접 들어가 있으므로 production compose와는 분리해야 한다.

#### 2. Client가 서비스 endpoint를 직접 하드코딩한다

Client에는 다음 endpoint가 직접 박혀 있다.

```text
auth    http://127.0.0.1:8081
match   http://127.0.0.1:8083
profile http://127.0.0.1:8084
shop    http://127.0.0.1:8086
game    127.0.0.1:9000
```

개발 단계에서는 빠르지만, 운영 구조에서는 bootstrap/config/discovery가 필요하다.

목표 구조:

```text
Client
  -> bootstrap config endpoint 또는 local config
  -> auth/match/profile/shop base URL 획득
  -> matchmaking status에서 game server endpoint/ticket 획득
  -> game server 접속 시 match ticket 검증
```

즉, Client가 항상 `127.0.0.1:9000`으로 붙는 구조는 로컬 smoke에는 맞지만 실제 매치 운영 구조는 아니다.

#### 3. Matchmaking과 GameServer 할당 계약이 아직 없다

현재 matchmaking은 `matched:{matchID}` 상태와 Kafka `MatchCreated` event를 만들지만, 실제 game server instance, room id, connect token, player slot 계약은 보이지 않는다.

목표 계약:

```text
Matchmaking
  -> MatchCreated(matchId, players, mode, mmr)

GameServer Allocator / Director
  -> game server process/room 할당
  -> signed match ticket 발급

Client
  -> /matchmaking/status
  <- matched + endpoint + ticket + expiresAt

GameServer
  -> connect 시 ticket 검증
  -> userId/matchId/team/slot 확정
```

서버 권위 보안의 핵심은 “클라이언트가 나는 몇 번 entity라고 주장하지 못하게 하는 것”이다. 접속 시 계정 token과 match ticket으로 session-to-entity mapping을 서버가 확정해야 한다.

#### 4. HTTP client가 최소 구현 단계다

Client `CHttpClient`는 WinHTTP로 요청/응답을 처리하고 Bearer token header를 붙인다. 다만 timeout, TLS 정책, retry/backoff, structured error, request id, cancellation 같은 운영 기본기가 아직 약하다.

단기 권장:

```text
timeout 기본값
status code별 error mapping
JSON parse 실패와 transport 실패 분리
config 기반 base URL
auth token refresh 실패 처리
```

#### 5. 서비스 테스트가 없다

`go test ./...`는 통과하지만 모두 `[no test files]`다. Auth refresh/revoke, matchmaking duplicate join, payment idempotency, shop purchase transaction 같은 곳은 테스트 가치가 높다.

가장 먼저 필요한 테스트:

```text
auth
  - register/login/refresh/logout
  - revoked refresh token 재사용 실패

matchmaking
  - duplicate join 거부
  - range expansion
  - matched 상태 TTL

payment/shop
  - idempotency key
  - balance update transaction
  - insufficient balance
```

### Riot식 구조 관점의 판단

공개적으로 Riot 내부 구현을 단정할 수는 없다. 다만 대규모 온라인 게임의 일반 원칙으로 보면, Winters의 방향은 맞다.

```text
실시간 게임 서버
  - 짧은 tick, 낮은 latency, 서버 권위, match-local state

플랫폼 서비스
  - 계정, 인증, 결제, 상점, 매칭, 랭킹, 전적

이벤트 버스
  - match result, payment result, inventory update, telemetry

데이터베이스
  - 영속 계정/경제/전적
```

중요한 점은 실시간 서버가 결제/상점/프로필 DB를 직접 물고 늘어지면 안 된다는 것이다. 실시간 서버는 매치 시작 전에 검증된 loadout/rune/skin 같은 match input만 받고, 매치 중에는 gameplay truth만 책임지는 방향이 맞다.

### Session 3 결론

Backend 분리는 좋다. 하지만 아직은 local development stack이다. 다음 단계는 서비스를 더 쪼개는 것이 아니라, `Client bootstrap config`, `match ticket`, `game server allocation`, `service tests`, `production secret policy`를 세우는 것이다. 이 다섯 가지가 들어오면 Client/Server/Backend의 계약이 훨씬 단단해진다.

## Session 4 - Build System, CMake/Ninja, Project Filters, CI/CD

### 현재 상태

Winters 본체의 현재 build spine은 Visual Studio/MSBuild다.

```text
Winters.sln
  Engine              -> Engine/Include/Engine.vcxproj
  Client              -> Client/Include/Client.vcxproj
  Server              -> Server/Include/Server.vcxproj
  WintersAssetConverter
  DX12SmokeHost
```

현재 repository root에는 Winters 본체용 `CMakeLists.txt`, `CMakePresets.json`, Ninja preset, CI workflow가 없다. 발견된 CMake 파일은 ImGui external examples 쪽이다. 따라서 지금 기준에서 “CMake/Ninja를 이미 쓴다”가 아니라 “도입하려면 새 spine을 설계해야 한다”가 정확하다.

### 좋은 기반

1. `Winters.sln`이 Engine, Client, Server, Tools를 한 곳에서 묶는다.
2. Client와 DX12SmokeHost는 solution dependency로 Engine을 먼저 빌드한다.
3. Server는 Engine project reference를 가진다.
4. Client/Server는 `FlatcCodegen` target으로 schema codegen을 빌드 전에 실행한다.
5. `UpdateLib.bat`이 Engine DLL/lib/header와 third-party DLL 배포를 한 곳에서 처리한다.
6. Debug/Release와 DX11/DX12 구성이 명시되어 있다.

### 이번 검증 결과

프로젝트/필터 정합성 스크립트 기준:

```text
Engine              items=333 filters=333 missing_files=0 missing_filter=0 stale_filter=0
Client              items=292 filters=292 missing_files=0 missing_filter=0 stale_filter=0
Server              items=104 filters=104 missing_files=0 missing_filter=0 stale_filter=0
WintersAssetConverter items=12 filters=12 missing_files=0 missing_filter=0 stale_filter=0
DX12SmokeHost       items=1 filters=1 missing_files=0 missing_filter=0 stale_filter=0
```

Engine의 Lua 통합 파일 33개가 `.vcxproj`에는 있고 `.vcxproj.filters`에는 없던 상태였는데, `12. Scripting\00. Lua VM` 필터와 `LuaRuntime.cpp` 필터를 추가해 정리했다.

### 핵심 위험

#### 1. vcxproj 수동 파일 목록은 계속 누락을 만든다

현재는 새 `.cpp/.h`를 만들 때 다음 목록을 모두 맞춰야 한다.

```text
*.vcxproj
*.vcxproj.filters
EngineSDK copy 대상
Client/Server include path
필요 시 UpdateLib 배포
```

이 방식은 작은 팀에는 가능하지만, 챔피언/시스템/툴이 계속 늘면 누락이 반복된다.

단기 대책:

```text
Tools/check_project_items.ps1
  - vcxproj item이 실제 파일인지 검사
  - filters에 없는 item 검사
  - filters에만 남은 stale item 검사
```

중기 대책:

```text
Directory.Build.props
  - 공통 C++ standard, warning, include, output 규칙 중앙화

build.ps1
  - msbuild 경로 탐색
  - Engine/Server/Client/Tools 표준 빌드
  - flatc/go test/diff-check 포함
```

#### 2. EngineSDK export가 build step에 묶여 있다

`UpdateLib.bat`은 실용적이지만, Client pre-build와 Engine post-build 양쪽에서 호출된다. Header copy와 DLL deploy가 같은 스크립트에 섞여 있어 실패 원인 추적이 어려워질 수 있다.

목표 분리:

```text
export_sdk_headers
copy_engine_runtime
copy_thirdparty_runtime
copy_client_assets
```

이렇게 target을 나누면 CI 로그와 로컬 실패 지점이 명확해진다.

#### 3. FlatBuffers codegen은 좋지만 변경 감지가 없다

Client/Server가 매번 `run_codegen.bat`을 실행하는 구조는 단순하고 안전하다. 다만 schema가 늘면 빌드 시간이 늘고, generated 파일 변경 여부가 CI에서 검증되지 않을 수 있다.

권장:

```text
schema check
  - run_codegen
  - git diff --exit-code Shared/Schemas/Generated
```

이 검사는 “schema 변경했는데 generated code 안 올림” 문제를 막는다.

#### 4. CMake/Ninja는 지금 바로 갈아타기보다 병행 spine이 맞다

현재 Windows/VS 프로젝트가 잘 돌아가고, third-party lib 경로와 post-build 배포가 vcxproj에 많이 들어 있다. 한 번에 CMake로 옮기면 gameplay 구현 속도가 크게 떨어질 수 있다.

권장 순서:

```text
1. msbuild 기반 build.ps1 표준화
2. project/filter consistency check 추가
3. C++ shared source list manifest 작성
4. Engine/Shared/GameSim부터 CMake object/static target 실험
5. Server headless target을 Ninja로 빌드
6. Client/renderer는 마지막에 이동
```

즉, CMake/Ninja의 첫 목표는 “전체 엔진 이전”이 아니라 “CI에서 빠르게 Server/GameSim 검증”이어야 한다.

### CI/CD 목표

첫 CI는 거창할 필요가 없다.

```text
ci-fast
  - git diff --check
  - project/filter consistency
  - Shared/Schemas run_codegen and generated diff check
  - go test ./... in Services
  - msbuild Server Debug x64
  - msbuild Client Debug x64

ci-render-smoke
  - DX12SmokeHost Debug-DX12
  - asset converter smoke
  - optional screenshot/render hash

ci-release
  - Release x64
  - package runtime DLL/assets
  - artifact upload
```

Gameplay 구조를 잡는 지금 시점에서는 `ci-fast`가 가장 중요하다. Server/GameSim이 매번 깨지지 않는 것이 챔피언 확장보다 먼저다.

### Session 4 결론

현재 MSBuild spine은 실용적으로 맞다. CMake/Ninja는 “지금 당장 전체 전환”보다 “GameSim/Server 검증을 빠르게 돌리는 병행 빌드”로 시작하는 것이 좋다. 이번 세션에서는 Engine Lua 파일의 filter 누락을 정리했고, 장기적으로는 project/filter consistency check를 CI에 넣어 같은 문제가 재발하지 않게 해야 한다.

## Session 5 - Planning, Design Data, Champion Authoring Workflow

### 현재 상태

문서는 많다. 부족한 것은 문서량이 아니라 “최종 원본”이다.

현재 챔피언 관련 정보는 여러 곳에 나뉘어 있다.

```text
Client/Private/GameObject/SkillTable.cpp
  - legacy SkillDef table

Client/Private/GameObject/Champion/*/*_Registration.cpp
  - champion visual asset
  - client skill registration
  - legacy hook / gameplay hook / visual hook registration

Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp
  - server/default cooldown, range, timing, action lock

Shared/GameSim/Champions/*GameSim.cpp
  - authoritative skill behavior

Shared/GameSim/Registries/*
  - stats/scaling/reward registry shell
```

이 구조는 10명 안팎의 챔피언을 빠르게 붙일 때는 유연하다. 하지만 150명 목표라면 같은 스킬의 cooldown/range/timing이 Client table, Registration, Server default에 중복될 수 있다.

### 좋은 기반

1. 챔피언별 registration 파일 패턴이 생겼다.
2. `CChampionRegistry`, `CSkillRegistry`, `CGameplayHookRegistry`, `CVisualHookRegistry`가 있어 registry 기반 구조로 넘어갈 길이 있다.
3. `Shared/GameSim/Registries`가 이미 stats/scaling/reward 데이터의 정착지를 암시한다.
4. `plan-rules` 문서가 있어 구현 계획을 코드 패치 단위로 만들 수 있다.
5. gotchas 문서가 있어 반복 실수를 팀 지식으로 남기는 문화가 있다.
6. `.wmesh/.wskel/.wanim`과 champion asset pipeline 문서가 있어 아트 파이프라인 방향이 있다.

### 핵심 위험

#### 1. Champion data source of truth가 갈라져 있다

현재는 같은 성격의 데이터가 여기저기 있다.

```text
cooldown/range/timing
  - Client SkillTable
  - champion Registration
  - Shared ChampionRuntimeDefaults

asset/animation key
  - ChampionTable
  - champion Registration
  - visual hook code

server behavior
  - CommandExecutor branch
  - GameplayHookRegistry
  - champion GameSim
```

장기 목표:

```text
ChampionSpec
  - champion id/name/role/tags
  - stat growth
  - skill slot definitions
  - cooldown/range/mana/timing/stage
  - gameplay primitive list
  - visual cue ids
  - animation keys
  - asset references
```

단, 처음부터 거대한 JSON/DB로 모두 옮기면 구현 속도가 떨어진다. 지금은 C++ registry를 유지하되, `Shared/GameSim` 기준의 authoritative spec을 먼저 만들고 Client는 그 spec의 visual projection만 소비하는 쪽이 좋다.

#### 2. 문서가 계획서 중심이라 현재 구조 판단 문서가 부족해질 수 있다

`.md/plan`과 `.md/TODO`에는 과거 계획과 작업 로그가 많다. 좋은 자산이지만, 나중에 새 사람이 들어오면 “그래서 현재 정답은 무엇인가?”를 찾기 어렵다.

문서 계층을 나누는 것이 좋다.

```text
AGENTS.md / CLAUDE_Legacy.md
  - 행동 규칙, 절대 지켜야 할 흐름

.md/architecture/*
  - 현재 구조의 정답
  - dependency direction
  - module ownership
  - 장기 원칙

.md/plan/*
  - 구현 전 계획
  - 코드 패치 단위 지시서

.md/TODO/*
  - 날짜별 작업 로그, 완료 보고, handoff

.claude/gotchas.md
  - 반복 실수 방지 규칙만
```

이 원칙은 이미 AGENTS.md의 Document Policy와 맞다.

#### 3. champion authoring 완료 기준이 더 구체적이어야 한다

앞으로 비에고/사일러스/애쉬/제드/잭스를 계속 붙이면 “스킬이 구현됐다”의 의미가 흔들릴 수 있다.

챔피언 완료 기준은 최소한 다음 축으로 봐야 한다.

```text
Gameplay
  - server command validation
  - damage/status/projectile/area/dash/form primitive
  - cooldown/rank/resource
  - death/respawn/passive interaction

Replication
  - snapshot state
  - event/cue
  - client visual application

Presentation
  - FBX/animation key
  - FX cue
  - UI icon/cooldown/state
  - sound hook if available

AI
  - CanUseSkill profile
  - targetability/visibility query
  - no direct mutation

Verification
  - server sim log
  - event/snapshot log
  - client render confirmation
  - regression build
```

이 체크리스트가 있어야 “비에고 E가 보인다”에서 끝나지 않고, “적 AI가 못 보고, 타겟팅이 막히고, 클라 표현도 맞고, 네트워크로 재현된다”까지 간다.

### Data-driven의 적정선

지금 당장 모든 스킬을 Lua/JSON으로 옮길 필요는 없다. 더 안전한 순서는 이렇다.

```text
1. C++ primitive를 안정화한다.
   Status, Buff, Damage, Projectile, Area, Dash, Summon, FormOverride, SpellbookOverride

2. C++ ChampionSkillModule에서 primitive를 조합한다.
   ViegoE = Area + Status(Invisible) + Event(MistCue)

3. 반복되는 숫자/asset/timing을 table/spec으로 내린다.
   cooldown/range/timing/animation/fx id

4. spec 검증기를 만든다.
   missing asset, invalid slot, duplicate hook id, server/client mismatch

5. Lua/script는 designer iteration이 필요한 곳부터 제한적으로 쓴다.
   UI layout, simple skill composition, debug sandbox
```

즉, scripting은 “문장을 자연어처럼 적으면 게임이 이해한다”가 아니다. 대체로 엔진이 미리 제공한 함수와 데이터 구조를 스크립트 언어로 호출하는 것이다.

예:

```text
자연어가 아님:
  "비에고가 안개 속에 들어가면 적에게 안 보이게 해줘"

스크립팅에 가까움:
  Area.Create(owner, shape, duration)
  Area.OnInside(SelfOnly, ApplyStatus(Invisible, 0.25, RefreshDuration))
  Event.Emit(MistLoopCue)
```

### Session 5 결론

Winters의 협업/문서 기반은 이미 꽤 강하다. 다음 병목은 문서를 더 늘리는 것이 아니라, champion spec의 원본을 정하고 “완료 기준”을 동일하게 적용하는 것이다. 당장은 `Shared/GameSim` authoritative spec과 Client visual projection을 분리하는 것부터 시작하면 된다.

## Session 6 - Performance, Server Tick, Snapshot, Runtime Operations

이 세션은 “지금 당장 빠르게 만들자”가 아니라, 서버 권위 구조를 유지하면서 어디를 측정하고 어디를 나중에 최적화해야 하는지 보는 세션이다. 결론부터 말하면 Winters의 현재 구조는 5대5 또는 소규모 테스트 기준으로는 충분히 단순하고 이해 가능하다. 다만 150명 챔피언, 수많은 projectile/status/area, 운영 서버까지 상정하면 성능의 핵심은 알고리즘 하나가 아니라 다음 네 가지다.

```text
1. deterministic correctness
2. tick phase observability
3. snapshot/event bandwidth control
4. allocation and repeated world scan reduction
```

성능 최적화는 StatusControl, Damage, Projectile, Area 같은 gameplay primitive가 안정된 뒤에 해야 한다. 아직 로직의 의미가 흔들리는 단계에서 빠르게 만들면 버그가 빨라질 뿐이다.

### 현재 tick 구조

`Server/Private/Game/GameRoom.cpp`의 서버 루프는 대략 다음 흐름이다.

```text
CGameRoom::Tick
  lock m_stateMutex
  DrainCommands
  ServerBotAI
  ExecuteCommands
  SimulationSystems
  Record Lag History
  BroadcastEvents
  BroadcastSnapshot
```

그리고 `Shared/GameSim/DeterministicTime.h` 기준 서버 시뮬레이션은 30Hz다.

```cpp
inline constexpr float kFixedDt = 1.0f / 30.0f;
inline constexpr uint32_t kTicksPerSecond = 30;
```

이 전제는 좋다. 클라이언트 FPS, 렌더링 보간, 이펙트 재생 속도와 별개로 서버 판정은 고정 tick으로 굴러간다. 실제 운영형 게임도 “렌더 프레임”과 “서버 simulation step”을 분리한다.

### 현재 구조의 좋은 점

- 서버 tick이 한 곳에서 순서대로 진행되므로 디버깅하기 쉽다.
- `DeterministicEntityIterator`를 통해 반복 순서를 고정하려는 의도가 있다.
- `CJobSystem`, `CCPUProfiler`, `CSpatialIndex` 같은 향후 확장 도구가 이미 있다.
- `Session::Send`는 IOCP 기반 비동기 송신 큐를 사용하므로 네트워크 송신 자체는 blocking send 방식보다 낫다.
- snapshot/event를 서버에서 만들고 클라이언트가 visual projection으로 소비하는 방향은 맞다.

이 구조는 아직 작고 직접적이라 오히려 좋다. 문제는 “확장될 때 어디가 비용이 되는지”를 미리 알고 있어야 한다는 점이다.

### 가장 큰 병목 후보 1 - tick 전체 lock

`CGameRoom::Tick`은 현재 `m_stateMutex`를 잡은 상태로 command 처리, simulation, event broadcast, snapshot broadcast까지 진행한다. 초기에는 안전하지만, tick 내부 작업이 늘어날수록 lock 보유 시간이 길어진다.

위험은 두 가지다.

```text
1. 외부 thread가 room state를 읽거나 명령을 넣을 때 대기 시간이 길어진다.
2. snapshot/event 직렬화와 session lookup/send 준비까지 lock 안에 있으면 네트워크 쪽 비용이 simulation lock에 섞인다.
```

당장 고칠 필요는 없지만, 장기 구조는 다음 방향이 맞다.

```text
lock 안:
  command queue drain
  authoritative simulation
  replicated event/snapshot payload 준비

lock 밖:
  session별 packet wrap
  socket send queue push
  logging/metrics export
```

주의할 점은 snapshot payload가 lock 밖에서 원본 world를 다시 보면 안 된다는 것이다. lock 안에서 immutable payload나 frame result를 만들어 빼고, lock 밖에서는 그것을 송신만 해야 한다.

### 병목 후보 2 - 세션별 전체 snapshot build

`Phase_BroadcastSnapshot`은 session마다 `SnapshotBuilder::Build`를 호출한다.

```text
for each session:
  Build(snapshot, yourNetId)
  Send(snapshot)
```

`SnapshotBuilder::Build`는 entity를 모으고 정렬하고 FlatBuffer를 새로 만든다. 플레이어 수가 적으면 문제 없지만, 세션 수 N과 entity 수 E에 대해 비용이 `N * E`로 커진다.

장기 구조는 둘 중 하나로 가야 한다.

```text
Option A - shared full snapshot + per-recipient envelope
  world snapshot은 tick당 한 번 만든다.
  yourNetId, camera/team-specific 정보만 작은 envelope로 분리한다.

Option B - interest management / AOI snapshot
  recipient별로 보이는 entity만 보낸다.
  넓은 맵, 관전자, 대규모 entity에 유리하다.
```

Winters는 MOBA 구조이므로 처음에는 Option A가 현실적이다. LoL 규모로 가면 결국 Option B 또는 team/vision 기반 필터가 필요하다.

### 병목 후보 3 - repeated world scan

현재 여러 시스템이 `DeterministicEntityIterator<T>::CollectSorted`를 호출한다. 이 함수는 매번 vector를 만들고 world를 순회한 뒤 sort한다.

이 방식은 안전하고 결정적이다. 하지만 status, projectile, AI, movement, death, snapshot이 늘어날수록 같은 tick 안에서 비슷한 목록을 계속 만든다.

특히 다음 위치가 scan-heavy하다.

```text
MoveSystem
  SpatialAgentComponent 전체 수집
  MoveTargetComponent 전체 수집
  mover마다 blocker 후보 비교

BotLaneAISystem
  enemy champion 검색
  last hit minion 검색
  enemy structure 검색
  allied minion 검색
  lane minion count
  turret danger 계산

SnapshotBuilder
  TransformComponent 전체 수집
  netId 기준 정렬
  entity별 cooldown/rank/inventory/status 조립
```

바로 최적화하기보다는 먼저 profiler counter를 붙여야 한다.

```text
CollectSorted 호출 횟수
component별 collected entity 수
system별 ms
snapshot bytes
event count
projectile count
status effect count
command count
```

그 다음에 공통 list cache, frame scratch allocator, spatial query, lane blackboard를 적용하는 순서가 안전하다.

### 병목 후보 4 - event와 packet allocation

`Phase_BroadcastEvents`는 event마다 각 session으로 packet을 만든다. `Session::Send`는 큐 기반 비동기 송신이라 방향은 좋지만, event가 많아지면 tick마다 작은 packet이 많이 생긴다.

장기적으로는 다음 구조가 낫다.

```text
tick event batch
  DamageEvent[]
  SkillCastEvent[]
  ProjectileSpawnEvent[]
  ProjectileHitEvent[]
  EffectCueEvent[]

send
  session당 tick event packet 1개
```

이렇게 하면 syscall/packet wrapping/queue object 수가 줄고, 클라이언트도 tick 단위로 visual cue를 안정적으로 정렬할 수 있다.

### Profiler 사용 원칙

`Engine/Private/Core/Profiler/CPUProfiler.cpp`에는 `WINTERS_PROFILING` 기반 CPU profiler가 있다. 다만 scope 종료 시 mutex를 잡으므로 inner loop마다 박으면 profiler 자체가 병목이 된다.

추천 사용 위치는 coarse phase다.

```text
GameRoom.Tick
GameRoom.DrainCommands
GameRoom.BotAI
GameRoom.ExecuteCommands
GameRoom.StatusEffect
GameRoom.Move
GameRoom.AttackChase
GameRoom.ChampionTicks
GameRoom.Projectiles
GameRoom.Damage
GameRoom.Death
GameRoom.Events
GameRoom.Snapshot
```

inner loop는 timer scope보다 counter가 낫다.

```text
numEntities
numProjectiles
numStatusEffects
numSnapshotBytes
numBroadcastEvents
numCommands
```

### JobSystem 적용 시점

`Engine/Private/Core/JobSystem.cpp`의 job system은 이미 존재한다. 하지만 server-authoritative simulation에 바로 병렬화를 넣으면 재현성, 순서, race, floating point 차이 문제가 생긴다.

적용 순서는 다음이 맞다.

```text
지금:
  single-thread authoritative simulation
  phase order 고정
  profiler/counter 추가

다음:
  read-only query/cache build를 job으로 분리
  예: visibility precompute, spatial index rebuild, static asset preparation

나중:
  명시적 read/write set을 가진 simulation phase만 병렬화
  결과 commit 순서는 deterministic하게 고정
```

즉 JobSystem은 “있으니까 쓰는 것”이 아니라, phase contract가 생긴 뒤에 써야 한다.

### SpatialIndex 적용 방향

`Engine/Private/ECS/SpatialIndex.cpp`는 이미 radius query를 제공한다. 하지만 현재 GameSim의 많은 검색은 여전히 full scan이다.

적용 우선순위는 다음이 현실적이다.

```text
1. projectile hit 후보
2. area/aura inside 후보
3. AI target 후보
4. movement blocker 후보
5. vision/stealth visibility 후보
```

특히 Viego E, Ashe W/R, Zed shadow, Jax E 같은 기능이 붙으면 projectile/area/status 후보 검색이 급격히 많아진다. 이때 `SpatialIndex`를 gameplay query primitive로 승격시키는 것이 좋다.

### 운영 관점에서 필요한 것

운영 서버까지 생각하면 성능은 FPS가 아니라 다음 지표로 봐야 한다.

```text
server tick p50/p95/p99
tick overrun count
command queue depth
snapshot bytes per second
event bytes per second
send queue depth per session
disconnect/reconnect count
sim rollback/replay mismatch count
match allocation latency
service request latency
database query latency
redis/kafka failure rate
```

LoL 같은 게임은 “프레임이 빨라 보이는 것”보다 “서버 판정이 늦지 않고, 재현 가능하며, 악성 클라이언트 입력에도 흔들리지 않는 것”이 훨씬 중요하다.

### Session 6 결론

Winters의 성능 방향은 아직 무너져 있지 않다. 오히려 지금은 단순한 single-thread authoritative tick이 맞다. 다만 다음 작업 전에 profiler/counter를 먼저 깔아야 한다.

우선순위는 다음이다.

```text
1. StatusControl Query 소비처 연결로 gameplay correctness 확보
2. GameRoom phase profiler/counter 추가
3. Viego E / Ashe W,R로 status + projectile + snapshot 검증
4. snapshot shared build 또는 최소한 snapshot 비용 측정
5. SpatialIndex를 projectile/area 후보 검색에 연결
6. event batch packet 구조 검토
7. JobSystem은 deterministic phase contract 이후 적용
```

즉 “최적화”보다 “측정 가능한 서버 권위 구조”가 먼저다. 이 순서가 150명 챔피언을 받쳐도 무너지지 않는 길이다.

## Session 7 - GameSim Folder Structure Cleanup Direction

현재 Winters 루트 바로 아래의 `GameSim`은 실제 소스 폴더가 아니라 shortcut이고, 실제 authoritative gameplay 코드는 `Shared/GameSim` 아래에 있다. 따라서 정리 기준은 “루트에 GameSim 폴더를 새로 만드는 것”이 아니라 `Shared/GameSim` 내부의 역할 경계를 선명하게 하는 것이다.

현재 구조는 다음과 같다.

```text
Shared/GameSim
  Champions
  Components
  Definitions
  Registries
  Systems
  DeterministicRng.h
  DeterministicTime.h
  EntityIdMap.h
  World.h
```

큰 방향은 맞다. 문제는 `Systems`가 너무 많은 책임을 한 폴더에 담기 시작했고, `Components`도 domain별 응집도가 약해지고 있다는 점이다. 아직 파일 수가 폭발한 단계는 아니지만, Viego/Sylas/Ashe/Zed/Jax를 계속 붙이면 아래처럼 정리하지 않으면 금방 찾기 어려워진다.

### 목표 구조

장기 목표는 다음처럼 domain별로 나누는 것이다.

```text
Shared/GameSim
  Core
    DeterministicRng.h
    DeterministicTime.h
    EntityIdMap.h
    World.h
    DeterministicEntityIterator.h

  Components
    Combat
    Movement
    Status
    Network
    AI
    ChampionState

  Definitions
    Champion
    Item
    Map
    Skill
    Snapshot

  Registries
    Champion
    Item
    Skill
    Skin
    Reward

  Systems
    Combat
    Movement
    Status
    Projectile
    Area
    AI
    Replication
    Progression

  Champions
    Ashe
    Irelia
    Jax
    Viego
    Yasuo
    Yone
```

다만 지금 당장 이 구조로 대이동하면 `.vcxproj`, `.filters`, include path, Client/Server 공유 컴파일이 한꺼번에 흔들린다. 그래서 “물리 이동”보다 먼저 “논리 경계”부터 확정해야 한다.

### 정리 원칙

`Shared/GameSim/Core`는 deterministic runtime만 가진다.

```text
가능:
  tick time
  rng
  entity id mapping
  deterministic iteration

금지:
  champion-specific logic
  client visual
  server session/network send
```

`Shared/GameSim/Systems/Status`는 상태 이상의 유일한 판정 진입점이 된다.

```text
StatusEffectSystem
GameplayStateQuery
future: StatusApplicationHelpers
```

`Shared/GameSim/Systems/Combat`은 damage/basic attack/on-hit 판정을 가진다.

```text
DamagePipeline
DamageQueueSystem
CombatFormula
future: OnHitEffectSystem
```

`Shared/GameSim/Systems/Projectile`은 projectile movement, hit candidate, on-hit effect를 가진다. 지금은 이 책임 일부가 `Server/GameRoom` 안에 있으므로, 장기적으로 빼내야 한다.

`Shared/GameSim/Systems/Area`는 Viego E, Morgana W 같은 장판/aura primitive를 가진다. 다음 세션의 핵심이다.

`Shared/GameSim/Champions`는 champion별 “조합 코드”만 가진다.

```text
좋은 예:
  ViegoE = Area + Status(Invisible) + EffectCue
  AsheW = ProjectileFan + OnHitEffect(Slow)

나쁜 예:
  ViegoGameSim 안에서 직접 enemy AI visibility까지 수정
  AsheGameSim 안에서 snapshot state를 직접 조작
```

### 적용 순서

지금 바로 폴더를 옮기기보다 다음 순서가 안전하다.

```text
1. StatusControl Query 소비처 연결 완료
2. Area/Aura primitive 추가
3. Projectile OnHitEffect 추가
4. Spellbook/FormOverride 추가
5. Champion별 특수 기능을 primitive 조합으로 이관
6. Systems 내부를 domain 하위 폴더로 물리 이동
7. Components 내부를 domain 하위 폴더로 물리 이동
8. vcxproj / filters / include 경로 정리
```

즉 폴더 정리는 구조를 만드는 첫 작업이 아니라, 구조가 생긴 뒤에 물리적으로 정돈하는 마지막 작업에 가깝다. 지금 먼저 해야 할 일은 `Status`, `Area`, `Projectile`, `Spellbook`, `ChampionState`의 책임 경계를 코드에서 확정하는 것이다.

### Session 7 결론

GameSim 정리의 핵심은 `Shared/GameSim`을 유지하되, `Systems`와 `Components`를 domain별로 쪼갤 준비를 하는 것이다. 당장 파일 이동을 시작하면 빌드/필터/공유 include가 크게 흔들릴 수 있으므로, 이번 단계에서는 문서로 목표 구조를 고정하고 다음 primitive 구현이 안정된 뒤 물리 이동한다.

## Session 8 - InGame UI Shop Atlas, HUD State, Purchase Visual Handoff

이 세션은 “UI 비주얼 완성”과 “구매 시 HUD state/icon 반영”을 목표로 했다. 중요한 전제는 Engine/GameSim 경계 정리와 endpoint config 정리를 이번 세션에 섞지 않는 것이다. 상점과 HUD의 체감 완성도를 먼저 올리고, Engine UI가 `Shared/GameSim`을 직접 include하는 문제는 별도 세션에서 정리한다.

### 현재 상태

인게임 상점/HUD 흐름은 다음처럼 연결되어 있다.

```text
Client input / UI click
  -> UI_Manager::TryBuyInGameItem
  -> InGameBootstrapBridge buy callback
  -> CommandSerializer::SendBuyItem
  -> Server GameSim CommandExecutor::HandleBuyItem
  -> SnapshotBuilder gold/inventory
  -> Client SnapshotApplier GoldComponent/InventoryComponent
  -> UI_Manager::BuildChampionHUDState
  -> Champion HUD inventory icon draw
```

이번 세션 전에는 `itemshop_texture_atlas.png`가 존재했지만 코드/JSON/Lua 어디에서도 참조하지 않았다. 상점은 `상점1.png`를 거의 본체처럼 깔고, 아이템은 `Resource/Texture/UI/Items/itemId_*.png`를 개별 로딩하는 상태였다.

이번 세션 후에는 다음 파일들이 기준점이 된다.

```text
Client/Bin/Resource/UI/itemshop_atlas_manifest.json
  - itemshop_texture_atlas.png crop manifest

Engine/Private/Manager/UI/UI_Manager.cpp
  - shop atlas manifest load
  - atlas sprite crop rendering
  - item selection + BUY button flow
  - weak client-side purchase prediction for HUD display

Engine/Public/Manager/UI/UI_Manager.h
  - shop atlas manifest member
  - selected shop item id
  - predicted purchase helper declaration
```

주의할 점은 `Client/Bin/Resource`가 `.gitignore` 대상이라는 것이다. 새 manifest는 실제 런타임에는 필요하지만 `git status`에는 보이지 않는다. 빌드 후에는 `Client/Bin/Debug/Resource/UI/itemshop_atlas_manifest.json`에도 복사되어 있다.

### 이번 세션에서 완료된 것

1. `itemshop_texture_atlas.png`를 직접 쓰는 manifest를 추가했다.

```text
Client/Bin/Resource/UI/itemshop_atlas_manifest.json
  textures.shop -> Resource/Texture/UI/itemshop_texture_atlas.png
  sprites.main.panel
  sprites.left.panel
  sprites.bottom.bag.panel
  sprites.header.strip
  sprites.tab.active
  sprites.search.box
  sprites.button.long
  sprites.button.dim
  sprites.slot.frame
  category/search/coin/close icon sprites
```

2. `UI_Manager::LoadInGameShopAssets`가 상점 atlas manifest를 읽고 texture SRV를 로드한다.

```text
Resource/UI/itemshop_atlas_manifest.json
fallback:
Client/Bin/Resource/UI/itemshop_atlas_manifest.json
```

3. `UI_Manager::DrawInGameShop`가 `상점1.png` 전체 배경 중심에서 atlas crop 조립 중심으로 바뀌었다.

```text
atlas crop:
  main panel
  right owned-item panel
  left recommendation/bag panel
  tab/header/search strip
  item slot frames
  buy button
  category icons

reference:
  상점1.png는 낮은 alpha alignment guide로만 출력
```

4. 상점 상호작용이 “아이템 클릭 즉시 구매”에서 “아이템 선택 후 BUY 클릭”으로 정리되었다.

```text
item icon click
  -> selected item id update
  -> detail area name/price/icon update

BUY click
  -> TryBuyInGameItem
  -> server buy command send
  -> weak local HUD prediction
```

5. 구매 직후 HUD inventory 아이콘이 보이도록 약한 client-side prediction을 추가했다.

```text
UI_Manager::TryApplyPredictedInGamePurchase
  - local champion GoldComponent 확인
  - InventoryComponent 확인
  - gold 부족 / inventory full status 표시
  - client copy의 gold/inventory만 즉시 반영
  - m_InGameInventorySlots 갱신
```

이 예측은 presentation feel을 위한 약한 UI 예측이다. 서버 snapshot이 오면 `SnapshotApplier`가 authoritative gold/inventory 값으로 다시 덮는다.

6. HUD inventory slot은 item id 숫자 fallback 대신 실제 item icon PNG를 먼저 그린다.

```text
State.InventoryItemIds[n]
  -> FindInGameShopItem(itemId)
  -> pSRV icon 있으면 AddImage
  -> 없으면 itemId text fallback
```

### 좋은 기반

1. 구매 command 자체는 이미 `CommandSerializer::SendBuyItem`에서 서버로 전송된다.
2. 서버 `CommandExecutor::HandleBuyItem`이 gold 차감, inventory 추가, stat dirty 처리를 authoritative하게 수행한다.
3. `SnapshotBuilder`가 gold/inventory를 snapshot에 실어 보내고, Client `SnapshotApplier`가 `GoldComponent`와 `InventoryComponent`에 반영한다.
4. `ChampionHUDState`에는 HP/MP/Level/XP/Cooldown/Gold/Inventory/SkillRank/Stat 필드가 이미 들어 있다.
5. `UIAtlasManifest`가 HUD와 상점 양쪽에서 쓸 수 있는 단순 JSON atlas loader로 자리잡았다.
6. item icon PNG는 `Resource/Texture/UI/Items/{itemId}_*.png` 자동 탐색으로 이미 연결되어 있다.

### 남은 리스크

#### 1. Engine UI가 아직 GameSim을 직접 include한다

이번 세션은 의도적으로 이 문제를 고치지 않았다. 현재 `UI_Manager.cpp`는 여전히 다음 include에 의존한다.

```text
Shared/GameSim/Components/GoldComponent.h
Shared/GameSim/Components/InventoryComponent.h
Shared/GameSim/Components/SkillRankComponent.h
Shared/GameSim/Components/StatComponent.h
Shared/GameSim/Definitions/ItemDef.h
```

다음 Engine boundary 세션에서는 이 경로를 제거해야 한다.

목표 구조:

```text
Client adapter
  -> Shared/GameSim component 읽기
  -> ChampionHUDState / InGameShopCatalogItem 생성
  -> GameInstance UI_* API로 Engine에 전달

Engine UI
  -> ChampionHUDState / InGameShopCatalogItem 같은 DTO만 소비
  -> Shared/GameSim include 금지
```

#### 2. `itemshop_atlas_manifest.json`이 ignored resource 경로에 있다

현재 repo 정책상 `Client/Bin/Resource`는 `.gitignore` 대상이다. 따라서 다른 세션/머신에서 이 manifest가 누락되면 상점 atlas crop이 로드되지 않고 fallback 사각형 또는 낮은 alpha reference만 보일 수 있다.

해결 선택지는 두 가지다.

```text
Option A:
  resource manifest를 추적 가능한 source asset 경로로 옮기고 빌드/복사에 포함한다.

Option B:
  현 resource 경로를 유지하되 예외 규칙 또는 별도 resource package 규칙을 둔다.
```

단기 작업에서는 `Client/Bin/Debug/Resource/UI/itemshop_atlas_manifest.json` 존재 여부도 확인해야 한다.

#### 3. 상점 atlas crop 좌표는 1차 근사값이다

이번 세션의 crop 좌표는 `itemshop_texture_atlas.png`를 기준으로 실제 조각을 붙이는 첫 단계다. 목표 이미지와 픽셀 단위로 맞춘 최종 좌표는 아니다.

다음 visual tuning 기준:

```text
P로 상점 열기
  -> main/right/left panel 위치
  -> search strip, tab, buy button 위치
  -> item slot frame alignment
  -> gold text, item price text 위치
  -> HUD와 겹치지 않는지 확인
```

#### 4. 약한 구매 예측은 local ECS copy를 수정한다

`TryApplyPredictedInGamePurchase`는 서버 권위 결과가 아니라 UI feel을 위한 client-side prediction이다. 정상 네트워크 상황에서는 다음 snapshot이 같은 값 또는 authoritative 값으로 보정한다.

주의할 점:

```text
가능:
  구매 직후 HUD에 아이콘을 즉시 보여주는 presentation prediction

금지:
  client prediction을 gameplay truth로 취급
  server reject 이후에도 client UI가 계속 틀린 값을 유지
```

현재는 gold 부족/inventory full을 client copy 기준으로 막는다. 서버 reject 이유를 event로 내려주는 구조는 아직 없다.

#### 5. Lua UI host와 native UI가 동시에 존재한다

현재 `LuaUIHost`는 초기화되지만 `ui_boot.lua`는 빈 render function에 가깝다. native HUD/shop이 실제 출력을 담당한다. Lua UI는 다음 세션에서 UI layout scripting을 키울 수 있는 기반이지만, 지금은 기능 완성 경로가 아니다.

원칙:

```text
현재:
  native C++ UI_Manager가 shop/HUD를 그림

나중:
  Lua는 layout iteration 또는 thin declarative UI로 확장
```

### 검증 결과

정적 diff 검증:

```text
git diff --check
```

결과:

```text
통과
```

Client Debug x64 빌드:

```text
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" .\Winters.sln /p:Configuration=Debug /p:Platform=x64 /t:Client
```

결과:

```text
빌드 성공
오류 0개
기존 SDK/DLL interface 계열 warning 다수
```

PATH에 `msbuild`가 잡혀 있지 않아 절대 경로 MSBuild를 사용했다.

### 다음 세션 진입점

가장 가까운 다음 세션은 runtime visual QA다.

```text
1. Client Debug 실행
2. 인게임 진입
3. P로 상점 열기
4. atlas crop UI가 보이는지 확인
5. item icon 선택
6. BUY 클릭
7. HUD inventory slot에 item icon이 즉시 표시되는지 확인
8. snapshot 이후 gold/inventory가 authoritative 값으로 유지/보정되는지 확인
```

그 다음 구현 순서는 다음이 안전하다.

```text
1. 상점 atlas crop 좌표/스케일 visual tuning
2. shop catalog DTO 도입
3. ChampionHUDState submit API 도입
4. UI_Manager에서 Shared/GameSim include 제거
5. Client adapter가 HUD/shop DTO를 생성
6. 상태창.png 또는 stats panel도 같은 ChampionHUDState를 소비하게 연결
7. server reject/fail feedback event 검토
8. endpoint config 정리 세션으로 분리
```

### Session 8 결론

상점/HUD 시각 완성 목표는 1차로 달성했다. 이제 상점은 `itemshop_texture_atlas.png`를 실제 atlas crop으로 사용하고, 구매 요청 직후 HUD inventory에는 실제 item icon이 들어온다. 단, 이것은 UI 세션의 완성이지 Engine boundary 정리의 완성은 아니다.

다음 세션에서 가장 중요한 것은 runtime 화면으로 좌표를 확인하고, 그 다음 `ChampionHUDState`와 shop catalog를 Client adapter에서 생성하도록 옮기는 것이다. 이 순서를 지키면 “롤처럼 보이는 상점/HUD”와 “Engine이 GameSim을 직접 모르는 구조”를 둘 다 잡을 수 있다.
> 2026-05-25 update: `Tools/DX12SmokeHost` and the solution project were deleted after the integration review. Treat any DX12SmokeHost references below as historical evidence; active RHI validation lives in Engine/Client.
