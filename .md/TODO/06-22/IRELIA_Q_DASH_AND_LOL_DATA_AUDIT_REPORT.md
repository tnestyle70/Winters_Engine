# 이렐리아 Q 대시 / LoL 데이터 감사 반영 보고서

작성 시각: 2026-06-22

## 본질 원칙

- gameplay truth는 Shared/GameSim과 Server가 만든다.
- Client는 같은 규칙을 읽어 예측/표현만 한다.
- 같은 규칙을 서버와 클라이언트에 각각 숫자로 두지 않는다.
- 데이터 감사는 총량 하나가 아니라 소유 계층별 잔존 위치를 분리해서 본다.

## 반영 내용

### 이렐리아 Q 도착점 원자화

- `Shared/GameSim/Champions/Irelia/IreliaGameSim.h`
  - `IreliaGameSim::kQDashStopGap = 1.35f` 추가.
  - `IreliaGameSim::ResolveQDashEndPos(casterPos, targetPos)` 추가.
  - Q 대시의 더 나눌 수 없는 결과 규칙은 "대상 중심이 아니라 대상 앞 stop gap 위치"로 정의.

- `Shared/GameSim/Champions/Irelia/IreliaGameSim.cpp`
  - 서버 권위 Q 대시 도착점 계산을 위 helper로 교체.

- `Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp`
  - legacy/local visual dash도 같은 helper를 사용하도록 교체.
  - 기존에는 대상 중심점까지 가서 서버의 stop gap 위치와 어긋날 수 있었다.
  - 결과적으로 Q 애니메이션 중 대상 뒤로 넘어가 보이는 예측/표현 오차를 줄인다.

### LoL 데이터 감사 분리

- `Tools/LoLData/Collect-LoLLegacyDataAudit.ps1`
  - 없는 경로를 안전하게 건너뛰도록 `Invoke-RgLines`, `Invoke-RgFiles` 보강.
  - visual 관련 잔존 카운트를 `expectedClientPublicVisual`, `toolExtraction`, `legacyGameplaySource`, `clientLegacyRuntime`, `sharedGameSimAuthoritative`, `serverAuthoritative`로 분리.

- 생성 결과:
  - `.md/TODO/06-22/LOL_LEGACY_DATA_AUDIT.json`
  - `.md/TODO/06-22/LOL_VISUAL_TIMING_SEED_PARITY.json`

## 검증 결과

- `Tools/LoLData/Collect-LoLLegacyDataAudit.ps1`
  - 통과.
  - champion 17, skill stage 94.
  - visual breakdown:
    - expectedClientPublicVisual 188
    - toolExtraction 33
    - legacyGameplaySource 300
    - clientLegacyRuntime 168
    - sharedGameSimAuthoritative 601
    - serverAuthoritative 0

- `Tools/LoLData/Export-LoLChampionVisualTimingSeed.ps1`
  - 통과.
  - champion 17, skill stage 94, mismatch 0.

- `git diff --check`
  - 통과.

- `Shared/GameSim/Include/GameSim.vcxproj` Debug x64
  - 통과.

- `Server/Include/Server.vcxproj` Debug x64
  - 통과.
  - 기존 EngineSDK C4275 경고 2개.

- `Client/Include/Client.vcxproj` Debug x64
  - 통과.
  - 기존 EngineSDK C4275 경고 유지.

- `Tools/SimLab/SimLab.vcxproj` Debug x64
  - 통과.

- `Tools/Bin/Debug/SimLab.exe`
  - PASS.
  - same-seed replay hash: `67F2A97563B8DB04`
  - seed+1 hash: `5DA19645E291A29B`

## 남은 의심 지점

- `Data/Gameplay/ChampionGameData/champions.json`와 `Shared/GameSim/Generated/ChampionGameData.generated.*` 안에 visual timing/yaw 계열이 아직 남아 있다.
- 다음 단계는 client visual runtime reader를 실제 사용 경로로 붙인 뒤, GameSim authoritative data에서 visual timing/yaw를 제거하는 것이다.
- `SkillTable`, `ChampionTable`, champion registration의 legacy visual/gameplay 혼합은 아직 삭제 대상이다.
- `SummonerSpellGameData`는 `ChampionGameData.summonerSpells`에서 분리해야 한다.

## 다음 패치 방향

1. `ChampionVisualData` client reader를 runtime 경로에 붙인다.
2. `ChampionGameData`에서 visual timing/yaw를 제거한다.
3. `SummonerSpellGameData`를 독립 generated data로 분리한다.
4. `SkillTable`, `ChampionTable` 의존 호출을 제거하고 registry/bootstrap을 새 데이터 소유권으로 교체한다.
5. F5 실제 플레이에서 이렐리아 Q를 대상 정지/이동 상태 모두로 확인하고, 필요하면 nav/collision stop gap을 data 값으로 승격한다.
