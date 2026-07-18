Session - Minion Kill Feed And Critical Atlas Indicator
좌표: 없음 · 축 C7 권위와 표시의 일치, C8 검증 병목
관련: 2026-07-16_STRUCTURE_KILL_FEED_ICON_PLAN.md, 2026-07-17_FIORA_AUTHORITATIVE_VITAL_RIPOSTE_BLADEWORK_REMEDIATION_PLAN.md, 2026-07-17_ZED_PASSIVE_E_R_AUTHORITATIVE_FX_PLAN.md

## 1. 결정 기록

- 문제·제약: 서버 kill-feed는 비챔피언 source가 챔피언을 처치하면 이벤트를 버리고, 구조물 처치 이벤트에서도 `sourceChampion=NONE`을 챔피언 portrait로 그려 빈 원이 된다. `offscreenping_atlas.png`의 미니언 alpha 실측 범위는 1024 atlas 기준 `x=972..1012`, `y=266..313`이다.
- 전진·실패: 미니언을 가짜 champion id로 치환하면 wire 계약은 작아지지만 roster id와 UI asset lookup의 의미가 깨진다. source net entity를 클라이언트 presentation에서 `MinionComponent/MinionStateComponent`로 확인하고 기존 atlas SRV를 재사용한다.
- 메커니즘: 서버는 champion 또는 minion source가 Champion/Turret/Inhibitor를 처치한 경우만 kill-feed를 발행한다. 클라이언트는 source net entity가 minion이면 2px 여백 UV `970,264..1014,315`를 52px 이내로 확대해 표시한다.
- 문제·제약: `DamageEvent.bWasCrit`는 이미 서버 결과에서 직렬화되지만 UI는 글자 크기만 바꾼다. Fiora E 1타와 Zed 약자멸시 추가 피해는 실제 crit가 아니므로 `bWasCrit`를 거짓으로 바꾸면 안 된다.
- 메커니즘·대가: `DamageFlag_ShowCriticalIndicator`를 서버 damage request presentation flag로 추가하고 `DamageEvent.flags`에 직렬화한다. 실제 crit는 `bWasCrit`, Fiora E 1·2타와 Zed passive bonus는 flag로 아이콘만 요청한다. wire field 2바이트와 generated schema diff가 추가되지만 gameplay crit truth는 보존된다.
- atlas 근거: `statspanel_atlas.png`는 512x512이고 치명타 child cell은 `232,488..256,512` 24x24다. 새 PNG를 만들지 않고 이 UV를 숫자 왼쪽에 표시한다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/DamageTypes.h

기존 코드:

```cpp
enum eDamageFlags : uint32_t
{
    DamageFlag_None = 0,
    DamageFlag_CanCrit = 1u << 0,
    DamageFlag_CanLifesteal = 1u << 1,
    DamageFlag_OnHit = 1u << 2,
};
```

아래로 교체:

```cpp
enum eDamageFlags : uint32_t
{
    DamageFlag_None = 0,
    DamageFlag_CanCrit = 1u << 0,
    DamageFlag_CanLifesteal = 1u << 1,
    DamageFlag_OnHit = 1u << 2,
    DamageFlag_ShowCriticalIndicator = 1u << 3,
};
```

### 2-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Fiora/FioraGameSim.cpp

`FioraGameSim::PrepareDamageRequest`에서 `bladeworkPendingHitOrdinal` 검사 바로 아래에 추가:

```cpp
        if (state.bladeworkPendingHitOrdinal == 0u)
            return false;

        request.flags |= DamageFlag_ShowCriticalIndicator;
        if (state.bladeworkPendingHitOrdinal != 2u)
            return false;
```

기존의 `if (state.bladeworkPendingHitOrdinal != 2u) return false;`는 위 블록으로 교체한다. 1타는 아이콘만, 2타는 기존 강제 crit 계산과 아이콘을 모두 유지한다.

### 2-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Zed/ZedGameSim.cpp

`ZedGameSim::EnqueuePassiveBasicAttackDamage`의 `request.targetMissingHpRatioOverride = ratio;` 아래에 추가:

```cpp
        request.flags = DamageFlag_ShowCriticalIndicator;
```

약자멸시 bonus request만 표시되며 같은 공격의 일반 BA damage request에는 이 flag가 붙지 않는다.

### 2-4. C:/Users/user/Desktop/Winters/Shared/Schemas/Event.fbs

기존 코드:

```text
table DamageEvent {
    sourceNet:uint;
    targetNet:uint;
    amount:float;
    type:ubyte;
    bWasCrit:bool;
    bKilled:bool;
    skillId:ushort;
}
```

