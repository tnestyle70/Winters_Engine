# 툴 & 에디터 개발 — 기술면접 대비

> 대상: 자체 DX11 엔진(Winters) 제작 경험 기반, 게임 클라이언트/서버 프로그래머 + 툴 프로그래머 포지션 기술면접.
> 근거: 이 문서의 모든 "내 프로젝트" 인용은 실제 레포 파일 경로를 명시한다. 경로가 없는 항목은 일반 개념 서술이다.

## 출제 경향 개요

툴/에디터 파트 면접은 크게 네 축으로 나온다.

1. **UI 아키텍처** — 즉시모드(ImGui) vs 보존모드, 왜 게임 툴은 ImGui가 표준이 됐는가, ID 스택/상태 동기화 같은 실전 함정.
2. **데이터 파이프라인** — 원본(FBX/glb) → 중간 포맷(쿠킹) → 런타임 로드. "왜 런타임에 FBX를 파싱하면 안 되나"는 거의 고정 출제. 바이너리 직렬화의 버전 관리/호환성까지 파고든다.
3. **에디터 코어 기능** — Undo/Redo(커맨드 패턴), 피킹/기즈모(레이 캐스팅), 크래시로부터 데이터 보호(원자적 저장/오토세이브). 실무자는 여기서 "실제로 만들어 봤는지"를 가장 빨리 판별한다.
4. **툴의 목적 의식** — 아티스트/디자이너 UX, 대량 배치 처리, 관측 가능성(프로파일러/디버그 오버레이). "툴은 사람의 반복 노동을 줄이고 데이터의 품질을 기계로 보증하는 물건"이라는 관점이 있는지 본다.

언리얼/유니티 에디터를 써 본 경험만 말하면 감점, **직접 만든 툴의 설계 결정과 실패 경험**을 말하면 가점. Winters에는 LoL 맵 에디터(`Client/Private/Scene/Scene_Editor.cpp`), 에셋 컨버터(`Tools/WintersAssetConverter/`), 프로파일러(`Engine/Include/ProfilerAPI.h`), Undo 트랜잭션이 있는 EldenRing 에디터(`EldenRingEditor/`), 이펙트 툴 설계서(`.md/plan/EffectTool/`)까지 전부 실물이 있으므로 이 카테고리는 오히려 주력 어필 포인트다.

---

## 핵심 개념 정리

### 1. 즉시모드(Immediate Mode) UI vs 보존모드(Retained Mode) UI

**정의.**
- **보존모드**: UI 요소가 객체 트리(위젯 트리)로 메모리에 "보존"된다. 앱은 트리를 만들고, 프레임워크가 상태 변화·이벤트·그리기를 관리한다. Win32, Qt, WPF, 언리얼 Slate/UMG, 웹 DOM이 이 계열.
- **즉시모드**: 매 프레임 UI를 코드로 다시 "선언"한다. 위젯 객체가 없고, `ImGui::Button("Save")` 호출 자체가 그리기 명령 생성 + 입력 판정 + 반환값(클릭 여부)까지 한 번에 처리한다. Dear ImGui가 대표.

**동작 원리 (ImGui 기준).**
1. 매 프레임 `NewFrame()` → 사용자 코드가 위젯 함수를 호출 → 내부적으로 정점/인덱스 버퍼(draw list)를 쌓는다.
2. 위젯 식별은 객체 포인터가 아니라 **ID 스택 해시**(라벨 문자열 + 부모 윈도우/`PushID` 체인의 해시)로 한다.
3. 프레임 끝에 draw list를 렌더러 백엔드(DX11이면 SRV + 정점버퍼)로 제출한다.
4. 포커스/스크롤 위치 같은 최소한의 상태만 ImGui 내부 storage에 남고, **데이터의 진실은 항상 애플리케이션 변수**다. `ImGui::DragFloat("Scale", &scale)`처럼 포인터를 넘기므로 UI와 데이터가 항상 동기화되어 있다 — "상태 동기화 버그"라는 범주 자체가 사라진다.

**짧은 예시.**
```cpp
// 보존모드: 버튼 객체 생성 + 콜백 등록 + 상태 동기화 코드가 따로 존재
// 즉시모드: 이 한 줄이 생성/판정/렌더 전부
if (ImGui::Button("Save Stage (Ctrl+S)")) Save_CurrentStage();
ImGui::DragFloat3("Position", &pos.x, 0.1f);   // 데이터 = 진실, UI는 뷰
```

**트레이드오프.**

| 관점 | 즉시모드(ImGui) | 보존모드 |
|---|---|---|
| 상태 동기화 | 불필요(데이터가 곧 진실) | Model→View 동기화 코드 필요, 버그 온상 |
| 개발 속도 | 압도적으로 빠름(툴 프로토타이핑 최적) | 초기 비용 큼 |
| 레이아웃/스타일 | 자동 레이아웃 빈약, 커스텀 스킨 어려움 | 디자이너급 레이아웃/애니메이션/접근성 |
| 성능 특성 | 매 프레임 재구축(위젯 수천 개까진 무시 가능), 게임 루프와 자연 결합 | idle 시 0 cost 가능(이벤트 드리븐), 대신 무효화(invalidation) 관리 필요 |
| 사용자 대면 UI | 부적합(게임패드 내비게이션, 로컬라이즈, 애니메이션 약함) | 적합 |

**게임 개발 맥락.** 사내 툴/에디터/디버그 오버레이는 "프로그래머가 오늘 만들어서 아티스트가 내일 쓰는" 물건이라 즉시모드가 표준. 반면 출시 게임 HUD는 보존모드(또는 자체 UI)로 간다. Winters도 이 경계를 규칙으로 박았다 — `.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §2: "ImGui는 툴/튜너 전용이고 런타임 HUD가 아니다". 엔진 쪽 통합은 `Engine/Public/Editor/ImGuiLayer.h`(class `CImGuiLayer`)가 담당하고, 실전 패널은 `Client/Private/Scene/Scene_Editor.cpp`의 `Render_MenuBar/Render_Palette/Render_Hierarchy/Render_Inspector`(각각 240/290/447/516행 부근), 이펙트 튜너 `Client/Public/UI/EffectTuner.h`, 프로파일러 오버레이 `Engine/Private/Manager/Profiler/ProfilerOverlay.cpp`로 실사용 중.

**실전 함정(꼬리질문 단골).**
- 같은 라벨 두 개 = 같은 ID = 한쪽 클릭이 씹힘 → `PushID(i)` / `"Delete##unique"` 로 해결.
- 디버그 할당 매크로와의 충돌: Winters는 `Engine_Defines.h`의 디버그 `new` 매크로가 ImGui/Tracy의 placement new를 깨뜨려서 `#pragma push_macro("new") / #undef new / include / pop_macro` 격리 패턴을 쓴다 (`Client/Public/Scene/Scene_Editor.h` 15~18행, `Engine/Include/ProfilerAPI.h` 16~19행에 실물).
- "ImGui는 느리지 않냐" → 위젯이 수만 개가 아닌 한 병목은 거의 항상 다른 곳. 리스트 수만 항목은 `ImGuiListClipper`로 보이는 것만 그린다.

### 2. 에디터–런타임 분리 설계

**정의.** 에디터는 데이터를 **생산·검증**하는 실행 경로, 런타임은 그 데이터를 **소비**하는 실행 경로다. 같은 프로세스에 있더라도 씬/월드/코드 경로를 분리해서, 에디터 전용 코드가 게임 로직에 침투하거나 게임 성능을 오염시키지 않게 한다.

**원리 & 규칙 (Winters 실물).**
- LoL 클라이언트에서 `CScene_Editor`(`Client/Public/Scene/Scene_Editor.h`)와 `CScene_InGame`(`Client/Private/Scene/Scene_InGame.cpp` 외 분할 파일 다수)은 **같은 `IScene` 인터페이스의 형제 씬**이다. 인게임에서 에디터 씬으로 전환해 배치 → `Ctrl+S` 저장 → ESC로 복귀. 씬 전환은 self-destruct(현재 씬 파괴)라 전환 호출 직후 즉시 `return`이 필수인데, 이걸 주석으로 박아뒀다(`Scene_Editor.cpp` 159행: "ESC → Scene 전환은 self-destruct 를 유발 → 즉시 return 필수 (use-after-free 방지)").
- 에디터가 만든 데이터와 런타임이 읽는 데이터는 **같은 로드 경로**를 탄다. `.md/architecture/WINTERS_CODEBASE_COMPASS.md` Editor 섹션(93~100행, 141~155행): "Editor 전용 기능은 normal F5 runtime을 숨기거나 우회하지 않는다", "Editor에서 만든 데이터가 runtime에서 로드될 때는 같은 resource/path/validation contract를 탄다". 즉 에디터에서만 보이고 게임에선 다르게 로드되는 이중 경로를 금지.
- 더 큰 스케일에서는 에디터를 **Engine SDK의 별도 소비자 실행 파일**로 세운다 — `EldenRingEditor/`가 그 스캐폴드로, `CEldenRingEditorScene`(`EldenRingEditor/Public/EldenRingEditorScene.h`)이 Viewport/ContentBrowser/WorldCell/Outliner/Details/Transaction/FxGraph/Sequencer/WorldPartition/Log 패널을 가진 UE 스타일 에디터 셸이다.

**게임 개발 맥락.** 언리얼의 에디터 월드 vs PIE(Play In Editor) 월드 분리, `WITH_EDITOR` 컴파일 경계와 같은 문제의식이다. 분리에 실패하면 (a) 에디터 전용 데이터가 출시 빌드에 실려 나가고, (b) "에디터에선 됐는데 쿠킹 빌드에선 깨진다"는 이중 경로 버그가 나오고, (c) 게임 코드가 에디터 타입에 의존해 빌드 시간이 폭발한다.

### 3. 에셋 파이프라인 — 원본 포맷을 왜 쿠킹하나

**정의.** DCC 원본(FBX/glb/PNG)을 오프라인에서 엔진 전용 중간/최종 포맷으로 변환(쿠킹, cooking)하고, 런타임은 그 결과물만 로드하는 구조. "임포트는 한 번, 로드는 매번"이라는 비대칭에 최적화한다.

