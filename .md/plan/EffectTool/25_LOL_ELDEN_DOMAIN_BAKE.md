# 25. LoL / Elden 도메인 분리 박제 (P-11 회피, InitDesc 주입 + Bootstrap + manifest 매핑)

작성일: 2026-05-07
재박제일: 2026-05-07 (CLAUDE.md §8.2 본문 룰 — stub 0 / 라인 번호 / 추상 0)
권위: 본 25 = 17 마스터 §15 부속 8번. EFX-6 합격 후 진입.
의존: 부속 19 (`FxSystemInitDesc`), 부속 18 (manifest dump), 17 §0.1.4.

목적:
- LoL/Elden 도메인 InitDesc 주입 박제 (Engine constexpr 0 강제)
- LoLFxBootstrap / EldenFxBootstrap 본문 풀
- 11 LoL 챔프 hookId manifest + 5 Elden 보스 manifest

박제 진입 전 8 단계 관문:
- 관문 A: §1 4 항목, TBD 0
- 관문 B: 헤더 + cpp 동시
- 관문 C: 두 도메인 한 번에
- 관문 D: Bootstrap = component / asset 등록만
- 관문 E: mask 미사용
- 관문 F: PLAN_AUTHORING_PITFALLS P-11 사례 차용
- 관문 G: Bootstrap = Scene 진입 시 1 회
- 관문 H: Bootstrap = static, 인스턴스 0

---

## §0.1 5/7 codex 본문 룰 적용 (재박제)

본 25 v1 의 stub 1 위치 본문화:

```txt
1. Champion 별 전체 hook (BA + Q/W/E/R + passive)
   v1 = "Yasuo 대표만 본문. 다른 챔프 hook = EFX-0 코드 작업 시점에 추가"
   v2 = 11 챔프 모두 hook list 본문 (Champion 별 5~8 hook 평균. Yasuo 풀 + 10 챔프 핵심 hook)
```

---

## §1 사전 결정 (TBD 0)

| 결정 항목 | 결정값 | 근거 |
|---|---|---|
| 도메인 분리 위치 | `Client/Public/FX/Domains/{LoL,Elden}/` | 17 §0.1.4 |
| InitDesc 주입 | LoL/Elden Bootstrap.OnStart 가 `CGameInstance::Set_DomainFxDesc(desc)` 호출. SpawnRequestSystem 가 desc 사용 | P-11 회피 |
| Manifest 매핑 | LoL = 11 챔프 우선 hook + Champion 별 5~8 hook. Elden = 보스 5 + weapon 3 + magic 12 | EFX-0 합격 |
| FxBudget 분리 | LoL = 5ms / 4096 / 16. Elden = 8ms / 16384 / 64 | 17 §2.3 |

---

## §2 신규 파일 트리

```txt
Client/Public/FX/Domains/
  LoL/
    LoLFxBudget.h
    LoLFxBootstrap.h
    LoLFxManifest.h
  Elden/
    EldenFxBudget.h
    EldenFxBootstrap.h
    EldenFxManifest.h

Client/Private/FX/Domains/
  LoL/
    LoLFxBootstrap.cpp
    LoLFxManifest.cpp
  Elden/
    EldenFxBootstrap.cpp
    EldenFxManifest.cpp
```

---

## §3 헤더 박제 (v1 그대로 — 변경 0)

v1 §3 의 6 헤더 (`LoLFxBudget.h / EldenFxBudget.h / LoLFxManifest.h / EldenFxManifest.h / LoLFxBootstrap.h / EldenFxBootstrap.h`) 모두 본문 풀. v1 박제 룰 위반 0. re-quote 생략.

---

## §4 cpp 본문 박제 (전문, L1-, stub 0)

### §4.1 `LoLFxManifest.cpp` (L1-L160, 11 챔프 hook 본문)

