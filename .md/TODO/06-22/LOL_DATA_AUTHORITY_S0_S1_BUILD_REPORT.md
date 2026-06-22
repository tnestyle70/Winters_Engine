# LOL Data Authority S0/S1 Build Report

작성일: 2026-06-22

## 결론

S0/S1 범위의 본질 정리 반영은 완료했다.

이번 반영의 핵심은 런타임 동작을 바꾸는 삭제/교체가 아니라, 앞으로 데이터를 안전하게 쪼개기 위한 계약과 감사 장치를 먼저 세운 것이다. 기존 프레임 안에서 데이터를 읽는 경로는 그대로 두었고, 새 계약 헤더는 포함되지 않으면 동작에 영향을 주지 않는 독립 구조로 추가했다.

빌드 검증 결과 `GameSim`, `Server`, `Client`, `SimLab` 직접 빌드는 모두 오류 0개로 통과했다.

## 반영 파일

- `Shared/GameSim/Definitions/DataPackManifest.h`
- `Shared/GameSim/Definitions/LoLPublicGameHintData.h`
- `Tools/LoLData/Collect-LoLLegacyDataAudit.ps1`
- `.md/TODO/06-22/LOL_DATA_LEGACY_AUDIT.json`
- `.md/plan/refactor/09_LOL_DATA_ATOM_EXTRACTION_COLLAB_PLAN.md`
- `.md/plan/refactor/10_LOL_DATA_AUTHORITY_PATCH_PIPELINE_PLAN.md`

## 반영 내용

### DataPackManifest

데이터 팩의 최소 식별자를 추가했다.

- `uSchemaVersion`: 구조 버전
- `uDataVersion`: 데이터 버전
- `uBuildHash`: 빌드 산출물 식별
- `uRulesetId`: 룰셋 식별
- `eVisibility`: 서버 전용, 클라이언트 공개, 공유 계약, 테스트 전용 구분

의심 기준:

- 값 자체는 게임 규칙이 아니다.
- 데이터가 어느 경계에 존재할 수 있는지만 표현한다.
- `Shared/GameSim`에 있어도 Engine, Client, Server 타입을 끌어오지 않는다.

### LoLPublicGameHintData

클라이언트가 가져도 되는 공개 힌트의 최소 계약을 추가했다.

- 챔피언 ID
- 데이터 버전
- 공개 해시
- 스킬 슬롯별 공개 가능 범위/쿨다운 힌트

의심 기준:

- 데미지 공식, 서버 판정, 성장 수치, AI/밸런스 비밀값은 넣지 않았다.
- 클라이언트 UI와 약한 예측에 필요한 공개 정보만 담을 수 있게 했다.
- 실제 런타임 연결은 아직 하지 않았다. 기존 동작 회귀를 막기 위해서다.

### Legacy Data Audit

잔존 데이터 혼합 상태를 JSON으로 뽑는 감사 스크립트를 추가했다.

감사 대상:

- `ChampionGameData` 안의 챔피언 수
- 스킬 stage 수
- gameplay 데이터에 남아 있는 visual/timing 필드 수
- Client 등록 파일에 남아 있는 `SkillDef`, `ChampionDef` 수
- Server 오브젝트 하드코딩 후보 수
- Projectile visual catalog 잔존 후보 수

## 감사 결과

`Data/Gameplay/ChampionGameData/champions.json`

- 챔피언 수: 17
- `summonerSpells` 보유 챔피언 수: 1
- 스킬 stage 수: 94
- `visualYawOffset` 수: 17
- `animPlaySpeed` 수: 94
- `castFrame` 수: 94
- `recoveryFrame` 수: 94

Legacy 등록/혼합 후보:

- `SkillDef` 관련 등록 파일: 17
- `ChampionDef` 관련 등록 파일: 12
- `SkillDef` 관련 라인: 238
- `ChampionDef` 관련 라인: 188
- visual 필드 혼합 후보 라인: 1095
- server object hardcode 후보 라인: 43
- projectile visual catalog 후보 라인: 27

해석:

아직 삭제 대상은 많이 남아 있다. 다만 이번 단계에서 삭제하지 않은 이유는 본질적으로 회귀 방지 때문이다. 기존 리더와 registration 경로를 바로 끊으면 챔피언 스폰, 스킬 애니메이션, FX, snapshot replay 경로가 동시에 흔들릴 수 있다.