아래로 교체:

```text
table DamageEvent {
    sourceNet:uint;
    targetNet:uint;
    amount:float;
    type:ubyte;
    bWasCrit:bool;
    bKilled:bool;
    skillId:ushort;
    flags:ushort;
}
```

`Shared/Schemas/run_codegen.bat`으로 `Shared/Schemas/Generated/cpp/Event_generated.h`, `Shared/Schemas/Generated/go/Shared/Schema/DamageEvent.go`를 재생성한다.

### 2-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.cpp

기존 코드:

```cpp
                event.bWasCrit,
                event.bKilled,
                event.skillId);
```

아래로 교체:

```cpp
                event.bWasCrit,
                event.bKilled,
                event.skillId,
                event.flags);
```

### 2-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp

기존 source gate:

```cpp
        const bool_t bChampionSource =
            world.HasComponent<ChampionComponent>(request.source);
        const bool_t bStructureKill =
            objectKind == eKillFeedObjectKind::Turret ||
            objectKind == eKillFeedObjectKind::Inhibitor;
        if (!bChampionSource && !bStructureKill)
            return;
```

아래로 교체:

```cpp
        const bool_t bChampionSource =
            world.HasComponent<ChampionComponent>(request.source);
        const bool_t bMinionSource =
            world.HasComponent<MinionComponent>(request.source) ||
            world.HasComponent<MinionStateComponent>(request.source);
        const bool_t bSupportedMinionTarget =
            objectKind == eKillFeedObjectKind::Champion ||
            objectKind == eKillFeedObjectKind::Turret ||
            objectKind == eKillFeedObjectKind::Inhibitor;
        if (!bChampionSource && !(bMinionSource && bSupportedMinionTarget))
            return;
```

Damage event는 이미 `event.flags = request.flags & 0xffffu`를 수행하므로 추가 truth owner는 만들지 않는다.

### 2-7. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

`ApplyDamage`의 UI 호출을 아래로 교체:

```cpp
    const bool_t bShowCriticalIndicator =
        ev->bWasCrit() ||
        (ev->flags() & DamageFlag_ShowCriticalIndicator) != 0u;
    CGameInstance::Get()->UI_Push_DamageNumber(
        damageTextPos,
        ev->amount(),
        ev->type(),
        ev->bWasCrit(),
        ev->bKilled(),
        bShowCriticalIndicator);
```

`ApplyKillFeed`의 team 판정 아래에 추가:

```cpp
    const EntityID sourceEntity = ev->sourceNet() != NULL_NET_ENTITY
        ? ResolveLiveEntity(world, entityMap, ev->sourceNet())
        : NULL_ENTITY;
    const bool_t bSourceMinion = IsMinionEntity(world, sourceEntity);
```

`UI_Push_KillFeedBanner` 호출의 `bSourceAlly` 뒤에 `bSourceMinion`을 전달한다.

### 2-8. C:/Users/user/Desktop/Winters/Engine/Include/GameInstance.h

기존 API 두 개를 아래 시그니처로 교체:

```cpp
    void UI_Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
        u8_t iDamageType, bool_t bWasCrit, bool_t bKilled,
        bool_t bShowCriticalIndicator = false);
    void UI_Push_KillFeedBanner(u8_t iSourceActorId, u8_t iTargetActorId,
        u8_t iObjectKind, u8_t iTargetTeam,
        bool_t bSourceAlly, bool_t bSourceMinion, const char* pMessage);
```

### 2-9. C:/Users/user/Desktop/Winters/Engine/Private/GameInstance.cpp

동일 API 정의와 `CUI_Manager` 전달 호출에 `bShowCriticalIndicator`, `bSourceMinion`을 끝까지 전달한다.

### 2-10. C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h

`DamageFloater`에 아래 field를 `bWasCrit` 다음에 추가:

```cpp
        bool_t bShowCriticalIndicator = false;
```

`KillFeedBanner`에 아래 field를 `bSourceAlly` 다음에 추가:

```cpp
        bool_t bSourceMinion = false;
```

public method 인자와 private state에 아래 변경을 반영:

```cpp
    void Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
        u8_t iDamageType, bool_t bWasCrit, bool_t bKilled,
        bool_t bShowCriticalIndicator = false);
    void Push_KillFeedBanner(u8_t iSourceActorContentId, u8_t iTargetActorContentId,
        u8_t iObjectKind, u8_t iTargetTeam,
        bool_t bSourceAlly, bool_t bSourceMinion, const char* pMessage);

    void* m_pSRV_StatsPanelAtlas = nullptr;
```