```cpp
#include "FX/Domains/LoL/LoLFxManifest.h"

namespace Winters::LoL
{
    namespace
    {
        const std::vector<FxHookManifestEntry> g_PriorityHooks = {
            { L"Annie",   L"Q_Fireball",      L"Resource/FX/Annie/Q_Fireball.wfx" },
            { L"Ashe",    L"Q_VolleyOpening", L"Resource/FX/Ashe/Q_VolleyOpening.wfx" },
            { L"Fiora",   L"E_Stab",          L"Resource/FX/Fiora/E_Stab.wfx" },
            { L"Garen",   L"E_JudgmentSpin",  L"Resource/FX/Garen/E_JudgmentSpin.wfx" },
            { L"Irelia",  L"Q_Stab",          L"Resource/FX/Irelia/Q_Stab.wfx" },
            { L"Jax",     L"Q_LeapStrike",    L"Resource/FX/Jax/Q_LeapStrike.wfx" },
            { L"Kalista", L"Q_PiercingSpear", L"Resource/FX/Kalista/Q_PiercingSpear.wfx" },
            { L"Riven",   L"Q_BrokenWings",   L"Resource/FX/Riven/Q_BrokenWings.wfx" },
            { L"Yasuo",   L"Q_Straight",      L"Resource/FX/Yasuo/Q_Straight.wfx" },
            { L"Yone",    L"Q_MortalSteel",   L"Resource/FX/Yone/Q_MortalSteel.wfx" },
            { L"Zed",     L"Q_RazorShuriken", L"Resource/FX/Zed/Q_RazorShuriken.wfx" },
        };

        // 11 챔프 전체 hook (BA + Q/W/E/R + passive). Yasuo 풀, 10 챔프 핵심 hook.
        const std::vector<FxHookManifestEntry> g_AnnieHooks = {
            { L"Annie", L"BA",          L"Resource/FX/Annie/BA.wfx" },
            { L"Annie", L"Q_Fireball",  L"Resource/FX/Annie/Q_Fireball.wfx" },
            { L"Annie", L"W_Incinerate",L"Resource/FX/Annie/W_Incinerate.wfx" },
            { L"Annie", L"E_MoltenShield",L"Resource/FX/Annie/E_MoltenShield.wfx" },
            { L"Annie", L"R_SummonTibbers",L"Resource/FX/Annie/R_SummonTibbers.wfx" },
        };
        const std::vector<FxHookManifestEntry> g_AsheHooks = {
            { L"Ashe", L"BA",                L"Resource/FX/Ashe/BA.wfx" },
            { L"Ashe", L"Q_VolleyOpening",   L"Resource/FX/Ashe/Q_VolleyOpening.wfx" },
            { L"Ashe", L"W_Volley",          L"Resource/FX/Ashe/W_Volley.wfx" },
            { L"Ashe", L"E_HawkShot",        L"Resource/FX/Ashe/E_HawkShot.wfx" },
            { L"Ashe", L"R_EnchantedCrystalArrow", L"Resource/FX/Ashe/R_CrystalArrow.wfx" },
        };
        const std::vector<FxHookManifestEntry> g_FioraHooks = {
            { L"Fiora", L"BA",        L"Resource/FX/Fiora/BA.wfx" },
            { L"Fiora", L"Q_Lunge",   L"Resource/FX/Fiora/Q_Lunge.wfx" },
            { L"Fiora", L"W_Riposte", L"Resource/FX/Fiora/W_Riposte.wfx" },
            { L"Fiora", L"E_Stab",    L"Resource/FX/Fiora/E_Stab.wfx" },
            { L"Fiora", L"R_GrandChallenge", L"Resource/FX/Fiora/R_GrandChallenge.wfx" },
        };
        const std::vector<FxHookManifestEntry> g_GarenHooks = {
            { L"Garen", L"BA",                L"Resource/FX/Garen/BA.wfx" },
            { L"Garen", L"Q_DecisiveStrike",  L"Resource/FX/Garen/Q_DecisiveStrike.wfx" },
            { L"Garen", L"W_Courage",         L"Resource/FX/Garen/W_Courage.wfx" },
            { L"Garen", L"E_JudgmentSpin",    L"Resource/FX/Garen/E_JudgmentSpin.wfx" },
            { L"Garen", L"R_DemacianJustice", L"Resource/FX/Garen/R_DemacianJustice.wfx" },
        };
        const std::vector<FxHookManifestEntry> g_IreliaHooks = {
            { L"Irelia", L"BA",            L"Resource/FX/Irelia/BA.wfx" },
            { L"Irelia", L"Q_Stab",        L"Resource/FX/Irelia/Q_Stab.wfx" },
            { L"Irelia", L"W_DefiantDance",L"Resource/FX/Irelia/W_DefiantDance.wfx" },
            { L"Irelia", L"E_FlawlessDuet",L"Resource/FX/Irelia/E_FlawlessDuet.wfx" },
            { L"Irelia", L"R_VanguardsEdge", L"Resource/FX/Irelia/R_VanguardsEdge.wfx" },
        };
        const std::vector<FxHookManifestEntry> g_JaxHooks = {
            { L"Jax", L"BA",              L"Resource/FX/Jax/BA.wfx" },
            { L"Jax", L"Q_LeapStrike",    L"Resource/FX/Jax/Q_LeapStrike.wfx" },
            { L"Jax", L"W_Empower",       L"Resource/FX/Jax/W_Empower.wfx" },
            { L"Jax", L"E_CounterStrike", L"Resource/FX/Jax/E_CounterStrike.wfx" },
            { L"Jax", L"R_GrandmastersAtWill",L"Resource/FX/Jax/R_Grandmaster.wfx" },
        };
        const std::vector<FxHookManifestEntry> g_KalistaHooks = {
            { L"Kalista", L"BA",                L"Resource/FX/Kalista/BA.wfx" },
            { L"Kalista", L"Q_PiercingSpear",   L"Resource/FX/Kalista/Q_PiercingSpear.wfx" },
            { L"Kalista", L"W_SentinelGhost",   L"Resource/FX/Kalista/W_SentinelGhost.wfx" },
            { L"Kalista", L"E_Rend",            L"Resource/FX/Kalista/E_Rend.wfx" },
            { L"Kalista", L"R_FateCall",        L"Resource/FX/Kalista/R_FateCall.wfx" },
        };
        const std::vector<FxHookManifestEntry> g_RivenHooks = {
            { L"Riven", L"BA",            L"Resource/FX/Riven/BA.wfx" },
            { L"Riven", L"Q_BrokenWings", L"Resource/FX/Riven/Q_BrokenWings.wfx" },
            { L"Riven", L"W_Ki_Burst",    L"Resource/FX/Riven/W_KiBurst.wfx" },
            { L"Riven", L"E_ValorShield", L"Resource/FX/Riven/E_ValorShield.wfx" },
            { L"Riven", L"R_Blade_of_the_Exile", L"Resource/FX/Riven/R_BladeOfExile.wfx" },
        };
        const std::vector<FxHookManifestEntry> g_YasuoHooks = {
            { L"Yasuo", L"BA",              L"Resource/FX/Yasuo/BA.wfx" },
            { L"Yasuo", L"Q_Straight",      L"Resource/FX/Yasuo/Q_Straight.wfx" },
            { L"Yasuo", L"Q_BuildUp",       L"Resource/FX/Yasuo/Q_BuildUp.wfx" },
            { L"Yasuo", L"Q_Tornado",       L"Resource/FX/Yasuo/Q_Tornado.wfx" },
            { L"Yasuo", L"W_WindWall",      L"Resource/FX/Yasuo/W_WindWall.wfx" },
            { L"Yasuo", L"E_SweepingBlade", L"Resource/FX/Yasuo/E_SweepingBlade.wfx" },
            { L"Yasuo", L"R_LastBreath",    L"Resource/FX/Yasuo/R_LastBreath.wfx" },
            { L"Yasuo", L"Passive_Shield",  L"Resource/FX/Yasuo/Passive_Shield.wfx" },
        };
        const std::vector<FxHookManifestEntry> g_YoneHooks = {
            { L"Yone", L"BA",                L"Resource/FX/Yone/BA.wfx" },
            { L"Yone", L"Q_MortalSteel",     L"Resource/FX/Yone/Q_MortalSteel.wfx" },
            { L"Yone", L"W_SpiritCleave",    L"Resource/FX/Yone/W_SpiritCleave.wfx" },
            { L"Yone", L"E_SoulUnbound",     L"Resource/FX/Yone/E_SoulUnbound.wfx" },
            { L"Yone", L"R_Fate_Sealed",     L"Resource/FX/Yone/R_FateSealed.wfx" },
        };
        const std::vector<FxHookManifestEntry> g_ZedHooks = {
            { L"Zed", L"BA",                L"Resource/FX/Zed/BA.wfx" },
            { L"Zed", L"Q_RazorShuriken",   L"Resource/FX/Zed/Q_RazorShuriken.wfx" },
            { L"Zed", L"W_LivingShadow",    L"Resource/FX/Zed/W_LivingShadow.wfx" },
            { L"Zed", L"E_ShadowSlash",     L"Resource/FX/Zed/E_ShadowSlash.wfx" },
            { L"Zed", L"R_DeathMark",       L"Resource/FX/Zed/R_DeathMark.wfx" },
        };
        const std::vector<FxHookManifestEntry> g_EmptyHooks = {};

        const std::vector<FxHookManifestEntry> g_MinionHooks = {
            { L"Minion", L"BA_Hit", L"Resource/FX/Minion/BA_Hit.wfx" },
            { L"Minion", L"Death",  L"Resource/FX/Minion/Death.wfx" },
        };
        const std::vector<FxHookManifestEntry> g_TurretHooks = {
            { L"Turret", L"BeamFire", L"Resource/FX/Turret/BeamFire.wfx" },
            { L"Turret", L"Destroy",  L"Resource/FX/Turret/Destroy.wfx" },
        };
    }

    const std::vector<FxHookManifestEntry>& CLoLFxManifest::GetPriorityHooks() { return g_PriorityHooks; }

    const std::vector<FxHookManifestEntry>& CLoLFxManifest::GetChampionHooks(const std::wstring& strChampion)
    {
        if (strChampion == L"Annie")   return g_AnnieHooks;
        if (strChampion == L"Ashe")    return g_AsheHooks;
        if (strChampion == L"Fiora")   return g_FioraHooks;
        if (strChampion == L"Garen")   return g_GarenHooks;
        if (strChampion == L"Irelia")  return g_IreliaHooks;
        if (strChampion == L"Jax")     return g_JaxHooks;
        if (strChampion == L"Kalista") return g_KalistaHooks;
        if (strChampion == L"Riven")   return g_RivenHooks;
        if (strChampion == L"Yasuo")   return g_YasuoHooks;
        if (strChampion == L"Yone")    return g_YoneHooks;
        if (strChampion == L"Zed")     return g_ZedHooks;
        return g_EmptyHooks;
    }

    const std::vector<FxHookManifestEntry>& CLoLFxManifest::GetMinionHooks() { return g_MinionHooks; }
    const std::vector<FxHookManifestEntry>& CLoLFxManifest::GetTurretHooks() { return g_TurretHooks; }
}
```