## 빌드 검증

### Audit

명령:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/LoLData/Collect-LoLLegacyDataAudit.ps1 -OutputPath .md/TODO/06-22/LOL_DATA_LEGACY_AUDIT.json
```

결과: 통과

### Whitespace

명령:

```powershell
git diff --check
```

결과: 통과

비고:

LF가 CRLF로 바뀔 수 있다는 Git 경고만 있었다. whitespace error는 없었다.

### Solution Target 시도

명령:

```powershell
MSBuild.exe Winters.sln /t:GameSim /p:Configuration=Debug /p:Platform=x64 /m
```

결과: 실패

사유:

`Winters.sln`에는 `GameSim`이라는 solution target이 없어 `MSB4057`이 발생했다. 코드 오류가 아니라 호출 대상 지정 문제다. 이후 개별 `.vcxproj` 직접 빌드로 검증했다.

### GameSim

명령:

```powershell
MSBuild.exe Shared\GameSim\Include\GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과: 통과

- 경고: 17
- 오류: 0

주요 경고:

- 기존 UTF-8 문자 경고 `C4828`

### Server

명령:

```powershell
MSBuild.exe Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과: 통과

- 경고: 84
- 오류: 0

주요 경고:

- 기존 Engine/EngineSDK DLL interface 경고 `C4251`, `C4275`
- 기존 UTF-8 문자 경고 `C4828`

### Client

명령:

```powershell
MSBuild.exe Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과: 통과

- 경고: 105
- 오류: 0

주요 경고:

- 기존 EngineSDK DLL interface 경고 `C4251`, `C4275`
- 기존 UTF-8 문자 경고 `C4828`
- 기존 `std::async` 반환값 폐기 경고 `C4858`

### SimLab

명령:

```powershell
MSBuild.exe Tools\SimLab\SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과: 통과

- 경고: 0
- 오류: 0

## 회귀 판단

이번 변경은 기존 runtime reader를 교체하지 않았다.

따라서 현재 프레임 안에서 기존 코드가 데이터를 받아오고 읽는 흐름은 그대로 유지된다.

- Client visual registration 경로 유지
- `ChampionGameDataDB` 기존 조회 경로 유지
- Server GameSim 판정 경로 유지
- snapshot/event 경로 유지
- SimLab 링크 검증 통과

회귀 위험은 낮다. 새 계약 헤더와 감사 스크립트는 다음 단계의 분리 기준을 세우는 장치이며, 기존 실행 경로의 truth owner를 바꾸지 않는다.

## 남은 본질 작업

### S1 Seed

`ChampionGameData`에서 visual/timing 필드를 분리할 첫 seed를 만든다.

대상:

- `visualYawOffset`
- `animPlaySpeed`
- `castFrame`
- `recoveryFrame`

목표:

- gameplay truth는 `ChampionGameData`에 남긴다.
- client playback hint는 `ChampionVisualData` 또는 equivalent generated view로 옮긴다.
- 기존 reader parity test를 먼저 두고 전환한다.

### S2 Summoner Spell

`ChampionGameData.summonerSpells`를 `SummonerSpellGameData`로 분리한다.

이유:

- 챔피언의 본질은 챔피언 기본 규칙이다.
- 소환사 주문은 loadout/ruleset 선택이다.
- 챔피언 정의 안에 있으면 패치/모드/밴픽 정책과 섞인다.

### S3 SkillDef/ChampionDef Deletion Path

Client 등록 파일의 `SkillDef`, `ChampionDef`를 바로 삭제하지 말고 generated/adapter parity를 먼저 둔다.

순서:

1. 기존 table과 새 atom view가 같은 값을 내는지 비교한다.
2. 하나의 챔피언을 seed로 전환한다.
3. registration에서 gameplay 값을 제거한다.
4. visual hook과 FX cue만 남긴다.
5. 마지막에 `SkillTable`, `ChampionTable` 삭제 여부를 판단한다.

## 최종 판단

본질 기준으로 이번 단계는 "삭제"가 아니라 "경계 확정"이다.

서버가 진실을 소유하고, 클라이언트는 공개 힌트와 visual playback만 가진다는 북극성에 맞게 첫 계약을 만들었다. 동시에 잔존 혼합 데이터를 숫자로 고정했기 때문에, 다음 단계부터는 감이 아니라 audit 수치를 줄이는 방식으로 리팩터링할 수 있다.
