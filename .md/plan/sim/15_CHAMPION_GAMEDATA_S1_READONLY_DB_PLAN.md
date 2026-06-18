Session - S1 ChampionGameData Read-Only DB Facade

1. 반영해야 하는 코드

목표는 서버 권위 데이터 지향 구조의 첫 진입점을 만드는 것이다. S1에서는 gameplay behavior를 바꾸지 않는다. `ChampionGameData` 타입과 `ChampionGameDataDB` facade를 추가하고, facade 내부는 기존 `ChampionRuntimeDefaults` 함수를 그대로 호출한다. 이후 세션에서 시스템들이 이 facade를 바라보도록 옮길 수 있게 서버와 클라이언트 프로젝트에만 연결한다.

새 파일: `Shared/GameSim/Definitions/ChampionGameData.h`

```cpp
#pragma once

#include "GameContext.h"
#include "Shared/GameSim/Definitions/ChampionStatsDef.h"
#include "Shared/GameSim/Definitions/SkillDef.h"
#include "WintersTypes.h"

inline constexpr u32_t kChampionGameDataSchemaVersion = 1;
inline constexpr u8_t kChampionGameDataSkillSlotCount = 5;
inline constexpr u8_t kChampionGameDataSkillStageMax = 2;

struct ChampionGameDataSkillStage
{
    f32_t lockDurationSec = 0.6f;
    f32_t animPlaySpeed = 1.f;
    f32_t castFrame = 0.f;
    f32_t recoveryFrame = 0.f;
};

struct ChampionGameDataSkill
{
    bool_t bValid = false;
    u8_t slot = 0;
    eTargetMode targetMode = eTargetMode::Self;
    u8_t stageCount = 1;
    f32_t stageWindowSec = 0.f;
    f32_t cooldownSec = 0.f;
    f32_t rangeMax = 0.f;
    f32_t manaCost = 0.f;
    u16_t skillId = 0;
    u16_t scalingTableId = 0;
    u32_t gameplayPolicyId = 0;
    u32_t visualCueId = 0;
    ChampionGameDataSkillStage stages[kChampionGameDataSkillStageMax] = {};
};

struct ChampionGameData
{
    bool_t bValid = false;
    eChampion champion = eChampion::END;
    u32_t dataVersion = 1;
    u32_t authoringHash = 0;
    ChampionStatsDef stats{};
    f32_t visualYawOffset = 0.f;
    ChampionGameDataSkill skills[kChampionGameDataSkillSlotCount] = {};
};
```

새 파일: `Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h`

```cpp
#pragma once

#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/ChampionGameData.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"

namespace ChampionGameDataDB
{
    u32_t GetSchemaVersion();
    u32_t GetBuildHash();

    const ChampionGameData* FindChampion(eChampion champion);
    const ChampionGameDataSkill* FindSkill(eChampion champion, u8_t slot);

    ChampionStatsDef ResolveStats(eChampion champion);
    StatComponent BuildStat(eChampion champion, u8_t level = 1);

    f32_t ResolveSkillRange(eChampion champion, u8_t slot);
    f32_t ResolveSkillCooldown(eChampion champion, u8_t slot);
    ChampionSkillTimingDefaults ResolveSkillTiming(eChampion champion, u8_t slot, u8_t stage = 1);
    ChampionBasicAttackTimingDefaults ResolveBasicAttackTiming(eChampion champion);
    bool_t IsSkillTwoStage(eChampion champion, u8_t slot);
    f32_t ResolveSkillStageWindowSec(eChampion champion, u8_t slot);
    f32_t ResolveVisualYawOffset(eChampion champion);
}
```

새 파일: `Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.cpp`