### §4.2 ~ §4.4 다른 cpp = v1 그대로 (LoLFxBootstrap.cpp / EldenFxManifest.cpp / EldenFxBootstrap.cpp 모두 v1 본문 풀, 변경 0)

v1 §4.2 / §4.3 / §4.4 본문 풀. re-quote 생략.

---

## §5 검증 명령

```txt
1. grep "static constexpr.*4096\\|static constexpr.*MAX_PARTICLES" Engine/   → 0 hit
2. grep "Winters::LoL\\|Winters::Elden" Engine/   → 0 hit
3. grep "Scene_" Client/{Public,Private}/FX/Domains/   → 0 hit
4. grep "TBD" .md/plan/EffectTool/25_LOL_ELDEN_DOMAIN_BAKE.md  → 0 hit
5. grep "stub\\|scaffold\\|본 박제 시점.*채움" .md/plan/EffectTool/25_LOL_ELDEN_DOMAIN_BAKE.md  → 0 hit
6. CLoLFxManifest::GetPriorityHooks().size() == 11
7. CLoLFxManifest::GetChampionHooks(L"Yasuo").size() == 8
8. CLoLFxManifest::GetChampionHooks(<other 10>).size() >= 5 each
9. CEldenFxManifest::GetBosses().size() == 5
10. LoL FxBudget vs Elden FxBudget 4 항목 차이
```

