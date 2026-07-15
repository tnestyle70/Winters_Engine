# 세 엔진의 개별 특징과 설계 철학 — Winters / Unreal / Unity 심층 비교

작성일: 2026-07-10. Winters 자체 엔진 경험을 축으로 언리얼(UE 5.6/5.7)·유니티(Unity 6)의 정체성과 설계 트레이드오프를 대조하는 기술면접·툴 포트폴리오 대비 참조 문서다. 외부 사실은 출처 도메인을 괄호로 병기하고, 이 세션에서 웹 검증되지 않은 일반 공학 지식은 (일반 지식), 검증 시도 후 확인 실패했거나 2차 출처뿐인 항목은 (미검증)으로 표기한다. 마켓플레이스(Fab/Asset Store) 상세는 자매 문서 `.md/engines/03_MARKETPLACES_TOOLS_COLLABORATION.md`가 소유하므로 여기서는 설계 철학과 맞닿는 지점만 다룬다.

---

## §1 언리얼 엔진 — "엔진 = 에디터 = 프레임워크"의 일체형 설계

### 1.1 정체성

언리얼은 엔진·에디터·게임 프레임워크가 하나의 코드베이스로 통합된 일체형 제품이다. 에디터 전체가 자사 C++ UI 프레임워크 Slate로 작성되어 있고, 사용자가 만드는 에디터 툴도 같은 Slate 위젯 체계 위에 선다 (dev.epicgames.com). C++ 소스는 GitHub로 공개되지만(EULA 동의 필요, (일반 지식)), 실무적으로는 런처 배포 바이너리 엔진("Rocket 빌드")만으로 C++ 프로젝트 생성·플러그인 개발·`RunUAT BuildPlugin -Rocket` 패키징까지 전부 가능하고, 소스 빌드는 엔진 코드 자체의 수정이나 풀 심볼 심층 디버깅에만 필요하다 (forums.unrealengine.com). 단, 런처 엔진으로 컴파일한 플러그인 바이너리는 그 엔진 버전에 정확히 결속되어 커스텀/소스 빌드 엔진에서는 재컴파일 없이 동작하지 않는다 (forums.unrealengine.com). "가져다 쓰는 블랙박스"와 "고쳐 쓰는 오픈 코드" 사이를 사용자가 선택할 수 있다는 점이 유니티와의 1차 차별점이다.

### 1.2 UObject 리플렉션 / GC / UPROPERTY — 왜 만능 기반인가

(일반 지식) 언리얼의 모든 상위 기능은 하나의 기반 위에 서 있다: `UCLASS`/`UPROPERTY`/`UFUNCTION` 매크로를 UnrealHeaderTool(UHT)이 파싱해 리플렉션 메타데이터 코드를 생성하고, 이 메타데이터가 다음을 동시에 구동한다.

| 상위 기능 | 리플렉션이 하는 일 (일반 지식) |
|---|---|
| GC | 리플렉션된 UObject 포인터 그래프를 루트에서 mark-and-sweep 추적 — C++인데 GC가 가능한 이유 |
| 직렬화 (.uasset) | 프로퍼티 목록을 순회하며 자동 저장/로드, 버전 마이그레이션 |
| 에디터 Details 패널 | `UPROPERTY` 메타(EditAnywhere, Category 등)로 편집 UI 자동 생성 |
| Blueprint 노출 | `BlueprintCallable`/`BlueprintReadWrite`로 스크립팅 표면 자동 개방 |
| 네트워크 복제 | `Replicated` 프로퍼티 자동 동기화 + RPC |

이 세션에서 검증된 부분은 에디터 쪽 착지점이다. Details 패널 커스터마이징은 클래스 단위 `IDetailCustomization`과 구조체/프로퍼티 타입 단위 `IPropertyTypeCustomization`으로 나뉘고, 리플렉션된 프로퍼티를 `IPropertyHandle`이라는 트랜잭션 래퍼로 감싸 그 핸들을 통해 쓰기만 하면 undo/redo와 멀티 오브젝트 편집까지 보존된다 (dev.epicgames.com). 즉 "리플렉션에 한 번 등록하면 GC·저장·에디터·스크립팅·네트워크가 공짜"라는 규모의 경제가 UObject의 본질이고, 대가는 이 생태계에 참여하려면 반드시 UObject를 상속해야 한다는 전면 종속이다.

### 1.3 모듈 시스템 — .uplugin / Build.cs / UBT

플러그인은 JSON 디스크립터 `.uplugin` + 모듈별 `Build.cs` + Public/Private 소스 폴더로 구성되고, UnrealBuildTool(UBT)이 이를 읽어 빌드한다 (dev.epicgames.com). 핵심은 모듈 `Type` 필드다: `Runtime`, `RuntimeNoCommandlet`, `Developer`, `Editor`, `EditorNoCommandlet`, `Program` 중 하나로 선언하며, **Editor 타입 모듈은 출시 게임에 절대 로드되지 않는다** (dev.epicgames.com). 그래서 상용 툴 플러그인의 표준형은 "에셋 클래스는 Runtime 모듈, 에디터 툴은 Editor 모듈"의 2-모듈 구성이다. 로딩 시점도 `ELoadingPhase`(EarliestPossible ~ Default ~ PostEngineInit ~ None)로 데이터 선언하며, 에디터 UI 플러그인은 통상 Default 또는 PostEngineInit을 쓴다 (dev.epicgames.com).

이 구조가 시사하는 바: Winters가 `.claude/gotchas.md`의 의존성 경계 규칙("EngineSDK/inc를 비-Engine 프로젝트 include path에 넣지 않는다")을 사람이 지키는 반면, UE는 같은 규칙을 **모듈 타입 + UBT가 기계 강제**한다. 에디터 전용 코드가 출시 빌드에 실리는 사고가 빌드 시스템 수준에서 불가능하다.

부속 사실 두 가지. 첫째, 신규 플러그인 다이얼로그는 7종 템플릿(Blank, Content Only, Blueprint Library, Editor Mode, Editor Standalone Window, Editor Toolbar Button, Third Party Library)을 제공하고, 위저드가 생성한 플러그인은 `<Name>.uplugin` + `Source/<Name>/<Name>.Build.cs` + `F<Name>Module`(IModuleInterface) / `F<Name>Commands`(TCommands) / `F<Name>Style`(FSlateStyleSet) 보일러플레이트를 갖는다 (dev.epicgames.com, 커뮤니티 교차 확인). 둘째, Live Coding(Ctrl+Alt+F11)은 .cpp 함수 본문 수정은 런타임 패치가 되지만, 공식 문서가 "Live Coding이 활성인 동안 Slate 윈도우 코드는 컴파일할 수 없다"고 명시하듯 헤더/생성자/Slate 변경은 에디터 종료 후 재빌드가 필요하다 (dev.epicgames.com) — 일체형 설계의 반복(iteration) 비용이 드러나는 지점이다.

