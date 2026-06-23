Session - 클라이언트 스폰의 gameplay 성격 하드코딩(타워 AI/구조물 HP/미니언 스탯)을 서버와 같은 Shared resolver로 모아 중복을 제거한다.

1. 반영해야 하는 코드

전제: 01 계획서의 `ObjectGameDataDB`가 반영되어 있어야 한다. Client는 Shared/GameSim resolver를 읽을 수 있으나(read-only), gameplay truth를 새로 만들지 않는다. 여기서 읽는 값은 client 예측/표시용이며 서버 판정을 대체하지 않는다.

1-1. C:/Users/tnest/Desktop/Winters/Client/Private/Manager/Structure_Manager.cpp

파일 상단 include 블록에 아래를 추가:

기존 코드:

```cpp
#include "Manager/Structure_Manager.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Registries/ObjectGameData/ObjectGameDataDB.h"
```

확인 필요:
- 실제 첫 include 줄을 확인하고 anchor를 교정한다.

`EntityID CStructure_Manager::Spawn_FromEntry(...)`에서 구조물 HP 하드코딩(line 358-362 근처):

```text
Turret = 3000f
Nexus = 5500f
Inhibitor = 4000f
```

을 아래로 교체한다(정확한 기존 코드 라인은 구현 직전 확인):

```cpp
    const f32_t fMaxHp = ObjectGameDataDB::ResolveStructureMaxHp(
        bNexus ? eStructureKind::Nexus
               : (bInhibitor ? eStructureKind::Inhibitor : eStructureKind::Turret));
```

같은 함수의 turret AI 하드코딩(line 379-383 근처):

```text
attackRange = 7.75f
attackDamage = 180f(Nexus) / 150f
projectileSpeed = 18f
```

을 아래로 교체한다:

```cpp
    const TurretAIGameDef& turretAI = ObjectGameDataDB::ResolveStructure().turretAI;
    // attackRange / attackDamage(=bNexus ? nexusAttackDamage : attackDamage) / projectileSpeed 를 turretAI에서 읽는다.
```

확인 필요:
- `Spawn_FromEntry`의 구조물 HP/AI 지역변수명과 `bNexus`/`bInhibitor` 판정 경로를 실제 코드로 확인 후 표현을 맞춘다.
- collider 반경 1.5f / halfExtents {1.5,3.0,1.5} / offset {0,1.5,0}(line 407, 417-418)는 client 표시 collider다. 서버 collider 프로파일과 의미가 다르면 이 세션에서 건드리지 않는다(visual collider 분리는 별도 판단).

1-2. C:/Users/tnest/Desktop/Winters/Client/Private/Manager/Minion_Manager.cpp

미니언 전투 스탯은 이미 공유 `ResolveMinionCombatDef`(MinionCombatDef.h)를 쓴다. 단일 진입점으로 통일한다.

`EntityID CMinion_Manager::Spawn_Minion(...)`에서 `ResolveMinionCombatDef(roleType)` 직접 호출을:

기존 코드:

```cpp
    ResolveMinionCombatDef(
```

아래로 교체:

```cpp
    ObjectGameDataDB::ResolveMinion(
```

그리고 파일 상단에 include 추가:

```cpp
#include "Shared/GameSim/Registries/ObjectGameData/ObjectGameDataDB.h"
```

확인 필요:
- `Minion_Manager.cpp`의 `ResolveMinionCombatDef` 호출이 정확히 한 곳인지 확인(여러 곳이면 모두 교체).
- `kDefaultMinionScale = 0.006f`(line 278)와 `kTibbersVisualScale`은 모델 표시 scale(visual)이다. gameplay가 아니므로 이 세션에서 데이터화/이동하지 않는다(11 시리즈 visual seed 소유).

1-3. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/ChampionSpawnService.cpp

챔피언 스탯은 이미 `CChampionStatsRegistry::Instance().Resolve()`로 서버와 같은 registry를 읽는다. 이 세션에서는 변경하지 않는다.

확인 필요:
- `CChampionStatsRegistry`와 `ChampionGameDataDB`가 같은 generated 소스를 보는지 점검. 둘이 서로 다른 stat 경로면 별도 통합 세션이 필요하다(이 파이프라인 범위 밖, 16 시리즈로 이관).

2. 검증

미검증:
- 빌드 미검증
- F5에서 client 구조물/미니언 표시·예측이 기존과 동일한지 미검증

검증 명령:
- git diff --check
- & "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" .\Client\Include\Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64

소유권 확인:
- rg -n "7.75f|projectileSpeed = 18|= 5500f|= 4000f|= 3000f" Client/Private/Manager/Structure_Manager.cpp
  -> client 전용 gameplay 리터럴이 0이어야 한다(visual scale 제외).

수동 확인:
- F5: 타워/넥서스/억제기 HP 바와 사거리 표시가 기존과 동일한지.
- F5: 미니언 스탯/이동이 기존과 동일한지.

확인 필요:
- Client가 `ObjectGameDataDB`(Shared/GameSim)를 include해도 forbidden dependency가 아닌지 확인(Client는 Shared를 읽을 수 있음). Engine 경유가 아니라 Shared 직접 include인지 확인.
