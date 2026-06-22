# Ping, Death, Minion Projectile Bugfix Report

작성일: 2026-06-22

## 결론

요청한 세 버그에 대한 코드 반영과 자동 검증 파이프라인을 완료했다.

이번 수정의 핵심 원칙은 다음과 같다.

- 핑 UI는 입력 조건을 `Ctrl + 우클릭`으로 고정한다.
- 사망 상태의 플레이어 입력은 애니메이션과 실제 이동에 영향을 줄 수 없다.
- 미니언 투사체는 서버의 projectile destroy 이벤트와 같은 생명주기를 따른다.
- 검증은 리소스 존재성, diff whitespace, Client 빌드, Server 빌드를 통과해야 한다.

자동 검증 결과는 모두 통과했다. 런타임 화면 육안 확인은 별도 F5 세션에서 확인하면 된다.

## 반영 범위

### 1. Ping Wheel

관련 파일:

- `Client/Private/Scene/Scene_InGame.cpp`
- `Client/Public/Scene/Scene_InGame.h`
- `Engine/Private/Manager/UI/UI_Manager.cpp`
- `Engine/Public/Manager/UI/UI_Manager.h`
- `Engine/Private/GameInstance.cpp`
- `Engine/Include/GameInstance.h`
- `EngineSDK/inc/GameInstance.h`

반영 내용:

- 핑 휠 표시 조건을 `Ctrl` 키가 눌린 상태의 우클릭으로 제한했다.
- `Ctrl`이 없거나 ImGui가 마우스를 캡처 중이거나 플레이어가 사망한 경우 핑 휠이 열리지 않게 했다.
- 우클릭을 누른 시점의 월드 좌표를 저장하고, 우클릭을 놓는 방향으로 핑 종류를 결정한다.
- 선택된 핑은 월드 좌표 기반 맵 핑 마커로 3초 동안 화면에 표시한다.
- 핑 텍스처는 `Client/Bin/Resource` 하위 런타임 리소스만 사용한다.

방향 매핑:

```text
중앙: 기본 ping
오른쪽: on_my_way_new.png
위쪽: caution.png
아래쪽: assist.png
왼쪽: mia_new.png
```

회귀 방지 포인트:

- 기존 우클릭 이동 입력은 핑 휠이 활성화된 동안 소비되도록 유지한다.
- `Ctrl` 없는 우클릭에서는 핑 텍스처가 나오지 않는다.
- UI Manager가 월드 좌표를 화면 좌표로 투영하므로, 클라이언트 UI 표시 책임만 가진다.

### 2. Champion Death Lock

관련 파일:

- `Client/Private/Scene/Scene_InGame.cpp`
- `Client/Public/Scene/Scene_InGame.h`
- `Server/Private/Game/GameRoom.cpp`
- `Server/Private/Game/GameRoomInternal.h`
- `Shared/GameSim/Components/RespawnComponent.h`

반영 내용:

- 서버 기본 챔피언 사망 시간을 3초로 고정했다.
- 사망 시작 시 `RespawnComponent::respawnDelay`와 `respawnTimer`를 모두 3초로 설정한다.
- 클라이언트는 로컬 플레이어 사망 상태를 `HealthComponent`, snapshot state flag, `PoseStateComponent::Dead`로 판정한다.
- 사망 상태에서는 다음 입력 및 로컬 런타임 경로를 차단한다.

```text
movement
attack
combat input
skill dispatch
dash
flash
active skill runtime
local champion runtime
local post animation override
target hover / attack cursor
ping wheel
```

- 사망 상태 진입 시 남아 있는 이동 목적지와 nav path를 제거한다.
- 기존 dead pose 및 dead animation 경로가 입력/로컬 애니메이션 갱신에 의해 덮이지 않도록 했다.
- 화면 회색 오버레이는 기존 HLSL UI raw image pass를 사용해 전체 화면에 렌더한다.

회귀 방지 포인트:

- 사망 truth는 서버 GameSim 및 snapshot 상태가 기준이다.
- 클라이언트는 입력, 약한 예측, UI, 시각 효과만 잠근다.
- 사망 중 입력이 서버 이동 명령이나 로컬 애니메이션 변경으로 새어 나가지 않도록 주요 entry point에 직접 guard를 추가했다.

### 3. Minion Projectile Cleanup

관련 파일:

