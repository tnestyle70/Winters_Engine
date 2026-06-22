# LOL Visual Timing Seed Plan Build Report

작성일: 2026-06-22

## 결론

다음 단계 계획서 작성과 현재 코드 기준 검증 파이프라인을 완료했다.

계획서: `.md/plan/refactor/11_LOL_VISUAL_TIMING_SEED_EXTRACTION_PLAN.md`

이번 세션의 실제 반영은 계획서 작성과 legacy audit 갱신이다. 계획서 안의 `Export-LoLChampionVisualTimingSeed.ps1`는 다음 구현 대상이며, 이번 세션에서는 실제 도구 파일로 추가하지 않았다. 따라서 현재 runtime reader, `ChampionGameDataDB`, Client visual registration, Server GameSim 판정 경로는 변경되지 않았다.

## 본질 판단

다음 구현의 원자 단위는 `ChampionGameData` 안에 섞인 네 종류의 visual playback field다.

- `visualYawOffset`
- `animPlaySpeed`
- `castFrame`
- `recoveryFrame`

이 값들은 서버 판정의 canonical truth가 아니라 client visual playback seed다. 따라서 다음 구현은 값을 삭제하지 않고 먼저 `ClientPublic/Visual` seed로 복제한 뒤, legacy source와 seed가 같은 값을 갖는지 parity report로 고정한다.

## 작성한 계획

계획서는 다음 구현만 남겼다.

- `Tools/LoLData/Export-LoLChampionVisualTimingSeed.ps1` 새 도구 계획
- `Data/Gameplay/ChampionGameData/champions.json`에서 visual-only field 추출
- `Data/LoL/ClientPublic/Visual/Champion/ChampionVisualTimingSeed.json` 생성
- `.md/TODO/06-22/LOL_VISUAL_TIMING_SEED_PARITY.json` 생성
- mismatch가 0이 아니면 실패

의도적으로 빠진 것:

- `ChampionGameData.h` 삭제 없음
- `ChampionGameDataDB.cpp` reader 전환 없음
- `ResolveVisualYawOffset` 삭제 없음
- `animPlaySpeed/castFrame/recoveryFrame` 삭제 없음

이유:

seed parity 없이 먼저 삭제하면 frame 안에서 animation, FX, action timing, visual yaw 조회가 동시에 흔들릴 수 있다.

## Audit

명령:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\LoLData\Collect-LoLLegacyDataAudit.ps1 -OutputPath .md\TODO\06-22\LOL_DATA_LEGACY_AUDIT.json
```

결과: 통과

주요 수치:

- champion count: 17
- summoner spell count: 1
- skill stage count: 94
- visualYawOffset count: 17
- animPlaySpeed count: 94
- castFrame count: 94
- recoveryFrame count: 94
- SkillDef registration files: 17
- ChampionDef registration files: 12
- visual field 혼합 후보 라인: 1095

## Whitespace

명령:

```powershell
git diff --check
```

결과: 통과

비고:

CRLF 변환 경고만 있었다. whitespace error는 없었다. 빌드 중 FlatBuffers generated schema 파일도 CRLF 경고 목록에 추가로 보였다.

## Build

### GameSim

명령:

```powershell
MSBuild.exe Shared\GameSim\Include\GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과: 통과

- 경고: 0
- 오류: 0

### Server

명령:

```powershell
MSBuild.exe Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과: 통과

- 경고: 2
- 오류: 0

주요 경고:

- 기존 EngineSDK DLL interface 경고 `C4275`

### Client

명령:

```powershell
MSBuild.exe Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과: 통과

- 경고: 14
- 오류: 0

주요 경고:

- 기존 EngineSDK DLL interface 경고 `C4275`

### SimLab

명령:

```powershell
MSBuild.exe Tools\SimLab\SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과: 통과

- 경고: 0
- 오류: 0

## 회귀 판단

현재 기준 회귀 위험은 낮다.

이번에는 계획서와 audit JSON만 갱신했고 runtime code를 변경하지 않았다. 따라서 기존 프레임 안에서 데이터를 받아오고 읽는 경로는 그대로 유지된다.

- `ChampionGameDataDB` 기존 조회 유지
- `ResolveVisualYawOffset` 유지
- `ResolveSkillTiming` 유지
- Client `SkillTable`/`ChampionTable` 유지
- Server GameSim 판정 유지
- SimLab 링크 검증 통과

## 다음 구현 기준

다음 세션에서 실제로 구현할 때의 성공 조건은 아래 하나다.

```text
legacy champions.json의 visual playback 값과 ClientPublic visual seed 값이 완전히 같다.
```

그 다음에만 reader 전환과 삭제를 진행한다.

삭제 후보:

- `ChampionGameData.visualYawOffset`
- `ChampionGameDataSkillStage.animPlaySpeed`
- `ChampionGameDataSkillStage.castFrame`
- `ChampionGameDataSkillStage.recoveryFrame`

삭제 전 필수 확인:

```powershell
rg -n "visualYawOffset|animPlaySpeed|castFrame|recoveryFrame|ResolveVisualYawOffset" Shared/GameSim Server Client Tools
```

runtime reader가 visual seed 또는 Client visual DB로 이동하기 전에는 삭제하지 않는다.