### 1.4 Slate / UMG — 에디터 확장 표면의 사다리

Slate는 선언형(`SNew` + 슬롯 문법) 보존모드 C++ UI 프레임워크이고, 언리얼 에디터 전체가 이것으로 만들어졌다. UMG는 Slate 위의 UObject 래퍼다 (dev.epicgames.com). 에디터 확장 표면은 난이도 사다리를 이룬다 — 이 세션에서 각 단이 공식 문서로 검증됐다:

| 단계 | 표면 | 내용 (dev.epicgames.com) |
|---|---|---|
| a | Editor Utility Widget | UMG 위젯 블루프린트로 도킹 가능한 에디터 탭. C++ 없음, 수 분 내 가시 결과. Tools 메뉴에 등록 유지 |
| a' | Scripted Asset/Actor Actions | `AssetActionUtility`/`ActorActionUtility` 상속 + Call In Editor 함수 = 콘텐츠 브라우저/아웃라이너 우클릭 메뉴 항목 |
| b | Python 에디터 스크립팅 | 내장 Python 3.11.8, 에디터 전용(쿠킹 빌드/PIE 게임 로직 불가). 배치 에셋 처리·파이프라인 자동화 |
| c | C++ 플러그인 템플릿 | Editor Standalone Window 템플릿 → `RegisterNomadTabSpawner`로 노마드 탭 등록, `OnSpawnPluginTab()`이 `SDockTab` 반환 |
| d | raw Slate | 에디터 자체와 같은 위젯 체계. 진지한 에디터 툴의 필수 층 |
| e | 커스텀 에셋 타입 + `FAssetEditorToolkit` | §1.6 — Niagara/Behavior Tree/Material 에디터가 전부 이 패턴 |
| f | `IDetailCustomization` / `IPropertyTypeCustomization` | Details 패널 주입, `IPropertyHandle` 경유 쓰기로 undo 보존 |
| g | `UEditorSubsystem` + `UToolMenus` | 에디터 수명 공유 싱글턴 서비스 + 메뉴/툴바 확장(`ExtendMenu("LevelEditor.MainMenu.Tools")`) |

ImGui 즉시모드 툴을 만들어 온 입장에서 Slate는 "위젯 트리 + invalidation을 프레임워크가 관리하는 보존모드"라는 반대 진영이며, 에디터급 도킹/스타일/접근성을 얻는 대신 Slate 자체가 거대한 유지보수 대상이 된 것이 UE의 선택이다(`.md/interview/tool-development.md` §11).

### 1.5 Blueprint의 본질 — 바이트코드 VM, 그리고 한계

(일반 지식) Blueprint는 노드 그래프가 아니라 그 그래프를 컴파일한 **바이트코드를 UFunction 단위로 해석 실행하는 VM**이다. 리플렉션 기반이라 C++ 함수 호출·프로퍼티 접근이 자연스럽게 연결되고, 디자이너가 코드 배포 없이 로직을 조립할 수 있는 것이 존재 이유다. 한계도 본질에서 나온다 (일반 지식):

- 인터프리터 디스패치 비용 — 틱마다 도는 무거운 로직에는 부적합하고, 결정론이 필요한 시뮬레이션에 넣기 어렵다.
- Blueprint 에셋은 바이너리 uasset이므로 diff/merge가 사실상 불가 — 협업 시 잠금(lock) 운영으로 회피한다.
- 대형 그래프의 가독성 붕괴("스파게티") — 팀 규약 없이는 유지보수 부채가 코드보다 빠르게 쌓인다.

Blueprint 전용 프로젝트에는 플러그인 템플릿 중 Content Only만 제공된다는 사실 (dev.epicgames.com) 이 보여주듯, C++ 없는 개발은 가능하지만 확장 표면이 뚜렷하게 좁아진다. Winters가 Blueprint VM을 채택하지 않은 근거는 §3.6에서 다룬다.

### 1.6 uasset / DDC 파이프라인과 커스텀 에셋 타입

(일반 지식) 임포트된 모든 에셋은 UObject 직렬화 결과인 바이너리 `.uasset` 패키지가 되고, 플랫폼별 파생 데이터(셰이더 컴파일, 텍스처 압축 등)는 DDC(Derived Data Cache)에 (원본, 변환 설정, 버전)의 해시 키로 캐시되어 팀·CI가 공유한다. "임포트 결과(uasset)와 파생물(DDC)의 2단 분리"가 요지이며, Winters의 `.w*` cook이 "쿠킹 결과물은 (원본, 설정, 컨버터 버전)의 순수 함수"라는 같은 원리 위에 있다(`.md/interview/tool-development.md` §3).

새 에셋 타입을 1급 시민으로 만드는 패턴은 이 세션에서 검증됐다 (dev.epicgames.com, 커뮤니티 교차 확인):

1. 에셋 클래스 — 순수 데이터면 `UDataAsset`(팩토리 없이 콘텐츠 브라우저 생성 가능), AssetManager 비동기 로드가 필요하면 `UPrimaryDataAsset`, **자기 전용 에디터가 필요한 에셋(타임라인/그래프)이면 plain UObject** 파생.
2. `UFactory` 파생 — 우클릭 생성용 FactoryNew, 파일 드래그 임포트용 Factory, 선택적으로 Reimport.
3. 콘텐츠 브라우저 등록 — UE 5.2+부터 `UAssetDefinition`이 레거시 `FAssetTypeActions_Base`를 대체 (dev.epicgames.com). CDO 자동 등록 여부는 (미검증).
4. `FAssetEditorToolkit` 파생 — 탭 레이아웃/툴바/Slate 패널을 가진 전용 에디터. Epic 자신의 Niagara/Behavior Tree/Material 에디터가 전부 이 구조다.

이 패턴이 상용 Fab 툴과 사내 파이프라인 툴의 공통 골격이며, 어빌리티 타임라인 에디터의 UE 이식(로드맵 ③, `.md/interview/tool-development.md` §12)이 도달해야 할 지점이다. 파이프라인의 폐쇄성을 증언하는 검증 사실 하나: Fab 코드 플러그인은 소스만 제출하고 Epic 툴체인이 엔진 버전별로 바이너리를 컴파일·배포한다 (dev.epicgames.com, forums.unrealengine.com) — 바이너리 에셋/바이너리 호환성 관리를 플랫폼이 대신 짊어지는 구조다.

### 1.7 Niagara — System > Emitter > Module, 컴파일된 sim

