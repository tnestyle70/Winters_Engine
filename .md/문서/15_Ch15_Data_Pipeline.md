# Ch15. Data Pipeline (DataTable / CurveTable / ContentRegistry / Data Assets)

> Winters 현재: 게임 데이터 분산 (코드 안 hardcoded + JSON + `.dat` 바이너리).
> 레퍼런스: `UnrealEngine/Engine/Source/Runtime/Engine/Classes/Engine/DataTable.h`, `CurveTable.h`, `Runtime/AssetRegistry/`.

---

## 1. 기초 원리 — 기획자/디자이너 협업의 80%가 여기서 일어난다

게임 데이터의 분류:

| 데이터 종류 | 변경 빈도 | 누가 편집 | 형태 |
|------------|----------|----------|------|
| 챔프 스탯 (HP, AD) | 매주 | 기획자 | 표 (DataTable) |
| 레벨별 곡선 (HP per lvl) | 시즌마다 | 기획자 | 그래프 (CurveTable) |
| 스킬 데이터 (cooldown, damage) | 매주 | 기획자 | 자산 (DataAsset) |
| 아이템 정의 | 매주 | 기획자 | 자산 |
| 퀘스트 | 매일 | 기획자 + 디자이너 | 자산 + 트리 |
| NPC 대사 | 매일 | 디자이너 | 자산 (Dialogue Tree) |
| 캐릭터 메시 | 자주 | 아티스트 | 바이너리 (.wmesh) |
| 텍스처 | 자주 | 아티스트 | 바이너리 |
| 사운드 | 자주 | 사운드 디자이너 | 바이너리 |
| 레벨 | 자주 | 레벨 디자이너 | 자산 (.umap) |
| FX | 자주 | VFX 아티스트 | 자산 (.niagara) |
| 애니메이션 | 자주 | 애니메이터 | 자산 (.anim) |

이걸 다 **하드코드**하면 매주 코드 수정 + 재빌드 + 재배포. 라이브 서비스는 매일 변경되므로 불가.

해결: **데이터를 자산으로 분리** → 코드 변경 없이 핫패치 / 시즌 변경.

---

## 2. 핵심 — UE5 Data 5대 구성

### 2.1 DataTable (Excel 같은 표)

`Source/Runtime/Engine/Classes/Engine/DataTable.h:32~50`:

```cpp
/**
 * Base class for all table row structs to inherit from.
 */
USTRUCT(BlueprintInternalUseOnly)
struct FTableRowBase
{
    GENERATED_USTRUCT_BODY()
    FTableRowBase() { }
    virtual ~FTableRowBase() { }

    virtual void OnPostDataImport(const UDataTable* InDataTable, const FName InRowName,
                                  TArray<FString>& OutCollectedImportProblems) {}
};
```

사용 패턴:
```cpp
// 1. row struct 정의
USTRUCT(BlueprintType)
struct FChampionRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName ChampionId;
    UPROPERTY(EditAnywhere) float BaseHealth;
    UPROPERTY(EditAnywhere) float BaseAD;
    UPROPERTY(EditAnywhere) FSoftObjectPath MeshPath;
};

// 2. .csv 파일 또는 Editor에서 데이터 입력
ChampionId,BaseHealth,BaseAD,MeshPath
Ezreal,500,55,"/Game/Char/Ezreal.Ezreal"
Irelia,580,63,"/Game/Char/Irelia.Irelia"

// 3. UDataTable asset import → 런타임 lookup
const FChampionRow* Row = ChampionTable->FindRow<FChampionRow>(FName("Ezreal"), TEXT("Lookup"));
```

### 2.2 CurveTable / Curve

```text
LevelCurve_Health.csv
Level, Value
1,    500
2,    580
3,    660
...
18,   2700
```

`UCurveTable` or `UCurveFloat` asset. 런타임 `Eval(level)`로 보간 lookup.

LoL "챔프 레벨별 HP" = 챔프 1개당 curve 1개. 18개 row가 아니라 곡선 식 (smooth).

### 2.3 DataAsset

복잡한 데이터를 **자산 1개** 단위로.

```cpp
UCLASS(BlueprintType)
class UAbilityDataAsset : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere) FName AbilityId;
    UPROPERTY(EditAnywhere) float Cooldown;
    UPROPERTY(EditAnywhere) float Cost;
    UPROPERTY(EditAnywhere) TArray<FAbilityTaskDesc> Tasks;
    UPROPERTY(EditAnywhere) FGameplayTagContainer GrantedTags;
};
```

Editor에서 "New Ability" → 자산 1개 생성. DataTable과 달리 자산 자체에 reference / sub-asset.

### 2.4 PrimaryAssetId / AssetManager

