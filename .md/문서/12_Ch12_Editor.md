# Ch12. Editor (DCC / Blueprint / ContentBrowser / WorldEditor)

> Winters 현재: `Engine/Public/Editor/` (ImGui in-game tuner), `Client/Private/Scene/Scene_Editor.cpp` (맵 에디터).
> 레퍼런스: `UnrealEngine/Engine/Source/Editor/` (143개 모듈), `UnrealEd/`, `PropertyEditor/`, `BlueprintGraph/`, `ContentBrowser/`.

---

## 1. 기초 원리 — Editor는 게임의 5배 크기다

UE5 Source Editor 모듈 수: **143개** (Runtime 188개와 비슷한 규모).

왜 이렇게 크나:
- 데이터를 만드는 사람은 코더보다 많다 (기획자, 아티스트, 디자이너)
- 데이터를 만드는 시간은 게임 코드 짜는 시간의 10배
- 데이터 만드는 도구가 부실하면 인건비가 폭발

**Editor의 4가지 목적**:
1. **Asset 입력** — DCC 툴(Maya, Blender, Photoshop) → 엔진 포맷 변환
2. **Asset 편집** — 머티리얼, 애니메이션, 위젯, 시퀀스, 캐릭터를 엔진 안에서 직접
3. **Scene 편집** — 월드에 actor 배치, 라이팅, 콜리전, AI 경로
4. **Play Test** — 코드 + 데이터를 즉시 실행 / 디버그

이걸 다 만들면 **별도 desktop app** 1개가 더 만들어진다. UnrealEditor.exe가 그것.

---

## 2. 핵심 — UE5 Editor 6대 축

### 2.1 UnrealEd (코어)

`Source/Editor/UnrealEd/`. 에디터 메인 엔진.

`UnrealEdEngine` (`Classes/Editor/UnrealEdEngine.h`):
- 게임 엔진의 superset. 일반 `UEngine`이 가진 모든 것 + 에디터 전용 기능.
- PIE (Play-In-Editor) 관리
- Save/Load asset coordination
- Undo/Redo stack (Transactional system)

### 2.2 PropertyEditor / DetailsPanel

UObject의 reflection 정보를 읽어 자동 UI 생성. UPROPERTY 매크로 + meta가 결정.

```cpp
UCLASS()
class UMyCharacter : public APawn
{
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats", meta=(ClampMin=0, ClampMax=100))
    float Health = 100;

    UPROPERTY(EditAnywhere, Category="Stats", meta=(EditCondition="bUseShield"))
    float Shield = 50;

    UPROPERTY(EditAnywhere, Category="Stats")
    bool bUseShield = true;
};
```

이 클래스를 DetailsPanel에 띄우면:
```text
[Stats]
  Health   [████████████------] 100   (slider 0~100)
  Use Shield   [✓]
  Shield   [██████-----------]  50   (editable when bUseShield)
```

→ 디자이너가 코드 한 줄 안 보고 stat 조정.

`Source/Editor/PropertyEditor/Private/DetailCategoryBuilderImpl.h` 등.

### 2.3 ContentBrowser

`Source/Editor/ContentBrowser/`. 모든 asset 탐색/검색/import.

```text
┌────────────────────────────────────────────────┐
│ Filter ▼  [search...]               [+ New ▼] │
├────────┬───────────────────────────────────────┤
│ Tree   │ Grid                                   │
│ ├─ Game│ [Character.png] [Hero.fbx] [Q.anim]   │
│ │ ├─Maps │ [Skill.ability] [HUD.umg] ...        │
│ │ ├─FX │  ...                                   │
│ │ └─UI │                                       │
└────────┴───────────────────────────────────────┘
```

기능:
- Asset 검색 (이름 / 클래스 / 태그)
- Reference viewer (이 asset을 누가 쓰나, 이 asset이 뭘 쓰나)
- Drag & drop import (.fbx → SkeletalMesh)
- Migrate (project 간 asset 이동, dep 자동 동행)

### 2.4 Blueprint Graph

비주얼 스크립팅. UE5의 정체성.

`Source/Editor/BlueprintGraph/`.

```text
[Event BeginPlay] ──▶ [Set Health to 100] ──▶ [Print String "Spawned"]
                                                    ▲
[Event Tick] ──▶ [If Health < 30] ─true─┐           │
                                  false─┘           │
                                        ──▶ [Play Sound HeartBeat]
```

각 노드 = 함수 호출 / 분기 / 변수 read/write. Compile 시 native instructions으로 변환 (VM 또는 nativization).

기획자가 prototyping용. 시니어 코더가 native로 옮기는 협업 패턴.

### 2.5 World Editor (Level Editor)

`Source/Editor/LevelEditor/`. 월드에 actor 배치.