Niagara는 UE의 주력 VFX 시스템으로, System(이펙트 컨테이너) > Emitter(스택 가능·재사용 가능한 파티클 발생기) > Module(기본 빌딩 블록)의 3계층 노드/스택 에디터로 저작하고, 시뮬레이션은 CPU/GPU sim 코드로 **컴파일되어** 실행된다 (dev.epicgames.com). 두 가지가 중요하다. 첫째, "그래프는 에셋, 런타임은 컴파일된 플랜"이라는 계약은 Winters WFX(`Engine/Public/FX/Graph/FxGraphValidator.h`의 검증 + `Engine/Private/FX/Exec/FxExecPlan.cpp`의 실행 플랜 컴파일)와 동형이다(`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §1). 둘째, Niagara는 툴 프레임워크가 아니라 §1.6 패턴(커스텀 에셋 + `FAssetEditorToolkit` + Slate 그래프 에디터)으로 **만들어진 결과물**이라 (dev.epicgames.com), 툴 개발자 포트폴리오가 도달해야 할 지점의 정답지 역할을 한다.

### 1.8 라이선스 / 로열티 모델

(일반 지식) 엔진 사용 자체는 무료이고, 게임 제품당 누적 총매출 100만 USD를 넘는 부분에 5% 로열티가 붙는다. 마켓플레이스는 2024-10-22 출시된 Fab으로 통합(구 UE Marketplace + Sketchfab Store + Quixel Megascans + ArtStation Marketplace 흡수)됐고, 수익 배분은 볼륨 무관 88/12 단일 요율이다 (unrealengine.com, dev.epicgames.com). "엔진은 공짜, 성공하면 나눈다 + 자산 생태계는 후한 배분으로 키운다"는 플랫폼 전략이다. Fab 제출 절차·리뷰·라이선스 티어 상세는 자매 문서 03이 다룬다.

### 1.9 강점 도메인과 대가

Sensor Tower 2025 리포트 기준 언리얼은 자체 엔진 진영의 점유율을 빠르게 흡수하며 AAA 스튜디오의 기본 선택지가 되고 있다 (sensortower.com). Steam 매출 기준 점유율이 유니티를 앞선다는 수치(~31% vs ~26%)는 2차 출처라 (미검증) 이지만, "언리얼 = 고예산·고매출 편중, 유니티 = 출시 수량 편중"이라는 방향성은 공식 리포트와 일치한다. 대가는 명확하다: 리플렉션 코드젠 + 모듈 시스템 + 일체형 에디터가 만드는 긴 빌드 시간과 학습 곡선, 헤더/Slate 수정 시 에디터 재시작 루프 (dev.epicgames.com), 그리고 "모든 것이 UObject 생태계를 통과해야 한다"는 구조적 복잡도다.

---

## §2 유니티 엔진 — 클로즈드 C++ 코어 + C# 래퍼

### 2.1 정체성

공식 매뉴얼이 명시한다: "The Unity engine is built with native C/C++ internally, however it has a C# wrapper that you use to interact with it" (docs.unity3d.com). 렌더러·물리·에셋 파이프라인 등 네이티브 코어는 클로즈드 소스이고, 게임 코드와 에디터 툴은 전부 C#으로 그 코어를 호출한다. 경계는 P/Invoke류 바인딩 + 마샬링으로 동작하며, 모든 `UnityEngine.Object`는 네이티브 C++ 객체의 얇은 C# 래퍼다 — 이 경계에서 유명한 "fake null"(네이티브는 파괴됐는데 C# 래퍼는 살아 있고 `==` 연산자가 오버로드된 상태)이 나온다 (일반 지식). 언리얼이 "소스를 열고 C++로 통합"했다면, 유니티는 "코어를 닫고 스크립팅 표면을 넓힌" 반대 방향의 설계다.

### 2.2 스크립팅 런타임 — Mono JIT vs IL2CPP AOT vs CoreCLR 로드맵

| 백엔드 | 방식 | 특성 (docs.unity3d.com) |
|---|---|---|
| Mono | JIT | 에디터 런타임이자 데스크톱 기본. 빌드 빠름, Reflection.Emit 등 동적 기능 가능. JIT 금지 플랫폼 사용 불가 |
| IL2CPP | AOT | Roslyn C#→IL→코드 스트리핑→**IL을 표준 C++로 변환**→플랫폼 네이티브 컴파일. 빌드 느리고 바이너리 크지만 호환성·성능 향상. Reflection.Emit 불가. iOS/콘솔 사실상 필수 |
| CoreCLR | (로드맵) | Unity 6.7에서 실험적 데스크톱 플레이어, **6.8에서 C# 레이어의 기반이 되며 Mono 제거** — .NET 10/C# 14, AppDomain 도메인 리로드를 Assembly Load Context(수정 코드 지점 리로드)로 대체. 로드맵이므로 일정은 변동 가능 (discussions.unity.com) |

CoreCLR 전환은 단순 런타임 교체가 아니라 "스크립트 수정 → 도메인 리로드 대기"라는 유니티 특유의 반복 비용 구조를 바꾸는 사건이라, 에디터 툴 개발자에게 직접적 이해관계가 있다. 유니티는 이 전환에 집중하기 위해 2026년 신규 애니메이션/월드빌딩 워크플로 기능을 일시 중단했다 (discussions.unity.com, digitalproduction.com). 부속: Asset Store 제출 가이드라인은 이미 "Editor 6.6부터 스크립트 포함 패키지는 Domain Reload 비활성 상태의 Fast Enter Playmode를 지원해야 한다"(2.5.h)로 이 방향을 선반영했다 (assetstore.unity.com).

### 2.3 Boehm 증분 GC와 zero-allocation 문화

유니티 관리 힙의 GC는 Boehm-Demers-Weiser 수집기이고 기본이 증분(incremental) 모드 — 수집 작업을 여러 프레임에 분할한다 (docs.unity3d.com). 비세대(non-generational)·비압축(non-compacting)이라 객체를 이동시키지 않으므로 힙 단편화가 누적될 수 있고, 증분 모드는 write barrier 오버헤드를 대가로 긴 stop-the-world를 회피한다 (docs.unity3d.com). Mono/IL2CPP 어느 백엔드든 같은 GC다. 이 GC의 한계가 유니티 성능 문화 전체를 규정했다: 프레임당 관리 할당 0바이트를 목표로 하는 코딩 관례, 오브젝트 풀링, struct 기반 API, 그리고 GC를 아예 우회하는 네이티브 컨테이너 + Burst의 DOTS가 그 산물이다 (docs.unity3d.com 기반 종합). Winters가 GC 없는 C++에서 명시적 수명 관리로 얻는 예측 가능성을, 유니티는 "관리 언어의 생산성 + 할당 회피 규율"이라는 우회로로 산다.

### 2.4 GameObject/Component 합성 + MonoBehaviour 라이프사이클

유니티의 씬 모델은 순수 합성(composition)이다: GameObject는 이름 + Transform + 컴포넌트 평면 리스트일 뿐, 언리얼 Actor처럼 상속할 게임플레이 클래스 트리가 없다. 사용자 코드는 `MonoBehaviour`(→ Behaviour → Component → Object 파생)를 상속해 컴포넌트로 붙이고, 엔진이 정해진 순서로 호출한다 (docs.unity3d.com):

```text
Awake → OnEnable → Start → [FixedUpdate + 물리 시뮬레이션, 프레임당 0~N회]
→ Update → LateUpdate → 애니메이션 콜백(OnAnimatorMove/IK)
→ 렌더 콜백(OnPreCull/OnPreRender/...) → OnDisable → OnDestroy
```

ECS 관점에서 보면 이것은 네이티브 엔진이 관리 경계를 넘어 인스턴스마다 가상 함수를 호출해 주는 구조 — 캐시 지역성과 병렬화가 구조적으로 막혀 있고, DOTS는 정확히 이 지점을 탈출하려고 만들어졌다.

### 2.5 YAML + GUID(.meta) 직렬화 — 툴링 접근성의 근본

씬(.unity)·프리팹(.prefab)·대부분의 에셋은 텍스트 UnityYAML로 직렬화된다. 파일 안의 각 객체는 클래스 ID(`!u!1` = GameObject, `!u!4` = Transform)와 로컬 fileID를 갖고, 모든 에셋은 최초 임포트 시 옆에 생기는 `.meta` 파일에 GUID를 부여받으며 `.meta`에는 임포터 설정도 함께 저장된다. 파일 간 참조는 (대상 파일 GUID, 파일 내 fileID) 쌍으로 기록된다 (docs.unity3d.com, unity.com). 이 설계의 귀결:

- 에셋을 이동/이름 변경해도 참조가 깨지지 않는다(GUID 간접 참조). 반대로 `.meta`를 잃으면 GUID가 재생성되어 참조가 조용히 전멸한다(핑크 머티리얼, missing script) (docs.unity3d.com).
- 씬/프리팹이 diff·merge·grep·외부 툴 생성이 가능한 **텍스트**다. 임포트 결과물은 Library 캐시에 격리되고 항상 원본+설정에서 재생성 가능하다 (docs.unity3d.com).
- 에디터 자체가 게임과 같은 C#으로 스크립팅되므로, 정적 메서드 하나가 곧 툴이다 (docs.unity3d.com):

```csharp
[MenuItem("Tools/Do Something")]
static void DoSomething() { Debug.Log("done"); }   // Assets/Editor 아래에 두면 끝
```

에디터 확장 사다리도 이 접근성 위에 선다 (docs.unity3d.com — 전 단 공식 문서 검증): `[MenuItem]` 원라이너 → `EditorWindow`(Unity 6 공식 권장은 UI Toolkit: `CreateGUI()` + UXML/USS, 우클릭 생성 템플릿 제공, 도메인 리로드 수 초 뒤 즉시 가동) → `[CustomEditor]`/`PropertyDrawer`(타입/필드 단위 인스펙터 커스텀 — Odin Inspector의 사업 전체가 이 표면의 산업화다, odininspector.com) → `ScriptedImporter`(`[ScriptedImporter(version, "확장자")]` + `OnImportAsset(ctx)`로 자체 파일 포맷을 1급 에셋화 — Winters AssetConverter의 유니티 등가물) → `EditorTool`/Overlays/Handles(씬 뷰 조작기) → UPM 패키지(단, 패키지 안의 Editor 폴더는 `Assets/Editor`와 달리 마법 폴더가 아니라 `.Editor.asmdef`의 includePlatforms=Editor가 필요하다). 편집 API 쪽 핵심은 `SerializedObject`/`SerializedProperty`가 "개별 직렬화 필드의 dirtying을 자동 처리해 undo 시스템과 프리팹 오버라이드 스타일링에 태워 준다"는 것 (docs.unity3d.com) — undo·멀티 편집·오버라이드 표시가 공짜다.

"영속 상태가 전부 안정적 신원(GUID+fileID)을 가진 텍스트 + 에디터와 게임이 같은 언어 + 직렬화 API가 undo까지 내장"이라는 3중 속성이, 언리얼의 바이너리 uasset 세계와 대비해 유니티 에디터 툴링이 압도적으로 접근하기 쉬운 근본 이유다(연구 종합 분석).

### 2.6 ScriptableObject

ScriptableObject는 "클래스 인스턴스와 독립적으로 정보를 저장하는 데이터 컨테이너"로, GameObject에 붙지 않고 `.asset` 프로젝트 에셋으로 저장된다. 공식 용례는 프리팹 인스턴스 간 데이터 공유(메모리 절약), 에디터 세션 데이터 영속화, 런타임에 읽는 재사용 데이터 에셋이고, 배포 빌드에서는 새 데이터를 저장할 수 없는 읽기 전용이라 세이브 시스템이 아니라 저작 데이터 전용이다 (docs.unity3d.com). 아이템/스킬 정의 카탈로그, 에셋 기반 이벤트 채널 같은 상용 툴·아키텍처의 데이터 백본이며 (unity.com), 언리얼 UDataAsset, Winters 정의 팩(authoring 소스 → cook)의 유니티 등가물이다. 차이는 편집 즉시성이다: SO는 인스펙터에서 라이브 편집되는 반면, Winters는 서버 권위 수치 보호를 위해 "authoring 소스 역기록 후 재cook"을 강제한다(`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §2).

