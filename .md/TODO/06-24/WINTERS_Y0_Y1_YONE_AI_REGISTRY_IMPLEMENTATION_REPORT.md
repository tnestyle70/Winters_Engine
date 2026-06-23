# Winters Y0+Y1 Yone AI Registry Implementation Report

작성일: 2026-06-24

## 결론

Y0/Y1 반영 완료.

- Y0: `ExecuteLaneCombat`의 챔피언별 전술 분기점을 함수포인터 레지스트리로 교체.
- Y1: Yone 챔피언 전술을 레지스트리에 등록.
- Yone E 재시전 귀환은 `GameCommand`만 발행하며, 실제 상태 변경은 기존 `CommandExecutor -> YoneGameSim::OnE -> StartSoulReturn` 경로가 담당.
- `git diff --check`, `GameSim`, `Server`, 전체 `Verify-LoLDataDrivenPipeline.ps1` 검증 통과.

## 반영 파일

- `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`

## 본질 기준 구현 내용

### 1. 공용 AI는 챔피언 전술을 알지 않는다

`ExecuteLaneCombat`에서 `TryExecuteYasuoChampionCombat`를 직접 호출하던 구조를 제거하고, 챔피언 ID로 전술 함수를 찾는 레지스트리로 이동했다.

```cpp
using ChampionCombatTacticsFn = bool_t (*)(
    CWorld&, const TickContext&, EntityID, ChampionAIComponent&,
    ChampionComponent&, const Vec3&, const ChampionAIContext&,
    std::vector<GameCommand>&);
```

레지스트리 현재 항목:

- `eChampion::YASUO -> TryExecuteYasuoChampionCombat`
- `eChampion::YONE -> TryExecuteYoneChampionCombat`

이제 새 챔피언 전술은 `ExecuteLaneCombat` 본문을 다시 수정하지 않고 레지스트리 항목 추가로 연결할 수 있다.

### 2. Yone AI는 명령만 만든다

Yone 전술은 직접 HP, 위치, 쿨다운, 상태 컴포넌트를 변경하지 않는다.

발행하는 것은 `GameCommand`뿐이다.

- Q/W/R/첫 E: 기존 `EmitSkillCommand` 사용.
- E 재시전 귀환: `IsSkillReady`를 우회해 직접 `CastSkill` 커맨드 생성.

### 3. Yone E 귀환은 2단계 스킬 계약을 따른다

기존 `CommandExecutor` 계약상 2단계 재시전은 `cmd.itemId == 2u`다.

따라서 Yone이 `bSoulUnboundActive && !bReturning` 상태이고 귀환 조건을 만족하면:

- `cmd.kind = CastSkill`
- `cmd.slot = E`
- `cmd.itemId = 2u`
- `cmd.groundPos = YoneSimComponent.anchorPosition`
- `cmd.direction = currentPosition -> anchorPosition`

쿨다운 우회 자체는 AI가 임의로 성공시키는 것이 아니라, `CommandExecutor`가 stage window와 two-stage 정의를 검증한다.

### 4. Yone 귀환 판단

Yone은 영혼 분리 중 아래 조건 중 하나를 만족하면 E 재시전을 발행한다.

- `soulTimerSec <= 0.75f`
- 자기 HP가 재진입 기준 이하
- 적 포탑 위험 안에 있음
- 공격 대상이 사라짐
- 현재 교전 HP 교환이 불리함

이는 봇이 상태를 조작하는 것이 아니라, 전술적으로 "돌아갈 시점"을 판단하는 최소 단위다.

## 회귀 검증

### Whitespace

명령:

```powershell
git diff --check
```

결과:

- 통과
- CRLF 변환 경고만 출력

### GameSim 빌드

명령:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" Shared\GameSim\Include\GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과:

- 경고 0
- 오류 0
- `WintersGameSim.lib` 생성

### Server 빌드

명령:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

결과:

- 오류 0
- 기존 DLL interface 계열 경고 2개
- `WintersServer.exe` 생성

### Data Driven 전체 파이프라인

명령:

```powershell
powershell -ExecutionPolicy Bypass -File Tools\LoLData\Verify-LoLDataDrivenPipeline.ps1
```

결과:

- Definition pack freshness 통과
- Legacy ownership audit 출력
- Client visual timing parity mismatch 0
- GameSim 빌드 통과
- Server 빌드 통과
- Client 빌드 통과
- SimLab same-seed replay OK
- SimLab seed sensitivity OK
- Whitespace validation 통과
- 최종 `PASS`

## 남은 주의점

- Client/Server 빌드 중 `C4275` DLL interface 경고는 기존 EngineSDK `ISystem` 상속 경고이며 이번 변경으로 새로 생긴 오류는 아니다.
- Yone E 귀환 로그는 `yone-e-soul-return`로 남는다. 실제 귀환 성공 여부는 `CommandExecutor`의 `accept stage2`와 `YoneSim` 로그로 추적 가능하다.
- 다음 챔피언 전술 추가 시 공용 lane combat 본문을 직접 늘리지 말고 `kChampionCombatTacticsRegistry`에 전술 함수를 등록한다.