- 뷰포트 (perspective + ortho)
- Outliner (actor 트리)
- Details panel (선택 actor의 property)
- Toolbar (PIE, Save, Build Lighting)
- Foliage tool, Landscape tool, Geometry tool, BSP

### 2.6 Specialized Editors

각 asset 종류마다 전용 에디터. UE5 143개 중 대부분 이거.

```text
AnimationEditor          .anim asset 편집 (curve, notify)
AnimationBlueprintEditor AnimBP 그래프 편집
MaterialEditor           shader graph
PhysicsAssetEditor       ragdoll 본 + collision
BehaviorTreeEditor       BT 그래프
EnvironmentQueryEditor   EQS 노드
NiagaraEditor            VFX 노드 그래프
SequencerEditor          Sequence 트랙/키프레임
UMGEditor (WidgetBlueprintEditor) 위젯 디자이너
DataTableEditor          엑셀 같은 표
CurveEditor              float/vector curve
StaticMeshEditor         mesh + lod + collision
SkeletalMeshEditor       skinned mesh + skeleton
SkeletonEditor           본 트리 + retarget
LandscapeEditor          terrain 페인팅
FoliageEditor            식생 배치
```

각 에디터 = 별도 module + 별도 UI + 별도 undo stack + 별도 import/export.

---

## 3. 심화

### 3.1 Reflection / UObject System

UE5 Editor의 모든 자동화의 뿌리.

```cpp
UCLASS()    // → UHT가 .gen.h에 Z_Construct_UClass_UMyClass 생성
class UMyClass : public UObject
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)   // → UHT가 property record를 metadata에
    float Value = 0;

    UFUNCTION(BlueprintCallable)   // → UHT가 function record + Blueprint stub
    void DoSomething();
};
```

UHT (UnrealHeaderTool)가 빌드 시 .h를 파싱해 reflection 코드 생성. 런타임에 `GetClass()`로 모든 property/function/meta 조회 가능.

이게 없으면:
- DetailsPanel 못 만든다 (property 모름)
- Serialization 못 한다 (필드 모름)
- Blueprint 못 짠다 (function 모름)
- Undo/Redo 못 한다 (property change tracking 불가)

**Ch12를 시작하기 전에 Ch13 Tooling의 HeaderTool이 먼저 와야 한다.**

### 3.2 Transactional / Undo System

```cpp
const FScopedTransaction Transaction(LOCTEXT("ChangeHealth", "Change Health"));
MyActor->Modify();          // 이 시점에 reflection으로 snapshot
MyActor->Health = 80;
// 트랜잭션 종료 시 undo buffer에 push
// Ctrl+Z → snapshot 복원
```

property가 reflection 등록되어 있어야 작동.

### 3.3 PIE (Play-In-Editor)

에디터 안에서 게임 실행. World가 PIE world로 duplicate, 같은 process에서 simulate.

- PIE 시작 시 모든 변경사항 save 안 한 채로 simulate
- 도중에 stat / property 수정 가능
- 게임 끝나면 원본 world 복원

Hot-iteration의 핵심. 디자이너가 변경 → 즉시 PIE → 확인.

### 3.4 Asset Pipeline / DDC

- DCC 툴에서 export (FBX, PNG, WAV)
- Editor의 import factory가 변환 (UStaticMeshFactory, UTextureFactory)
- DDC (Derived Data Cache)에 cooked binary 저장
- 다른 사람이 같은 source 받으면 DDC에서 자동 fetch (재변환 안 함)

UE5: `Source/Runtime/DerivedDataCache/`.

### 3.5 Source Control 통합

Perforce / Git / SVN. Editor가 직접 lock/checkout/submit.

큰 팀일수록 필수. WorldPartition + External Actor가 이걸 가능하게 함 (한 .umap 충돌이 아니라 actor.uasset 단위 협업).

### 3.6 Hot Reload

C++ 코드 변경 → 재컴파일 → 게임 안 끄고 새 .dll swap. UE5 5.x는 Live Coding (`Live++` 기반).

조심: 메모리 레이아웃 변경되는 변경(class에 멤버 추가)은 hot reload 불가. 함수 본문 수정만 가능.

---

## 4. Winters 매핑

### 4.1 현재 상태

- `Engine/Public/Editor/` — ImGui 기반 in-game tuner
- `Client/Private/Scene/Scene_Editor.cpp` — 맵/구조물 배치
- B-6.7 박제: Structure_Manager / Jungle_Manager singleton, Stage1.dat binary save/load
- Phase G (Effect Tool 계획): 11 파일 6295줄 — Niagara/FXR 참조 노드 그래프

→ 1차 in-game tuner level. Reflection 시스템 없음.