### 2.7 Prefab / Variant

프리팹은 GameObject 계층 템플릿이고, 인스턴스는 원본과의 링크 + 인스턴스별 오버라이드 목록을 유지한다. Prefab Variant는 "베이스 프리팹의 프로퍼티를 상속하고, variant의 오버라이드가 베이스 값에 우선하며, variant를 다른 variant의 베이스로 쓸 수 있는" 구성 상속 메커니즘이다 (docs.unity3d.com). 클래스 파생(언리얼 Blueprint 상속)이 아니라 **직렬화된 오버라이드 리스트**로 상속을 구현했다는 점이 유니티답다 — 데이터가 텍스트이므로 오버라이드의 실체가 파일에서 눈으로 보인다.

### 2.8 DOTS — Entities / Jobs / Burst, 자작 ECS와의 비교

DOTS는 Entities(아키타입 기반 ECS, 청크 메모리 레이아웃) + C# Job System(의존성·레이스 검출 안전장치가 있는 멀티스레드 잡) + Burst(IL을 LLVM으로 네이티브 최적화 컴파일) 스택이다 (unity.com). 청크 크기 16KB, 구조 변경은 EntityCommandBuffer로 sync point에서 처리한다는 세부는 (일반 지식). 2025-12 공식 발표 기준: Entities/Collections/Mathematics/Entities Graphics가 Unity 6.4부터 에디터 동봉 코어 패키지가 되고(Physics/Netcode는 2026년 합류), int InstanceID를 대체하는 `EntityId` 타입이 GameObject와 Entity를 단일 신원으로 통합하며, GameObject에 ECS 컴포넌트 데이터를 붙이는 "ECS for All" 하이브리드 전략이 장기 방향이다 (discussions.unity.com 공식 포스트). 채택 현실은 하이브리드가 다수다 — Cities: Skylines 2 등 출시 실적은 있으나, Jobs+Burst는 넓게, 순수 Entities 게임플레이는 좁게 쓰이고, 팀 적응에 3~6개월이 든다는 수치는 2차 출처라 (미검증).