```cpp
// /Game/Abilities/Ezreal_Q.uasset
// PrimaryAssetId: "Ability:Ezreal_Q"

FPrimaryAssetId Id("Ability", "Ezreal_Q");
UAssetManager::Get().LoadPrimaryAsset(Id, ...);

// 또는 chunk 단위 로드
UAssetManager::Get().LoadPrimaryAssetsWithType("Ability");   // 모든 ability 로드
```

게임이 어떤 자산을 메모리에 둘지 / cook할지의 단위.

### 2.5 AssetRegistry

`Source/Runtime/AssetRegistry/`. 모든 자산의 메타데이터 색인.

```text
asset path                       class             tags                deps
/Game/Char/Ezreal.uasset         Champion          tag.adc,tag.range   ['Ezreal_Q', 'Ezreal.fbx']
/Game/Abilities/Ezreal_Q.uasset  Ability           tag.skillshot       ['ezreal_q_fx.niagara']
/Game/FX/ezreal_q_fx.uasset      Niagara           tag.fx               []
```

Editor가 시작될 때 모든 .uasset header를 스캔 → 색인. ContentBrowser, reference viewer, 자산 검색, smart cook이 이 색인 위에.

---

## 3. 심화

### 3.1 Asset Dependency Graph

자산 간 reference를 그래프로 추적.

```text
Ezreal 자산 (Champion)
  └─→ Ezreal_Q (Ability)
       └─→ ezreal_q_fx (Niagara)
            └─→ ezreal_q_trail.uasset (Texture)
```

기능:
- Cook 시 reference 전부 cook
- Editor에서 "이 자산을 누가 쓰나" reverse lookup
- "고아 자산" (어디서도 안 쓰는) 검출
- Migrate (project 간 이동 시 dependency 자동 동행)

### 3.2 Soft vs Hard Reference

- **Hard**: `UEzrealQAbility* Q;` — 로드 시 같이 메모리에. cook 시 같이 묶임.
- **Soft**: `TSoftObjectPtr<UEzrealQAbility> Q;` — path만 들고 lazy load. 미니지 챔프는 안 로드.

LoL 5v5: 자기 팀 + 적 팀 챔프 10개만 hard. 나머지 150-10 = 140개는 soft.

### 3.3 Async Load + Streaming Handle

```cpp
TArray<FSoftObjectPath> Paths = { Ezreal_Q.ToSoftObjectPath(), Ezreal_W.ToSoftObjectPath(), ... };
UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(Paths,
    FStreamableDelegate::CreateLambda([]() { /* loaded */ }));
```

미니지 챔프 로딩 화면 = 이 패턴.

### 3.4 Data Validation

cook 전에 데이터 정합 검사:
- 모든 ability id가 reference 가능한가?
- HP/MP가 음수 아닌가?
- 텍스처가 power-of-2인가?
- 누락된 reference 있는가?

`UEditorValidatorBase` 기반. Editor에서 실시간 + cook 시 fail-fast.

### 3.5 Data Editor (기획자용)

UE5는 DataTable 자체를 엑셀처럼 편집 가능 (`DataTableEditor`). 외부 .xlsx → CSV → import도 OK.

라이브 서비스에서는 흔히:
1. 기획자가 Google Sheet 편집
2. 서버 hot-reload endpoint POST → 시즌 중에도 밸런스 패치
3. 게임 서버는 변경된 표 다시 로드

LoL 챔프 밸런스 패치가 핫픽스로 들어가는 원리.

### 3.6 Localization Data

```text
Strings/
  ChampionNames.csv
    Id,         en-US,        ko-KR,    ja-JP
    Ezreal,     "Ezreal",     "이즈리얼", "エズリアル"
    Irelia,     "Irelia",     "이렐리아", "イレリア"
  AbilityDescs.csv
    Id,                          en-US,                       ko-KR
    Ezreal_Q,                    "Fires a projectile...",     "투사체를 발사한다..."
```

`FText` 시스템이 자동 lookup. culture 변경 시 모든 UI/대사 일괄 swap.

---

## 4. Winters 매핑

### 4.1 현재 상태

흩어진 데이터:
- `Client/Public/GameObject/SkillTable.h` — 챔프 hard-coded
- `Shared/GameSim/Definitions/` — JSON 기반 일부
- `Client/Bin/Resource/Stage1.dat` — 바이너리 (Stage editor)
- `Shared/GameSim/Registries/` — Registry 패턴 (`project_phase_b11d_v31_ezreal_pending.md` memory 박제)

→ 일관된 data pipeline 미정립. Phase B-11d / Phase 1 / Phase D 분리되어 진행 중.

### 4.2 Ch15 통합 데이터 시스템 (제안)

