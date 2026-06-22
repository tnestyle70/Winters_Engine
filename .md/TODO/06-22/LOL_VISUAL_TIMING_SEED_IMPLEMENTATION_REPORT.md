# LOL Visual Timing Seed Implementation Report

작성일: 2026-06-22

## 결론

`ChampionGameData`에 섞여 있던 visual playback field를 `ClientPublic/Visual` seed로 복제하는 첫 구현을 완료했다.

이번 구현은 runtime reader를 바꾸지 않는다. 서버 판정 경로와 클라이언트 기존 visual registration은 그대로 두고, 다음 단계의 안전한 reader 전환을 위한 seed와 parity report만 추가했다.

## 반영 파일

- `Tools/LoLData/Export-LoLChampionVisualTimingSeed.ps1`
- `Data/LoL/ClientPublic/Visual/Champion/ChampionVisualTimingSeed.json`
- `.md/TODO/06-22/LOL_VISUAL_TIMING_SEED_PARITY.json`
- `.md/TODO/06-22/LOL_DATA_LEGACY_AUDIT.json`
- `.md/TODO/06-22/LOL_VISUAL_TIMING_SEED_IMPLEMENTATION_REPORT.md`

## 본질 기준

이번 원자 단위는 네 값이다.

- `visualYawOffset`
- `animPlaySpeed`
- `castFrame`
- `recoveryFrame`

이 값들은 서버가 신뢰하는 gameplay truth가 아니다. 클라이언트 모델 방향, 애니메이션 재생 속도, 시각 이벤트 프레임에 필요한 playback seed다.

따라서 다음 원칙을 적용했다.

- 삭제하지 않는다.
- reader를 바꾸지 않는다.
- 먼저 `ClientPublic/Visual`로 같은 값을 복제한다.
- parity report로 값이 다르지 않음을 검증한다.

## Parity 결과

명령:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\LoLData\Export-LoLChampionVisualTimingSeed.ps1
```

결과:

- champion count: 17
- skill stage count: 94
- mismatch count: 0

생성 파일:

- `Data/LoL/ClientPublic/Visual/Champion/ChampionVisualTimingSeed.json`
- `.md/TODO/06-22/LOL_VISUAL_TIMING_SEED_PARITY.json`

## Audit 결과

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
- visual field 혼합 후보 라인: 1290

비고:

`visualFieldsInGameplayOrLegacy` 값은 이전보다 증가했다. 새 `ClientPublic/Visual` seed와 export 스크립트가 같은 검색 패턴에 잡히기 때문이다. 이것은 의도된 중간 상태다. 다음 audit 개선에서는 `ClientPublic/Visual`을 정상 분리 대상으로 구분하고, `Shared/GameSim`/`Server` 쪽 잔존 visual field만 별도 카운트해야 한다.

## Whitespace

명령:

```powershell
git diff --check
```

결과: 통과

## Build

### GameSim

명령:

```powershell
MSBuild.exe Shared\GameSim\Include\GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과:

- 경고: 0
- 오류: 0

### Server

명령:

```powershell
MSBuild.exe Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과:

- 경고: 2
- 오류: 0

주요 경고:

- 기존 EngineSDK DLL interface 경고 `C4275`

### Client

명령:

```powershell
MSBuild.exe Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과:

- 경고: 14
- 오류: 0

주요 경고:

- 기존 EngineSDK DLL interface 경고 `C4275`

### SimLab

명령:

```powershell
MSBuild.exe Tools\SimLab\SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과:

- 경고: 0
- 오류: 0

## 회귀 판단

회귀 위험은 낮다.

이번 변경은 새 data seed와 tool만 추가했다. 기존 runtime code는 변경하지 않았다.

유지된 경로:

- `ChampionGameDataDB`
- `ResolveVisualYawOffset`
- `ResolveSkillTiming`
- Client `SkillTable`
- Client `ChampionTable`
- Server GameSim 판정
- Snapshot/Event 흐름

## 다음 단계

다음 구현은 audit을 더 본질적으로 나누는 것이다.

현재 audit은 `ClientPublic/Visual` seed까지 visual leak처럼 센다. 다음 단계에서는 아래처럼 분리한다.

```text
정상 위치
- Data/LoL/ClientPublic/Visual/**
- Client visual registry/tooling

의심 위치
- Shared/GameSim
- Server
- Data/Gameplay legacy source
- Client legacy SkillDef/ChampionDef runtime table
```

그 다음에 reader 전환을 시작한다.

순서:

1. `ChampionVisualTimingSeed.json`을 읽는 Client visual DB 추가
2. `ResolveVisualYawOffset` 호출자를 Client visual DB로 이동
3. `animPlaySpeed/castFrame/recoveryFrame` 사용자를 visual playback DB로 이동
4. parity 유지
5. `ChampionGameData`에서 visual field 삭제

삭제는 reader 이동 후에만 진행한다.