### 2-11. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

atlas path 상수에 추가:

```cpp
    constexpr const wchar_t* kPathStatsPanelAtlas =
        L"Resource/Texture/UI/HUD/statspanel_atlas.png";
```

초기화에서 한 번 로드하고 `Shutdown`에서 `ReleaseSRV(m_pSRV_StatsPanelAtlas)` 한다. `Push_DamageNumber`와 `Push_KillFeedBanner`는 새 bool을 floater/banner에 저장한다.

`DrawDamageFloaters`에서 `floater.bShowCriticalIndicator`일 때 512 atlas의 UV `232,488..256,512`를 damage text 왼쪽에 배치한다. text+icon 전체 폭을 중앙 정렬하며 실제 `bWasCrit`만 기존 26px crit font를 사용한다.

`DrawKillFeedBanners` source draw를 아래 분기로 바꾼다:

```cpp
        if (banner.bSourceMinion && m_pSRV_OffscreenPingAtlas)
        {
            pDraw->AddCircleFilled(sourceCenter, kRadius,
                UI_ColorWithAlpha(24, 28, 34, 0.92f * alpha), 48);
            UI_DrawPingAtlasSpriteCentered(
                pDraw,
                m_pSRV_OffscreenPingAtlas,
                UIPingAtlasSprite{ 970.f, 264.f, 1014.f, 315.f },
                sourceCenter,
                kRadius * 1.86f,
                tintColor);
            pDraw->AddCircle(sourceCenter, kRadius,
                banner.bSourceAlly ? allyColor : enemyColor, 48, 2.5f);
        }
        else
        {
            DrawKillFeedCircleImage(/* 기존 champion portrait 인자 */);
        }
```

### 2-12. C:/Users/user/Desktop/Winters/EngineSDK/inc/GameInstance.h

Engine public header 동기화 산출물이므로 `Engine/Include/GameInstance.h`와 동일 시그니처를 반영한다. 이번 세션은 사용자가 실행 중인 클라/서버를 건드리지 않기 위해 `Engine/UpdateLib.bat`은 실행하지 않는다.

## 3. 검증

예측:

- 미니언이 챔피언 또는 포탑/억제기를 마지막 타격하면 서버 kill-feed event가 1회 생성되고 source 쪽에 빈 portrait 대신 크게 자른 청록 미니언 glyph가 표시된다.
- 일반 crit, Infinity Edge 등 item stat로 확률 roll에 성공한 BA는 기존 `bWasCrit=true`만으로 치명타 아이콘이 표시된다.
- Fiora E 1타는 일반 크기 숫자+아이콘, E 2타는 기존 crit 크기 숫자+아이콘으로 보이며 damage 수치는 바뀌지 않는다.
- Zed 약자멸시 bonus damage floater에만 아이콘이 붙고 동일 BA가 실제 crit이면 기본 BA floater도 서버 `bWasCrit`에 따라 아이콘이 붙는다.
- atlas load 실패 시 숫자와 kill-feed 원형 fallback은 유지되며 게임플레이 판정은 영향받지 않는다.

검증 명령:

```powershell
Shared\Schemas\run_codegen.bat
rg -n "DamageFlag_ShowCriticalIndicator|bShowCriticalIndicator|bSourceMinion|970.f, 264.f|232.f, 488.f" Shared Client Engine EngineSDK
git diff --check -- Shared/GameSim/Definitions/DamageTypes.h Shared/GameSim/Champions/Fiora/FioraGameSim.cpp Shared/GameSim/Champions/Zed/ZedGameSim.cpp Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.cpp Shared/Schemas/Event.fbs Shared/Schemas/Generated/cpp/Event_generated.h Shared/Schemas/Generated/go/Shared/Schema/DamageEvent.go Client/Private/Network/Client/EventApplier.cpp Engine/Include/GameInstance.h EngineSDK/inc/GameInstance.h Engine/Private/GameInstance.cpp Engine/Public/Manager/UI/UI_Manager.h Engine/Private/Manager/UI/UI_Manager.cpp
```

미검증:

- 사용자 요청에 따라 Client/Server/Engine 빌드와 인게임 눈 검증은 이번 세션에서 실행하지 않는다.
- 실제 16:9 HUD에서 아이콘 간격과 겹침은 사용자 인게임 캡처로 확인한다.

확인 필요:

- 빌드 시 schema codegen 이후 Server와 Client Debug x64를 함께 검증해야 한다.