- `Client/Private/Network/Client/EventApplier.cpp`
- `Client/Public/Network/Client/EventApplier.h`
- `Tools/VerifyMinionProjectileAssets.ps1`

반영 내용:

- projectile spawn 시 WFX cue 또는 fallback billboard로 생성된 visual entity를 projectile net id에 묶어 추적한다.
- server `ProjectileHit` 이벤트가 `bDestroyed`로 들어오면 net projectile entity와 연결 visual entity를 함께 제거한다.
- 같은 projectile net id가 다시 spawn될 경우 기존 visual을 먼저 제거해 중복 잔상을 막는다.
- fallback billboard spawn 함수가 생성 entity id를 반환하도록 바꿔 추적 가능하게 했다.

리소스 검토 결과:

```text
Data/LoL/FX/Object/Minion/ranged_projectile_blue.wfx
Data/LoL/FX/Object/Minion/ranged_projectile_red.wfx
```

위 두 WFX cue와 내부 참조 리소스는 존재한다.

`Client/Bin/Resource/Texture/Object/Minion_Order/Ranged`,
`Client/Bin/Resource/Texture/Object/Minion_Chaos/ranged` 아래에서는 직접적인 원작 projectile 전용 텍스처 파일을 찾지 못했다. 현재 Winters 런타임에서는 위 WFX cue가 미니언 투사체 visual source다.

회귀 방지 포인트:

- 투사체 생명주기는 서버 event를 따른다.
- 클라이언트 visual만 별도 추적하고, projectile truth 자체는 client visual 쪽에서 만들지 않는다.
- WFX cue가 있으면 WFX visual을, 없으면 fallback billboard를 같은 destroy 경로로 제거한다.

## 검증 결과

### Ping Asset Verification

명령:

```powershell
powershell -ExecutionPolicy Bypass -File Tools\VerifyPingWheelAssets.ps1
```

결과:

```text
PASS
```

검증 내용:

- ping wheel cursor
- default ping
- on my way ping
- danger ping
- assist ping
- missing ping

### Minion Projectile Resource Verification

명령:

```powershell
powershell -ExecutionPolicy Bypass -File Tools\VerifyMinionProjectileAssets.ps1
```

결과:

```text
PASS
```

검증 내용:

- blue ranged projectile WFX 존재
- red ranged projectile WFX 존재
- WFX 내부 참조 model, texture, erode texture 존재
- Minion texture 폴더의 직접 projectile texture 후보 조사

### Whitespace Verification

명령:

```powershell
git diff --check
```

결과:

```text
PASS
```

비고:

LF가 CRLF로 바뀔 수 있다는 Git 경고만 출력되었고 whitespace error는 없었다.

### Client Build

명령:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /p:CL_MPCount=1
```

결과:

```text
PASS
Warnings: 14
Errors: 0
```

비고:

경고는 기존 EngineSDK `ISystem` DLL-interface 계열 `C4275` 경고다.

### Server Build

명령:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /p:CL_MPCount=1
```

결과:

```text
PASS
Warnings: 2
Errors: 0
```

비고:

경고는 기존 EngineSDK `ISystem` DLL-interface 계열 `C4275` 경고다.

## 남은 수동 확인

자동 검증과 빌드는 통과했다. 실제 화면 상호작용은 다음 항목만 F5 런타임에서 육안 확인하면 된다.

```text
1. Ctrl 없이 우클릭하면 ping wheel이 나오지 않는다.
2. Ctrl + 우클릭 press/hold 상태에서 ping wheel이 나온다.
3. 우클릭 release 방향에 맞는 ping image가 맵 위치에 3초 표시된다.
4. 챔피언 사망 시 dead animation이 입력으로 끊기지 않는다.
5. 사망 중 이동, 공격, 스킬, flash, dash 입력이 실제 이동/애니메이션에 영향을 주지 않는다.
6. 사망 중 화면 전체에 회색 overlay가 보인다.
7. 미니언 공격 projectile이 hit/destroy 시점에 남지 않는다.
```

## 최종 판단

이번 변경은 서버 권위 흐름을 유지한다.

```text
Client Input
-> GameCommand
-> Server GameSim
-> Snapshot/Event
-> Client Visual
```

핑은 클라이언트 UI 입력과 visual marker로 제한했고, 사망 시간은 서버 GameSim에서 고정했으며, 미니언 투사체 제거는 서버 projectile event에 종속시켰다.

따라서 현재 기준에서 자동 검증상 회귀는 발견되지 않았다.
