# 16. 협업 / 툴링 / 빌드 — 면접 대비 세션

> 도메인 상태: **working** (정직성 지도 #16 기준)
> 근거 코드: `Tools/SimLab/main.cpp`, `Tools/Harness/Run-S17RhiValidation.ps1`, `Tools/Harness/Run-BotAiValidation.ps1`, `Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1`, `cmake/*.cmake`, `CMakePresets.json`, `CMakeLists.txt`
> 근거 문서: `.md/collab/OWNERSHIP_MATRIX.md`, `.md/collab/HARNESS_RULES.md`, `.md/TODO/05-19/CMAKE_NINJA_MIGRATION_GUIDE.md`

---

## 0. 한 줄 본질 + 현재 상태

**한 줄 본질**: "여러 머신·여러 AI 에이전트가 같은 엔진 repo를 동시에 수정해도 (a) GameSim 결정론이 깨지지 않고 (b) Client/Engine/Shared 의존성 경계가 무너지지 않고 (c) 빌드 산출물이 재현 가능하도록, 사람의 규율 대신 **실행 가능한 게이트(headless sim runner + PowerShell 검증 하니스 + CMake/Ninja 빌드)**로 강제하는 도메인."

**현재 성숙도 (혼재 — 정직하게)**:
- **production/working**: SimLab 결정론 골든 러너(`Tools/SimLab/main.cpp`, exit code 게이트), PowerShell 검증 하니스 2종(`Run-S17RhiValidation.ps1`, `Run-BotAiValidation.ps1`), CMake/Ninja Multi-Config 빌드(`Engine` + `EldenRingClient` + `EldenRingEditor`).
- **부분/병렬 운영**: CMake 전환은 Engine + Elden 계열만 완료. **LoL Client/Server는 여전히 `.vcxproj`/MSBuild**. → "점진/병렬 전환 단계".
- **사람 규율(코드 아님)**: `OWNERSHIP_MATRIX.md`의 소유권 자체는 게이트가 아니라 합의 문서. 자동 강제되는 건 **의존성 audit(rg)뿐**.
- **레드플래그(즉사)**: **Perforce 경험 아님.** 코드에 Perforce 전무, `P4`는 전부 "Phase 4" 약자. 관련 Perforce 문서는 `.md/TODO/05-15/07_PERFORCE_P4_WORKFLOW_DEMO.md` 같은 **계획서뿐**(`SourceControlBridge`도 "새 파일을 만든다"는 미래형). 면접에서 절대 Perforce를 입에 올리지 않는다.

---

## 1. 핵심 개념 (본질)

이 도메인은 "빌드 스크립트 좀 짰다"가 아니라 **분산 협업 환경에서 소프트웨어 정합성을 유지하는 문제**다. 면접관에게 가르치듯 1차 원리부터.

### 1.1 결정론(Determinism)이 왜 협업 게이트인가

서버권위 멀티플레이 게임의 시뮬레이션은 **같은 입력 + 같은 초기 상태 + 같은 코드 → 같은 결과**여야 한다(determinism). 이게 깨지면:
- 서버/클라가 같은 틱을 다르게 계산 → 위치·HP 불일치(desync).
- 재현(replay)·리플레이·서버 사이드 검증 불가.

협업 관점에서 더 중요한 건: **두 사람이 각자 GameSim 코드를 고친 뒤 머지했을 때, 결정론이 조용히 깨지는 회귀를 어떻게 자동 탐지하느냐**다. 사람 눈으로는 "60초 시뮬을 두 번 돌렸더니 미묘하게 다르다"를 못 잡는다. 그래서 **상태를 해시로 압축해서 비교**한다.

- 1차 원리: 시뮬레이션 전체 상태(위치·HP·골드·레벨·RNG state)는 결국 바이트 덩어리다. 이걸 **FNV-1a 같은 비암호 해시로 fold**하면 64비트 지문이 된다.
- same-seed 두 런의 per-tick 해시 수열이 완전히 일치 → 결정론 OK.
- **seed를 바꿨는데 해시가 같다 → 해시가 실제로 상태를 캡처 못 하고 있다는 뜻**(false positive 방지). 이 "음성 대조군"이 있어야 게이트가 신뢰된다.

### 1.2 비결정론의 출처 (왜 강제 규약이 필요한가)

결정론을 깨는 흔한 원인:
1. **부동소수점/실행순서**: 컨테이너를 해시 순서(주소 순)로 순회하면 머신·런마다 순서가 달라진다 → 엔티티는 **EntityID 정렬 순회**로 강제.
2. **시간**: `std::chrono::now()` 같은 wall-clock 사용 → **고정 dt(fixed timestep)**로 대체.
3. **난수**: 표준 RNG는 구현 의존 → **xorshift 류 결정론 RNG(seed 명시, state 직렬화 가능)**로 대체.

코드 근거: SimLab은 `DeterministicRng`(seed), `DeterministicTime::kFixedDt`, `DeterministicEntityIterator`(정렬 순회)를 강제 사용한다(`Tools/SimLab/main.cpp:262,306,437`). 해시에 `rng.GetState()`까지 fold(`:437`)해서 RNG 발산도 잡는다.

### 1.3 의존성 경계(dependency boundary)가 왜 빌드 게이트인가

Winters는 Client / Engine / Shared / Server 레이어 구조다. 핵심 규칙: **하위 레이어(Engine)의 concrete 구현(DX11/DX12)이 상위/공용 헤더(Client/Public, Shared, 공용 RHI public header)로 누출되면 안 된다.**

왜 1차 원리로 위험한가:
- `Client/Public`이 `ID3D11Device*`를 들고 있으면 → Client가 DX11에 컴파일타임 결합 → 백엔드 교체(DX12) 불가, RHI 추상화가 무의미해진다.
- DLL 경계에서 concrete 그래픽스 타입이 public ABI에 들어가면 컴파일 단위마다 다른 레이아웃을 볼 수 있다.

사람 코드리뷰로는 매번 놓친다. 그래서 **`rg`(ripgrep)로 금지 심볼 패턴을 audit하고, 매치가 0건이 아니면 빌드를 FAIL** 시킨다. 이게 "린트로서의 grep"이다.

### 1.4 빌드 시스템: MSBuild vs CMake/Ninja, 그리고 `.vcxproj.filters`의 본질

- **MSBuild + `.vcxproj`/`.vcxproj.filters`**: Visual Studio 네이티브. 문제는 `.filters` 파일이 **수기 XML로 폴더 트리를 표현**한다는 것. 파일 하나 추가/이동할 때마다 사람이 XML을 고쳐야 하고, 두 머신이 동시에 고치면 **머지 컨플릭트의 단골**이 된다.
- **CMake + Ninja**: 빌드 그래프를 선언적으로 기술. `source_group(TREE ...)`가 **실제 디스크 폴더 구조에서 VS 필터를 자동 생성**한다 → 수기 `.filters` 편집이 사라진다. Ninja는 파일 기반 그래프로 incremental/parallel 빌드가 빠르고, CLI 빌드에 filter 파일 자체가 필요 없다.
- **Multi-Config(Ninja Multi-Config)**: 단일 configure로 Debug/Release를 모두 빌드할 수 있는 generator. `CMAKE_CONFIGURATION_TYPES=Debug;Release`(`CMakePresets.json:15`).

근거: `WintersEngine.cmake`의 `WintersEngineSourceGroup()`는 정규식으로 파일을 도메인 폴더(`02. RHI\01. DX11` 등)로 매핑하고(`:131-145`), `WintersWorkspaceMap.cmake`는 `source_group(TREE ...)`로 repo 전체를 브라우징용 트리로 생성(`:144-170`).

---

## 2. 왜 이 선택인가 — 기술 스택 선택 이유 + Trade-off

### 2.1 결정론 검증: 헤드리스 sim runner vs 풀 게임 e2e vs 유닛테스트

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **헤드리스 GameSim runner(SimLab)** ← 채택 | 그래픽/네트워크 없이 시뮬 코어만 빠르게(초 단위) 재현 비교, exit code로 CI 게이트화 | 실게임이 아닌 **FlatWalkable 평면 미러**(navgrid 없음), 렌더/넷코드 회귀는 못 잡음 | 협업에서 깨지기 가장 쉬운 게 **시뮬 결정론**이라 거기에 집중. 한계를 정직하게 인정(아래 레드플래그) |
| 풀 게임 e2e 자동화 | 실제 경로 검증 | 느리고 flaky, 헤드리스 불가, 머신마다 GPU 차이 | 1인 프로젝트 ROI 나쁨 |
| GoogleTest 류 유닛테스트 | 표준, 세밀 | 시스템 간 상호작용·전체 결정론은 못 봄 | SimLab의 골든 비교가 더 적합 |

근본 Trade-off: **"실게임과 100% 동일한 경로"를 포기하고 "빠르고 재현 가능한 시뮬 코어 미러"를 택함.** SimLab의 `RunMatch`는 `CGameRoom::Phase_SimulationSystems`의 GameSim-only 부분을 손으로 미러링한 것(`Tools/SimLab/main.cpp:384` 주석 "Mirror of CGameRoom::Phase_SimulationSystems"). → 게이트가 통과해도 "GameSim 코어 결정론"만 보장하지, 풀 라운드를 보장하지 않는다.

### 2.2 의존성 강제: 코드리뷰 vs 정적 분석 도구 vs rg audit

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **rg(ripgrep) 패턴 audit** ← 채택 | 0 의존성, 빠름, 패턴이 명확("`ID3D11|ID3D12|d3d11.h`"), 게이트화 쉬움 | 의미론이 아닌 텍스트 매칭(false negative 가능) | 신입 1인 + AI 협업 범위에서 "금지 심볼이 public에 들어왔나"는 텍스트 매칭으로 95% 잡힘. ROI 최고 |
| clang-tidy / include-what-you-use | 의미론적 정확 | 설정·빌드 통합 비용 큼, Windows/MSVC에서 셋업 마찰 | 과함 |
| 순수 사람 코드리뷰 | 유연 | AI 에이전트/멀티머신에선 매번 새고, 비결정적 | 게이트가 아님 |

근본 Trade-off: **정밀도(semantic)를 포기하고 단순함·속도·게이트성을 택함.** `Run-S17RhiValidation.ps1:181`의 패턴 `ID3D11|ID3D12|d3d11\.h|d3d12\.h|DX11Shader|DX11Pipeline`, `Run-BotAiValidation.ps1:246`의 `Client/|Renderer|ImGui|d3d11|...`.

### 2.3 빌드: MSBuild 유지 vs CMake 전면 전환 vs 병렬

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **CMake/Ninja + MSBuild 병렬 운영** ← 채택 | `.filters` 수기 편집 제거(자동 생성), Ninja 빠른 빌드, 기존 `Winters.sln`을 검증 기준으로 유지 | 두 빌드를 둘 다 통과시켜야 함(이중 비용) | 한 번에 갈아엎으면 normal F5 흐름이 깨짐. 마이그레이션 가이드가 "1~2주 병렬"을 명시(`CMAKE_NINJA_MIGRATION_GUIDE.md:104`) |
| CMake 전면 전환 | 단일 소스 오브 트루스 | LoL Client/Server까지 한 번에 = 리스크 큼 | Engine+Elden부터 점진 |
| MSBuild 고수 | 익숙 | `.filters` 컨플릭트 문제 그대로 | 협업 마찰 못 줄임 |

근본 Trade-off: **이중 빌드 유지 비용**을 감수하고 **점진 전환의 안전성**을 택함. 그래서 하니스가 **CMake/Ninja 빌드와 MSBuild 빌드를 둘 다 step으로** 돌린다(`Run-S17RhiValidation.ps1:349,357`).

---

## 3. 실제 구현 (코드 근거)

### 3.1 SimLab — 결정론 골든 러너 (`Tools/SimLab/main.cpp`)

**자료구조/데이터 흐름**:
- `MatchResult { std::vector<u64_t> tickHashes; u64_t finalHash; }`(`:253-257`) — per-tick 해시 수열 + 전체 fold.
- `RunMatch(seed, tickCount)`(`:259`): 10명 챔피언 스폰(`kRoster`, `:267`) → 매 틱마다 (a) 결정론적 command 스크립트 생성 → (b) GameSim 시스템들 순서대로 Execute → (c) 상태 해시.
- 해시는 FNV-1a 상수(`1469598103934665603ull`/`1099511628211ull`, `:120-127,420`)로 각 챔피언의 pos.xyz/HP/dead/mana/level/gold + **`rng.GetState()`**(`:437`)를 fold. RNG state까지 넣는 게 핵심 — RNG 발산을 즉시 잡는다.

**결정론 강제 지점**:
- `DeterministicRng rng(seed)`(`:262`), `DeterministicTime::kFixedDt`(`:306`), `EntityIdMap`로 net id 발급(`:247`). 시스템 tick 순서가 고정(`:385-418`).
- `FlatWalkable`(`:86-118`): navgrid를 평면으로 대체 → **이게 "실게임이 아니다"의 핵심 한계**(IsWalkableXZ가 항상 true).

**판정 로직**(`main()`, `:603-659`):
1. `runA = RunMatch(seed)`, `runB = RunMatch(seed)`, `runC = RunMatch(seed+1)`.
2. `runA.finalHash != runB.finalHash` → **same-seed 발산 = FAIL**, 첫 발산 틱 출력(`:625-635`).
3. `runA.finalHash == runC.finalHash` → **seed 민감도 실패 = FAIL**(해시가 상태를 안 잡음, `:646-650`).
4. exit code 0/1로 게이트화(`:658`).

**골든 + 시나리오 프로브 혼합**: `RunYoneEReturnProbe()`(`:501-600`)는 단순 해시 비교가 아니라 **Yone E 2단(soul unbound → stage-2 return) 상태기계가 anchor로 복귀하는지**를 단언(assert)하는 시나리오 테스트. 스킬 상태기계 회귀를 구체적으로 잡는다.

### 3.2 검증 하니스 (`Tools/Harness/*.ps1`)

**`Run-S17RhiValidation.ps1`** — RHI/SceneRenderer 작업용. step 순서:
1. `git diff --check`(whitespace/conflict marker, `:330`)
2. **의존성 audit 2건**: `Client/Public`+`Shared`에서 concrete 그래픽스 심볼 0건(`:332`), 공용 RHI/Resource public header에서 0건(`:337`). `Invoke-RgNoMatchStep`(`:174`)은 **rg exit code 1(매치 없음)=PASS, 0(매치 있음)=FAIL**로 반전 해석.
3. **CMake/Ninja 빌드**: `WintersEngine WintersElden WintersEldenRingEditor`(`:348`).
4. **MSBuild 빌드**: `Winters.sln`(`:357`) — 이중 빌드.
5. **runtime smoke**(`:209-279`): 5개 exe를 `-WindowStyle Hidden`으로 띄우고 `$SmokeSeconds`(기본 8초) 생존 확인 → 살아있으면 PASS 후 정리(CloseMainWindow→Stop-Process), 즉시 죽으면 FAIL. DX12/DX11 두 경로(`--scene=probe`, `--rhi=dx11`)와 `--rhi-scene-only` 게이트까지 포함.
6. **타임스탬프 report 자동 생성**: `.md/build/{stamp}_S17_RHI_VALIDATION_HARNESS_REPORT.md`(`:18-21,281-327`) — step별 PASS/FAIL/exit/seconds + output tail.

**`Run-BotAiValidation.ps1`** — Bot AI/ChampionAI/CommandExecutor/champion GameSim 작업용:
1. `git diff --check`.
2. **ChampionAI 경계 audit**(`:243`): `Shared/GameSim/Systems/ChampionAI`가 `Client/|Renderer|ImGui|d3d11|...`에 의존하면 FAIL → **"AI는 command-only, 클라/렌더에 의존 금지" 규칙을 코드로 강제**.
3. **데이터-코드 contract 교차검증**(`Invoke-YoneEContractAudit`, `:138-193`): JSON(`SkillGameplayDefs.json`의 `skill.yone.e`)의 `stage.count>=2`, `stage.windowSeconds>0`와, AI 코드(`ChampionAISystem.cpp`의 `itemId=2u` emit)와, CommandExecutor(`bRequestedStage2 = cmd.itemId == 2u`)가 **세 곳 모두 일관**한지 검사. 데이터·AI·실행기가 따로 놀면 잡는다.
4. `Verify-LoLDataDrivenPipeline.ps1` 호출(`:251`).

### 3.3 데이터 파이프라인 검증 (`Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1`)

- **Definition pack freshness**: `Build-LoLDefinitionPack.py --check`(`:35`) — JSON→codegen 산출물이 최신인지(stale codegen = FAIL).
- Legacy ownership / goal status / raw asset path / visual timing parity audit(`:38-49`).
- MSBuild로 `GameSim/Server/Client/SimLab` 빌드(`:51-61`) → **SimLab.exe 실행으로 결정론 회귀**(`:64`) → 최종 `git diff --check`.

### 3.4 CMake 빌드 구조 (`cmake/*.cmake`, `CMakePresets.json`)

- `CMakeLists.txt`: Engine + `EldenRingClient` + `EldenRingEditor`만 build target(`:24-27`). LoL Client/Server 없음(MSBuild 잔존).
- `WintersEngine.cmake`: `WintersEngine`을 SHARED DLL로(`:61`), PCH(`:78`), `/utf-8` 등 공통 옵션(`WintersApplyMsvcCommonOptions`), `WintersEngineSourceGroup()`로 도메인별 VS 필터 자동 매핑(`:131-539`), POST_BUILD로 `UpdateLib.bat` 배포(`:544-551`).
- `WintersWorkspaceMap.cmake`: `source_group(TREE ...)`로 repo 전체(Client/Server/Engine/Shared/Shaders/Data/Services/Tools)를 브라우징용 트리 target으로 생성(`:144-170`) — **빌드 안 하는 LoL Client/Server도 VS에서 보이게**.
- `WintersCompilerOptions.cmake`: `/W3 /sdl /permissive- /utf-8 /FS`(`:5-11`), `MultiThreaded$<Debug>DLL` 런타임(`:25-27`).

### 3.5 Ownership Matrix (사람 규율, 코드 아님)

`.md/collab/OWNERSHIP_MATRIX.md`: 두 장비(Laptop/Desktop) 기본 소유 경로 표 + **Always-Lock Files**(한 packet에서만 수정: `Scene_InGame.h`, `ModelRenderer.h`, `Mesh.h`, `*.vcxproj`, `EngineSDK/inc/**`). **자동 강제는 의존성 audit뿐**이고, 소유권 분담 자체는 합의/규율이다(정직성 지도가 명시).

---

## 4. 검증 — 동작을 어떻게 증명했나

"됐다"의 판정 기준을 **exit code와 report**로 객관화했다.

1. **결정론**: SimLab이 same-seed 두 런의 per-tick 해시 수열 완전 일치(발산 시 첫 발산 틱 출력) + seed+1이 다른 해시 → exit 0. CI/하니스가 exit code로 게이트.
2. **음성 대조군**: seed 민감도 검사(`runA==runC면 FAIL`)로 "해시가 진짜 상태를 캡처하는가"를 역으로 증명. 골든 테스트의 흔한 함정(상수만 해시해서 항상 통과)을 차단.
3. **시나리오 회귀**: `RunYoneEReturnProbe`가 2단 스킬 상태기계의 anchor 복귀를 단언 — 단순 해시로는 안 잡히는 "의미 있는 게임플레이 불변식"을 명시 검사.
4. **의존성 경계**: rg audit이 `Client/Public`/`Shared`/공용 RHI header에서 DX concrete 심볼 0건임을 매 하니스 run마다 증명.
5. **데이터-코드 정합**: Yone E contract audit이 JSON·AI·CommandExecutor 3원 일관성을 검사.
6. **빌드 정합**: CMake/Ninja와 MSBuild **이중 빌드**가 둘 다 통과해야 PASS → 한쪽 빌드 시스템만 깨지는 회귀를 잡음.
7. **런타임 생존**: smoke가 5개 exe를 N초 생존/즉사 여부로 판정(즉사=크래시 회귀).
8. **재현 가능 기록**: 모든 하니스가 `.md/build/{timestamp}_*_HARNESS_REPORT.md`에 step별 결과+output tail을 남겨 "언제 무엇이 통과/실패했나"를 사후 추적.

> 정직 포인트: 이건 자동 CI 서버(GitHub Actions 등)가 아니라 **로컬에서 사람이 호출하는 PowerShell 게이트**다. "CI 파이프라인 구축"이라고 과장하지 않는다. "재현 가능한 검증 하니스 + report"가 정확한 표현.

---

## 5. 최적화

### 실제로 한 것
- **하니스 fail-fast**: 첫 실패에서 중단(`HARNESS_RULES.md:38`)해서 전체 빌드/smoke를 헛돌리지 않음.
- **문서-only 변경 단축 경로**: `-SkipRuntimeSmoke`(S17), `-SkipFullPipeline`(Bot AI)로 빌드 영향 없는 변경은 무거운 step을 건너뜀(`HARNESS_RULES.md:23,57`). 단, RHI/Renderer/scene code 수정 시엔 생략 금지를 규칙으로 명문화.
- **Ninja Multi-Config**: 단일 configure로 Debug/Release 빌드, incremental 빌드로 변경분만 재컴파일.
- **SimLab 규모 파라미터화**: `tickCount`/`seed`를 인자로(기본 1800틱=60초). 빠른 게이트는 짧은 틱, 정밀 검증은 긴 틱.

### 정량 수치
- **정직하게: 측정값 없음.** 빌드 시간/하니스 소요를 정량 비교한 수치는 정직성 지도에 없다. 하니스 report에 step별 `seconds`가 기록되긴 하지만(`Add-StepResult`의 Seconds), "MSBuild 대비 Ninja N% 빨라짐" 같은 비교 측정은 안 했다 → **"측정 예정"**.

### 계획 중인 최적화
- CMake LoL Client/Server 전환 완료 시 **MSBuild 이중 빌드 제거** → 하니스 빌드 시간 절반 목표(측정 후 판단).
- 하니스를 GitHub Actions 등 **진짜 CI로 승격**해서 머지 전 자동 게이트화(현재는 수동 호출).

---

## 6. 구현 예정 (Planned) — 동일한 깊이로

> 사용자는 실제로 구현할 것이므로, 구현된 부분과 같은 수준으로 설계한다.

### 6.1 CMake 전면 전환 (LoL Client/Server)

- **무엇**: 현재 MSBuild에 남은 LoL `Client`/`Server`/`Shared/GameSim`을 CMake target으로.
- **왜**: `.vcxproj.filters` 수기 편집이 LoL 쪽에 여전히 남아 머지 컨플릭트의 단골(`OWNERSHIP_MATRIX.md`의 Always-Lock에 `Client.vcxproj.filters`, `Engine.vcxproj.filters`가 들어있는 이유). 이중 빌드 유지 비용 제거.
- **어떻게(설계)**: 마이그레이션 가이드(`CMAKE_NINJA_MIGRATION_GUIDE.md:84-107`)의 전환 순서대로 — (1) `Shared/GameSim`을 `OBJECT`/`INTERFACE` 라이브러리로 분리(Server/Client/SimLab가 같은 source list 공유, 중복 제거), (2) `Server` target(FlatBuffers codegen을 `add_custom_command`로), (3) `Client` target(DX11/Assimp/DirectXTK/FMOD를 `IMPORTED` target으로 `WintersThirdParty.cmake`에 모음), (4) `Tools/WintersAssetConverter`.
- **Trade-off 예상**: 명시형 `target_sources()` vs `file(GLOB CONFIGURE_DEPENDS)`. 가이드는 "기본 명시형, 추가가 잦은 폴더만 제한적 GLOB"(`:12`)을 권고 — GLOB는 새 파일 자동 인식이 편하지만 configure 트리거/누락 추적이 어렵다. Engine은 이미 GLOB를 씀(`WintersEngine.cmake:50`)이라 균형은 폴더 변동성에 따라.
- **검증**: 가이드의 완료 기준(`:131-137`) — Ninja Server/Client Debug 빌드 성공, VS generator로 필터가 실제 폴더대로, 기존 `Winters.sln` 산출물과 차이 설명 가능, 신규 파일 추가 시 `.filters` 편집 불필요. **S17 하니스에 CMake step으로 `WintersServer`/`WintersGame` target 추가**해서 이중 빌드 게이트 유지하다가, 안정화 후 MSBuild step 제거.

### 6.2 SimLab을 실게임 경로에 더 가깝게 (navgrid 미러)

- **무엇**: `FlatWalkable`(평면) 대신 실제 NavGrid를 SimLab에 주입하는 옵션.
- **왜**: 현재 SimLab은 navgrid 없는 평면 미러라 **pathfinding/벽 충돌이 얽힌 결정론 회귀를 못 잡는다**. 이게 가장 큰 한계(레드플래그).
- **어떻게**: Engine `NavGrid` 베이크 산출물을 헤드리스로 로드하는 `IWalkableQuery` 구현을 SimLab에 추가, `--navgrid` 플래그로 선택. baked navgrid는 결정론적 입력이므로 해시 비교 그대로 유효.
- **Trade-off**: 실경로에 가까워지지만 SimLab이 더 무거워지고(로드 시간) Engine navgrid 베이크에 의존 → 빠른 게이트 성격 약화. → 평면(빠른) + navgrid(정밀) 두 모드 유지.
- **검증**: 동일 seed에서 평면/navgrid 각각 same-seed 재현 + seed 민감도. navgrid 모드에서 의도적 벽 통과 버그를 심어 발산이 잡히는지 mutation test.

### 6.3 의존성 audit을 의미론적으로 강화

- **무엇**: rg 텍스트 audit → include 그래프 기반(레이어 위반 탐지).
- **왜**: 텍스트 매칭은 우회 가능(typedef, 매크로). 레이어 규칙(Client→Engine OK, Engine→Client 금지)을 구조적으로 강제하고 싶음.
- **어떻게**: `#include` 그래프를 파싱(간단한 스크립트로 충분)해서 "상위→하위만 허용" 규칙 위반을 FAIL. 큰 도구(clang-tidy) 없이 시작.
- **Trade-off**: 정확↑이지만 파서 유지비용. rg audit은 남겨 빠른 1차 방어로.
- **검증**: 의도적 역의존 include를 넣고 FAIL 확인.

### 6.4 (참고) 소스 컨트롤 브리지 — Perforce는 **계획서뿐, 미구현**

- `.md/TODO/05-15/07_PERFORCE_P4_WORKFLOW_DEMO.md`에 `SourceControlBridge`(`CheckOut`/`MarkForAdd`/`GetFileState` MVP, `P4CommandLineProvider`+`NullSourceControlProvider` fallback)가 **"새 파일을 만든다"**로 적혀 있으나 **코드 0줄**. 면접에서 "구현했다"고 말하면 즉사. "바이너리 에셋 협업을 위해 P4 워크플로를 **문서로 설계**해 뒀고, 실제 협업은 Git 기반"이 정확한 선.

---

## 7. 면접 예상 질문 & 모범 답변 (12개)

**Q1. (기본) 결정론 시뮬레이션이 뭐고 왜 검증하나?**
A. 같은 seed·같은 입력·같은 코드면 항상 같은 결과가 나와야 한다는 성질입니다. 서버권위 게임에서 desync 방지와 재현/리플레이의 전제고, 협업 관점에선 두 사람이 GameSim을 각자 고친 뒤 머지했을 때 결정론이 조용히 깨지는 회귀를 사람 눈으로 못 잡기 때문에 자동 게이트가 필요합니다. 그래서 매 틱 상태(pos·HP·gold·level·RNG state)를 FNV-1a로 해시해 수열로 비교합니다.

**Q2. (기본) SimLab은 어떻게 "결정론이 깨졌다"를 판정하나?**
A. 같은 seed로 두 번 돌려 per-tick 해시 수열이 완전히 일치하면 OK, 다르면 첫 발산 틱을 출력하고 exit 1입니다. 추가로 seed를 +1 해서 돌렸을 때 해시가 **달라야** 합니다 — 같으면 해시가 상태를 제대로 캡처 못 한다는 뜻이라 그것도 FAIL로 잡습니다. 이 음성 대조군이 골든 테스트가 항상-통과로 썩는 걸 막습니다.

**Q3. (기본) 빌드에서 `.vcxproj.filters`를 왜 없애려 했나?**
A. `.filters`는 폴더 트리를 수기 XML로 표현해서, 파일 추가/이동마다 사람이 고쳐야 하고 두 머신이 동시에 건드리면 머지 컨플릭트의 단골입니다. CMake `source_group(TREE)`가 실제 디스크 폴더에서 VS 필터를 자동 생성하게 바꾸면 그 수기 편집이 사라집니다. Ninja CLI 빌드에선 filter 파일 자체가 불필요하고요.

**Q4. (설계 의도) 의존성 경계를 왜 코드리뷰가 아니라 rg audit으로 강제했나?**
A. 멀티머신·AI 에이전트 협업에선 사람 리뷰가 매번 샙니다. 핵심 규칙 — Engine의 DX11/DX12 concrete가 `Client/Public`이나 공용 RHI header로 누출되면 RHI 추상화가 무의미해지고 백엔드 교체가 불가능 — 은 텍스트로 명확히 잡힙니다. 그래서 `ID3D11|ID3D12|d3d11.h|...` 패턴을 rg로 audit하고 매치가 0건이 아니면 빌드를 FAIL시킵니다. clang-tidy는 1인 프로젝트엔 셋업 마찰이 과해서 ROI가 안 맞았습니다.

**Q5. (설계 의도) Bot AI 하니스의 Yone E contract audit은 뭘 검사하나? 왜 필요한가?**
A. 데이터(JSON)·AI 코드·실행기(CommandExecutor) 세 곳이 따로 노는 걸 막습니다. `skill.yone.e`의 `stage.count>=2`·`windowSeconds>0`, AI가 stage-2 복귀로 `itemId=2u`를 emit, CommandExecutor가 `bRequestedStage2 = cmd.itemId==2u`로 받는 것 — 이 셋이 일관해야 2단 스킬이 동작합니다. 데이터 주도로 가면 "JSON은 고쳤는데 코드 계약이 안 맞는" 회귀가 생기기 쉬워서 교차검증을 게이트로 넣었습니다.

**Q6. (adversarial) "Perforce 경험 있다고 적으셨던데, 실제로 쓰셨나요?"**
A. **아니요. 제 repo에 Perforce 코드는 한 줄도 없고, 협업은 Git 기반입니다.** 코드에 보이는 `P4`는 전부 "Phase 4"의 약자입니다. 다만 바이너리 에셋 협업(checkout/lock/changelist)에서 Perforce가 강점이라는 건 알아서, `SourceControlBridge` 인터페이스(`CheckOut`/`GetFileState`, P4 provider + Null fallback)를 **문서로 설계**해 뒀습니다. 구현은 안 했고, 만든다면 P4 미설치 환경에서도 graceful fallback되게 추상화부터 짤 겁니다. (이력서에 Perforce를 경험으로 적지 않습니다.)

**Q7. (adversarial) "CMake로 마이그레이션했다는데, 정말 전체가 CMake인가요?"**
A. 아니요, **Engine과 EldenRing 계열(Client/Editor)만 CMake/Ninja로 전환**했고 LoL Client/Server는 아직 `.vcxproj`/MSBuild입니다. 그래서 하니스가 **CMake/Ninja 빌드와 MSBuild 빌드를 둘 다** step으로 돌려 이중 검증합니다. 한 번에 갈아엎으면 normal F5 흐름이 깨져서, 마이그레이션 가이드대로 1~2주 병렬 운영하며 점진 전환 중입니다. 다음은 `Shared/GameSim`을 OBJECT 라이브러리로 분리해 Server/Client가 공유하게 만드는 단계입니다.

**Q8. (adversarial) "SimLab이 결정론을 보장하면 실게임도 결정론인가요?"**
A. 그건 과장입니다. SimLab은 **navgrid 없는 평면(`FlatWalkable`)에서 GameSim 코어만 미러링**한 러너라, pathfinding·벽 충돌·렌더·넷코드가 얽힌 결정론은 못 잡습니다. 제가 보장하는 건 "서버 시뮬 코어의 결정론"으로 한정됩니다. 그래서 다음 단계로 baked NavGrid를 주입하는 `--navgrid` 모드를 설계해 두었고, 실경로에 가까운 결정론까지 게이트를 넓힐 계획입니다.

**Q9. (adversarial) "이거 CI 파이프라인인가요? 누가 자동으로 돌리나요?"**
A. 솔직히 **자동 CI 서버는 아니고, 로컬에서 사람이 호출하는 PowerShell 검증 하니스**입니다. 첫 실패에서 멈추고, step별 PASS/FAIL/exit/소요시간과 output tail을 타임스탬프 report로 남겨 재현 가능하게 만든 게 핵심입니다. GitHub Actions 같은 진짜 CI로 승격해 머지 전 자동 게이트화하는 게 다음 작업입니다. "CI 인프라 구축"이라고는 말하지 않습니다.

**Q10. (adversarial) "Ownership Matrix로 소유권을 자동 강제한다고요?"**
A. 자동 강제되는 건 **의존성 audit(rg)뿐**입니다. 소유권 분담표와 Always-Lock 파일 목록 자체는 두 장비가 머지 컨플릭트를 줄이기 위한 **합의/규율**이지, 빌드가 막아주진 않습니다. 정직하게 구분하면 "경계 위반은 게이트가 잡고, 소유권은 사람이 지킨다"입니다.

**Q11. (심화) FNV-1a 해시를 골든 비교에 쓸 때 약점은? 충돌은?**
A. FNV-1a는 암호 해시가 아니라 64비트 fold라 이론상 충돌 가능성은 있지만, 회귀 탐지 용도에선 무시할 수준입니다(서로 다른 상태가 같은 64비트로 충돌해 발산을 놓칠 확률이 극히 낮음). 더 현실적인 약점은 **해시에 안 넣은 필드는 못 잡는다**는 것 — 그래서 pos·HP·dead·mana·level·gold에 더해 `rng.GetState()`까지 fold합니다. RNG state를 넣는 게 발산을 가장 빨리 드러내는 trick입니다. 그리고 seed 민감도 검사로 "해시가 진짜 상태를 보는가"를 역검증합니다.

**Q12. (심화/확장) 이 협업 인프라를 팀 규모로 키운다면 무엇부터?**
A. 세 가지입니다. (1) 하니스를 GitHub Actions로 올려 PR마다 자동 게이트 — 사람 호출 의존 제거. (2) rg 텍스트 audit을 include 그래프 기반 레이어 검사로 강화해 우회를 막음. (3) CMake 전면 전환으로 빌드 소스 오브 트루스를 단일화하고 `.filters` 컨플릭트를 구조적으로 제거. 공통 철학은 "규율을 문서가 아니라 실행 가능한 게이트로 내린다"이고, 지금 인프라가 그 방향으로 이미 절반쯤 와 있습니다.

---

## 8. 30초 엘리베이터 피치

"여러 머신이랑 여러 AI 에이전트가 같은 엔진 repo를 동시에 고치다 보면 두 가지가 조용히 깨집니다 — 시뮬레이션 결정론이랑 레이어 의존성 경계요. 그래서 저는 이걸 사람 규율이 아니라 **실행 가능한 게이트**로 내렸습니다. 헤드리스 5v5 시뮬 러너 SimLab이 같은 seed 두 런의 per-tick 해시를 비교하고 RNG state까지 fold해서 결정론 회귀를 exit code로 잡고, PowerShell 하니스가 ripgrep으로 DX concrete가 public 헤더에 새는지 audit하면서 CMake/Ninja랑 MSBuild를 이중 빌드하고 데이터-코드 계약까지 교차검증합니다. 솔직히 자동 CI는 아니고 로컬 게이트고, CMake 전환도 Engine·Elden만 끝나 LoL은 MSBuild 병행 중입니다 — 근데 그 경계를 report로 정확히 그어두는 게 제 강점입니다."