Winters 자작 ECS와의 대조: Entities는 flecs/EnTT 아키타입 계열에 가까운 반면, Winters `Shared/GameSim`의 컴포넌트는 `static_assert(std::is_trivially_copyable_v<...>)`로 POD성을 컴파일 타임에 강제하는(`Shared/GameSim/Components/CombatActionComponent.h` 48행 등) 스냅샷 복제 최적화 설계다. 유니티 ECS의 제1목적이 캐시 지역성/병렬화(성능)라면, Winters ECS의 제1목적은 memcpy 가능한 상태 = 결정론 해시와 와이어 복제(네트워크 권위)라는 점이 같은 "ECS"라는 단어 아래의 다른 철학이다.

### 2.9 렌더링 — SRP(URP/HDRP) + RenderGraph

Scriptable Render Pipeline은 렌더 파이프라인 오케스트레이션(컬링, 패스 스케줄링, 드로우 제출)을 사용자 가시 C# 레이어(RenderPipelineAsset + RenderPipeline)로 끌어올린 프레임워크이고, URP(모바일~콘솔 확장형)와 HDRP(하이엔드 PC/콘솔, 디퍼드 기본, 레이트레이싱)가 공통 SRP Core 위에 선다 (docs.unity3d.com). Unity 6에서 URP는 RenderGraph — 렌더 패스와 리소스 사용을 선언하면 엔진이 수명/패스 병합을 관리하는 프레임 그래프 — 가 기본 활성화됐다 (docs.unity3d.com). DX11 즉시 컨텍스트 렌더러를 직접 짠 입장에서 보면 유니티가 Frostbite류 프레임 그래프 패턴 (일반 지식) 으로 수렴한 것이며, 대신 Built-in/URP/HDRP 3파이프라인 분열로 셰이더·에셋이 파이프라인 종속이라는 생태계 세금이 존재한다.

### 2.10 VFX — Shuriken vs VFX Graph

공식 비교 페이지 기준 (docs.unity3d.com):

| 항목 | Built-in Particle System (Shuriken) | VFX Graph |
|---|---|---|
| 시뮬레이션 | CPU | GPU 컴퓨트 |
| 규모 | 수천 개 | 수백만 개 |
| 월드 상호작용 | 유니티 물리 직접 통합 | 사용자 정의 요소(HDRP depth-buffer 충돌 등) |
| C# 접근 | 파티클별 읽기/쓰기 | 그래프 프로퍼티만 노출 |
| 파이프라인 | Built-in/URP/HDRP 전부 | URP/HDRP 전용 |

VFX Graph가 유니티의 Niagara 등가물이라는 프레이밍은 업계 통념이며(분석), Winters WFX가 참조하는 "그래프 에셋 + 컴파일/GPU 실행" 계보에 둘 다 속한다.

### 2.11 사업 모델 — 런타임피 취소 이후

논란의 런타임 피(설치당 과금)는 시행 전인 2024-09-12에 취소됐다 (unity.com). 대체 경제학: Unity Pro +8%(연 $2,200/시트), Enterprise +25%, Personal은 무료 유지에 매출 상한이 $200K로 2배 상향 + 스플래시 스크린 선택화 (unity.com, cgchannel.com). 버전 체계는 연도명에서 Unity 6.x로 전환 — 6.0 LTS(2024-10), 6.1(2025-04), 6.3 LTS(2025 말, 2027-12까지 지원), 차기 LTS는 6.7 (unity.com, discussions.unity.com). Asset Store는 70/30 배분에 게시 수수료 없음, 최저가 $4.99, 신규 제출 리뷰 약 10영업일 (assetstore.unity.com, support.unity.com) — Fab의 88/12와 대비되는 지점이지만, 에디터 툴의 구매자 전원이 유니티 생태계 안에 있으므로 발견성(discovery)은 Asset Store가 우위다(연구 종합 분석). 상세 비교는 자매 문서 03.

### 2.12 강점 도메인