**왜 런타임 파싱이 아니라 쿠킹인가 — 4가지 근거.**
1. **로드 속도**: FBX/glTF 파싱은 텍스트/트리 해석 + 후처리(탄젠트 생성, 본 가중치 정규화, 노드 평탄화)가 필요하다. 쿠킹된 바이너리는 레이아웃이 GPU 업로드 형태와 일치해서 파싱 없이 `memcpy`/포인터 캐스팅으로 끝난다. Winters `CWMeshLoader`(`Engine/Public/AssetFormat/Mesh/WMeshLoader.h`)는 파일 블롭을 통째로 읽고 정점/인덱스 블롭은 **zero-copy 포인터**(`pVertexBlob`이 `m_vRawFile` 내부를 가리킴)로 GPU에 직행한다.
2. **용량/의존성 제거**: 원본에는 런타임에 불필요한 데이터(에디터 노드, 커브, 중복 소스)가 많다. 실측 — 이렐리아 FBX 60MB → `.wmesh` 1.2MB (애니 데이터 분리 기준 약 50×).
3. **런타임에서 서드파티 파서 제거**: Assimp 같은 대형 라이브러리를 출시 클라이언트에서 떼어낼 수 있고(공격 표면·크래시 표면 축소), 로딩의 결정성이 좋아진다.
4. **검증 시점을 앞으로 당김**: 포맷 오류·본 수 초과·텍스처 채널 오류를 쿠킹/Validator 단계에서 실패시키면, 런타임의 "조용한 실패"가 빌드 타임의 "시끄러운 실패"로 바뀐다. `.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §2: "Validator가 게이트다. 조용히 기본값으로 폴백 금지".

**Winters 실물 파이프라인.**
- 컨버터: `Engine/Private/Tools/AssetConverter/main.cpp`를 별도 실행 파일 프로젝트 `Tools/WintersAssetConverter/WintersAssetConverter.vcxproj`로 빌드(엔진 DLL export 매크로를 무력화하는 `WINTERS_STATIC_BUILD` 트릭). 배치 스크립트 `Tools/convert_all_assets.bat`.
- 포맷: `Engine/Public/AssetFormat/` 아래 Mesh(`WMeshFormat.h`)/Anim(`WAnimFormat.h`, `WSkelFormat.h`)/Material(`WMaterialFormat.h`) + 공통(`WintersFileHeader.h`, `BinaryReader/Writer.h`).
- 실적: **27개 FBX/GLB → .wmesh 전수 변환 FAIL=0** (챔피언 5, 맵 1, 구조물 6, 정글 7, 미니언 8 — 2026-04-24 세션).
- **하이브리드 통합**: 전면 교체가 아니라 `CModel::LoadModel`에 fast-path를 얹었다 — 동명 `.wmesh`가 존재하고 `bone_count==0`(정적 메시)이면 zero-copy 로드 + Assimp 후처리 생략, 스킨드 메시는 기존 Assimp 경로로 자동 폴백, 텍스처(GLB 임베디드)는 Assimp 유지. 리스크를 정적 메시로 한정하고 단계적으로 이관하는 전형적 마이그레이션 전략.

**게임 개발 맥락.** 언리얼의 Import→uasset→Cook, 유니티의 AssetImporter→Library 캐시와 동일한 구조. 면접에서 "DDC(Derived Data Cache)가 왜 있나"까지 이어지면: 쿠킹 결과물은 (원본, 변환 설정, 컨버터 버전)의 순수 함수이므로 해시 키로 캐시/공유 가능하다 — 그래서 임포트 설정과 컨버터 버전이 캐시 키에 들어가야 한다고 답한다.

### 4. 바이너리 직렬화 설계 — 헤더/버전/호환성

**정의.** 구조화된 데이터를 바이트 열로 기록(Save)하고 복원(Load)하는 것. 툴 포맷 설계의 핵심은 데이터 자체가 아니라 **시간축**이다 — 포맷은 반드시 진화하고, 어제 저장한 파일을 내일 코드가 읽어야 한다.

**Winters의 두 실물 포맷.**

(1) 공통 에셋 헤더 — `Engine/Public/AssetFormat/Common/WintersFileHeader.h`:
```cpp
#pragma pack(push, 1)
struct WintersFileHeader {
    char     magic[4];       // "WINT"
    uint16_t version_major;  // 호환 깨지는 변경
    uint16_t version_minor;  // 하위 호환 추가
    uint32_t flags;          // WF_LZ4, WF_HAS_SHA256 (기능 비트)
    uint32_t content_size;
};
static_assert(sizeof(WintersFileHeader) == 16, "...");
#pragma pack(pop)
```
설계 포인트: **magic**(잘못된 파일 즉시 거부), **major/minor 분리**(major 불일치 = 거부, minor 상위 = 모르는 뒤쪽 데이터 무시), **flags 비트**(압축/해시 같은 opt-in 기능을 버전 올리지 않고 추가), **content_size**(트렁케이션 검출), `pack(1)` + `static_assert`(레이아웃을 컴파일 타임에 고정 — 이 조합이 "구조체를 그대로 fwrite해도 되는 유일한 조건"이다).

(2) 스테이지 포맷 — `Shared/GameSim/Definitions/MapDataFormats.h`:
```cpp
constexpr u32_t STAGE_MAGIC = 0x47545357;      // 'WSTG'
constexpr u32_t STAGE_VERSION = 5;
constexpr u32_t STAGE_VERSION_MIN_COMPAT = 3;  // v3~v5 로드 허용
struct StageHeader { u32_t magic; u32_t version; u32_t reserved[6]; };
```
`StructureEntry/JungleEntry/MinionWaypointEntry/BushEntry` 전부 고정 크기 POD + `static_assert`(예: `sizeof(BushEntry)==252`)이고, 각 엔트리에 `reserved` 필드를 미리 확보해 뒀다. 버전 이력도 실제로 겪었다: 에디터에서 미니언 배치를 제거하면서 v2→v3 bump(미니언은 런타임 스폰 대상이라 정적 배치가 무의미하다는 도메인 판단), 이후 부시/웨이포인트가 추가되며 v5까지 진화, `MIN_COMPAT=3`으로 구버전 스테이지 파일을 계속 읽는다. 저장 UX는 `Scene_Editor.cpp`의 `Save_CurrentStage()`(923행)가 실패 시 경로를 포함한 MessageBox, 성공 시 엔트리 수 요약을 띄우고 dirty 플래그를 내린다.

**엔디안/이식성.** Winters는 Windows/x64 단일 타깃이라 리틀엔디안 고정이 실용적 선택이다. 면접 답변용 일반론: (a) 포맷 명세에 엔디안을 **명시**하고(대부분 리틀 고정 — 주요 콘솔/PC 전부 리틀), (b) 크로스 플랫폼 툴이 필요하면 Reader/Writer 계층에서 스왑, (c) float은 IEEE754 가정을 명시. 진짜 함정은 엔디안보다 **구조체 패딩/정렬**(컴파일러·플랫폼마다 다름 → `pack(1)`+`static_assert`)과 **포인터/size_t 크기**다.

**자기비판(면접 고득점 포인트).** 2026-07-10 UE5.7 소스 대조 감사에서 스스로 찾은 약점: `.wmesh`의 `MeshMetaHeader`(`Engine/Public/AssetFormat/Mesh/WMeshFormat.h`)에는 magic만 있고 **버전 필드가 없다**. 공통 `WintersFileHeader`를 안 거치는 경로라 포맷을 바꾸면 구파일 판별이 불가능하다. 또 저장이 **원자적이지 않다**(temp+rename 부재 — 저장 중 크래시 시 파일 반파). 감사 로드맵에서 "포맷 버전 분기 로딩"과 "AtomicWriteFile 헬퍼"를 에디터 코어 선행 과제로 못박았다. 약점을 이미 식별하고 수리 순서까지 정한 상태라고 말하는 것이 완벽한 포맷을 주장하는 것보다 강하다.

### 5. 데이터 주도 설계 — EntityBlueprint와 .winters 포맷

**정의.** 동작(코드)과 내용(데이터)을 분리해서, 콘텐츠 변경이 리컴파일 없이 데이터 편집으로 끝나게 하는 설계. 툴 개발의 존재 이유 — 툴은 결국 "데이터를 안전하게 편집하는 UI"다.

**Winters 실물.**
- `CEntityBlueprint`(`Engine/Public/ECS/Systems/EntityBlueprint.h`): 엔티티 템플릿을 상속 계층이 아니라 **Installer 함수 조립(composition)** 으로 정의한다. `Add(Installer)`로 컴포넌트 설치 람다를 쌓고 `Spawn(world, pArg)`이 새 엔티티에 순서대로 적용. 학원 수업의 `CPrototype_Manager`(OOP 프로토타입 복제) 관례를 ECS 위에 흡수한 것으로, "I/O 없는 복제"(로딩 스레드에서 리소스 I/O는 1회, 이후 스폰은 순수 메모리 조립)가 핵심.
- 스테이지 데이터: 에디터 배치 결과가 `Data/StageN.dat`(§4)로 영속화되고 게임은 부팅 시 자동 로드 — 맵 배치가 코드 하드코딩(`defs[44]` 배열)에서 데이터로 이관된 역사가 있다(Scene_InGame 1354줄 비대화 → 에디터+데이터 분리 리팩터링).
- `.winters` 번들 계획: `.md/plan/WintersFormat/`(00~12, 13파일) — 공통 헤더/`.wmesh`/`.wanim`/`.wtex`/`.wmat`/`.wmap`/번들/무결성(SHA256, Ed25519 서명)/버저닝(`11_VERSIONING.md`)/컨버터 CLI/디버그 툴까지 12스테이지 설계.
- 상위 규율: `.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §2 — "툴은 gameplay truth를 만들 수 없다": 서버 권위 수치(스탯/타이밍/판정)는 authoring 소스 → cook → 정의 팩 경로로만 들어가고, ImGui 슬라이더 라이브 튜닝 값은 실험값일 뿐 저장은 authoring 소스로 역기록 후 재쿠킹한다. 이것이 UE의 DataAsset/DataTable에 해당하는 Winters식 계약.

**게임 개발 맥락.** "밸런스 디자이너가 코드 수정 없이 스킬 계수를 바꿀 수 있는가"가 리트머스 시험. 여기에 서버 권위 게임이면 "클라이언트 데이터와 서버 데이터의 진실 소유권" 문제가 겹치는데, Winters는 이걸 아키텍처 문서로 고정해 뒀다는 점이 차별점.

### 6. Undo/Redo — 커맨드 패턴과 트랜잭션

**정의.** 편집 조작을 `Do()/Undo()`를 가진 커맨드 객체로 물화(reify)해서 스택에 쌓는 패턴. Undo = undo 스택 pop → `Undo()` → redo 스택 push. 새 커맨드가 들어오면 redo 스택은 비운다(분기한 미래는 버린다).

**Winters 실물 — `EldenRingEditor/Public/World/EditorTransaction.h`:**
```cpp
class IEditorCommand {
    virtual void Do() = 0;
    virtual void Undo() = 0;
    virtual const char* Name() const = 0;   // History 패널 표시용
};
class CEditorTransaction {
    void Push(std::unique_ptr<IEditorCommand>);   // Do 실행 + undo 스택 적재
    void Undo(); void Redo(); void Clear();
    const std::vector<std::string>& History() const;
};
```
구체 커맨드 3종: `CAddPlacementCommand` / `CDeletePlacementCommand` / `CTransformPlacementCommand`. 설계 디테일이 면접 포인트다:
- **선택 상태도 복원 대상**: 각 커맨드가 `u32_t* pSelectedPlacementId`와 undo/do 후 선택값을 들고 있다. 삭제를 undo했는데 선택이 엉뚱한 데 가 있으면 사용자는 "고장"으로 느낀다.
- **삽입 위치 보존**: `insertIndex/originalIndex`를 저장해 undo 시 배열의 원래 자리에 되돌린다(순서가 의미인 문서에서 append로 되돌리면 데이터가 미묘하게 오염).
- **드래그 병합**: `CEldenRingEditorScene`(`EldenRingEditor/Public/EldenRingEditorScene.h`)은 Details 패널 트랜스폼 편집을 `m_detailsTransformBefore/Draft`로 잡아두고 `CommitDetailsTransformEdit()`에서 **커밋 시점에 커맨드 1개**로 만든다(`CancelDetailsTransformEdit()`도 별도). 슬라이더 드래그 매 프레임마다 커맨드를 만들면 undo 스택이 수백 개로 오염되는 고전 함정의 정석 해법.
- **왜 memento(스냅샷)가 아니라 command(차분)인가**: 월드 문서 전체 스냅샷은 메모리가 크고, 커맨드는 변경분만 담아 가볍다. 단, 커맨드는 "Undo가 Do의 완전한 역"임을 보장해야 해서 구현 난이도가 있다 — 그래서 복잡한 편집(터레인 페인트 등)은 부분 스냅샷을 커맨드 안에 담는 하이브리드가 실무 정답.

**자기비판.** LoL 맵 에디터(`Scene_Editor.cpp`)에는 undo가 없다. UE5.7 감사의 결론: "LoL툴 undo는 패턴 부재가 아닌 **공유 부재**" — 동작하는 `CEditorTransaction`이 EldenRingEditor에 이미 있으므로, 이를 공용 에디터 코어로 승격하는 것이 로드맵 1단계다.

### 7. 피킹(Picking)과 기즈모 — 레이 캐스팅

**정의.** 피킹 = 2D 마우스 좌표로 3D 월드의 위치/오브젝트를 선택하는 것. 표준 기법은 마우스 → 월드 레이 변환 후 기하와 교차 판정.

**레이 생성 원리.** 스크린 픽셀 → NDC(`x' = 2x/W - 1`, `y' = 1 - 2y/H`) → 역투영(near/far 두 점을 `(P·V)^-1`로 월드 변환) → `Origin + t·Dir` 레이. Winters는 이걸 `CInput::GetMouseWorldRay(camera, W, H)`로 캡슐화했고 에디터가 사용한다.

