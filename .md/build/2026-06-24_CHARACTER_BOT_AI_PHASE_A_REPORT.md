Session - Character Bot AI Phase A 기준선 고정과 Yone E runtime 계약 검증 결과

1. 반영해야 하는 코드

결론:
- Phase A는 PASS다.
- SimLab 기본 PASS 조건에 Yone E stage-2 return runtime probe를 포함했다.
- SimLab이 생성된 서버 gameplay definition pack을 직접 보도록 연결했다.

본질:
- Bot AI 구현 전에 runtime 데이터 계약부터 고정한다.
- JSON -> generated server definition -> `TickContext::pDefinitions` -> `CommandExecutor` -> GameSim 실행 경로가 실제로 이어져야 한다.
- 챔피언별 tactic은 이 검증선 위에서만 올린다.

반영 파일:
- `C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp`
- `C:/Users/user/Desktop/Winters/Tools/SimLab/SimLab.vcxproj`
- `C:/Users/user/Desktop/Winters/.md/plan/2026-06-24_CHARACTER_BOT_AI_PHASED_IMPLEMENTATION_PIPELINE_PLAN.md`

핵심 변경:
- `Tools/SimLab/main.cpp`에서 `ServerData::GetLoLGameplayDefinitionPack()`을 include하고, deterministic match와 Yone probe tick context의 `pDefinitions`에 연결했다.
- `RunYoneEReturnProbe()`를 추가해 Yone E 1타, stage window, `cmd.itemId = 2u` recast, `bReturning`, anchor 복귀, soul state clear를 검증한다.
- `main()`의 기본 PASS 조건에 Yone E probe 결과를 포함했다.
- `Tools/SimLab/SimLab.vcxproj`에 `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp`를 추가해 SimLab이 generated definition pack을 링크한다.

중요 발견:
- 최초 probe는 `E stage window closed before recast`로 실패했다.
- 원인은 SimLab `TickContext::pDefinitions`가 null이라 `GameplayDefinitionQuery`가 legacy fallback data를 보고 있었기 때문이다.
- 따라서 기존 SimLab PASS만으로는 Yone E JSON stage 계약이 generated runtime path에서 검증된다고 말할 수 없었다.
- 이번 수정 이후 SimLab은 생성된 서버 definition pack을 직접 사용한다.

2. 검증

실행 결과:
- `Tools/SimLab/SimLab.vcxproj` Debug x64 build: PASS
- `Tools/Bin/Debug/SimLab.exe`: PASS
- `Tools/Harness/Run-BotAiValidation.ps1 -SkipFullPipeline`: exit 0, WARN only
- `Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug`: PASS

주요 확인 로그:

```text
[YoneSim] E out caster=1 anchor=(0,0) end=(4,0)
[YoneSim] E return caster=1
[SimLab][YoneE] PASS: stage-2 return reached anchor=(0.00,0.00,0.00)
[SimLab] same-seed hash A=115B1D1F39A0C6BD B=115B1D1F39A0C6BD
[SimLab] seed+1 hash C=6A4DB7A1B1D43347
[SimLab] PASS
[LoLDataDriven] PASS
```

Bot AI validation harness:
- Report: `C:/Users/user/Desktop/Winters/.md/build/2026-06-24_100909_BOT_AI_VALIDATION_HARNESS_REPORT.md`
- WARN 사유는 `-SkipFullPipeline` 옵션으로 full data pipeline을 건너뛴 것이다.
- 이후 full data-driven pipeline을 별도로 실행했고 PASS를 확인했다.

남은 경고:
- `git diff --check`는 exit 0이었다. LF -> CRLF 안내 warning만 출력됐다.
- Server/Client build 중 기존 C4275 DLL interface warning이 출력됐다.
- `Server/Private/Game/GameRoomMinionAI.cpp`의 기존 C4828 encoding warning이 출력됐다.
- 위 경고들은 이번 Phase A runtime 계약 변경에서 새로 만든 실패 조건은 아니다.

다음 Phase 진입 조건:
- Phase A 기준선은 통과했다.
- Phase B에서는 공통 decision evidence를 추가한다.
- Phase B의 첫 작업은 `ChampionAIContext` 주변 실제 component와 field를 다시 확인한 뒤, gold/value/health/vision/objective evidence를 raw fact 수집과 score 계산으로 분리하는 것이다.