Sensor Tower 2025 기준 Steam 출시 게임의 절반 이상이 유니티 제작이며, 상단은 언리얼에 하단은 Godot에 압박받는 중간 포지션이다 (sensortower.com). 모바일 최상위 매출 게임의 ~70%가 유니티라는 수치는 2차 출처라 (미검증). 요약하면 유니티의 본진은 모바일·인디·볼륨 시장이고, 그 본진을 지탱하는 것이 §2.5의 접근성(텍스트 직렬화 + C# 단일 언어 + 에디터 스크립팅)이다.

---

## §3 Winters 엔진 — 서버 권위 결정론을 제1원칙으로 삼은 자체 DX11 엔진

### 3.1 정체성

Winters는 DX11 자체 엔진 위에 LoL 클론(서버 권위 멀티플레이)과 EldenRing 수직 슬라이스(에디터 방향)를 얹은 1인 주도 프로젝트다. 계층은 Engine(DLL — 렌더/ECS/리소스, RHI 계층 분리 진행 중 `Engine/Private/RHI/`) / Client / Server / Shared·GameSim(결정론 시뮬레이션)으로 나뉘고, 경계 규칙은 `.md/architecture/WINTERS_CODEBASE_COMPASS.md`와 `.md/architecture/WINTERS_DEPENDENCY_MAP.md`가 소유한다. 상용 엔진이 "범용성"을 파는 제품이라면, Winters는 "이 게임 장르(서버 권위 대전)의 불변식"에 모든 설계를 종속시킨 특화 엔진이다.

### 3.2 서버 권위 30Hz 결정론 GameSim — SimLab 해시 검증

시뮬레이션 시간은 `Shared/GameSim/Core/Determinism/DeterministicTime.h` 10행의 `static constexpr uint64_t kTicksPerSecond = 30`으로 고정된 정수 틱이고, 난수는 `Shared/GameSim/Core/Determinism/DeterministicRng.h`의 시드 기반 RNG다. 결정론은 주장하는 것이 아니라 기계로 검증한다 — `Tools/SimLab/main.cpp` 2행의 자기 선언이 계약이다:

```cpp
// Same seed + same scripted command stream => identical per-tick state hashes.
u64_t hash = 1469598103934665603ull;            // FNV-1a offset basis (420행)
HashU64(hash, tick);
// 엔티티별 pos.x/y/z, hp.fCurrent, bIsDead, mana, level, gold ...
HashU64(hash, rng.GetState());                  // RNG 내부 상태까지 해시에 포함
```

같은 시드의 두 실행이 틱별 해시 열까지 동일해야 PASS이고, 반대 방향의 자기 검증도 있다: "다른 시드가 같은 해시를 내면 해시가 상태를 못 잡고 있는 것"이라는 네거티브 테스트가 별도 FAIL 경로로 존재한다(`Tools/SimLab/main.cpp` 649행). 와이어 프로토콜은 FlatBuffers 스키마(`Shared/Schemas/Snapshot.fbs`, `Command.fbs`, `Event.fbs`, `Hello.fbs`)이고, 복제 경계의 verify 실패는 bare return으로 삼키지 않고 유계 트레이스/카운터를 남긴다는 규칙이 박제되어 있다(`.claude/gotchas.md` 2026-07-09 — 스키마 드리프트가 "네트워크 멈춤"으로 위장하는 것을 방지).

이 해시 체계가 §3.6에서 Blueprint VM을 거부하는 근거의 실물이다 — 해시가 곧 회귀 테스트이므로, 해시를 깨뜨릴 수 있는 비결정 요소(인터프리터, 프레임레이트 종속, 부동 컨텍스트 차이)는 시뮬레이션 층에 들어올 수 없다.

### 3.3 trivially-copyable ECS + 스냅샷 복제

GameSim 컴포넌트는 POD성이 컴파일 타임 강제된다 — `Shared/GameSim/Components/CombatActionComponent.h` 48행, `AttackChaseComponent.h` 23행 등 챔피언별 컴포넌트까지 전수 `static_assert(std::is_trivially_copyable_v<...>)`가 걸려 있고, 클라이언트 스모크 테스트(`Client/Private/GamePlay/SharedGameSimSmoke.cpp` 50~54행)도 같은 단언을 이중으로 확인한다. 서버는 `Server/Private/Game/SnapshotBuilder.cpp`가 월드 상태를 스냅샷으로 직렬화하고, 클라이언트는 `Client/Private/Network/Client/SnapshotApplier.cpp`가 적용한다 — 컴포넌트가 memcpy 가능하므로 스냅샷 생성·적용·해시가 전부 단순하고 빠르다.

경계도 코드로 지킨다: Shared 코드가 Engine ECS 헤더를 직접 include하지 않도록 어댑터 계층(`Shared/GameSim/Core/Ecs/`, `Core/World/World.h`)을 두고, `Tools/Harness/Check-SharedBoundary.ps1`이 직접 include를 GameSim 빌드 실패로 만든다(`.claude/gotchas.md` 2026-07-09). 이 모델은 유니티 DOTS의 unmanaged 컴포넌트 제약, 언리얼의 리플렉션 기반 프로퍼티 복제와 "복제 가능한 상태의 형태를 타입 시스템으로 강제한다"는 문제의식을 공유하되, 강제 수단이 다르다: UE는 리플렉션 메타데이터, 유니티는 아키타입 청크 규약, Winters는 `static_assert`라는 가장 저렴하고 직접적인 도구를 쓴다.

부속 설계 하나가 세 엔진 비교에서 유용하다: 스킬 타이밍 데이터가 **시각(클라이언트)과 판정(서버)으로 이원화**되어 있다. 연출 타이밍은 `Shared/GameSim/Definitions/SkillDef.h`의 `visualCastFrame`/`visualRecoveryFrame`(값 저작은 `Client/Private/GameObject/Champion/*/`의 `*_Registration.cpp` 14종), 권위 판정 타이밍은 `Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h`의 틱 기반 `ResolveBasicAttackWindupTicks`가 소유한다 — 언리얼 GAS의 몽타주(연출)와 게임플레이 판정의 분리, 유니티 Timeline(연출)과 게임 로직의 분리에 해당하는 구획을 자체 스키마로 구현한 것이다.

### 3.4 .w* cook 파이프라인 + Validator 게이트 철학

오프라인 쿠킹은 `Tools/WintersAssetConverter/WintersAssetConverter.vcxproj`(엔트리는 `Engine/Private/Tools/AssetConverter/main.cpp`, Assimp 링크)가 FBX/GLB를 `.wmesh/.wskel/.wanim/.wmat`으로 변환하고, 런타임 `CWMeshLoader`(`Engine/Public/AssetFormat/Mesh/WMeshLoader.h`)는 파일 블롭 내부를 가리키는 zero-copy 포인터로 GPU에 직행한다(실적: FBX/GLB 27개 전수 변환 FAIL=0, 이렐리아 FBX 60MB → .wmesh 1.2MB — `.md/interview/tool-development.md` §3). 공통 헤더 `Engine/Public/AssetFormat/Common/WintersFileHeader.h`는 magic("WINT") + major/minor 버전 + 기능 flags + content_size를 `pack(1)` + `static_assert(sizeof==16)`로 고정했고, 스테이지 포맷(`Shared/GameSim/Definitions/MapDataFormats.h`)은 v2→v5 실진화 + `STAGE_VERSION_MIN_COMPAT=3` 하위 호환을 유지한다.

철학의 핵심은 "Validator가 게이트다"이다(`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §2): 자산이 게임에 들어가는 유일한 경로는 cook된 `.w*` + 정의 팩이고, 런타임 프레임 코드는 원본 포맷(JSON/FBX)을 절대 읽지 않으며, 검증 실패는 조용한 폴백이 아니라 "무엇이, 어느 파일이, 왜"로 즉시 보고되고, 불합격 자산은 카탈로그에 사유와 함께 남는다. FX 쪽은 이 철학이 이미 코드다 — `CFxGraphValidator::Validate`가 노드별 이슈 목록(error/warning)과 위상 정렬 순서(topoOrder)를 반환하고(`Engine/Public/FX/Graph/FxGraphValidator.h`), 실행은 컴파일된 플랜(`Engine/Private/FX/Exec/FxExecPlan.cpp`)만 소비한다 — Niagara의 "그래프 에셋 → 컴파일된 sim" 계약과 동형. 자기 식별된 약점도 기록되어 있다: `.wmesh`에 버전 필드가 없고(공통 헤더 미경유 경로) 저장이 비원자적(temp+rename 부재)이라는 점은 2026-07-10 UE5.7 감사에서 HIGH로 분류되어 에디터 코어 선행 과제로 편입됐다(`.md/interview/tool-development.md` §4, Q35).

### 3.5 ImGui 툴 계층

Winters의 모든 저작·튜닝 UI는 ImGui이고(엔진 통합은 `Engine/Public/Editor/ImGuiLayer.h`), 실물 인벤토리는 다음과 같다:

| 툴 | 경로 | 역할 |
|---|---|---|
| LoL 맵 에디터 | `Client/Private/Scene/Scene_Editor.cpp` | 배치/NavGrid 브러시/`RenderNavGridOverlay` 오버레이/Ctrl+S 저장 — 게임과 같은 `IScene` 형제 씬 |
| FX 라이브 튜너 | `Client/Public/UI/EffectTuner.h` | 메모리 값 직접 튜닝(핫 리로드의 최소형) |
| 스킬 타이밍 튜너 | `Client/Private/UI/SkillTimingPanel.cpp` | `g_SkillTable`(SkillDef) 슬라이더 편집 → 소스 역기록 워크플로 — 어빌리티 타임라인 에디터의 직계 조상 |
| WFX 패널 | `Client/Public/UI/WfxEffectToolPanel.h` | FX 그래프 툴 패널 |
| EldenRing 에디터 셸 | `EldenRingEditor/Public/EldenRingEditorScene.h` | Viewport/ContentBrowser/WorldCell/Outliner/Details/Transaction/FxGraph/Sequencer/WorldPartition/BossTesting/Log 11패널 UE 스타일 셸 |
| 트랜잭션 코어 | `EldenRingEditor/Public/World/EditorTransaction.h` | `IEditorCommand(Do/Undo/Name)` + undo/redo 스택 + 구체 커맨드 3종 |

경계 규칙이 계층을 지탱한다: "ImGui는 툴/튜너 전용이고 런타임 HUD가 아니다", "에디터는 normal F5 런타임을 우회하지 않는다", "라이브 튜닝 값은 실험값이고 영속은 authoring 소스 역기록 + 재cook뿐"(`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §2, §4). UE의 Slate 사다리(§1.4), 유니티의 C# 에디터 사다리(§2.5)에 대응하는 Winters의 확장 표면은 "Engine SDK를 소비하는 별도 실행 파일/씬 + ImGui 패널"이며, 프레임워크 유지비 대신 레이아웃/스타일 상한을 지불한다.

### 3.6 의도적으로 채택하지 않은 것 — 아키텍처 근거

`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §5가 고정한 세 가지 거부와 근거:

| 거부 대상 | 근거 | 대체물 (실물 경로) |
|---|---|---|
| Blueprint 런타임 VM | 서버 권위 30Hz 결정론 틱에 인터프리터 로직이 들어오면 SimLab 해시 검증 체계가 무너진다 | 수치는 generated pack, 분기 로직은 champion×variant 함수 테이블 `Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h`, UI 연출만 Lua |
| UObject GC/리플렉션 전면 도입 | trivially-copyable 컴포넌트 + 스냅샷 복제 모델과 충돌 — GC 추적 포인터 그래프는 memcpy 복제와 양립 불가 | 리플렉션이 필요한 곳(에디터 프로퍼티 그리드)만 정의 팩 스키마에서 필드 메타를 코드젠 |
| uasset 파이프라인 호환 | 유지비가 이득을 압도 | Fab 구매 기준을 "소스 포맷(FBX 등) 포함"으로 고정하고 `.w*` cook 단일 경로로 수용 |

"무엇을 안 만들었는지를 엔진 불변식으로 설명할 수 있는가"가 이 절의 존재 이유다 — 툴 기능은 전부 유지보수 부채이므로, 인기 있는 기능이라도 결정론/스냅샷 복제라는 불변식과 충돌하면 거절한다(면접 문답 형태는 `.md/interview/tool-development.md` Q25).

### 3.7 2026-07-10 감사 메타패턴 — "생각은 현업급, 강제가 사람 의존"

UE 5.7 풀소스와의 7차원 대조 감사가 내린 결론: Winters의 규칙(경계, 계약, 검증 철학)은 현업 수준이지만, 그 규칙의 **강제 장치가 사람·문서·수동 grep에 의존**한다. UE는 같은 규칙을 모듈 타입/UBT/매크로/CI가 기계 강제한다(§1.3의 Editor 모듈 격리가 전형). 네트워크 권위 모델은 약점 0건이었던 반면, minidump 부재·CI 부재·버전 핸드셰이크 미강제·include 경계 미강제·ABI 드리프트·ensure류 부재가 HIGH로 잡혔고, 로드맵은 P0(배선만)→P1(강제형 구조)→P2(콘텐츠), 툴은 에디터 코어 선행(트랜잭션 공용 승격 + AtomicWriteFile + Validator 저장 경로 배선 + 격리 프리뷰 월드) → 어빌리티 타임라인 에디터 → 언리얼 이식 → Fab 순서로 확정됐다(`.md/interview/tool-development.md` §11~12, Q31). 이 메타패턴 자체가 세 엔진 비교의 결론을 예고한다: 상용 엔진의 본질은 개별 기능이 아니라 **규칙을 기계로 강제하는 인프라**다.

---

## §4 철학 축 비교표

| 축 | Unreal (UE 5.6/5.7) | Unity (Unity 6) | Winters |
|---|---|---|---|
| 객체 모델 | UObject 단일 뿌리 + Actor 상속 트리, UHT 리플렉션 코드젠 (일반 지식) | GameObject/Component 순수 합성, MonoBehaviour 라이프사이클 (docs.unity3d.com) | trivially-copyable POD 컴포넌트 ECS, `static_assert`로 형태 강제 (`Shared/GameSim/Components/`) |
| 메모리 전략 | UObject 전용 mark-and-sweep GC + 네이티브 수동 관리 병존 (일반 지식) | Boehm 증분 GC(비세대·비압축) + zero-alloc 문화 + DOTS 탈출구 (docs.unity3d.com) | GC 없음, 명시적 수명 + POD 컴포넌트 — 결정론과 스냅샷이 요구하는 형태 |
| 직렬화 | 바이너리 uasset(리플렉션 자동 직렬화) + DDC 파생 캐시 (일반 지식) | 텍스트 YAML + GUID(.meta) + fileID — diff/merge/grep 가능 (docs.unity3d.com) | 오프라인 cook `.w*` 바이너리(magic/version/flags 헤더) + JSON authoring 2단 (`Engine/Public/AssetFormat/`) |
| 스크립팅 | C++ 네이티브 + Blueprint 바이트코드 VM (일반 지식) | C# 전면(Mono JIT/IL2CPP AOT → CoreCLR 로드맵) (docs.unity3d.com) | C++ 전면, 데이터 분기는 GameplayHookRegistry 함수 테이블, Lua는 UI 한정 |
| 렌더링 | 일체형 고급 C++ 렌더러(Nanite/Lumen 세대) — 수정은 소스 빌드 (일반 지식) | SRP: C# 오케스트레이션 + URP/HDRP + RenderGraph 기본화 (docs.unity3d.com) | DX11 직접 제어 자체 렌더러 + RHI 계층 분리 진행 (`Engine/Private/RHI/`) |
| VFX | Niagara: System>Emitter>Module 그래프, 컴파일된 CPU/GPU sim (dev.epicgames.com) | Shuriken(CPU, 수천)/VFX Graph(GPU, 수백만, URP/HDRP 전용) (docs.unity3d.com) | WFX: 그래프 검증(`CFxGraphValidator`) → 컴파일된 실행 플랜, 결정적 FX와 시각 FX 구조 분리 |
| 네트워킹 | Actor/프로퍼티 복제 + RPC 내장, 서버 권위 프레임워크 (일반 지식) | Netcode for GameObjects/Entities 패키지 (unity.com) | FlatBuffers 스냅샷+커맨드, 30Hz 결정론 틱, SimLab 해시 검증 (`Shared/Schemas/`, `Tools/SimLab/`) |
| 툴 UI | Slate 보존모드(에디터 자체가 Slate) + UMG/EUW 사다리 (dev.epicgames.com) | C# 에디터 스크립팅 + UI Toolkit(UXML/USS) 권장 (docs.unity3d.com) | ImGui 즉시모드 전면 — 툴/튜너 전용, 런타임 HUD 금지 |
| 확장 표면 | .uplugin 모듈(Editor/Runtime 타입 분리) + 커스텀 에셋 + AssetEditorToolkit (dev.epicgames.com) | Assets/Editor 폴더·UPM 패키지·asmdef — 컴파일 한 번이면 툴 (docs.unity3d.com) | Engine SDK 소비자 실행 파일(EldenRingEditor) + Tools/ 독립 exe — 경계는 vcxproj include path |
| 마켓 | Fab 88/12, 코드 플러그인은 소스 제출·Epic이 컴파일 (dev.epicgames.com) | Asset Store 70/30, $4.99 하한, 큐레이션 리뷰 (assetstore.unity.com) | 판매 대상 아님 — Fab 자산의 소비자(cook 단일 수용 경로) + 툴 이식의 출발점 |

---

## §5 각 엔진이 포기한 것 — 설계는 트레이드오프의 기록이다

### 5.1 언리얼이 포기한 것: 단순성

UE는 "리플렉션에 등록하면 GC·직렬화·에디터·스크립팅·네트워크가 공짜"라는 규모의 경제를 사기 위해 단순성을 지불했다. 모든 것이 UObject·UHT·UBT를 통과해야 하므로 빌드가 길고, 헤더 하나 고치면 에디터를 내렸다 올려야 하며 (dev.epicgames.com), 바이너리 uasset은 텍스트 diff/merge를 포기했고, Slate라는 자체 UI 프레임워크 전체가 유지보수 대상이 됐다. AAA 팀에게는 이 비용이 "수백 명이 같은 계약 위에서 일하게 하는 값"으로 정당화되지만, 1인 개발자에게는 순수 세금이다.

### 5.2 유니티가 포기한 것: 소스 투명성과 네이티브 제어

유니티는 접근성(C# 단일 언어, 텍스트 직렬화, 수 초 만에 도는 에디터 툴)을 사기 위해 코어를 닫았다. 네이티브 C++ 코어는 볼 수도 고칠 수도 없고, 성능의 천장은 Boehm GC와 관리/네이티브 경계 마샬링이 정한다. 그 천장을 뚫으려 만든 DOTS는 사실상 "엔진 안의 두 번째 프로그래밍 모델"이라는 분열 비용을 낳았고, 유니티 스스로 EntityId 통합과 "ECS for All"로 그 분열을 다시 봉합하는 중이다 (discussions.unity.com). 렌더 파이프라인 3분열(Built-in/URP/HDRP)도 같은 계열의 세금이며, Mono→CoreCLR 전환기(6.4~6.8)에는 InstanceID·도메인 리로드·Mono 특유 동작에 기댄 코드가 명시적 마이그레이션 절벽을 만난다 (discussions.unity.com).

### 5.3 Winters가 포기한 것: 범용성

Winters는 서버 권위 결정론이라는 단일 불변식에 범용성을 지불했다. 범용 스크립팅 VM이 없고(디자이너가 로직을 조립할 수 없다), 리플렉션이 없고(에디터 프로퍼티 그리드를 공짜로 못 얻는다), 크로스 플랫폼이 없다(Windows/x64/DX11 고정이라 리틀엔디안 고정 같은 실용 단순화가 가능했다). 그 대가로 얻은 것이 상용 엔진도 부러워할 자산이다: 틱 해시로 기계 검증되는 시뮬레이션(`Tools/SimLab/`), memcpy 스냅샷 복제, "자산은 검증된 계약으로만 게임에 들어간다"는 단일 cook 경로. 남은 약점은 기능이 아니라 강제 — 규칙을 지키는 주체가 아직 사람이라는 것이며, 이것이 감사 이후 로드맵(P1 강제형 구조)의 표적이다.

### 5.4 면접 답변에서의 가치

세 엔진을 나란히 놓으면 "좋은 엔진"이라는 질문 자체가 성립하지 않음이 드러난다 — 각 엔진은 서로 다른 고객의 서로 다른 불변식에 최적화된 트레이드오프의 기록이다. 면접에서 이 문서가 뒷받침하는 답변 골격은 세 문장이다. 첫째, "언리얼의 본질은 개별 기능이 아니라 리플렉션 하나로 다섯 시스템을 사는 규모의 경제이고, 그 대가가 빌드 시간과 복잡도라는 것을 압니다." 둘째, "유니티의 본질은 텍스트 직렬화 + C# 에디터 스크립팅이 만드는 툴링 접근성이고, 그 대가가 클로즈드 코어와 GC 천장이라는 것을 압니다." 셋째, "제 엔진은 서버 권위 결정론이라는 불변식을 골랐고, 그래서 Blueprint VM과 UObject GC를 근거를 갖고 거절했으며, 남은 약점(사람 의존 강제)을 감사로 식별해 수리 순서까지 정해 뒀습니다." — 무엇을 만들었는지가 아니라 **무엇을 왜 포기했는지**를 말할 수 있는 지원자는 드물고, 그것이 이 비교의 실전 가치다.