```cpp
// Shared/Data/DataTable.h
template<typename RowT>
class CDataTable
{
    static_assert(std::is_base_of_v<TableRowBase, RowT>);
public:
    bool LoadFromCSV(const char* path);
    const RowT* FindRow(const char* key) const;
    void ForEach(std::function<void(const char* key, const RowT&)> fn) const;
};

// Shared/Data/CurveTable.h
class CCurveFloat
{
public:
    void   AddKey(f32_t time, f32_t value);
    f32_t  Eval(f32_t time) const;     // 보간
    eCurveInterp interp = eCurveInterp::Linear;
};

// Shared/Data/ContentRegistry.h
class CContentRegistry
{
public:
    void Initialize();
    void Scan(const char* contentDir);                  // .winters / .uasset 헤더 스캔
    AssetDesc* Find(AssetID id);
    std::vector<AssetID> GetAssetsByClass(const char* className);
    std::vector<AssetID> GetReferences(AssetID id);     // 자산 graph
};

// Shared/Data/AssetManager.h
class CAssetManager
{
public:
    // Sync
    template<typename T> AssetRef<T> Load(AssetID id);
    template<typename T> AssetRef<T> LoadByPath(const char* path);

    // Async
    using LoadCb = std::function<void(std::vector<AssetID>)>;
    StreamableHandle LoadAsync(const std::vector<AssetID>& ids, LoadCb cb);
    void Unload(AssetID id);

    void RegisterPrimaryAssetType(const char* type, AssetTypeConfig cfg);
};

// Shared/Data/DataAssetTypes/
//   .champion   (id, basestat, abilities[])
//   .ability    (id, cost, cooldown, effects[])
//   .item       (id, slot, stats, abilities[])
//   .quest      (id, objectives, rewards)
//   .npc        (id, mesh, ai, dialogue)
```

### 4.3 기획자 워크플로 (Ch16 협업과 합류)

```text
[Stage 1 — 최소 형태]
기획자가 .xlsx 편집 → CSV export → Resource/Data/Champion.csv
게임 시작 시 CDataTable<ChampionRow>::LoadFromCSV

[Stage 2 — 자산 단위]
Tools/WintersEditor (Ch12) → "New Champion" 자산 생성
Property panel에서 stat 편집 → .champion 파일 저장
ContentRegistry가 자동 색인

[Stage 3 — 라이브 패치]
Services/internal/liveops가 .champion override push
서버 hot-reload endpoint → 시즌 중 패치 가능
```

### 4.4 Bot AI / 서버 권위 불변식

- Bot AI도 같은 데이터 자산을 읽는다. `GameCommand`의 ability id는 같은 `.ability`로 resolve.
- 서버가 진실의 게이트. 클라가 다른 .ability를 가졌어도 서버가 거부 (anti-cheat).

### 4.5 단계별

```text
Ch15-Stage1  CDataTable + CSV 로딩 (현재 hard-coded → 표)
Ch15-Stage2  CCurveFloat / CCurveTable
Ch15-Stage3  Asset binary format (.wchampion .wability .witem ...)
Ch15-Stage4  ContentRegistry (asset header 스캔 + 색인)
Ch15-Stage5  AssetManager (sync / async load)
Ch15-Stage6  Soft reference (lazy load)
Ch15-Stage7  Dependency graph (cook + reference viewer)
Ch15-Stage8  Data Validation (cook 전 sanity)
Ch15-Stage9  Localization (CJK + RTL)
Ch15-Stage10 Live data patch (Ch14 LiveOps 연동)
Ch15-Stage11 Data Editor (Ch12와 합류)
```

### 4.6 게임별 적용

| 게임 | 필요 Stage |
|------|-----------|
| LoL (현재) | Stage 1, 2, 5, 8, 10 |
| 로아 | Stage 1~10 + 직업/룬/아이템 계층 데이터 |
| 엘든링 | Stage 1~9 + dialogue tree |
| GTA6 | Stage 1~11 전부 + region별 데이터 + audio bank |

---

## 5. 검증 명령

```powershell
# Stage 1
.\Tools\Bin\Debug\WintersAssetConverter.exe csv2bin Resource/Data/Champion.csv
# → Champion.dat

# Stage 4
.\Client\Bin\Debug-DX12\WintersGame.exe --content-scan
# 기대 로그
# [Content] scanned 1234 assets in 87ms
# [Content] by class: Champion=42, Ability=168, Item=312, FX=510, ...

# Stage 10
curl -X POST http://services.local/liveops/data-patch \
    -H "Content-Type: application/json" \
    -d '{"asset": "Ability:Ezreal_Q", "field": "cooldown", "value": 6.5}'
# 서버 hot-reload → 모든 게임 인스턴스에 즉시 반영
```

---

## 6. 다음 챕터로

Ch15 Stage 4 (ContentRegistry)가 와야 Ch12 Editor ContentBrowser가 동작.
Ch15 Stage 10 (Live data patch)는 Ch14 LiveOps와 합류.
Ch15가 Ch16 협업 토폴로지의 "데이터 자산 SSoT" 항목의 backbone.