**지면 평면 교차 — `Scene_Editor.cpp` `TryPickGroundPlane()`(846행):**
```cpp
if (fabsf(ray.Dir.y) < 1e-4f) return false;   // 평면과 평행 → 해 없음
const f32_t t = -ray.Origin.y / ray.Dir.y;    // y=0 평면: O.y + t·D.y = 0
if (t < 0.f) return false;                    // 카메라 뒤쪽 교차 거부
```
수식 유도까지 즉답 가능해야 한다: 평면 `y=0`에 대입하면 `t = -O.y / D.y`, 분모 0(평행) 가드와 `t<0`(뒤쪽) 가드가 필수. 이 한 함수가 오브젝트 배치, NavGrid 브러시 페인팅(`PaintNavGridAt` — 좌클릭 차단/우클릭 해제, 브러시 반경 셀 단위) 모두의 기반이다.

**오브젝트 피킹.** Winters는 챔피언 호버 타겟팅에 레이–실린더 교차(B-6, 사일러스 실린더 피킹)를 썼다 — LoL류는 캐릭터가 세로로 긴 실루엣이라 실린더 프록시가 값싸고 관대한(=UX 좋은) 판정. 대안 비교: (a) **수학적 프록시 교차**(구/캡슐/AABB/OBB) — 값싸고 CPU-only, 시각 메시와 오차. (b) **ID 버퍼 픽셀 피킹** — 오브젝트 ID를 오프스크린 타깃에 렌더 후 마우스 픽셀 1개 readback; 픽셀 정확도지만 GPU readback 지연(1프레임 지연 비동기로 처리)과 렌더 패스 추가. 에디터는 (b)가 UX 우수, 게임플레이 호버는 (a)가 정답인 경우가 많다.

**기즈모.** 이동/회전/스케일 핸들 자체도 "피킹 가능한 3D 오브젝트"다. 축 핸들은 레이–원기둥, 평면 핸들은 레이–사각형 판정; 드래그는 선택 축을 스크린에 사영해 마우스 델타를 축 delta로 환산; 카메라 거리에 비례해 크기를 키워 화면상 크기를 일정하게 유지(distance-proportional scale). 실무에선 ImGuizmo 같은 검증된 라이브러리 채택도 정답 — Winters 에디터의 다음 단계 후보로 인지하고 있다고 답하면 된다. 드래그 종료 시 §6의 트랜스폼 커맨드 1개로 커밋하는 연결까지 말하면 만점.

### 8. 이펙트 툴 설계 — 노드 그래프 (Phase G / WFX)

**정의.** VFX를 코드가 아니라 **그래프 에셋**(이미터 = 노드들 + 연결)으로 저장하고, 런타임은 그래프를 해석(또는 컴파일된 플랜을 실행)하는 구조. 언리얼 Niagara, FromSoft FXR가 참조 모델.

**Winters 설계 (`.md/plan/EffectTool/` 00~28, 총 29파일) 핵심 결정.**
- **그래프 데이터 모델**(`02_STAGE1_GRAPH_DATA_MODEL.md`): FxGraph를 JSON으로 저장, 평가 순서는 **Kahn's algorithm 위상 정렬**로 확정. 사이클은 위상 정렬이 전 노드를 소비하지 못하는 것으로 검출 → 에러. 실물: `Engine/Public/FX/Graph/FxGraphValidator.h`의 `CFxGraphValidator::Validate(FxEmitterGraph)`가 `FxValidationResult { bValid, issues(노드ID+메시지+에러여부), topoOrder }`를 반환한다 — 검증과 평가 순서 산출이 한 함수에서 나오는 구조.
- **파티클 풀**(`03_STAGE2_PARTICLE_POOL_SOA.md`): SoA 레이아웃 + swap-back kill(죽은 파티클을 마지막 원소와 스왑 후 count 감소 — 순회는 뒤→앞 강제). AoS 대비 캐시 효율과 SIMD 친화.
- **표현식 VM**(`05_STAGE4_EXPRESSION_VM.md`): 노드 파라미터 수식을 바이트코드 스택 머신으로 — Niagara VectorVM의 축소판. 아티스트가 `size = base * (1-age)^2` 같은 커브를 코드 배포 없이 편집.
- **렌더링/GPU**(`06/08_*`): 빌보드 인스턴싱 + 3 블렌드 모드 + 소팅, 장기적으로 그래프→HLSL 코드젠 + RWStructuredBuffer + Indirect Draw(`22_COMPILE_GRAPH_TO_HLSL_VM_BAKE.md`).
- **결정적 FX vs 시각 FX 분리**(`09_INTEGRATION.md`): 판정에 영향 주는 것(히트 판정 위치 등)은 서버 권위 시뮬레이션에, 순수 시각 연출은 클라이언트 FX에 — Niagara가 게임플레이 판정과 얽힐 때 생기는 비결정성 문제를 구조적으로 회피. 서버 권위 게임을 만들어 본 사람만 하는 설계 결정이라 어필 가치가 크다.
- **에디터 없이도 굴러가는 단계 설계**: Stage 3+5만으로 LoL 이펙트 90% 커버, 노드 에디터(imgui-node-editor, Stage 6)는 1인 개발 시 JSON 직접 편집으로 대체 가능 — "툴은 데이터 포맷이 먼저, GUI는 나중"이라는 우선순위.

현재 엔진에는 `Engine/Private/FX/Graph/FxGraphValidator.cpp`, `Engine/Private/FX/Exec/FxExecPlan.cpp` 등 그래프 검증/실행 플랜 계층이 실재하고, `EldenRingEditorScene`에 `DrawFxGraphPanel()/RunFxGraphValidation()`으로 에디터 배선이 시작돼 있다.

### 9. 프로파일러 툴 — thread-safe 계측

**정의.** 코드 구간의 소요 시간/횟수를 수집·시각화하는 계측 인프라. 툴 프로그래머 관점의 요구사항: (1) 계측이 대상의 성능을 바꾸지 말 것(관찰자 효과 최소화), (2) 출시 빌드에서 0 비용, (3) 멀티스레드 안전, (4) 결과가 행동으로 이어지는 시각화.

**Winters 실물 (`Engine/Include/ProfilerAPI.h`).**
```cpp
#define WINTERS_PROFILE_SCOPE(name) \
    ZoneNamedN(WINTERS_PROFILE_CAT(_tracyZone_, __LINE__), name, true); \
    ::CProfileScope WINTERS_PROFILE_CAT(_winProfScope_, __LINE__)(name)
```
- **RAII 스코프**: 생성자 Push, 소멸자 Pop — early return에도 안전. `QueryPerformanceCounter` 기반(`ProfilerEvent { pName, startTicks, endTicks, depth, threadId }`, `Engine/Public/Core/Profiler/ProfilerTypes.h`).
- **컴파일 타임 게이트**: `WINTERS_PROFILING` 미정의 시 매크로가 `((void)0)` — 출시 빌드 진짜 0 비용.
- **`__LINE__` 2단계 CONCAT**: `a##b` 직접 결합은 `__LINE__`이 확장되기 전에 붙어버려 같은 함수에 스코프 2개를 못 둔다 → 간접 매크로 한 겹으로 지연 확장. 직접 밟아 본 매크로 함정이라 실전 스토리로 쓰기 좋다.
- **유계(bounded) 버퍼**: 프레임당 이벤트 4096 / 카운터 128 상한 — 계측 자체의 무한 메모리 증식 방지.
- **카운터 시스템**: `WINTERS_PROFILE_COUNT(name, delta)` — 시간이 아니라 횟수(A* 방문 노드 수, 리패스 호출 수)를 세는 별도 축. 시간 프로파일과 카운터를 함께 봐야 "느린 이유"가 나온다.
- **Tracy 통합**: Tracy 구현부를 Engine DLL 한 곳(TRACY_EXPORTS)에만 두고 Client/Server는 import로 같은 인스턴스에 기록 — DLL 경계에서 프로파일러 인스턴스가 모듈별로 쪼개지는 고전 함정의 해법. 자체 F3 HUD와 Tracy 존을 한 매크로로 동시 기록.
- **멀티스레드 race 경험**: JobSystem 워커에서 `thread_local` 버퍼 수집이 race를 일으켜 → Decision/Apply 2-pass + 스레드 슬롯 고정(main=0, worker=idx+1)으로 수리(2026-04-28 미니언 전투 세션). "프로파일러도 동시성 버그가 나는 코드"라는 실감 스토리.

**성과 사례(꼭 말할 것).** 프레임 17.8ms 저하 사건: 11곳 스코프 + 4종 카운터 심고 → Nav/A*는 각 0.003ms로 무죄, `Minion::AnimUpdate`가 16ms(프레임의 90%)로 확정 → 정적 엔티티에 `RenderComponent::bAnimated=false` 도입 + 스키닝 갱신 스킵 → **17.8ms → 9ms**. "추측 대신 계측, 범인 확정 후 최소 수정"의 완결 서사.

### 10. 디버그 오버레이 철학 — 관측 가능성(Observability) 우선

**정의.** 버그/성능 증상을 만나면 코드 추론을 쌓기 전에 **권위 있는 코드 경로 주변에 눈을 먼저 단다** — 검사 가능한 디버그 UI/오버레이, 유계 로그, 시각 캡처.

**Winters의 명문화된 규칙** (`CLAUDE.md` Progressive Sections + `.claude/gotchas.md` 2026-05-29): 이동/패스파인딩 버그면 현재 셀, 다음 셀/웨이포인트, 해석된 경로, 보정 방향, stuck/resolve 사유를 오버레이로 노출하고 나서 튜닝한다. 파생 규칙들:
- **죽은 진단은 무진단보다 나쁘다**(gotchas 2026-07-09): sprintf로 포맷만 하고 출력 안 되는 실패 진단은 삭제 대상. 실패 경로 로그는 반드시 유계 출력.
- **복제 경계의 조용한 실패 금지**: FlatBuffers verify 실패를 bare return으로 삼키면 스키마 드리프트가 "네트워크 멈춤"과 구분 불가 → 유계 트레이스/카운터 필수.
- **로그 게이트**: `OutputDebugStringA`를 매크로로 게이트된 래퍼에 리맵(`WINTERS_ENABLE_NON_AI_DEBUG_STRING`) — 루틴 트레이스가 리뷰/런타임을 오염시키지 않게 하되, 명시적 디버깅 로그는 열어 준다.

에디터 실물로는 `Scene_Editor.cpp`의 `RenderNavGridOverlay()`(872행) — NavGrid 차단 셀을 월드에 겹쳐 그려서 "데이터를 눈으로 검증하며 편집"하게 한다. 툴 관점 요지: 디버그 오버레이는 임시 코드가 아니라 **툴 제품의 일부**로 설계한다.

### 11. 언리얼 에디터 구조와의 비교 (UE5.7 소스 감사 경험)

2026-07-10에 UE 5.7.4 풀소스와 Winters를 7차원으로 대조 감사했다(7 에이전트 병렬, 330회 코드 조사). 툴 관점 핵심 결론:

