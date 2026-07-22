# 2026-07-18 상점 아이템 가격 0·구매 불가 회귀 수정 계획

```text
Session - 상점 가격 0/구매 불가 회귀 원인 확정 + Codex 진행분(프레젠테이션 투영) 위 잔여 마감
좌표: 신규 좌표 후보 (데이터 소유권 이관 시 소비자 전수 절단 검사) · 축: C7 권위와 정합성
관련: .md/plan/2026-07-17_DATA_DRIVEN_100_PERCENT_AND_MAIN_MENU_PROFILE_PLAN.md §2-27~28
```

## 1. 결정 기록

```text
① 문제·제약: kItems[34](가격 300~3500) 삭제는 100% 플랜 §2-28의 의도적 조치이나, 클라 소비자
   (LoLUIContentRegistry 2곳·ChampionTuner 1곳)에 대한 지시가 플랜에 0줄. 클라 프로세스의
   CItemRegistry는 LoadFromItemDefs 호출자가 0곳(Server 2·SimLab 1뿐)이라 영구 공백 →
   상점 34종 전부 iPrice=0·bPurchasable=false, 구매는 UI_Manager.cpp:2691 로컬 게이트에서
   차단되어 BuyItem 명령이 서버 도달 0회. 서버는 Debug/Release 모두 실가격 보유(클라 단독 회귀).
② 순진한 해법의 실패: kItems 복원은 100% 플랜 자체 검증기(":1625 ItemRegistry kItems/
   ResetToDefaults가 없다")를 깨는 이중 진실 복귀. 클라의 ServerPrivate JSON 직접 로드는
   DefinitionManifest(Item 항목 없음 = 클라 비공개) 계약 위반.
③ 메커니즘: Codex 07-18 00:53 진행분 유지 — ItemGameplayDefs.json 단일 소스를 생성기가
   서버 팩(kItemDefs)과 클라 ShopItemPresentationDefinition 테이블로 이중 투영. 본 계획은
   그 위 잔여 3건만 마감: ChampionTuner 재배선 · 서버 registry-miss 무음 reject 트레이스 · 거짓 주석 3곳.
④ 대조: LoL 실물도 표시는 클라 데이터, 검증·효과는 서버 권위 — 현 CommandExecutor 구조와 동일.
   대안(Hello로 아이템 테이블 와이어 복제)은 스키마 변경+34종×17스탯 전송 대비 이득 없음.
⑤ 대가: 프레젠테이션 테이블은 컴파일 산출물 — JSON 수정 시 생성기 재실행+클라 재빌드 필요
   (op25 핫리로드 범위 밖, RuntimeVisualDefinitionOverlay는 ShopItem 미지원). 서버·클라 가격
   일치는 팩 buildHash 정합에만 의존. 클라 CItemRegistry는 영구 공백으로 남는다 — 클라 측
   GameSim 아이템 소비자가 새로 생기는 순간 이 선택이 틀리게 된다.
```

## 2. 반영해야 하는 코드

**적용 시점 주의**: 아래 3개 파일은 전부 Codex 세션의 활성 편집 범위다(마지막 관측 07-18 00:54 코드젠). **Codex 세션 정리 후** 각 "기존 코드" 앵커를 재확인하고, 이미 동일 취지로 반영돼 있으면 해당 섹션은 건너뛴다.

### 2-1. C:/Users/user/Desktop/Winters/Client/Private/UI/ChampionTuner.cpp

Item Balance (Live) 시트가 빈 클라 CItemRegistry를 읽어 라벨이 "?"로 나온다. 프레젠테이션 조회로 재배선한다.

기존 코드:

```cpp
#include "UI/ChampionTuner.h"
```

아래에 추가:

```cpp
#include "Client/Private/Data/LoLVisualDefinitionPack.h"
```

기존 코드 (익명 네임스페이스 헬퍼, ~282줄 — 본문은 price·stats.*만 읽으므로 시그니처만 바뀐다):

```cpp
	f32_t ResolveItemFieldBaseline(const ItemDef& item, int fieldIndex)
```

아래로 교체:

```cpp
	f32_t ResolveItemFieldBaseline(
		const ClientData::ShopItemPresentationDefinition& item, int fieldIndex)
```

기존 코드 ("Item Balance (Live)" CollapsingHeader 내부, ~1491줄):

```cpp
			const ItemDef* pSheetItem =
				CItemRegistry::Instance().Find(static_cast<u16_t>(state.sheetItemId));
```

아래로 교체:

```cpp
			const ClientData::ShopItemPresentationDefinition* pSheetItem =
				ClientData::FindShopItemPresentationDefinition(
					static_cast<u16_t>(state.sheetItemId));
```

(이후 사용부 `pSheetItem->displayName`·`ResolveItemFieldBaseline(*pSheetItem, ...)`는 멤버명이 동일해 수정 불필요. 시트의 오버라이드 "적용"은 기존대로 Practice 명령 → 서버 권위 경로 그대로.)