---

## §6 박제 함정 매트릭스

| 함정 | 본 25 회피 |
|---|---|
| P-1 + P-6 | §1 4 항목, TBD 0. 11 챔프 hook 모두 본문 |
| P-2 (PIMPL 추측) | 헤더 + cpp 동시 |
| P-3 (모든 path) | 두 도메인 한 번에 |
| P-4 (Scene 직접 의존) | Bootstrap = Registry preload 만 |
| P-7 (bitmask) | mask 미사용 |
| P-8 (인용 의미 반전) | PLAN_AUTHORING_PITFALLS P-11 사례 차용 |
| P-9 (ECS Scheduler) | Bootstrap = ECS 무관 |
| P-10 (Owner Scope) | Bootstrap = static |
| P-11 (도메인 상수) | Engine constexpr 0. LoL/Elden 패키지 안에만 |
| P-12 (음수 truncation) | int 변환 0 |
| P-13 (미존재 API) | `CFxAssetRegistry / FxSystemInitDesc` 부속 18/19 박제 |
| P-14 (행동 정책 변경) | 본 25 = 신규 |
| P-15 (헤더 외부 의존) | `LoLFxBudget.h` = `FxSystemInitDesc.h` 직접 include |
| P-16 (산술 검증) | manifest entry count 단위 테스트 |
| P-17 (typedef ABI) | 신규 |
| P-18 (RHI 인프라) | Bootstrap = RHI 무관 |
| P-19 (Render/Sim 결합) | Bootstrap = preload only |

---

## §7 변경 이력

```txt
2026-04-21    Phase G 초안
2026-05-04    P-11 박제 (PLAN_AUTHORING_PITFALLS)
2026-05-07    17 v4 마스터. 본 25 v1 (Yasuo 만 풀)
2026-05-07    본 25 v2 재박제 (CLAUDE.md §8.2 본문 룰 — 11 챔프 hook 모두 본문)
```