- **관통 메타패턴**: Winters는 "생각(아키텍처)은 현업급인데 강제(enforcement)가 사람·문서·수동 grep 의존". UE는 같은 규칙을 빌드시스템/매크로/CI가 **기계 강제**한다. 좋은 툴/에디터의 본질은 개별 기능이 아니라 "자산이 검증된 계약으로만 게임에 들어가게 만드는 파이프라인 규율"이다(`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §0).
- **UE 요소의 계약 단위 분해**(같은 문서 §1 표): Content Browser = "모든 자산은 카탈로그에 등록되고 참조가 추적된다", Import/Reimport = "원본→엔진 포맷 변환은 자동·재현 가능", Niagara = "VFX는 그래프 에셋 + 런타임은 컴파일된 plan", Sequencer = "연출은 트랙 자산". Winters는 각 계약의 등가물(AssetConverter, WFX+Validator, .wseq 골격, `CWorldCellDocument` — schema `winters.world.cell.v1`, `EldenRingEditor/Public/World/WorldCellDocument.h`)을 기존 소유권 경계 위에 매핑한다.
- **의도적으로 채택하지 않는 것**(§5): Blueprint 런타임 VM(서버 권위 30Hz 결정론 tick + SimLab 해시 검증과 충돌 → generated pack + GameplayHookRegistry + UI 한정 Lua로 대체), UObject/GC/리플렉션 전면 도입(trivially-copyable ECS 컴포넌트 + 스냅샷 복제와 충돌 → 에디터 프로퍼티 그리드는 스키마 코드젠으로 한정), uasset 호환(유지비가 이득 압도). "안 만든 이유"를 아키텍처 근거로 말할 수 있는 것이 감사의 최대 수확.
- **UE 대비 툴 약점 목록(교차검증됨)**: LoL 툴 undo/원자적 저장/검증/영속화 부재, `.wmesh` 버전 필드 부재, 누락 에셋 = 릴리스 무음(placeholder 없음), 크래시 가시성 0(minidump 부재). 전부 로드맵에 편입.
- **UI 계열 비교**: Slate는 선언형 보존모드(위젯 트리 + invalidation), ImGui는 즉시모드 — UE가 에디터급 레이아웃/도킹/스타일을 얻는 대신 Slate 자체가 거대한 유지보수 대상이 됐다. 1인~소수 팀은 ImGui가 압도적 ROI.

### 12. Fab 툴 방향 — 외부 자산 공급망과 포트폴리오 툴 순서

- **Fab 자산 수용 파이프라인**(`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §3): 모든 외부 자산은 "원료" 취급 — `Import 스테이징(원본 보존+라이선스 메타) → import manifest → WintersAssetConverter cook(.w*) → Validator 게이트 → Asset Catalog 등록 → 런타임은 카탈로그 경유로만 참조` 단일 경로. 클래스별 결정: 애니팩은 `.wskel` 리타겟 프로필(cook 시점 오프라인 베이크) 선결, Niagara 자산은 이식 불가 → 참조용 구매 후 WFX 노드로 재작성, uasset 전용 팩은 구매 금지("Source files included" 팩만).
- **툴 개발 순서 결정(2026-07-10 감사 결론)**: flagship 포트폴리오 툴(어빌리티 타임라인 에디터)을 지금 만들면 editor-tooling 약점을 전부 상속한다 → ① **에디터 코어 선행**(EldenRing `CEditorTransaction` 공용 승격 + AtomicWriteFile 헬퍼 + `CFxGraphValidator` 저장 경로 배선 + 격리 프리뷰 CWorld) → ② 타임라인 툴 → ③ 언리얼 UDataAsset + 커스텀 에셋 에디터로 이식 → ④ Fab 출시. "현업급 보완"과 "포트폴리오 툴"이 ①에서 공통 기반을 갖는 순서 설계 자체가 면접에서 우선순위 판단력 어필 소재다.

### 13. 툴 UX·크래시 안전·대량 배치 처리 (툴 프로그래머 3대 단골 주제)

**아티스트/디자이너 UX 원칙.**
- 피드백은 즉시·구체적으로: Winters 에디터는 저장 성공 시 "Stage%d.dat 저장 완료 + S/J/B 엔트리 수 + 경로", 실패 시 원인·경로를 MessageBox로 보여준다(`Scene_Editor.cpp` 923~948행). 메뉴바에 현재 스테이지 표시 + 오브젝트 카운트 + FPS 상시 노출(240~288행).
- 파괴적 행동에는 안전망: dirty 플래그(`m_bDirty`)로 미저장 상태 추적, 스테이지 전환 시 저장 여부 처리. 단축키는 사용 도구 관례를 따른다(Ctrl+S 저장, ESC 복귀 — F12는 VS 디버거 예약이라 에디터 토글 키에서 배제한 것도 실경험).
- "불합격도 보여라"(P1): 검증 실패 자산은 카탈로그에서 사라지는 게 아니라 '불합격' 상태+사유로 남아야 한다 — 아티스트가 뭘 고칠지 알 수 있게.
- 라이브 튜닝(슬라이더)과 영속 저장(Save Preset → authoring 소스 역기록)의 구분을 UI로 명시 — 디자이너가 "튜닝했는데 재시작하니 날아갔다"고 느끼지 않게.

**크래시가 데이터를 날리지 않게 하는 법 (모범답변 골격).**
1. **원자적 저장**: temp 파일에 완전히 쓰고 flush 후 rename(Windows `ReplaceFile`/`MoveFileEx`) — rename은 원자적이라 크래시 시점과 무관하게 "완전한 구파일 or 완전한 신파일"만 존재. (Winters 현재 미비 — AtomicWriteFile 헬퍼가 에디터 코어 로드맵 1순위임을 함께 말한다.)
2. **오토세이브/저널링**: 주기 스냅샷 + 크래시 후 복구 제안. 커맨드 스택(§6)이 있으면 조작 로그 리플레이 방식도 가능.
3. **크래시 자체의 가시화**: `SetUnhandledExceptionFilter` + `MiniDumpWriteDump`로 minidump — UE 감사에서 Winters HIGH 약점 1호로 식별, Phase 0 배선 과제.
4. **저장 전 검증**: Validator가 통과 못 하는 문서는 저장 자체를 막는 게 아니라 "불량 표시와 함께" 저장 — 사용자의 작업물은 어떤 경우에도 잃지 않는 것이 최우선.

**대량 에셋 배치 처리.**
- GUI가 아니라 **CLI가 본체**: `WintersAssetConverter.exe`는 headless 실행 파일이고 `Tools/convert_all_assets.bat`가 27개를 일괄 변환(FAIL=0 실적). CLI여야 CI 게이트에 올라간다(exit code 규약 준수 — 감사에서 "스크립트는 이미 규약 준수, CI wiring만 남음"으로 확인).
- **재현 가능성**: 변환은 (원본, 설정, 컨버터 버전)의 순수 함수여야 하고, manifest가 "무엇을 어떤 설정으로 어디로"를 기록한다. 수정 시간/해시 비교로 증분(incremental) 쿠킹.
- **실패 리포트 집계**: N개 중 몇 개가 어느 스테이지에서 왜 실패했는지 목록으로 — 1000개 배치에서 3개 실패가 묻히면 릴리스 사고가 된다.

---

## 예상 질문 & 모범답변

### Q1. 즉시모드 UI와 보존모드 UI의 차이를 설명하고, 게임 툴에서 ImGui가 표준이 된 이유를 말해보세요.