### 2-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

registry-miss는 정상 게임플레이 거절(골드 부족·슬롯 없음)과 달리 데이터 파이프라인 절단 신호인데 현재 무음이다. 이번 회귀에서 서버가 이 경로였다면 원인 추적이 더 늦어졌다 — 에러 정책(무음 드롭 금지)대로 bounded 트레이스를 단다.

기존 코드 (HandleBuyItem, ~3214줄):

```cpp
    const ItemDef* pItem = CItemRegistry::Instance().Find(cmd.itemId);
    if (!pItem)
        return;
```

아래로 교체:

```cpp
    const ItemDef* pItem = CItemRegistry::Instance().Find(cmd.itemId);
    if (!pItem)
    {
        // 정의 팩 미적재/절단 신호 — 정상 거절(골드·슬롯)과 구분해 관측한다.
        static u32_t s_iBuyItemRegistryMissLogs = 0u;
        if (s_iBuyItemRegistryMissLogs < 8u)
        {
            ++s_iBuyItemRegistryMissLogs;
            char msg[128]{};
            sprintf_s(msg, "[CommandExecutor] BuyItem reject: itemId=%u not in CItemRegistry (registry empty or pack not loaded)\n",
                static_cast<u32_t>(cmd.itemId));
            WintersOutputAIDebugStringA(msg);
        }
        return;
    }
```

### 2-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ItemDef.h

kItems 삭제 후 거짓이 된 주석 2곳 — 이번 회귀 분석에서 실제로 오독을 유발했다.

기존 코드:

```cpp
    // 정의 팩 아이템 배열로 전체 교체 (nullptr/0 은 무시 = 기존 값 유지).
    bool_t LoadFromItemDefs(const ItemDef* items, std::size_t count);
    // 컴파일된 기본 표(kItems)로 복귀.
    void Clear();
```

아래로 교체:

```cpp
    // 정의 팩 아이템 배열로 전체 교체. nullptr/0 은 레지스트리를 비우고 false.
    bool_t LoadFromItemDefs(const ItemDef* items, std::size_t count);
    // 레지스트리를 비운다 (컴파일 기본 표는 없다 — 미적재 프로세스는 공백).
    void Clear();
```

### 2-4. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 코드 (InitializeServerSimSystems, ~442줄):

```cpp
    // 활성 정의 팩의 아이템 값으로 아이템 레지스트리 재적재 (팩 미장착 시 컴파일 기본 표 유지).
```

아래로 교체:

```cpp
    // 활성 정의 팩의 아이템 값으로 아이템 레지스트리 재적재 (팩 미장착 시 레지스트리 공백 = 구매 전면 거절).
```

## 3. 검증 — 예측을 먼저 쓴다

```text
예측:
- Codex 진행분(LoLUIContentRegistry 재배선 + 생성 테이블) 빌드만으로 상점 증상은 해소된다:
  그리드에 34종 실가격(1001=300 등) 표시, BUY 시 "[UI_Manager] BuyItem rejected" 미출력,
  서버 골드 차감이 스냅샷으로 반영. ← 이 예측이 빗나가면 재배선 외 추가 절단점이 있다는 발견.
- ChampionTuner 재배선(2-1) 전에는 Item Balance 시트 라벨 "?" 관측, 후에는 실명 표시.
- SimLab 골든 해시 불변 (SimLab은 main.cpp:217에서 자체 LoadFromItemDefs — 본 계획 무영향).
- 2-2 트레이스는 정상 플레이에서 0회 출력이 기대값. 출력되면 그 자체가 파이프라인 절단 경보.
- 서버 권위 불변: 가격 검증·골드 차감은 CommandExecutor만 수행, Bot AI는 GameCommand
  생산자이며 게임플레이 진실을 직접 변경하지 않는다.
- 게이트: 사용자 재현 시나리오(Release 클라 3개 동시) 재실행 — 게이트 없으면 그게 발견이다: 있음.

검증 명령:
- msbuild Engine.vcxproj + Client.vcxproj (Debug/Release, /m:1) 후 인게임 상점 구매 1회 이상.
- (생성기 재실행 불필요 — 00:54에 이미 재생성됨. 이후 ItemGameplayDefs.json을 만졌다면
  python Tools/LoLData/Build-LoLDefinitionPack.py 재실행.)

미검증:
- Codex 재배선의 인게임 동작 (00:53 편집 직후 빌드·실행이 아직 없음 — 본 계획 최우선 검증 대상).

확인 필요:
- 적용 직전 세 파일의 앵커 재확인 (Codex 세션이 같은 취지 수정을 이미 넣었을 수 있음).
- ChampionTuner의 ItemDef.h 직접 include 여부 — 2-1 적용으로 CItemRegistry 사용이 사라진
  뒤 불용 include가 생기면 그때만 제거.
```
