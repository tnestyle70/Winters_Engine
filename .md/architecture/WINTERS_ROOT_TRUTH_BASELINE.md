# Winters Root Truth Baseline

S0는 `Winters/` 최상위 항목의 원본 여부와 의미 소유자만 고정한다.

Winters는 단일 샘플 프로젝트가 아니라, AAA급 게임과 엔진 제작 규모까지 확장 가능한 구조를 목표로 한다. 기준은 라이엇식 대형 라이브 게임 조직, 프롬소프트식 액션 RPG 제작 조직, 붉은사막/BlackSpace Engine급 대형 월드 제작 엔진, GTA급 오픈월드 엔진, Unreal/Unity급 범용 엔진이 감당하는 협업 규모다.

이 문장은 현재 Winters가 그 규모의 기능을 이미 갖췄다는 뜻이 아니다. 폴더와 소유권을 다시 잡을 때 수십 명, 나아가 수백 명이 동시에 작업해도 원본, 산출물, 런타임, 도구, 데이터, 서비스의 책임이 섞이지 않는 구조를 목표 제약으로 둔다는 뜻이다.

S0에서 묻는 질문은 둘뿐이다.

```text
이 항목은 편집 원본인가?
편집 원본이면 누가 의미를 소유하는가?
```

S0에서 하지 않는 일:
- 코드 변경
- 파일 이동
- build graph 변경
- 자동화 script 추가
- generated output 직접 편집

## 판정값

| Truth | 뜻 |
|---|---|
| Source | 편집 원본이다. |
| Derived | 생성물, 빌드 산출물, 로컬 상태, 로그, 캡처다. |
| External | repo 안에 있지만 의미 소유자는 외부 dependency다. |
| ConfirmNeeded | 최상위 path만으로 단정하지 않는다. |

`Owner`는 `Truth=Source`일 때만 의미가 있다. `Derived`와 `External`의 owner는 `None`이다.

## 최상위 기준선

| Path | Truth | Owner |
|---|---|---|
| `.claude/` | Source | Docs |
| `.git/` | Derived | None |
| `.md/` | Source | Docs |
| `.toolkit/` | Source | Tools |
| `.vs/` | Derived | None |
| `Client/` | Source | ClientLoL |
| `cmake/` | Source | BuildGraph |
| `Data/` | Source | Data |
| `EldenRingClient/` | Source | EldenRingClient |
| `EldenRingEditor/` | Source | EldenRingEditor |
| `Engine/` | Source | Engine |
| `Engine/Bin/` | Derived | None |
| `Engine/External/` | External | None |
| `Engine/ThirdPartyLib/` | External | None |
| `EngineSDK/` | Derived | None |
| `out/` | Derived | None |
| `Profiles/` | Derived | None |
| `Replay/` | Derived | None |
| `Server/` | Source | ServerAuthority |
| `Services/` | Source | Services |
| `Shaders/` | Source | Shaders |
| `Shared/GameSim/` | Source | SharedGameSim |
| `Shared/Network/` | Source | SharedNetwork |
| `Shared/Replay/` | Source | SharedReplay |
| `Shared/Schemas/` | Source | SharedSchemas |
| `Tools/` | Source | Tools |
| `Tools/Bin/` | Derived | None |
| `Tools/External/` | External | None |
| `Tools/Intermediate/` | Derived | None |
| `winters-skills/` | Source | Docs |
| `.gitignore` | Source | BuildGraph |
| `AGENTS.md` | Source | Docs |
| `CLAUDE.md` | Source | Docs |
| `CLAUDE_Legacy.md` | Source | Docs |
| `CMakeLists.txt` | Source | BuildGraph |
| `CMakePresets.json` | Source | BuildGraph |
| `imgui.ini` | Derived | None |
| `limgrave_lineup_log.txt` | Derived | None |
| `limgrave_showcase_log.txt` | Derived | None |
| `profiler.json` | Derived | None |
| `refactoring.md` | Source | Docs |
| `TODO.md` | Source | Docs |
| `UpdateLib.bat` | Source | BuildGraph |
| `Winters.sln` | Source | BuildGraph |
| `Winters.slnLaunch.user` | Derived | None |
| `winters_elven_showcase_capture.png` | Derived | None |

## 완료 조건

- 모든 최상위 항목은 `Truth` 하나만 가진다.
- `Truth=Source`인 항목은 `Owner` 하나만 가진다.
- `Derived`는 직접 편집하지 않는다.
- `External`은 Winters 의미 소유권으로 끌어오지 않는다.
- S0는 이 문서 하나만 만든다.
