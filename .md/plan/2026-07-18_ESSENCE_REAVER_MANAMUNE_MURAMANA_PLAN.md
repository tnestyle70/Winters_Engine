# 2026-07-18 정수 약탈자·마나무네·무라마나 검증 계획서

```text
Session - 정수 약탈자 Spellblade 마나 회복과 마나무네 Manaflow·무라마나 변환의 서버 권위 연결을 현재 코드 증거와 회귀 테스트로 닫는다.
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성 · C8 검증이 병목
관련: 2026-07-18_ITEM_ACTIVE_INVENTORY_INTERACTION_RESULT.md, 2026-07-18_IRELIA_E_UNIT_HIT_MARK_Q_RESET_RECALL_6S_PLAN.md
```

## 1. 결정 기록

```text
① 문제·제약: 아이템 가격/스탯 연결과 별개로 정수 약탈자의 스킬 후 다음 공격(이즈리얼 Q 온힛 포함), 마나무네 4충전/8초 회복/챔피언 2배/360 상한, 무라마나 변환·최대마나 기반 AD가 실제 DamageRequest와 Stat 재계산에 연결돼야 한다.
② 순진한 해법의 실패: Client UI나 이즈리얼 전용 hook에서 마나를 직접 바꾸면 일반 BA·다른 온힛 스킬·서버 결정성이 갈라진다. 무라마나를 별도 구매로만 처리하면 동일 inventory slot/runtime 상태가 끊긴다.
③ 메커니즘: generated ItemDef가 수치 권위이고, ItemEffectSystem이 accepted ability로 Spellblade를 arm한 뒤 landed OnHit에서 1회 소비한다. Manaflow는 landed attack/ability에서 충전과 bonusMana를 갱신하고 360에서 같은 slot을 3042로 바꾼다. StatSystem은 runtime bonusMana와 maxManaBonusAdRatio를 재계산한다.
④ 대조: pinned Data Dragon 16.14.1의 Manamune은 500 Mana, 4 charges, 8s, champion 2배, 360 transform이고 Muramana는 1000 Mana·2% max Mana AD다. Essence Reaver는 ability 후 next attack이며 Ezreal Q는 OnHit skill이라 같은 소비 경로다.
⑤ 대가·예산: 30% 천장 일은 아이템 설명/수치 acceptance 캡처, 70% 바닥 일은 SimLab 결정성·빌드다. 이미 반영된 구현을 중복 편집하지 않고 회귀가 있으면 최소 수정만 별도 RESULT에 기록한다.
```

## 2. 반영 코드

현재 구현은 이미 반영돼 있으므로 이 계획에서 새 gameplay 코드를 추가하지 않는다. 검토 대상과 현재 권위는 다음과 같다.

### 2-1. `Data/LoL/ServerPrivate/Gameplay/ItemGameplayDefs.json`

```text
3508 Essence Reaver: spellblade cooldown/baseAd/crit/manaRestore
3004 Manamune: flatMana 500, manaflow 8s/4/3/2x/360 -> 3042, maxManaBonusAdRatio 0.02
3042 Muramana: purchasable false, flatMana 1000, maxManaBonusAdRatio 0.02
```

### 2-2. `Shared/GameSim/Systems/Item/ItemEffectSystem.cpp`

현재 exact anchor:

```cpp
void CItemEffectSystem::OnAbilityCastAccepted(
```

```cpp
void CItemEffectSystem::PrepareOnHitDamage(
```

```cpp
void CItemEffectSystem::OnDamageLanded(
```

이 세 경로의 arm -> hit resolution -> landed consume/stack/transform 순서를 유지한다.

### 2-3. `Shared/GameSim/Systems/Stat/StatSystem.cpp`

현재 exact anchor:

```cpp
            if (pItemRuntime && pItemRuntime->slots[i].itemId == pItem->itemId)
                stat.manaMax += static_cast<f32_t>(pItemRuntime->slots[i].bonusMana);
```

```cpp
            if (pItem && pItem->maxManaBonusAdRatio > 0.f)
                stat.bonusAd += stat.manaMax * pItem->maxManaBonusAdRatio;
```

### 2-4. `Tools/SimLab/main.cpp`

현재 exact anchor:

```cpp
    bool_t RunManaItemPassivesProbe()
```

이 probe는 unarmed Ezreal Q 0회복, armed Q/BA 1회복, Manamune 첫 4회 챔피언 hit +24 mana, 360 누적 후 3042 변환·1000 mana·2% max mana AD·runtime slot sync를 검사한다.

## 3. 검증

```text
예측:
- `--mana-items-only`는 `Essence Reaver BA/Q mana + Manamune 360 -> Muramana` PASS를 출력한다.
- 생성팩 --check와 Server/SimLab Debug 빌드가 통과하며 item 값은 pinned 16.14.1과 일치한다.
- 일반 BA가 ability cast 없이 Spellblade를 임의 발동하지 않고, ability로 arm된 BA와 Ezreal Q만 동일 cooldown에서 1회 소비한다.

검증 명령:
python Tools/LoLData/Build-LoLDefinitionPack.py --check
msbuild Server/Include/Server.vcxproj Debug x64 /m:1
msbuild Tools/SimLab/SimLab.vcxproj Debug x64 /m:1
Tools/Bin/Debug/SimLab.exe --mana-items-only
git diff --check -- Shared/GameSim/Definitions/ItemDef.h Shared/GameSim/Components/ItemRuntimeComponent.h Shared/GameSim/Systems/Item/ItemEffectSystem.cpp Shared/GameSim/Systems/Stat/StatSystem.cpp Data/LoL/ServerPrivate/Gameplay/ItemGameplayDefs.json Tools/SimLab/main.cpp .md/plan/2026-07-18_ESSENCE_REAVER_MANAMUNE_MURAMANA_PLAN.md

미검증/CONFIRM_NEEDED:
- 정상 F5에서 실제 HUD mana 숫자와 inventory icon이 transform tick에 같은 frame으로 갱신되는지는 수동 확인이 필요하다.
```
