# Session - 비에고 영혼 흡수와 칼리스타 원혼 보초 결과 보고서

Date: 2026-06-24

## 목표

비에고의 적 처치 영혼, 영혼 흡수 애니메이션, 챔피언 변신, R 변신 해제 흐름을 서버 권위 구조로 정리했다. 동시에 칼리스타 W 원혼 보초를 서버 GameSim 엔티티로 만들고, 직선 왕복 이동, 60도 부채꼴 시야, 클라이언트 avatar/cone FX까지 연결했다.

## Winters 북극성

이번 작업의 북극성은 하나다.

```text
Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual
```

Winters에서 gameplay truth는 서버와 Shared/GameSim에 있어야 한다. 클라이언트는 입력, 약한 예측, 보간, 애니메이션, FX, UI를 맡는다. 그래서 이번 작업에서도 비에고가 언제 변신하는지, 칼리스타 보초가 어디에 있고 무엇을 보이게 하는지는 서버 GameSim이 결정한다. 클라이언트는 그 결과를 스냅샷으로 받아 모델, 재질, FX, FOW 표현으로만 보여준다.

## 나눌 수 없는 원자 단위

### 1. Command 원자

플레이어 의도는 `GameCommand` 하나로 들어온다.

- 비에고: 영혼 우클릭은 basic attack command처럼 들어오지만, target이 `ViegoSoulComponent`이면 일반 공격이 아니라 soul consume 명령으로 해석한다.
- 칼리스타: W는 self skill이 아니라 direction skill이다. 그래서 커서 방향이 서버로 전달되고, 서버가 그 방향으로 보초 경로를 만든다.

이 원자의 본질은 "의도만 전달하고 결과는 만들지 않는다"이다.

### 2. GameSim 원자

게임 결과는 Shared/GameSim에서만 확정한다.

- 비에고 영혼 생성은 `ViegoGameSim::TrySpawnSoulForKill`가 담당한다.
- 비에고 영혼 흡수는 즉시 변신하지 않고 `ViegoSimComponent`의 pending possession으로 기록된다.
- `ViegoGameSim::Tick`이 consume delay 이후 `FormOverrideComponent`를 붙인다.
- 칼리스타 W는 `KalistaSentinelComponent` 서버 엔티티를 생성하고, Tick에서 직선 왕복 이동과 cone 방향을 갱신한다.

이 원자의 본질은 "상태 변화는 서버 시뮬레이션이 소유한다"이다.

### 3. Replication 원자

서버 결과는 스냅샷으로만 클라이언트에 전달한다.

- 비에고 영혼은 `EffectAnchor` + `kSnapshotStateViegoSoulFlag`로 복제된다.
- 칼리스타 보초는 `EffectAnchor` + `EffectAnchorSubtype::KalistaWSentinel`로 복제된다.
- 새 네트워크 스키마를 늘리지 않고 기존 anchor/subtype 경로를 사용했다.

이 원자의 본질은 "클라이언트가 추측하지 않도록 필요한 최소 상태만 보낸다"이다.

### 4. Visual 원자

클라이언트 visual은 gameplay truth를 만들지 않는다.

- 비에고 영혼은 죽은 챔피언 모델을 붙이고, 초록 반투명 material override와 `Viego.Soul.Idle` FX를 적용한다.
- `ViegoConsumeSoul` 애니메이션은 form override 상태와 무관하게 항상 비에고 `passive_attack`으로 재생한다.
- 칼리스타 보초는 avatar billboard와 회색 cone ground decal을 붙인다.

이 원자의 본질은 "보이는 것은 결과의 표현이지 결과의 원인이 아니다"이다.

### 5. Data 원자

튜닝 값은 코드에 숨어 있지 않고 데이터로 드러나야 한다.

- 칼리스타 W의 direction target/range는 `Data/Gameplay/ChampionGameData/champions.json`에서 소유한다.
- 칼리스타 W lifetime, range, speed, sight range, radius, halfAngleCos는 `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`에 있다.
- definition pack을 재생성해 generated C++와 JSON을 동기화했다.

이 원자의 본질은 "기획/튜닝 가능한 값은 데이터가 소유한다"이다.

## 반영 내용

### 비에고

- 영혼 우클릭 시 즉시 `FormOverrideComponent`를 붙이던 흐름을 제거했다.
- `CommandExecutor`는 pending possession만 기록하고 `ViegoConsumeSoul` action state를 시작한다.
- `ViegoGameSim::Tick`이 delay 후 변신을 적용한다.
- BA/Q/W/E만 처치한 챔피언으로 override하고, R은 비에고 R로 유지한다.
- R 사용 시 active/pending possession을 해제한다.
- 영혼 스냅샷 수신 시 클라이언트가 처치 대상 챔피언 visual을 attach한다.
- 영혼 모델에 초록 반투명 material override를 유지한다.

### 칼리스타

- `KalistaSentinelComponent`를 추가해 서버 권위 보초 상태를 분리했다.
- Kalista W가 서버에서 보초 엔티티를 생성한다.
- 보초는 start/end 사이를 직선 왕복 이동한다.
- `VisionConeComponent`를 추가해 60도 부채꼴 시야를 표현한다.
- VisionSystem은 cone source에 한해 entity visibility와 FOW texture reveal을 cone 내부로 제한한다.
- 서버는 보초를 `EffectAnchorSubtype::KalistaWSentinel`로 복제한다.
- 클라이언트는 보초 snapshot을 받아 avatar/cone FX와 local vision component를 보강한다.

## 검증

실행한 검증:

```text
python Tools/LoLData/Build-LoLDefinitionPack.py --check
git diff --check
MSBuild Server\Include\Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
MSBuild Client\Include\Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

결과:

- definition pack check 통과: `0xB1428A52`
- `git diff --check` 통과. 기존 CRLF 변환 경고만 출력됨.
- Server Debug x64 빌드 통과: `Server/Bin/Debug/WintersServer.exe`
- Client Debug x64 빌드 통과: `Client/Bin/Debug/WintersGame.exe`

## 런타임 확인 포인트

- 비에고가 적을 처치하면 죽은 챔피언 위치에 영혼이 1회 생성되어야 한다.
- 영혼은 죽은 챔피언 모델 + 초록 반투명 연기 재질 + 비에고 초록 오라로 보여야 한다.
- 비에고가 영혼을 우클릭하면 먹기 애니메이션이 먼저 재생되고, 이후 BA/Q/W/E가 해당 챔피언 스킬로 바뀌어야 한다.
- 변신 중 R을 사용하면 변신이 풀리고 비에고 R이 실행되어야 한다.
- 칼리스타 W 사용 시 원혼 보초가 직선 왕복 이동하고, 회색 60도 cone 내부만 시야가 열려야 한다.

## 결론

이번 반영의 본질은 기능을 늘리는 것이 아니라 소유권을 선명하게 만드는 것이었다. 비에고의 영혼, 변신, 변신 해제와 칼리스타 보초의 이동, 시야, 복제는 모두 서버 GameSim이 결정한다. 클라이언트는 그 결정을 가장 얇은 형태로 받아 visual과 FOW로 표현한다. 이 구조가 Winters 폴더의 북극성에 맞는 가장 작은 원자 단위다.