```cpp
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"

namespace
{
    constexpr u32_t kChampionGameDataBuildHash = 0x00000001u;
}

namespace ChampionGameDataDB
{
    u32_t GetSchemaVersion()
    {
        return kChampionGameDataSchemaVersion;
    }

    u32_t GetBuildHash()
    {
        return kChampionGameDataBuildHash;
    }

    const ChampionGameData* FindChampion(eChampion)
    {
        return nullptr;
    }

    const ChampionGameDataSkill* FindSkill(eChampion, u8_t)
    {
        return nullptr;
    }

    ChampionStatsDef ResolveStats(eChampion champion)
    {
        return BuildDefaultChampionStatsDef(champion);
    }

    StatComponent BuildStat(eChampion champion, u8_t level)
    {
        return BuildDefaultChampionStat(champion, level);
    }

    f32_t ResolveSkillRange(eChampion champion, u8_t slot)
    {
        return GetDefaultChampionSkillRange(champion, slot);
    }

    f32_t ResolveSkillCooldown(eChampion champion, u8_t slot)
    {
        return GetDefaultChampionSkillCooldown(champion, slot);
    }

    ChampionSkillTimingDefaults ResolveSkillTiming(eChampion champion, u8_t slot, u8_t stage)
    {
        return GetDefaultChampionSkillTiming(champion, slot, stage);
    }

    ChampionBasicAttackTimingDefaults ResolveBasicAttackTiming(eChampion champion)
    {
        return GetDefaultChampionBasicAttackTiming(champion);
    }

    bool_t IsSkillTwoStage(eChampion champion, u8_t slot)
    {
        return IsDefaultChampionSkillTwoStage(champion, slot);
    }

    f32_t ResolveSkillStageWindowSec(eChampion champion, u8_t slot)
    {
        return GetDefaultChampionSkillStageWindowSec(champion, slot);
    }

    f32_t ResolveVisualYawOffset(eChampion champion)
    {
        return GetDefaultChampionVisualYawOffset(champion);
    }
}
```

기존 파일: `Server/Include/Server.vcxproj`

기존 코드:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Registries\ChampionStats\ChampionStatsRegistry.cpp" />
```

아래에 추가:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Registries\ChampionGameData\ChampionGameDataDB.cpp" />
```

기존 파일: `Client/Include/Client.vcxproj`

기존 코드:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Registries\ChampionStats\ChampionStatsRegistry.cpp" />
```

아래에 추가:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Registries\ChampionGameData\ChampionGameDataDB.cpp" />
```

기존 파일: `Server/Include/Server.vcxproj.filters`

기존 코드:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Registries\ChampionStats\ChampionStatsRegistry.cpp">
      <Filter>04. Shared\GameSim\Registries</Filter>
    </ClCompile>
```

아래에 추가:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Registries\ChampionGameData\ChampionGameDataDB.cpp">
      <Filter>04. Shared\GameSim\Registries</Filter>
    </ClCompile>
```

기존 파일: `Client/Include/Client.vcxproj.filters`

기존 코드:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Registries\ChampionStats\ChampionStatsRegistry.cpp">
      <Filter>07. Shared\GameSim\Registries</Filter>
    </ClCompile>
```

아래에 추가:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Registries\ChampionGameData\ChampionGameDataDB.cpp">
      <Filter>07. Shared\GameSim\Registries</Filter>
    </ClCompile>
```

2. 검증

S1 검증 기준은 "새로운 authoritative data entry point가 생겼지만 기존 gameplay 결과는 변하지 않는다"이다.

```powershell
rg -n "ChampionGameDataDB" Shared Client Server
```

확인할 것:

- `ChampionGameDataDB`는 새 파일과 vcxproj 연결 외에는 아직 기존 시스템 호출부를 바꾸지 않는다.
- `FindChampion`, `FindSkill`은 S1에서 `nullptr`을 반환한다. S1은 generated table을 도입하지 않는 세션이므로 정상이다.
- `Resolve*` 함수들은 모두 기존 `ChampionRuntimeDefaults` 함수로 위임한다.

```powershell
git diff --check
```

확인할 것:

- 공백 오류가 없어야 한다.

```powershell
msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64
msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64
```

확인할 것:

- 서버와 클라이언트 양쪽에서 새 shared cpp가 링크되어야 한다.
- S1은 호출부를 바꾸지 않으므로 런타임 gameplay behavior 차이가 없어야 한다.
