# Viego W Charge Glow Dash Tuning Report

Date: 2026-06-24
Plan: `.md/plan/2026-06-24_VIEGO_W_CHARGE_GLOW_DASH_TUNING.md`

## 1. 작업 결과

### W1 charge glow

반영 파일:
- `Client/Private/GameObject/Champion/Viego/Viego_Skills.cpp`
- `Client/Private/GameObject/Champion/Viego/Viego_FxPresets.cpp`
- `Client/Public/GameObject/Champion/Viego/Viego_FxPresets.h`
- `Data/LoL/FX/Champions/Viego/w_charge_glow.wfx`

결과:
- Viego W visual hook을 `skillStage` 기준으로 분리했다.
- W1은 `Viego.W.ChargeGlow`만 재생한다.
- W charge window 4.0초를 3등분해 0.0초, 1.33초, 2.67초에 glow가 1개씩 추가된다.
- W2가 발동되면 W1에서 생성한 charge glow entity들을 owner 기준으로 제거해 늦게 남는 glow를 막는다.

### W2 Soul/missile dash

반영 파일:
- `Client/Private/GameObject/Champion/Viego/Viego_FxPresets.cpp`
- `Client/Private/GameObject/Champion/Viego/Viego_Skills.cpp`

결과:
- Soul/missile 계열 `Viego.W.Cast`, `Viego.W.Missile`은 W2 stage에서만 재생된다.
- missile visual speed는 7.2에서 3.6으로 절반 조정했다.
- visual end distance는 4.6/4.8에서 2.3/2.4로 절반 조정했다.

### 서버 권위 dash 거리

반영 파일:
- `Data/Gameplay/ChampionGameData/champions.json`
- `Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json`
- `Data/LoL/SharedContract/DefinitionManifest.json`
- `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp`

결과:
- 원천 데이터 `champion == "VIEGO"`, `slot == 2`의 `rangeMax`를 8.0에서 4.0으로 낮췄다.
- `Build-LoLDefinitionPack.py`로 서버 JSON과 generated C++를 재생성했다.
- `MakeSkill_VIEGO_W()`의 `def.range.rangeMax`가 4.f로 반영됨을 확인했다.
- `dashDurationSec`는 0.26초를 유지했다. 서버 dash 거리가 절반이고 시간이 같으므로 실제 dash 속도도 절반이 된다.

## 2. 검증 결과

### 공백 검증

Command:
```powershell
git diff --check
```

Result:
- PASS
- 기존 작업트리의 LF/CRLF 경고만 출력됨.

### Definition pack freshness

Command:
```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --check
```

Result:
- PASS
- Definition pack: `0x42EA0952`
- Champions: 17, skills: 85, summoner spells: 1

### Standalone Client/Server Debug x64 build

Command:
```powershell
MSBuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
MSBuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
```

Result:
- PASS
- 빌드 전 실행 중이던 `WintersGame.exe`, `WintersServer.exe`를 종료해 Debug 산출물 잠금을 해제했다.
- 이 standalone 빌드는 W2 glow stop helper 추가 전 기준이다.
- W2 glow stop helper까지 포함한 최종 Client/Server 빌드는 아래 LoLDataDriven 통합 검증 안에서 다시 수행했고 PASS했다.
- 산출물:
  - `Client/Bin/Debug/WintersGame.exe`
  - `Server/Bin/Debug/WintersServer.exe`

### LoLDataDriven 통합 검증

Command:
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -Configuration Debug
```

Result:
- PASS
- 통과 항목:
  - Definition pack freshness
  - Legacy ownership audit
  - Client visual timing parity, mismatchCount 0
  - Build `Shared/GameSim/Include/GameSim.vcxproj`
  - Build `Server/Include/Server.vcxproj`
  - Build `Client/Include/Client.vcxproj`
  - Build `Tools/SimLab/SimLab.vcxproj`
  - SimLab deterministic regression
  - Whitespace validation
- SimLab result:
  - same-seed replay OK: `67F2A97563B8DB04`
  - seed sensitivity OK: `5DA19645E291A29B`
  - PASS

## 3. 남은 확인

- 실제 3-client 인게임 시각 확인은 아직 수행하지 않았다.
- 수동 확인 포인트:
  - W1 charge 중 0.0초, 1.33초, 2.67초 기준으로 glow가 1개, 2개, 3개로 증가하는지.
  - W1에는 Soul/missile이 나오지 않는지.
  - W2 dash 순간 charge glow가 제거되고 Soul/missile이 나가는지.
  - W2 dash 거리와 체감 속도가 기존 대비 절반 수준인지.
