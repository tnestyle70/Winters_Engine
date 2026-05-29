# EFX-0 Legacy Bridge와 기존 preset 자산화

작성일: 2026-05-07
상태: 구현 계획
의존:
- `01_CURRENT_CODEBASE_AUDIT_AND_ENTRY_GATES.md` S0
- `.md/plan/EffectTool/17_NIAGARA_FULL_REWRITE_MASTER.md` EFX-0

목적:
- 11개 챔피언의 직접 FX preset 코드를 `.wfx`/`.wmi` 자산으로 흡수할 발판을 만든다.
- 기존 시각 결과와 판정/스폰 타이밍을 보존한다.
- 바로 삭제하지 않고 legacy direct spawn과 asset spawn을 병행한다.

---

## 1. 현재 대상 파일

챔피언 preset:

```txt
Client/Private/GameObject/Champion/Annie/Annie_FxPresets.cpp
Client/Private/GameObject/Champion/Ashe/Ashe_FxPresets.cpp
Client/Private/GameObject/Champion/Ezreal/Ezreal_FxPresets.cpp
Client/Private/GameObject/Champion/Fiora/Fiora_FxPresets.cpp
Client/Private/GameObject/Champion/Garen/GarenFxPresets.cpp
Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp
Client/Private/GameObject/Champion/Jax/Jax_FxPresets.cpp
Client/Private/GameObject/Champion/Kalista/KalistaFxPresets.cpp
Client/Private/GameObject/Champion/Riven/RivenFxPresets.cpp
Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp
Client/Private/GameObject/Champion/Zed/ZedFxPresets.cpp
```

기존 bridge:

```txt
Client/Public/GameObject/FX/LegacyFxAdapter.h
Client/Private/GameObject/FX/LegacyFxAdapter.cpp
```

자산 registry:

```txt
Engine/Public/FX/FxAsset.h
Engine/Private/FX/FxAsset.cpp
Engine/Private/GameInstance.cpp
```

---

## 2. EFX-0 결정

```txt
1. 기존 CFxSystem::Spawn, CFxMeshSystem::Spawn, CFxBeamSystem::Spawn은 EFX-0에서 제거하지 않는다.
2. 각 preset 함수는 먼저 manifest entry와 asset dump path를 가진다.
3. 첫 변환 대상은 Irelia, Yasuo, Ezreal이다.
4. 나머지 챔피언은 manifest만 먼저 만든다.
5. .wfx는 system/emitter/render 정보를 담고, .wmi는 material instance 정보를 담는다.
6. texture path는 Client/Bin/Resource 기준 상대 경로를 유지하되 slash는 `/`로 통일한다.
7. direct spawn visual과 asset spawn visual을 같은 smoke에서 비교한다.
```

---

## 3. 신규 파일

```txt
Client/Public/GameObject/FX/FxLegacyManifest.h
Client/Private/GameObject/FX/FxLegacyManifest.cpp
Client/Public/GameObject/FX/FxLegacyAssetDumper.h
Client/Private/GameObject/FX/FxLegacyAssetDumper.cpp

Client/Bin/Resource/FX/Manifest/LoL/Irelia.fxmanifest.json
Client/Bin/Resource/FX/Manifest/LoL/Yasuo.fxmanifest.json
Client/Bin/Resource/FX/Manifest/LoL/Ezreal.fxmanifest.json
```

EFX-1 이후 `.fxmanifest.json`은 canonical `.wfx`와 `.wmi` 산출물의 source index 역할을 한다.

---

## 4. Manifest 스키마

초기 스키마:

```json
{
  "version": 1,
  "champion": "Irelia",
  "entries": [
    {
      "id": "Irelia.Q.Trail",
      "sourceFunction": "IreliaFx::SpawnQTrail",
      "sourceFile": "Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp",
      "renderTypes": ["Billboard"],
      "assetPath": "Client/Bin/Resource/FX/LoL/Irelia/Irelia_Q_Trail.wfx",
      "materialInstancePath": "Client/Bin/Resource/FX/LoL/Irelia/MI_Irelia_Q_Trail.wmi",
      "migrationState": "LegacyAndAssetParallel"
    }
  ]
}
```

`migrationState` 값:

```txt
LegacyOnly
LegacyAndAssetParallel
AssetPreferred
LegacyRemoved
```

EFX-0에서는 `LegacyRemoved`를 쓰지 않는다.

---

## 5. 구현 단계

### EFX0-1. Callsite inventory 고정

작업:

```txt
1. rg 결과를 `FxLegacyManifest.cpp`의 주석이 아니라 manifest JSON에 보존한다.
2. 각 직접 spawn callsite를 champion/skill/render type으로 분류한다.
3. 네트워크 event/snapshot applier의 FX는 `Network.Generic` bucket으로 따로 분리한다.
```

분류 bucket:

```txt
Champion.Skill
Champion.Passive
Champion.BasicAttack
Champion.Debug
Network.Event
System.UltWave
System.WindWall
```

완료 기준:

```txt
[ ] 11개 champion manifest 생성
[ ] Network/Event/Snapshot FX manifest 생성
[ ] sourceFunction이 비어 있는 entry 0
[ ] assetPath가 비어 있는 entry 0
```

### EFX0-2. LegacyFxAdapter 확장

현재 adapter는 billboard/mesh 위주다. EFX-0에서 beam/ribbon도 asset으로 바꿀 수 있게 확장한다.

대상:

```txt
Client/Public/GameObject/FX/LegacyFxAdapter.h
Client/Private/GameObject/FX/LegacyFxAdapter.cpp
```

