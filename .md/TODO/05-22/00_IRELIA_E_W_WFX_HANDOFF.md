# Irelia E/W WFX Handoff - 2026-05-22

다른 프로젝트에서 이어서 진행하기 위한 이렐리아 E/W 이펙트 계획서 고정본이다.

## Current Priority

1. `21_IRELIA_E_WFX_SIZE_FIX_PLAN.md`
   - 현재 이렐리아 E 이펙트가 너무 크게 보이는 문제 수정 계획.
   - 핵심: E place 레거시/WFX 중첩 제거, WFX 크기 축소, place cue 생명주기 추적.

2. `20_IRELIA_W_WFX_PIPELINE_PLAN.md`
   - 이렐리아 W를 E처럼 WFX cue 파이프라인으로 전환하는 최신 계획.
   - 핵심: `Irelia.W.Spin`, `Irelia.W.Aim`, `Irelia.W.Stage2Slash`를 Hold/Aim/Release cue로 분리.

3. `18_IRELIA_ORIGINAL_EFFECT_POLISH_PLAN.md`
   - 이렐리아 E 원본 이미지/PNG/FBX/WFX 매핑과 전체 폴리시 방향.
   - E place/connect/pop 구현 방향의 기준 문서.

4. `19_IRELIA_W_ORIGINAL_REFERENCE_PLAN.md`
   - W 01-04 이미지 기반 레거시 프리셋 강화안.
   - 최신 WFX 전환은 `20_IRELIA_W_WFX_PIPELINE_PLAN.md`가 우선이다.

## Historical Context

- `02_IRELIA_PHASE_D_EFFECTS_PLAN.md`
- `02b_IRELIA_PHASE_D_REMEDIATION_PLAN.md`

위 두 문서는 오래된 Phase D 이렐리아 효과/보정 계획이다. 현재 구현 기준은 18, 20, 21 문서가 더 최신이다.

## Reference Asset Folder

이미지 레퍼런스:

```text
C:\Users\user\Desktop\Winters\Client\Bin\Resource\Texture\UI\이펙트 이미지
```

주요 런타임 WFX/텍스처 경로:

```text
C:\Users\user\Desktop\Winters\Data\LoL\FX\Champions\Irelia
C:\Users\user\Desktop\Winters\Client\Bin\Resource\Texture\FX\Irelia
```

## Implementation Notes

- E 크기 버그는 코드 기능 부족보다 데이터/중첩 문제가 우선이다.
- `CFxCuePlayer`에는 `bOverrideSize`가 있으나 E처럼 emitter 타입이 섞인 cue에는 일괄 override가 거칠다.
- E place는 `CIreliaBladeSystem::SpawnPlaced`에서 기존 `SpawnEPlacedLayers`와 `Irelia.E.Place` WFX가 동시에 뜨고 있어 중첩이 발생한다.
- W는 기존 레거시 프리셋보다 `20_IRELIA_W_WFX_PIPELINE_PLAN.md` 기준의 WFX 분리가 맞다.

## Verification Baseline

```powershell
git diff --check
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' Winters.sln /m /p:Configuration=Debug /p:Platform=x64
```

빌드 전 `WintersServer.exe`가 실행 중이면 Server link가 잠길 수 있다.
