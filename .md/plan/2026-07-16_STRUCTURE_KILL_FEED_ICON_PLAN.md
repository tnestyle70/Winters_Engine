Session - Team Specific Structure Kill Feed Icons

## 1. 반영해야 하는 코드

### 본질 원인

서버 kill feed event는 `Turret/Inhibitor`와 `targetTeam`을 이미 보냈지만 Client가 `UI_Push_KillFeedBanner`로 넘길 때 target team을 버렸고 Engine UI는 object kind를 텍스트 `Structure/Objective`로만 렌더했다.

### 기존 파일: `Client/Private/Network/Client/EventApplier.cpp`

기존 kill feed UI 호출을 아래로 교체한다.

```cpp
CGameInstance::Get()->UI_Push_KillFeedBanner(
    ev->sourceChampion(),
    ev->targetChampion(),
    objectKind,
    ev->targetTeam(),
    bSourceAlly,
    pMessage);
```

### 기존 파일: `Engine/Include/GameInstance.h`, `Engine/Private/GameInstance.cpp`, `Engine/Public/Manager/UI/UI_Manager.h`

`iTargetTeam`을 호출 체인 끝까지 추가하고 banner state에 저장한다.

```cpp
void UI_Push_KillFeedBanner(u8_t iSourceActorId, u8_t iTargetActorId,
    u8_t iObjectKind, u8_t iTargetTeam,
    bool_t bSourceAlly, const char* pMessage);
```

### 기존 파일: `Engine/Private/Manager/UI/UI_Manager.cpp`

object kind가 turret이면 `minimap_tower_{blue|red}.png`, inhibitor이면 `minimap_inhibitor_{blue|red}.png`를 lazy load하고 target badge에 그린다.

```cpp
if (banner.iObjectKind == kKillFeedObjectStructure ||
    banner.iObjectKind == kKillFeedObjectObjective)
{
    if (void* pObjectIcon = FindOrLoadKillFeedObjectIcon(
            banner.iObjectKind, banner.iTargetTeam))
    {
        DrawKillFeedCircleImage(pDraw, targetCenter, kRadius,
            pObjectIcon, tintColor,
            banner.bSourceAlly ? enemyColor : allyColor);
        continue;
    }
}
```

로드 실패 fallback 문구도 각각 `Tower`, `Inhibitor`로 교체한다.

## 2. 검증

```powershell
Test-Path Client/Bin/Resource/Texture/UI/InGameUI/minimap_tower_blue.png
Test-Path Client/Bin/Resource/Texture/UI/InGameUI/minimap_tower_red.png
Test-Path Client/Bin/Resource/Texture/UI/InGameUI/minimap_inhibitor_blue.png
Test-Path Client/Bin/Resource/Texture/UI/InGameUI/minimap_inhibitor_red.png
Engine/UpdateLib.bat
msbuild Client/Include/Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64
```

인게임 gate는 blue/red 타워 파괴와 blue/red 억제기 파괴 각각에서 대상 진영 아이콘이 맞고 `Structure` 문자열이 표시되지 않는 것이다.
