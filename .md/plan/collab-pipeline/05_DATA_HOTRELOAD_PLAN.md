Session - dev 빌드에서 데이터 파일 변경을 감지해 재시작 없이 live 엔티티에 재적용하는 핫리로드 슬라이스를 만든다(서버 권위 유지).

1. 반영해야 하는 코드

설계 분기(중요):
- 런타임은 champions.json을 직접 파싱하지 않고 codegen된 generated table을 읽는다. 따라서 코드(generated)는 핫리로드 불가다.
- 데이터(수치)만 dev-only runtime overlay로 핫리로드한다. release 빌드에서는 전부 비활성.
- 흐름: 파일 watcher -> dev override JSON 파싱 -> dev override 설정 -> live StatComponent 재빌드. 서버 권위는 그대로(여전히 서버가 truth).

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Spawn/DataHotReload.h

JSON을 모르는 generic 파일 mtime watcher. 파싱/적용은 콜백이 담당(의존성 격리).

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"

#include <functional>
#include <string>
#include <vector>

// dev 전용 파일 변경 감지기. JSON/도메인은 모른다. 변경 시 콜백만 호출.
class CDataHotReload
{
public:
    using OnChange = std::function<void()>;

    void Watch(const std::wstring& path, OnChange onChange);
    void Poll();   // 서버 tick에서 dev 빌드 한정 호출

private:
    struct WatchedFile
    {
        std::wstring path;
        u64_t lastWriteTicks = 0ull;
        OnChange onChange;
    };
    std::vector<WatchedFile> m_watched;
};
```

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Spawn/DataHotReload.cpp

Win32 `GetFileAttributesExW`로 mtime만 비교(새 외부 의존성 없음).

새 파일:

```cpp
#include "Shared/GameSim/Spawn/DataHotReload.h"

#include <Windows.h>

namespace
{
    u64_t QueryWriteTicks(const std::wstring& path)
    {
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data))
            return 0ull;
        ULARGE_INTEGER t{};
        t.LowPart = data.ftLastWriteTime.dwLowDateTime;
        t.HighPart = data.ftLastWriteTime.dwHighDateTime;
        return static_cast<u64_t>(t.QuadPart);
    }
}

void CDataHotReload::Watch(const std::wstring& path, OnChange onChange)
{
    WatchedFile wf{};
    wf.path = path;
    wf.lastWriteTicks = QueryWriteTicks(path);
    wf.onChange = std::move(onChange);
    m_watched.push_back(std::move(wf));
}

void CDataHotReload::Poll()
{
    for (WatchedFile& wf : m_watched)
    {
        const u64_t now = QueryWriteTicks(wf.path);
        if (now != 0ull && now != wf.lastWriteTicks)
        {
            wf.lastWriteTicks = now;
            if (wf.onChange)
                wf.onChange();
        }
    }
}
```

확인 필요:
- Shared/GameSim TU가 `<Windows.h>`를 include해도 무방한지 확인(determinism 규칙상 gameplay 로직 아닌 dev 도구이므로 허용 범위). 문제가 되면 이 파일만 Server로 옮긴다.

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Spawn/DevStatOverride.h

dev-only 챔피언 stat override. 16 시리즈의 `ChampionGameDataDB`를 건드리지 않고 별도 overlay로 둔다.

새 파일:

```cpp
#pragma once

#include "Shared/GameSim/Definitions/ChampionTypes.h"
#include "Shared/GameSim/Definitions/ChampionStatsDef.h"

namespace DevStatOverride
{
#if defined(_DEBUG)
    // 있으면 우선, 없으면 nullptr. dev 빌드 전용.
    void SetOverride(eChampion champion, const ChampionStatsDef& def);
    const ChampionStatsDef* Find(eChampion champion);
    void Clear();
#endif
}
```

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Spawn/DevStatOverride.cpp

새 파일:

```cpp
#include "Shared/GameSim/Spawn/DevStatOverride.h"

#if defined(_DEBUG)
#include <unordered_map>

namespace
{
    std::unordered_map<int, ChampionStatsDef> s_overrides;
}

namespace DevStatOverride
{
    void SetOverride(eChampion champion, const ChampionStatsDef& def)
    {
        s_overrides[static_cast<int>(champion)] = def;
    }

    const ChampionStatsDef* Find(eChampion champion)
    {
        auto it = s_overrides.find(static_cast<int>(champion));
        return it == s_overrides.end() ? nullptr : &it->second;
    }

    void Clear()
    {
        s_overrides.clear();
    }
}
#endif
```

확인 필요:
- `eChampion`/`ChampionStatsDef` 헤더 경로 확인.

1-5. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomTick.cpp

dev 빌드 한정으로 watcher를 polling하고, 변경 시 dev override를 채운 뒤 live StatComponent를 재빌드한다.

서버 tick 본문(예: `CGameRoom::Tick` 내부)에 아래를 추가한다(정확한 anchor는 구현 직전 확인):

```cpp
#if defined(_DEBUG)
    m_dataHotReload.Poll();
#endif
```

확인 필요:
- `m_dataHotReload`는 `CGameRoom`에 추가할 `CDataHotReload` 멤버다. 멤버 추가 위치(`GameRoomInternal.h`)와 초기 `Watch(...)` 등록 위치(룸 초기화)는 구현 직전 확정한다.
- 변경 콜백은: dev override JSON(예: `Data/LoL/Test/DevStatOverride.json`)을 파싱 -> `DevStatOverride::SetOverride(champion, def)` -> `m_world.ForEach<StatComponent>(... CStatSystem::BuildBaseStats로 재빌드 ...)`.
- JSON 파싱은 기존 nlohmann(`Client/Public/Network/Backend/json.hpp`)을 Server에서 쓸 수 있는지 확인하고, 불가하면 dev override를 최소 key 파서로 처리한다.

1-6. C:/Users/tnest/Desktop/Winters/Data/LoL/Test/DevStatOverride.json

핫리로드 demo용 dev override 파일(Test 소유, production 데이터와 분리).

새 파일:

```json
{
  "schemaVersion": 1,
  "overrides": [
    { "champion": "IRELIA", "baseHp": 600.0, "baseAd": 65.0, "baseMoveSpeed": 5.0 }
  ]
}
```

2. 검증

미검증:
- 빌드 미검증
- dev에서 파일 저장 시 live 챔피언 stat이 재적용되는지 미검증

검증 명령:
- git diff --check
- & "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" .\Server\Include\Server.vcxproj /m /p:Configuration=Debug /p:Platform=x64

수동 확인 (dev 빌드):
- F5 중 `Data/LoL/Test/DevStatOverride.json`의 baseAd 변경 -> 재시작 없이 해당 챔피언 stat이 바뀌는지.
- release 빌드에서 override/watcher 경로가 컴파일·실행되지 않는지(`#if defined(_DEBUG)`).

확인 필요:
- 새 `DataHotReload.*`, `DevStatOverride.*` 4개 파일이 Server(및 Shared) 빌드 프로젝트에 포함되는지 확인.
- override는 dev 전용이며 server 권위를 대체하지 않는다(여전히 서버가 stat을 적용, override는 그 입력만 바꾼다).

전제:
- 본 세션은 01(데이터 def/resolver) 이후가 자연스럽다. S4(인스펙터)와 독립적으로 진행 가능.