### 4.2 Ch12 단계별 (CLAUDE.md 본 brief의 Stage A~D 재정리)

```text
[Stage A — 현재] In-game ImGui tuner
   장점: 진입 비용 0, 게임 안에서 즉시 튜닝
   한계: 디자이너가 게임 빌드 필요, 작업물 공유 어려움

[Stage B] Tools/WintersEditor.exe — 별도 desktop app
   ├── ContentBrowser
   │   ├── Asset 트리 (.wmesh, .wanim, .ability, .fx, .character)
   │   ├── 검색 / 필터
   │   ├── Reference viewer
   │   └── Drag&drop import
   ├── WorldEditor
   │   ├── 뷰포트 (DX12 client viewport embed)
   │   ├── Outliner (entity 트리)
   │   ├── Details panel (선택 entity property)
   │   └── Save / Load / PIE
   ├── DetailsPanel (reflection 기반)
   ├── BlueprintLite (Ability/AI 한정 노드 그래프)
   └── AssetImportPipeline (.fbx → .wmesh, .png → .wtex)

[Stage C] Reflection 시스템 (Ch13 의존)
   WINTERS_CLASS / WINTERS_PROPERTY 매크로
   HeaderTool로 .gen.h 자동 생성
   DetailsPanel 자동화

[Stage D] Hot Reload
   Live++ 또는 자체 patch system
   Win-only 우선, 추후 Mac/Linux
```

### 4.3 Ch12 추가 헤더 (제안)

```cpp
// Engine/Public/Reflection/Type.h
class WINTERS_ENGINE CType
{
public:
    const char* Name() const;
    u32_t       SizeBytes() const;
    const std::vector<CProperty>& Properties() const;
    const std::vector<CFunction>& Functions() const;

    void* Construct() const;
    void  Destruct(void* obj) const;
};

// Engine/Public/Reflection/Property.h
class WINTERS_ENGINE CProperty
{
public:
    const char* Name() const;
    CType*      ContainerType() const;
    CType*      ValueType() const;
    u32_t       Offset() const;
    PropertyMeta Meta() const;        // EditAnywhere, ClampMin, Category 등

    template<typename T> T    Get(const void* obj) const;
    template<typename T> void Set(void* obj, const T& v) const;
};

// Tools/WintersEditor/  (별도 desktop app)
// WintersEditor.vcxproj
//   ├── WintersEditor.exe
//   └── 의존: Engine.dll + ImGui-Docking 또는 Slate-등가

// Tools/WintersHeaderTool/  (Ch13)
// .h 파싱 → .gen.h 생성
```

### 4.4 Bot AI / 서버 권위와의 관계

Editor는 클라/툴 전용. 서버는 Editor를 모른다. 하지만 Editor가 다루는 데이터(.ability, .character)는 server cooker가 같이 cook → 런타임에 서버/클라가 같은 데이터 사용.

### 4.5 게임별 적용

| 게임 | 필요 Stage |
|------|-----------|
| LoL (현재 + 1차) | Stage A (이미) — Stage B는 챔프 30+ 가면 필요 |
| 로아 / 엘든링 | Stage B 필수 + C 부분 |
| GTA6 / MMO | Stage B/C/D 전부 |

### 4.6 Phase G EffectTool과의 합류

memory 박제 `project_phase_g_fx_plan.md`:
> Niagara/FXR 참조 노드 그래프 이펙트 시스템 11 파일 (6295 줄) .md/plan/EffectTool/ 작성

→ Phase G EffectTool은 Ch12 Stage B의 **첫 specialized editor**다. UE5 NiagaraEditor 등가.

전체 Stage B 안에서:
```text
Tools/WintersEditor/
├── ContentBrowser
├── WorldEditor
├── EffectEditor   (Phase G 계획서 기반 — 1순위)
├── AbilityEditor  (Ch8 GAS 기반)
├── AnimEditor     (Ch4 montage/notify 기반)
└── SequencerEditor(Ch11 기반)
```

---

## 5. 검증 명령

```powershell
# Stage A (현재)
.\Client\Bin\Debug-DX12\WintersGame.exe --editor-imgui

# Stage B (미래)
.\Tools\Bin\Debug\WintersEditor.exe

# 기대 흐름
# WintersEditor 실행 → Project 선택 → ContentBrowser 열림
# Resource\Char\Irelia 폴더 → Irelia.character drag
# DetailsPanel에 stat/ability/montage list 표시
# Edit Ability → AbilityEditor 새 창
# Play (PIE) → 임베드 viewport에서 게임 실행
```

---

## 6. 다음 챕터로

Ch12 Stage B는 **Ch13 Tooling** (Reflection / HeaderTool)이 안 오면 짜기 매우 힘들다. 디자이너 협업 핵심 entry point.