**정의/원리**: 보존모드는 위젯 객체 트리를 메모리에 유지하고 프레임워크가 이벤트/무효화/그리기를 관리한다(Qt, Slate, DOM). 즉시모드는 매 프레임 UI를 코드로 재선언하며 위젯 객체가 없다 — `if (ImGui::Button("Save"))` 한 줄이 생성+입력판정+드로우리스트 축적을 전부 수행하고, 위젯 식별은 라벨+ID 스택 해시로 한다.
**왜/트레이드오프**: 게임은 어차피 매 프레임 전체를 다시 그리는 루프라 즉시모드가 자연스럽게 얹힌다. 최대 이점은 **상태 동기화의 소거** — 데이터 포인터를 직접 넘기므로(`DragFloat("Scale", &scale)`) Model→View 동기화 버그 범주가 사라진다. 대가는 레이아웃/스타일/접근성의 빈약함이라, 사용자 대면 UI에는 부적합.
**내 프로젝트**: Winters의 모든 툴 UI가 ImGui다 — 맵 에디터 패널 4종(`Client/Private/Scene/Scene_Editor.cpp`의 `Render_MenuBar/Palette/Hierarchy/Inspector`), 프로파일러 오버레이, EffectTuner(`Client/Public/UI/EffectTuner.h`). 그리고 "ImGui는 툴/튜너 전용, 런타임 HUD 아님"을 아키텍처 문서 규칙으로 고정했다(`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §2).
**함정/꼬리질문**: "매 프레임 재구축이면 느리지 않나" → 위젯 수천 개 수준에선 무시 가능하고, 대량 리스트는 `ImGuiListClipper`로 가시 영역만 그린다. idle 시에도 CPU를 쓰는 건 사실이라 상주형 독립 툴이면 이벤트 드리븐 프레임 스로틀을 얹는다.

### Q2. ImGui에서 같은 라벨의 버튼을 두 개 만들면 무슨 일이 생기고, 어떻게 해결합니까?

**원리**: ImGui는 위젯을 객체가 아니라 ID(라벨 해시 + ID 스택)로 식별한다. 같은 스코프에 같은 라벨이면 ID 충돌 — 한쪽의 클릭/활성 상태가 다른 쪽과 섞여 입력이 씹히거나 오동작한다.
**해결**: 루프에서는 `ImGui::PushID(i)`/`PopID()`, 개별 위젯은 `"Delete##row3"`처럼 `##` 뒤에 비표시 고유 접미사. 창 단위로도 ID 스택이 분리된다는 것까지 말하면 충분.
**내 프로젝트**: 맵 에디터 Hierarchy가 Structure/Jungle/Bush 목록을 인덱스 루프로 그리므로 PushID 계열 처리가 필수였다.
**꼬리질문 대비**: "라벨을 바꾸면 위젯 상태가 유지되나?" → ID가 바뀌므로 상태(포커스 등)는 리셋된다. 동적 라벨엔 `###` (라벨은 바뀌어도 ID는 고정) 을 쓴다.

### Q3. ImGui를 프로젝트에 통합할 때 겪을 수 있는 저수준 함정을 하나 들어보세요.

**모범답변**: 디버그 할당 매크로와의 충돌. MSVC CRT 누수 추적용으로 `#define new DBG_NEW`를 쓰면 ImGui/Tracy 내부의 placement new(`new(ptr) T`)가 매크로 확장으로 깨져 컴파일 에러가 난다.
**내 프로젝트**: Winters는 `#pragma push_macro("new") / #undef new / #include <imgui.h> / #pragma pop_macro("new")` 격리 패턴을 표준화했다 — `Client/Public/Scene/Scene_Editor.h` 15~18행, `Engine/Include/ProfilerAPI.h` 16~19행에 실물이 있다.
**추가 포인트**: DX11 백엔드에선 폰트 아틀라스 SRV 수명, 멀티 모듈(DLL) 환경에서 ImGui 컨텍스트 공유(컨텍스트 포인터를 명시적으로 넘기거나 단일 모듈에 격리)도 함정이다.

### Q4. 에디터와 런타임(게임)을 왜 분리해야 합니까? 본인은 어떻게 분리했나요?

**왜**: (1) 에디터 전용 코드/데이터가 출시 빌드에 실려 나가는 것 방지, (2) "에디터에선 되는데 게임에선 깨지는" 이중 로드 경로 방지, (3) 게임 코드가 에디터 타입에 의존하는 역방향 결합 방지.
**내 프로젝트**: 두 층위로 했다. LoL 클라이언트 안에서는 `CScene_Editor`와 `CScene_InGame`을 같은 `IScene` 인터페이스의 형제 씬으로 분리(원래 Scene_InGame이 1354줄로 비대해지며 하드코딩 배열 + 에디터 UI + 전투 로직이 섞여 있던 것을 분리 리팩터링). 데이터는 `Data/StageN.dat` 하나가 유일한 소스이고, 에디터의 저장과 게임의 부팅 로드가 **같은 `CMapDataIO`**(`Client/Public/Map/MapDataIO.h`)를 탄다. 더 큰 스케일에서는 에디터를 Engine SDK의 별도 소비자로 세웠다 — `EldenRingEditor/`는 엔진 위의 독립 에디터 셸이다.
**규칙화**: `.md/architecture/WINTERS_CODEBASE_COMPASS.md`에 "Editor 전용 기능은 normal F5 runtime을 숨기거나 우회하지 않는다 / Editor 데이터는 runtime과 같은 resource/path/validation contract를 탄다"로 명문화.
**꼬리질문 대비**: "언리얼은 어떻게 하나" → 에디터 월드 vs PIE 월드 분리, `WITH_EDITOR` 컴파일 경계, 쿠킹 시 에디터 전용 데이터 스트리핑 — 같은 문제의식의 대규모 버전.

### Q5. 씬 전환 코드에서 use-after-free가 날 수 있는 이유와 방지책은?

**원리**: `Change_Scene(new)`은 보통 현재 씬을 파괴한다(self-destruct). 그 호출이 현재 씬의 멤버 함수 안에서 일어나면, 호출 복귀 후의 `this`는 이미 해제된 메모리다.
**내 프로젝트**: Winters 에디터의 ESC 복귀가 정확히 이 패턴이라, `Request_BackToInGame()` 호출 직후 **즉시 return**을 코드 주석으로 강제했다(`Client/Private/Scene/Scene_Editor.cpp` 159~164행). 팀 차원에서는 gotchas 문서에 박제해 재발 방지.
**더 나은 설계(꼬리질문)**: 전환을 즉시 수행하지 않고 "다음 프레임 시작 시 처리"로 지연(deferred scene change)시키면 호출자 규약 자체가 필요 없어진다 — 리팩터링 방향으로 인지하고 있다.

### Q6. 원본 FBX를 런타임에 직접 파싱하지 않고 중간 포맷으로 쿠킹하는 이유는 무엇입니까?

**모범답변 4축**: (1) 로드 속도 — 파싱+후처리(탄젠트/본 정규화) 제거, GPU 업로드 형태와 일치하는 레이아웃으로 zero-copy 로드. (2) 용량 — 런타임 불필요 데이터 제거. (3) 의존성/결정성 — 출시 클라이언트에서 Assimp 같은 대형 파서 제거. (4) 검증 전진 — 포맷 오류를 런타임의 조용한 실패가 아니라 쿠킹 타임의 시끄러운 실패로.
**내 프로젝트**: `.wmesh` 파이프라인. 별도 실행 파일 `WintersAssetConverter.exe`(`Engine/Private/Tools/AssetConverter/main.cpp` + `Tools/WintersAssetConverter/WintersAssetConverter.vcxproj`)로 Assimp import → `.wmesh` 기록, 런타임 `CWMeshLoader`(`Engine/Public/AssetFormat/Mesh/WMeshLoader.h`)는 파일 블롭 내부를 가리키는 `pVertexBlob/pIndexBlob` zero-copy 포인터로 GPU 직행. 27개 에셋 전수 변환 FAIL=0, 이렐리아 FBX 60MB → 1.2MB(약 50×).
**꼬리질문 대비**: "그럼 반복 작업(iteration) 중엔 불편하지 않나" → 하이브리드 폴백(Q7)과 핫 리로드로 푼다. "왜 언리얼은 uasset+DDC 2단인가" → import 결과(uasset)와 플랫폼별 파생 데이터(DDC)를 분리해 캐시 공유하기 위함.

### Q7. 27개 에셋을 전수 변환하고도 왜 전면 교체가 아니라 '하이브리드' 통합을 했습니까?

**정의**: `CModel::LoadModel`에 fast-path를 얹었다 — 동명 `.wmesh` 존재 + `bone_count==0`(정적 메시)이면 zero-copy 로드 + Assimp 후처리 생략, 스킨드 메시(챔피언/미니언)는 기존 Assimp 경로 자동 폴백, GLB 임베디드 텍스처도 Assimp 유지.
**왜**: 리스크 관리다. 스킨드 메시는 `.wskel/.wanim`(스켈레톤/애니 분리 포맷 — `Engine/Public/AssetFormat/Anim/`에 포맷 실물 존재)까지 완성돼야 완전 이관이 되는데, 그 전에 전면 교체하면 검증 안 된 경로가 게임 전체를 인질로 잡는다. 정적 메시로 이득(맵 46MB 등 대형 에셋 로드)의 대부분을 먼저 회수하고, 기존 경로를 안전망으로 남겼다.
**일반화**: 파이프라인 마이그레이션의 정석 = "새 경로는 opt-in fast-path로 시작, 구 경로는 폴백으로 유지, 클래스별로 단계 이관". 면접에서 "빅뱅 교체 vs 점진 이관" 질문의 실전 사례로 쓴다.
**꼬리질문 대비**: "폴백이 영원히 남으면?" → 폴백 히트를 카운터로 계측해 이관 완료 시점을 데이터로 판단하고, 완료 후 제거를 로드맵에 명시한다.

### Q8. 바이너리 파일 포맷을 설계할 때 헤더에 반드시 넣는 것들과 그 이유를 말해보세요.

**모범답변**: ① magic(파일 종류 오인 즉시 거부), ② 버전(포맷 진화 대비 — major/minor 분리 시 "major 불일치=거부, minor 상위=무시" 정책 가능), ③ 크기/오프셋(트렁케이션 검출, 섹션 스킵), ④ 기능 flags(압축/해시 같은 opt-in을 버전 bump 없이), ⑤ reserved(향후 필드를 크기 변경 없이).
**내 프로젝트**: `Engine/Public/AssetFormat/Common/WintersFileHeader.h` — "WINT" magic + `version_major/minor` + `flags(WF_LZ4/WF_HAS_SHA256)` + `content_size`, `pack(1)` + `static_assert(sizeof==16)`. 스테이지 포맷도 동일 사상(`Shared/GameSim/Definitions/MapDataFormats.h`: 'WSTG' magic + version + `reserved[6]`).
**함정**: 구조체를 그대로 fwrite하려면 POD + 고정 크기 + 명시적 패킹 + static_assert가 전제 조건이라는 것 — 하나라도 빠지면 컴파일러/플랫폼이 바뀔 때 파일이 깨진다. Winters는 모든 포맷 구조체에 `static_assert`로 크기를 고정했다(`sizeof(BushEntry)==252` 등).

### Q9. 저장 포맷의 버전 관리를 실제로 어떻게 했습니까? 하위 호환은?

**내 프로젝트**: 스테이지 포맷이 v2→v5까지 실제로 진화했다. v2→v3는 에디터에서 미니언 배치를 **제거**하면서 bump(미니언은 넥서스 런타임 스폰 + 웨이포인트 AI 이동 대상이라 정적 배치가 도메인적으로 무의미하다는 판단 — 기능 삭제도 버전 사유), 이후 부시/미니언 웨이포인트 추가로 v5. 현재 `STAGE_VERSION=5, STAGE_VERSION_MIN_COMPAT=3`(`Shared/GameSim/Definitions/MapDataFormats.h` 8~9행) — 로드 시 버전이 [3,5] 범위면 버전별 분기 로드로 구파일을 계속 읽는다.
**원리 일반화**: 쓰기는 항상 최신 버전으로, 읽기는 MIN_COMPAT 이상을 버전 분기로 수용. 엔트리에 reserved 필드를 미리 두면 마이너 확장은 버전 bump 없이 흡수된다.
**자기비판(가점 포인트)**: 반면 `.wmesh`는 magic만 있고 버전 필드가 없다 — 2026-07-10 UE5.7 감사에서 스스로 HIGH 약점으로 식별했고, 포맷 버전 분기 로딩을 로드맵 Phase 2에 편입했다. "완벽했다"보다 "약점을 기계적으로 찾아내는 감사를 돌렸고 수리 순서를 정했다"가 훨씬 강한 답이다.

### Q10. 엔디안과 플랫폼 호환성 문제는 어떻게 다룹니까?

**모범답변**: 실무 우선순위로 답한다. (1) 포맷 명세에 바이트 오더를 **명시** — 현세대 PC/콘솔/모바일이 전부 리틀엔디안이라 "리틀 고정"이 사실상 표준이고, Winters도 Windows/x64 단일 타깃이라 리틀 고정. (2) 크로스엔디안이 진짜 요구되면 Reader/Writer 계층에서 스왑(런타임 포맷은 타깃 네이티브로 쿠킹하는 게 정석 — 스왑 비용을 툴 쪽으로). (3) 실제로 더 자주 깨지는 건 엔디안이 아니라 **패딩/정렬**(컴파일러별 상이)과 포인터 크기 — 그래서 `pack(1)`+`static_assert`+고정폭 타입(`uint32_t`)이 1차 방어선. (4) float은 IEEE754 가정 명시.
**꼬리질문 대비**: "그럼 텍스트(JSON)로 저장하면 다 해결 아닌가" → authoring 포맷(에디터 소스)은 JSON도 좋다 — 실제로 EldenRingEditor의 월드 셀 문서는 JSON authoring(schema `winters.world.cell.v1`)이고, 런타임 소비 포맷만 바이너리로 쿠킹하는 2단 구조가 정답. diff/merge 가능성(협업)은 authoring 포맷의 요구사항이지 런타임 포맷의 요구사항이 아니다.

### Q11. 데이터 주도 설계란 무엇이고, 엔티티 생성을 어떻게 데이터화했습니까?

**정의**: 동작(코드)과 내용(데이터)의 분리 — 콘텐츠 변경이 리컴파일 없이 데이터 편집으로 끝나게.
**내 프로젝트**: 3층이다. ① 엔티티 조립 — `CEntityBlueprint`(`Engine/Public/ECS/Systems/EntityBlueprint.h`): 컴포넌트 설치 람다(`Installer`)를 조립(composition)해 두고 `Spawn(world, pArg)`으로 복제. 리소스 I/O는 로딩 스레드에서 1회, 이후 스폰은 순수 메모리 조립 — 학원 수업의 Prototype 패턴을 ECS 위에 흡수한 형태. ② 레벨 데이터 — 에디터 배치 → `StageN.dat`, 게임은 데이터만 소비(하드코딩 배열 `defs[44]`를 데이터로 이관한 역사). ③ 게임플레이 수치 — authoring 소스 → cook → 정의 팩 경로만 허용, ImGui 튜너 값은 실험값(`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §2 "툴은 gameplay truth를 만들 수 없다").
**꼬리질문 대비**: "데이터 주도의 단점은?" → 데이터 오류가 컴파일 에러가 아니라 런타임 오류가 된다 → 그래서 Validator/스키마 검증이 데이터 주도의 필수 짝이다. 이 연결을 아는지가 시니어 판별 포인트.

### Q12. Undo/Redo를 어떻게 구현합니까? 커맨드 패턴과 memento 패턴 중 무엇을 왜 선택했나요?

**원리**: 커맨드 = 조작을 `Do()/Undo()` 객체로 물화해 스택 관리(변경 차분만 저장, 가벼움, 단 Undo가 Do의 완전한 역이어야 함). memento = 상태 스냅샷 저장(구현 단순·확실, 메모리 큼). 실무는 커맨드 기본 + 복잡한 편집(터레인 페인트 등)은 부분 스냅샷을 커맨드에 담는 하이브리드.
**내 프로젝트**: `EldenRingEditor/Public/World/EditorTransaction.h` — `IEditorCommand{Do/Undo/Name}` + `CEditorTransaction`(undo/redo 스택 + History 라벨 목록, Transaction 패널에서 이력 시각화). 구체 커맨드 3종(Add/Delete/Transform Placement). 디테일 두 개를 꼭 말한다: **선택 상태 복원**(각 커맨드가 selectedPlacementId 포인터와 복원값을 소유 — undo 후 선택이 엉뚱하면 사용자는 고장으로 인식), **원위치 복원**(`insertIndex/originalIndex` 저장 — append로 되돌리면 순서 데이터 오염).
**꼬리질문 대비**: "redo 스택은 언제 비우나" → 새 커맨드 Push 시(분기된 미래 폐기). "메모리 무한 증가는" → 스택 상한 + 오래된 항목 드롭, 또는 저장 시점 마커.

### Q13. 슬라이더 드래그로 오브젝트를 움직이면 undo 항목이 수백 개 생길 텐데 어떻게 합니까?

**모범답변**: 트랜잭션 경계를 "값 변경"이 아니라 "편집 제스처"에 맞춘다. 드래그 시작 시 before 값을 캡처하고, 드래그 중에는 draft를 직접 갱신(커맨드 생성 없음), 드래그 종료(커밋) 시 before→after를 담은 **커맨드 1개**를 Push한다. 취소(ESC)면 before로 롤백하고 커맨드를 만들지 않는다.
**내 프로젝트**: `CEldenRingEditorScene`이 정확히 이 구조다 — `m_detailsTransformBefore/m_detailsTransformDraft` + `CommitDetailsTransformEdit()/CancelDetailsTransformEdit()`(`EldenRingEditor/Public/EldenRingEditorScene.h` 42~43, 65~68행). ImGui에서는 `IsItemActivated()/IsItemDeactivatedAfterEdit()`가 제스처 경계 감지에 쓰인다.
**꼬리질문 대비**: "연속된 같은 종류 커맨드 병합(coalescing)은?" → 커맨드에 `MergeWith(prev)` 훅을 두는 방식도 있으나, 제스처 경계 방식이 더 예측 가능해서 우선한다.

### Q14. 마우스 클릭으로 3D 월드 좌표를 얻는 과정을 처음부터 설명해보세요.

**모범답변**: ① 픽셀 → NDC: `x' = 2x/W - 1`, `y' = 1 - 2y/H`(y 뒤집기). ② NDC의 near점(z=0)과 far점(z=1)을 `(View·Proj)^-1`로 월드 변환(w-나누기 포함) → 두 점을 잇는 레이 `O + t·D`. ③ 대상 기하와 교차 판정.
**내 프로젝트**: `CInput::GetMouseWorldRay(camera, W, H)`로 캡슐화했고, 맵 에디터의 배치/브러시가 전부 이 위에 서 있다(`Client/Private/Scene/Scene_Editor.cpp` `TryPickGroundPlane` 846행). 챔피언 호버는 레이–실린더 교차(사일러스 실린더 피킹, B-6).
**함정**: 뷰포트가 창 전체가 아닐 때(에디터 도킹) 뷰포트 오프셋 보정 누락, DPI 스케일, y축 방향 실수가 3대 단골 버그.

### Q15. 지면(y=0 평면) 피킹 공식을 유도하고 엣지 케이스를 말해보세요.

**유도**: 레이 `P(t) = O + t·D`를 평면 `y=0`에 대입 → `O.y + t·D.y = 0` → `t = -O.y / D.y`.
**엣지 케이스**: ① `|D.y| < ε`(레이가 평면과 평행) — 해 없음, 0나눗셈 가드. ② `t < 0` — 교차점이 카메라 뒤 → 거부. ③ 일반 평면 `n·X = d`로 확장하면 `t = (d - n·O)/(n·D)`.
**내 프로젝트**: `Scene_Editor.cpp` 857~862행이 정확히 이 두 가드를 포함한 실물이다. 이 함수 하나로 오브젝트 배치와 NavGrid 브러시 페인팅(좌클릭=차단, 우클릭=해제, 셀 반경 브러시)을 모두 구동한다.
**꼬리질문 대비**: "지형이 평면이 아니면?" → 하이트필드면 레이마칭/쿼드트리, 메시면 BVH 레이캐스트 — 에디터는 정확도, 게임플레이는 속도 우선으로 프록시를 고른다.

### Q16. 오브젝트 피킹에서 수학적 교차 판정과 ID 버퍼(픽셀 피킹)의 트레이드오프는?

**모범답변**: 수학 프록시(구/캡슐/실린더/OBB) — CPU-only, 프레임 지연 없음, 값싸다. 단 시각 메시와 오차가 있고 오브젝트가 겹치면 정렬 처리 필요. ID 버퍼 — 오브젝트 ID를 오프스크린 렌더타깃에 그리고 마우스 아래 1픽셀을 readback; 픽셀 정확도(알파 컷아웃까지 정확)지만 렌더 패스 추가 + GPU→CPU readback은 스톨을 피하려 1프레임 지연 비동기로 처리해야 한다.
**선택 기준**: 에디터의 정밀 선택은 ID 버퍼가 UX 우수, 게임플레이 호버(초당 수십 회, 관대한 판정이 오히려 좋음)는 프록시가 정답.
**내 프로젝트**: LoL 호버 타겟팅에 실린더 프록시를 채택 — 세로로 긴 캐릭터 실루엣에 맞고, "약간 관대한" 판정이 타겟팅 UX에 유리하다는 도메인 판단까지 포함된 선택이었다.

### Q17. 트랜스폼 기즈모를 직접 만든다면 어떻게 설계하겠습니까?

**모범답변**: ① 기즈모 핸들도 피킹 가능한 3D 오브젝트 — 축은 레이–원기둥, 평면 핸들은 레이–쿼드. ② 드래그 변환: 선택 축을 스크린 공간에 사영하고 마우스 델타를 그 축 방향 성분으로 환산(또는 축을 포함하는 드래그 평면과의 레이 교차를 프레임마다 풀어 월드 델타 산출). ③ 화면 크기 불변: 카메라 거리에 비례해 기즈모 월드 크기를 스케일. ④ 스냅(그리드/각도), 로컬/월드 좌표계 토글. ⑤ 드래그 종료 시 undo 커맨드 1개로 커밋(Q13 연결).
**실무 판단**: 검증된 ImGuizmo 채택도 정답 — "만들 수 있지만 사는(빌리는) 판단"을 보여주는 게 시니어 시그널. Winters 에디터는 현재 Inspector 수치 편집 + 클릭 배치 단계이고, 기즈모는 에디터 코어(트랜잭션 공용화) 이후 순번으로 로드맵에 있다.

### Q18. 노드 그래프 기반 이펙트 툴을 설계한다면 데이터 모델부터 어떻게 잡겠습니까?

**내 프로젝트 기반 답변**: Phase G 설계서(`.md/plan/EffectTool/`, 29파일)에서 실제로 내린 결정들 — ① **그래프 = 자산**: 이미터별 노드 목록 + 에지 목록을 JSON으로 저장(authoring), 런타임은 컴파일된 실행 플랜(`Engine/Private/FX/Exec/FxExecPlan.cpp`)을 소비. ② **평가 순서 = Kahn 위상 정렬**: 진입차수 0 노드부터 큐로 소비; 전 노드를 소비 못 하면 사이클 → 검증 에러. 실물 `CFxGraphValidator::Validate`가 `{bValid, issues[](노드ID+사유), topoOrder}`를 반환(`Engine/Public/FX/Graph/FxGraphValidator.h`) — 검증과 실행 순서 산출을 한 곳에서. ③ **파티클 스토리지 = SoA + swap-back kill**(뒤→앞 순회 강제 — 스왑된 원소 건너뛰기 버그 방지). ④ **수식 = 바이트코드 VM**(Niagara VectorVM 축소판) — 아티스트가 커브/수식을 코드 배포 없이 편집.
**우선순위 판단**: 노드 GUI(Stage 6)보다 데이터 포맷+실행기(Stage 3+5)가 먼저 — LoL 이펙트 90%는 GUI 없이 JSON 편집으로 커버 가능하다고 산정하고 단계를 배치했다. "툴 = GUI"가 아니라 "툴 = 데이터 계약 + 그걸 다루는 인터페이스들(CLI/GUI)"이라는 관점.

### Q19. 이펙트가 게임플레이 판정에 영향을 주는 경우(히트 이펙트 등) 서버 권위 게임에서는 어떻게 설계합니까?

**모범답변**: **결정적 FX와 시각 FX를 분리**한다. 판정에 관여하는 것(투사체 위치, 히트 타이밍)은 서버 권위 시뮬레이션의 결정론 tick에 속하고, 파티클/글로우/트레일 같은 순수 연출은 클라이언트 FX 시스템이 큐(cue) 이벤트를 받아 재생만 한다. 이러면 이펙트 시스템의 난수/프레임레이트 의존이 판정에 새어 들어갈 수 없다.
**내 프로젝트**: Phase G `09_INTEGRATION.md`에서 이 분리를 설계 원칙으로 박았다 — Niagara처럼 강력한 범용 시스템이 게임플레이와 얽힐 때 생기는 비결정성 문제를 구조로 회피. Winters는 서버 권위 30Hz + SimLab 해시 검증 체계라 이 분리가 필수 전제다. 클라이언트 FX 재생은 cue 이름 기반으로 중앙화(`FxCuePlayer`)해 폴백 정책도 카탈로그에서 관리한다.

### Q20. 프로파일러를 직접 만들었다고 했는데, 계측 매크로 설계에서 신경 쓴 점을 설명해보세요.

**내 프로젝트**(`Engine/Include/ProfilerAPI.h`): ① **RAII 스코프** — 생성자 Push/소멸자 Pop이라 early return·예외에도 짝이 보장. ② **출시 0 비용** — `WINTERS_PROFILING` 미정의 시 매크로가 `((void)0)`으로 완전 소거. ③ **`__LINE__` 2단계 CONCAT** — `a##b`는 인자를 확장 전에 붙이므로 간접 매크로 한 겹(`WINTERS_PROFILE_CAT`)으로 지연 확장해야 같은 블록에 스코프 2개가 가능(직접 밟은 버그). ④ **유계 버퍼** — 프레임당 이벤트 4096/카운터 128 상한(`Engine/Public/Core/Profiler/ProfilerTypes.h`)으로 계측 자체의 메모리 폭주 방지. ⑤ **시간+횟수 이원 계측** — 스코프(시간)와 카운터(`WINTERS_PROFILE_COUNT` — A* 방문 노드 수 등)를 분리; "느리다"의 원인이 '한 번이 느림'인지 '많이 불림'인지 구분된다. ⑥ **Tracy 이중 기록** — 자체 F3 HUD와 Tracy 존을 한 매크로로 동시 기록하되, Tracy 구현부를 Engine DLL 한 곳(TRACY_EXPORTS/IMPORTS)에만 둬 모듈 간 인스턴스 분열을 막았다.
**꼬리질문 대비**: "계측 오버헤드는?" → QPC 2회 + 배열 기록 수준으로 존당 수십 ns; 그래도 초고빈도 내부 루프에는 스코프가 아니라 카운터를 쓴다.

### Q21. 멀티스레드 환경에서 프로파일러가 깨졌던 경험이 있다면?

**내 프로젝트**: JobSystem 워커 병렬화 중 프로파일러 수집 버퍼에서 race가 났다(2026-04-28). 해결: ① 수집을 Decision/Apply **2-pass**로 나눠 기록과 집계 시점을 분리, ② 스레드 슬롯을 고정 배정(main=0, worker=idx+1)해 스레드별 버퍼가 겹치지 않게. `ProfilerEvent`에 `threadId`를 담아 오버레이에서 스레드별로 구분 표시.
**일반화**: 계측 인프라의 정석은 스레드-로컬 버퍼에 lock-free로 기록하고 프레임 경계에서 단일 스레드가 수확(harvest)하는 구조다. "프로파일러가 lock을 잡으면 측정 대상의 병렬성을 바꿔 버린다(관찰자 효과)"까지 말하면 만점.

### Q22. 계측으로 실제 성능 문제를 해결한 사례를 구체적 수치로 말해보세요.

**내 프로젝트**: 프레임이 17.8ms로 떨어진 사건. 추측하지 않고 11곳에 스코프 + 4종 카운터를 심었다 — 결과: 의심하던 Nav/A*는 각 0.003ms로 무죄, `Minion::AnimUpdate`가 16ms로 프레임의 90%(스키닝 본 갱신이 정적 오브젝트에까지 돌고 있었음). 수정: `RenderComponent::bAnimated` 플래그 도입, 정적 엔티티(맵/구조물/정글) 스폰 시 false → 애니 갱신 스킵. 결과: **17.8ms → 9ms**(~110fps). 부수 발견으로 엔진 루프의 Update/Render 중복 호출도 계측 중 드러나 제거.
**서사 포인트**: "증상 → 계측 배치 → 용의자 기각 → 범인 확정 → 최소 수정 → 수치로 검증"의 완결 사이클. 이 사건 이후 "최적화는 scope/counter와 frame budget으로 증명한다"를 아키텍처 규칙으로 명문화했다(`.md/architecture/WINTERS_CODEBASE_COMPASS.md` 성능 섹션 112행).

### Q23. 디버그 오버레이/가시화에 대한 본인의 철학은?

**모범답변**: "관측 가능성 우선" — 증상을 만나면 코드 추론을 쌓기 전에 권위 있는 코드 경로에 눈(오버레이/유계 로그/시각 캡처)을 먼저 단다. 이걸 개인 습관이 아니라 **팀 규칙**으로 명문화했다(`CLAUDE.md` Progressive Sections: 이동 버그면 현재 셀/다음 웨이포인트/경로/보정 방향/stuck 사유를 오버레이로 노출부터).
**파생 원칙 3개**: ① 죽은 진단은 무진단보다 나쁘다 — 포맷만 하고 출력 안 되는 실패 로그는 삭제, 실패 경로는 유계 출력 필수. ② 복제/직렬화 경계의 조용한 실패 금지 — verify 실패를 bare return으로 삼키면 스키마 드리프트가 네트워크 멈춤과 구분 불가. ③ 루틴 로그는 게이트 — 디버그 출력을 매크로 게이트 래퍼로 묶어 리뷰/런타임 소음 차단.
**실물**: 에디터의 NavGrid 오버레이(`Scene_Editor.cpp` `RenderNavGridOverlay`), 프로파일러 F3 오버레이. 실패 사례로 배운 것: 미니언 stuck 버그에서 코드 추론 3회 반복보다 프로파일러 카운터 5분이 정답이었다(2026-04-28).

### Q24. 언리얼 에디터 구조와 비교했을 때 자체 툴 체계의 강점과 약점을 평가해보세요.

**내 프로젝트**: UE 5.7.4 풀소스와 Winters를 7차원으로 대조 감사했다(2026-07-10, 에이전트 병렬 330회 코드 조사). 결론의 메타패턴 — Winters는 아키텍처(규칙) 수준은 현업급인데 **강제 장치**가 사람/문서/수동 검사 의존이고, UE는 같은 규칙을 빌드시스템/매크로/CI가 기계 강제한다. 툴 관점 구체 약점: LoL 에디터 undo 부재(단, 동작하는 `CEditorTransaction`이 EldenRingEditor에 있어 '패턴 부재'가 아니라 '공유 부재'), 원자적 저장 부재, `.wmesh` 버전 필드 부재, 누락 에셋 무음(placeholder 없음), minidump 부재.
**강점**: 네트워크 권위 모델은 감사에서 약점 0건, 에셋 쿠킹 파이프라인/FX 그래프+Validator는 UE와 동형 구조를 이미 갖췄다.
**핵심 어필**: UE에서 가져올 것은 개별 툴이 아니라 "자산은 검증된 계약으로만 게임에 들어간다"는 파이프라인 규율이라고 결론짓고, Content Browser=카탈로그 계약, Import=재현 가능한 변환, Niagara=그래프 자산+컴파일된 플랜 식으로 **계약 단위로 분해해 이식**하는 문서를 만들었다(`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §1 표).

### Q25. 언리얼의 Blueprint나 UObject 리플렉션 같은 시스템을 왜 채택하지 않았습니까?

**모범답변**(채택 안 한 이유를 아키텍처 근거로): ① **Blueprint 런타임 VM** — 서버 권위 30Hz 결정론 tick에 인터프리터 로직을 넣으면 시뮬레이션 해시 검증(SimLab) 체계가 무너진다. 대체: 수치는 generated pack, 분기 로직은 champion×variant 함수 테이블(GameplayHookRegistry), UI 연출만 Lua. ② **UObject/GC/리플렉션** — Winters ECS의 trivially-copyable 컴포넌트 + 스냅샷 복제 모델과 충돌; 리플렉션이 정말 필요한 곳(에디터 프로퍼티 그리드)은 정의 팩 스키마에서 필드 메타를 **코드젠**하는 것으로 한정. ③ **uasset 호환** — 유지비가 이득을 압도, 구매 기준을 "소스 포맷 포함"으로 고정하는 게 더 싸다. (`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §5)
**면접 포인트**: "무엇을 만들지"만큼 "무엇을 안 만들지"의 근거가 중요하다 — 툴 기능은 전부 유지보수 부채이므로, 자기 엔진의 불변식(결정론/스냅샷 복제)과 충돌하는 기능은 인기가 있어도 거절한다는 판단 기준을 보여준다.

### Q26. 툴이 크래시했을 때 사용자(아티스트)의 30분 작업이 날아가지 않게 하려면 어떻게 설계합니까?

**모범답변 4층**: ① **원자적 저장** — temp 파일에 완전 기록+flush 후 rename(Windows `ReplaceFile`) — rename의 원자성 덕에 어떤 시점에 죽어도 "온전한 구파일 또는 온전한 신파일"만 존재. 절대 원본 파일을 직접 열어 덮어쓰지 않는다. ② **오토세이브** — 주기 스냅샷(별도 파일명)+시작 시 복구 제안; 커맨드 스택이 있으면 조작 저널 리플레이도 가능. ③ **크래시 가시화** — `SetUnhandledExceptionFilter`+minidump로 원인 수집(고쳐야 재발이 멈춘다). ④ **저장과 검증의 분리** — 검증 실패 문서도 '불량 마크와 함께' 저장은 되게; 사용자 작업물 보존이 데이터 순결성보다 우선.
**정직한 자기평가**: Winters LoL 에디터는 현재 dirty 플래그+명시 저장까지만 있고 원자적 저장/오토세이브는 미비 — UE5.7 감사에서 식별해 에디터 코어 선행 과제(AtomicWriteFile 헬퍼)로 로드맵 1순위에 올려 둔 상태라고 답한다. 문제를 아는 것과 모르는 것의 차이를 보여주는 답.

### Q27. 아티스트/디자이너용 툴의 UX에서 프로그래머가 흔히 놓치는 것은 무엇입니까?

**모범답변**: ① **피드백 부재** — 저장/변환이 됐는지 안 됐는지 침묵하는 툴. Winters 에디터는 저장 성공 시 엔트리 수+경로, 실패 시 원인+경로를 즉시 표시(`Scene_Editor.cpp` 923~948행), 메뉴바에 현재 스테이지/오브젝트 카운트/FPS 상시 노출. ② **실험과 영속의 경계 모호** — 슬라이더로 튜닝한 값이 저장되는지 아닌지 불명확하면 디자이너는 툴을 불신한다; Winters는 "ImGui 튜너=실험값, Save Preset=authoring 소스 역기록 후 재쿠킹"을 명시적 버튼으로 분리하는 규칙을 문서화. ③ **에러 메시지가 프로그래머용** — "무엇이, 어느 파일이, 왜" 형식으로(Validator 이슈에 노드 ID+사유를 담는 `FxValidationIssue` 구조가 그 예). ④ **불합격 자산 은폐** — 실패한 에셋이 목록에서 사라지면 아티스트는 뭘 고칠지 모른다; '불합격' 상태+사유로 카탈로그에 남긴다. ⑤ **관례 무시** — 해당 직군이 쓰는 DCC의 단축키/용어 관례를 따른다.
**핵심 문장**: "툴의 고객은 컴파일러가 아니라 옆자리 동료다. 툴 신뢰가 무너지면 사람들은 엑셀과 수작업으로 돌아가고, 그때부터 데이터 품질은 아무도 보증하지 못한다."

### Q28. 에셋 1,000개를 일괄 변환하는 배치 파이프라인을 설계해보세요.

**모범답변**: ① **CLI가 본체** — GUI 툴은 CLI 위의 껍질; CLI여야 CI에 올라간다. exit code 규약(성공 0/실패 비0) 준수. ② **manifest 주도** — "무엇을 어떤 설정으로 어디로"를 선언 파일로; 명령 인자 나열은 재현 불가능해진다. ③ **증분 처리** — (원본 해시, 설정, 컨버터 버전)이 같으면 스킵; 컨버터 버전이 캐시 키에 들어가야 컨버터 수정 시 전체 재쿠킹이 자동 트리거. ④ **실패 격리+집계** — 한 에셋 실패가 배치를 중단시키지 않되, 종료 시 실패 목록(파일/스테이지/원인)을 집계 리포트로; 1000개 중 3개 실패가 묻히면 릴리스 사고. ⑤ **병렬화** — 에셋 단위 독립이므로 파일 단위 잡 분배가 자연스럽다.
**내 프로젝트**: `WintersAssetConverter.exe` + `Tools/convert_all_assets.bat`로 27개 전수 변환 FAIL=0을 실행했고, Fab 자산 수용 설계에서 manifest 필수 필드(출처/라이선스/재배포 가능 여부)까지 규정했다(`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §3-7). 다음 단계는 컨버터의 라이브러리화 — 에디터가 같은 코드를 in-process로 호출해 임포트 UI를 만드는 방향(§4).

### Q29. 에셋이 누락된 채 게임이 출시되는 사고를 파이프라인으로 어떻게 막습니까?

**모범답변**: 3중 방어. ① **쿠킹 타임** — 참조 무결성 검사: 레벨/정의 팩이 참조하는 에셋이 카탈로그에 있는지 cook 단계에서 검증, 없으면 빌드 실패. ② **런타임 개발 빌드** — 누락 시 눈에 띄는 **placeholder 에셋**(마젠타 체커 텍스처, 에러 메시) + 유계 로그; 조용한 스킵이 최악이다. ③ **CI 게이트** — 변환 스크립트 exit code를 CI가 강제.
**정직한 자기평가**: UE5.7 감사에서 Winters의 현재 상태는 "누락 에셋 = 릴리스 무음(placeholder 없음)"으로 HIGH 약점 식별 — placeholder 에셋 도입이 로드맵 Phase 2에 있다. 과거 사고 사례도 있다: UI PNG 로드 실패가 흰 사각형으로만 나타나 원인 추적에 세션 하나를 썼다(경로 문자열의 `\U` 이스케이프 충돌) — "실패를 시끄럽게"의 필요성을 몸으로 배운 케이스.

### Q30. 핫 리로드(에디터에서 수정 → 게임에 즉시 반영)는 어떻게 설계합니까?

**모범답변**: ① **감지** — 파일 워처(`ReadDirectoryChangesW`) 또는 툴이 직접 리로드 신호 전송. ② **교체 단위 설계가 본체** — 리소스 핸들을 간접화(핸들→실데이터 테이블)해 두면 데이터 포인터 스왑만으로 교체 가능; GPU 리소스는 사용 중 프레임 종료 후 교체(지연 파괴). ③ **부분 실패 안전** — 새 파일이 검증 실패면 기존 데이터 유지 + 에러 표시(리로드가 게임을 죽이면 안 씀). ④ **상태 보존 정책** — 이펙트 리로드 시 재생 중 인스턴스를 재시작할지 유지할지 명시적 결정.
**내 프로젝트**: Phase G 설계서 `10_DEBUG_TOOLS.md`에 FX Hot Reload + Replay + Determinism Checker를 디버그 툴 축으로 설계했고, `26_HOT_RELOAD_AND_COOKED_BINARY_BAKE.md`에서 쿠킹된 바이너리와 핫 리로드의 공존(개발=JSON 직독+리로드, 출시=cooked 바이너리)을 다뤘다. 라이브 튜닝(ImGui 슬라이더)은 이미 EffectTuner로 실사용 중 — 핫 리로드의 최소형이 "메모리 값 직접 튜닝"이고, 파일 단위 리로드는 그 확장이라는 관점.

### Q31. 툴 개발 우선순위는 어떻게 정합니까? 만들고 싶은 툴과 만들어야 하는 툴이 다를 때는?

**내 프로젝트 사례로 답변**: 포트폴리오용 flagship 툴(어빌리티 타임라인 에디터)을 바로 만들고 싶었지만, UE5.7 감사 결과 "지금 만들면 editor-tooling 약점(undo 부재/비원자 저장/검증 미배선/영속화 부재)을 전부 상속한다"는 결론이 나왔다. 그래서 순서를 뒤집었다: ① 에디터 코어 선행 — EldenRing `CEditorTransaction` 공용 승격 + AtomicWriteFile 헬퍼 + 이미 존재하지만 저장 경로에 미배선인 `CFxGraphValidator` 연결 + 격리 프리뷰 월드 → ② 그 위에 타임라인 툴 → ③ 언리얼 이식(UDataAsset+커스텀 에셋 에디터) → ④ Fab 출시.
**일반화**: 툴의 기반 기능(undo/저장 안전/검증)은 각 툴에 복붙되는 게 아니라 코어로 한 번 만들어 공유해야 하고, 코어가 없는 상태에서 툴 수를 늘리면 부채가 툴 수만큼 복제된다. "만들 순서가 곧 아키텍처 결정"이라는 답.

### Q32. 에디터에서 편집하는 데이터가 서버 권위 값(밸런스 수치)일 때 특별히 지켜야 할 것은?

**모범답변**: 에디터는 **authoring 소스만** 편집하고, 런타임(특히 서버)이 소비하는 팩은 반드시 cook을 거쳐 생성한다 — 런타임 팩을 직접 편집하는 UI는 만들지 않는다. 라이브 튜닝은 로컬 실험으로 격리하고, 저장은 authoring 소스 역기록 + 재쿠킹 + build hash 갱신. 이래야 (a) 클라이언트가 수치를 조작할 통로가 없고, (b) 서버/클라이언트가 같은 데이터 빌드를 쓰는지 hash로 검증 가능하고, (c) 데이터 변경 이력이 소스 관리에 남는다.
**내 프로젝트**: "툴은 gameplay truth를 만들 수 없다"를 대원칙 P2로 문서화(`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md` §2-3). 감사에서 dataBuildHash 불일치가 로그만 남기고 계속 진행하는 것을 약점으로 잡아 버전 핸드셰이크 게이트를 로드맵 Phase 0에 올렸다 — 툴 파이프라인과 네트워크 무결성이 만나는 지점까지 설계 범위로 본다는 어필.

### Q33. 맵 에디터에서 내비게이션 데이터(NavGrid)는 어떻게 편집하게 했습니까?

**내 프로젝트**: 두 경로다. ① **자동 생성** — 구조물 배치 데이터로부터 `Mark_StructuresOnNavGrid()`가 AABB 반경만큼 셀 차단(터렛 2m/억제기 3m/넥서스 4m) — 배치 데이터가 곧 내비 차단의 소스. ② **수동 페인팅** — 에디터에 NavGrid 브러시 모드(`Scene_Editor.cpp` `PaintNavGridAt`, 822행): 지면 피킹 위치에 셀 반경 브러시로 좌클릭=차단/우클릭=해제, `RenderNavGridOverlay`로 차단 셀을 월드에 겹쳐 그려 눈으로 확인하며 편집, 스테이지와 함께 Save/Load(`SaveCurrentNavGrid/LoadCurrentNavGrid`).
**설계 포인트**: 게임플레이 데이터(A*가 소비)와 그 시각화(오버레이)를 항상 짝으로 만든다 — 보이지 않는 데이터를 편집하게 하면 반드시 불량 데이터가 들어온다. 자동 생성과 수동 오버라이드의 공존은 실무 내비 파이프라인(Recast 자동 베이크 + 수동 마커)과 같은 구도.

### Q34. 에디터에서 배치 대상을 선정할 때 "에디터에 넣지 않기로 한 것"이 있습니까? 왜죠?

**내 프로젝트**: 미니언. 초기 버전엔 미니언 배치가 있었지만 제거하고 포맷 버전을 bump했다(v2→v3). 이유 — 미니언은 넥서스에서 **런타임 주기 스폰**되어 라인 웨이포인트를 따라 이동하는 존재라 "고정 위치 배치"라는 에디터 개념 자체가 도메인과 안 맞았다. 대신 에디터에는 미니언 **웨이포인트**(팀/레인/순번 — `MinionWaypointEntry`)를 배치 대상으로 남겼다 — 정적인 것(경로 데이터)과 동적인 것(스폰 개체)을 분리한 것.
**일반화**: 툴 기능은 "기술적으로 가능한가"가 아니라 "그 데이터의 수명 주기가 정적인가"로 정한다. 런타임 시스템이 소유해야 할 것을 에디터가 배치하게 만들면 두 소유자가 충돌한다. 기능을 지우면서 포맷 버전을 올린 것 — "삭제도 버전 사유"라는 직렬화 감각까지 묶어서 답할 수 있는 사례.

### Q35. 지금까지 만든 툴 중 가장 아쉬운 결정과, 다시 만든다면 바꿀 것은?

**모범답변(정직+구체)**: ① `.wmesh`에 버전 필드를 안 넣은 것 — 공통 `WintersFileHeader`(version_major/minor 있음)를 설계해 놓고 `.wmesh`가 그걸 안 쓰고 자체 magic만 갖게 방치했다. 포맷이 두 규약으로 갈라진 것 자체가 문제 — 다시 만들면 모든 `.w*`가 공통 헤더를 강제 통과하게 Writer 계층에서 막는다. ② LoL 에디터에 undo 없이 기능을 늘린 것 — EldenRing 에디터에서 트랜잭션을 만들 때 공용 코어로 승격했어야 했는데 툴별로 갈라졌다. ③ 저장이 비원자적인 것.
**마무리**: 셋 다 UE5.7 소스 감사로 **스스로 식별**했고 "에디터 코어 선행" 로드맵으로 수리 순서를 확정했다 — 아쉬운 결정을 말하는 질문은 사실 "약점을 발견하는 프로세스가 있는가"를 묻는 것이므로, 감사→로드맵→우선순위 재배치의 사이클로 답을 맺는다.

---

## 내 프로젝트 연결 포인트

면접에서 그대로 쓸 수 있는 어필 문장들. (숫자와 경로는 전부 실물 검증됨)

1. "맵 에디터를 게임과 같은 `IScene` 인터페이스의 형제 씬으로 분리하고, 에디터 저장과 게임 로드가 같은 `CMapDataIO` 계약을 타게 만들었습니다. 배치 → Ctrl+S → 재시작 → 자동 로드 복원까지 풀 사이클이 동작합니다." (`Client/Private/Scene/Scene_Editor.cpp`, `Client/Public/Map/MapDataIO.h`)
2. "스테이지 바이너리 포맷을 v2에서 v5까지 실제로 진화시켰고, `STAGE_VERSION_MIN_COMPAT=3`으로 구버전 파일 하위 호환을 유지합니다. 미니언 배치 기능을 '제거'하면서 버전을 올린 경험 — 삭제도 버전 사유라는 걸 압니다." (`Shared/GameSim/Definitions/MapDataFormats.h`)
3. "FBX/GLB 27개를 자체 `.wmesh`로 전수 변환(FAIL=0)하고 런타임에 하이브리드로 통합했습니다 — 정적 메시는 zero-copy fast-path, 스킨드는 기존 경로 폴백. 이렐리아 FBX 60MB가 1.2MB가 됐습니다." (`Tools/WintersAssetConverter/`, `Engine/Public/AssetFormat/Mesh/WMeshLoader.h`)
4. "Undo/Redo는 커맨드 패턴 트랜잭션으로 구현했고, 선택 상태 복원과 원위치 삽입, 드래그 1제스처=1커맨드 커밋까지 처리했습니다." (`EldenRingEditor/Public/World/EditorTransaction.h`, `EldenRingEditorScene.h`)
5. "프로파일러를 직접 만들어(RAII 스코프+QPC+F3 오버레이+카운터, 출시 빌드 0 비용, Tracy DLL 단일 인스턴스 통합) 17.8ms 프레임 저하의 범인을 스키닝 갱신으로 확정하고 9ms로 복구했습니다. 추측 대신 계측이 제 기본 사이클입니다." (`Engine/Include/ProfilerAPI.h`)
6. "이펙트 툴은 Niagara/FXR를 계약 단위로 분석해 노드 그래프 자산(JSON+Kahn 위상 정렬) → 컴파일된 실행 플랜 구조로 설계했고, 그래프 Validator(`CFxGraphValidator` — 이슈 목록+topoOrder 반환)는 엔진에 실물로 들어가 있습니다. 서버 권위 게임이라 결정적 FX와 시각 FX를 구조적으로 분리했습니다." (`Engine/Public/FX/Graph/FxGraphValidator.h`, `.md/plan/EffectTool/`)
7. "UE 5.7.4 풀소스와 제 엔진을 7차원 대조 감사해서 툴 약점(undo 공유 부재/비원자 저장/포맷 버전 부재/누락 에셋 무음)을 스스로 식별했고, '에디터 코어 선행 → flagship 툴 → 언리얼 이식 → Fab 출시' 순서로 로드맵을 재배치했습니다. 무엇을 안 만들지(Blueprint VM, UObject 리플렉션)도 결정론/스냅샷 복제라는 엔진 불변식 근거로 결정했습니다." (`.md/architecture/WINTERS_UE_FAB_TOOL_ADOPTION.md`)
8. "툴의 대원칙을 문서로 강제합니다 — '툴은 런타임 계약의 생산자다', 'Validator가 게이트다, 조용한 폴백 금지', '툴은 gameplay truth를 만들 수 없다', '에디터는 F5 런타임을 우회하지 않는다'." (`.md/architecture/WINTERS_CODEBASE_COMPASS.md` Tools/Editor 섹션)

## 마지막 점검 체크리스트

- [ ] 즉시모드 = 매 프레임 재선언 + ID 스택 해시 + 데이터가 진실(동기화 소거) / 보존모드 = 위젯 트리 + invalidation. 툴은 ImGui, 출시 HUD는 보존모드.
- [ ] ImGui ID 충돌 → `PushID`/`##`/`###`. 디버그 new 매크로 충돌 → push_macro/undef 격리 (Scene_Editor.h 실물).
- [ ] 에디터-런타임 분리: 같은 IScene 형제 씬 + 같은 로드 계약. "에디터는 F5 런타임을 우회하지 않는다." 씬 전환 self-destruct → 즉시 return.
- [ ] 쿠킹 4근거: 로드 속도(zero-copy)/용량(60MB→1.2MB)/파서 의존 제거/검증 전진. 27개 FAIL=0, 하이브리드 폴백(정적만 fast-path).
- [ ] 헤더 5요소: magic/version(major·minor)/size/flags/reserved. pack(1)+static_assert. STAGE v5 + MIN_COMPAT 3. 자기비판: .wmesh 버전 필드 없음(감사 식별, 로드맵 편입).
- [ ] 엔디안: 리틀 고정 명시가 실무 표준. 진짜 적은 패딩/정렬. authoring=JSON, 런타임=바이너리 2단.
- [ ] Undo: 커맨드 패턴, redo 스택은 새 커맨드에 비움, 선택 상태+삽입 위치 복원, 드래그=커밋 시 1커맨드 (EditorTransaction.h 실물). LoL 툴 undo는 '공유 부재'.
- [ ] 피킹: 픽셀→NDC→역투영→레이. 지면 t = -O.y/D.y (평행 가드 + t<0 가드). 프록시 vs ID 버퍼 트레이드오프. 기즈모 = 거리 비례 스케일 + 축 사영 드래그.
- [ ] FX 툴: 그래프=JSON 자산, Kahn 위상 정렬(사이클=미소비 노드), SoA+swap-back kill(뒤→앞), 표현식 VM, 결정적 FX/시각 FX 분리. Validator 실물 존재.
- [ ] 프로파일러: RAII+QPC, WINTERS_PROFILING 게이트=출시 0비용, __LINE__ 2단 CONCAT, 유계 버퍼, 시간+카운터 이원, 스레드 슬롯 고정+2-pass. 17.8→9ms 서사.
- [ ] 관측 가능성: 오버레이 먼저, 죽은 진단 삭제, 경계 verify 실패는 유계 트레이스, 루틴 로그 게이트.
- [ ] UE 비교: "규칙은 있는데 기계 강제가 없다" 메타패턴. 계약 단위 이식(카탈로그/재현 가능 변환/그래프 자산). 안 만드는 것: Blueprint VM/UObject/uasset — 결정론 근거.
- [ ] 크래시 안전: temp+rename 원자 저장 / 오토세이브 / minidump / 검증-저장 분리. Winters 현재 미비 + 로드맵 1순위라고 정직하게.
- [ ] 배치 처리: CLI 본체, manifest 주도, 증분(해시+컨버터 버전), 실패 집계, exit code → CI.
- [ ] UX: 즉시 피드백(저장 결과+카운트), 실험 vs 영속 분리(튜너/Save Preset), 불합격도 보이게, 에러는 "무엇이/어디서/왜".
- [ ] 툴 순서: 에디터 코어(트랜잭션 공용화+원자 저장+Validator 배선) → 타임라인 툴 → 언리얼 이식 → Fab. "순서가 곧 아키텍처".