추가 함수:

```cpp
namespace LegacyFx
{
    FxAsset MakeAssetFromBeam(const FxBeamComponent& src, const char* pszAssetName);
    FxAsset MakeAssetFromRibbon(const FxRibbonComponent& src, const char* pszAssetName);

    FxAssetHandle FxAssetFromBeam(CFxAssetRegistry& registry,
        const FxBeamComponent& src,
        const char* pszAssetName);

    FxAssetHandle FxAssetFromRibbon(CFxAssetRegistry& registry,
        const FxRibbonComponent& src,
        const char* pszAssetName);
}
```

완료 기준:

```txt
[ ] Beam emitter asset 생성
[ ] Ribbon emitter asset 생성
[ ] 기존 Billboard/Mesh 변환 결과와 필드 손실 0
```

### EFX0-3. Asset dump command

첫 구현은 별도 툴보다 Client debug panel 버튼이 빠르다.

대상:

```txt
Client/Public/UI/EffectTuner.h
Client/Private/UI/EffectTuner.cpp
```

변경 방향:

```txt
1. 기존 Irelia-only spawn test는 유지한다.
2. `Dump Legacy FX Assets` 버튼을 추가한다.
3. 버튼은 manifest를 읽고 등록된 dumper callback을 실행한다.
4. 산출물은 Client/Bin/Resource/FX/LoL/{Champion}/ 아래에 쓴다.
```

주의:

```txt
EFX-0에서는 JSON writer를 간단히 둘 수 있다.
EFX-1에서 structured canonical writer로 교체한다.
```

완료 기준:

```txt
[ ] Irelia manifest -> .wfx/.wmi dump
[ ] Yasuo manifest -> .wfx/.wmi dump
[ ] Ezreal manifest -> .wfx/.wmi dump
[ ] dump 후 CFxAssetRegistry::LoadFromFile 성공
```

### EFX0-4. Asset spawn 병행 경로

기존 preset 함수 내부에서 실험 플래그로 asset spawn을 병행한다.

권장 플래그:

```txt
WINTERS_FX_ASSET_PARALLEL_SPAWN
```

패턴:

```cpp
#if defined(WINTERS_FX_ASSET_PARALLEL_SPAWN)
    const FxAssetHandle h = CFxSystem::GetAssetRegistry().FindByName("Irelia.Q.Trail");
    if (h.IsValid())
        return CFxSystem::SpawnFromAsset(world, h, fx.vWorldPos, fx.attachTo);
#endif
    return CFxSystem::Spawn(world, fx);
```

주의:

```txt
1. 병행 경로는 기본 off.
2. 최초 적용은 Irelia QTrail / QMark / WSpin / EBeam / RPulse만.
3. 기존 direct spawn path는 fallback으로 유지한다.
```

완료 기준:

```txt
[ ] Irelia 5개 핵심 FX asset spawn 가능
[ ] legacy fallback 유지
[ ] visual smoke에서 spawn timing drift 없음
```

---

## 6. Irelia 1차 entry

우선 entry:

```txt
Irelia.Q.Trail
Irelia.Q.Mark
Irelia.W.Spin
Irelia.W.Stage2Slash
Irelia.E.Blade
Irelia.E.Beam
Irelia.R.Pulse
Irelia.R.BladeFan
Irelia.R.UltWaveRing
```

Irelia는 mesh, beam, billboard, ground decal의 혼합이라 EFX-0 검증에 적합하다.

---

## 7. Yasuo 1차 entry

우선 entry:

```txt
Yasuo.Q.Slash
Yasuo.Q.Tornado
Yasuo.W.WindWall
Yasuo.E.DashDust
Yasuo.R.Landing
```

Yasuo는 wind wall과 trail 검증에 적합하다.

---

## 8. Ezreal 1차 entry

우선 entry:

```txt
Ezreal.Q.ProjectileHead
Ezreal.Q.ProjectileTrail
Ezreal.W.Orb
Ezreal.E.BlinkFlash
Ezreal.R.Bow
Ezreal.R.Missile
```

Ezreal은 projectile 이동, 네트워크 event FX, mesh + billboard 조합 검증에 적합하다.

---

## 9. 검증

Grep:

```powershell
rg "WINTERS_FX_ASSET_PARALLEL_SPAWN" Client/Private/GameObject/Champion
rg "CFxSystem::Spawn\\(|CFxMeshSystem::Spawn\\(" Client/Private/GameObject/Champion
rg "LegacyRemoved" Client/Bin/Resource/FX/Manifest
```

기대:

```txt
1. EFX-0에서 `LegacyRemoved`는 0 hit.
2. 직접 spawn callsite는 남아 있어도 manifest coverage가 있어야 한다.
3. asset parallel flag가 붙은 곳은 Irelia/Yasuo/Ezreal 일부에만 있다.
```

Visual smoke:

```txt
Case A
  Direct local Irelia
  Q/W/E/R 1회씩 cast
  Legacy path screenshot, Asset path screenshot 비교

Case B
  Direct local Yasuo
  Q/W/E/R 1회씩 cast
  WindWall blockable flag 유지 확인

Case C
  Direct local Ezreal
  Q projectile 이동, R missile mesh 출력 확인
```

완료 기준:

```txt
[ ] 3 champion asset dump 성공
[ ] 3 champion asset spawn 병행 성공
[ ] 기존 direct spawn path 삭제 0
[ ] smoke에서 크래시 0
[ ] path slash 깨짐 0
```

