# Ch13. Tooling (UBT / UHT / DDC / Cook / AssetConverter / Profiler)

> 2026-05-25 update: `Tools/DX12SmokeHost` is removed from the active toolchain. Keep backend validation inside Engine/Client configs or existing project smoke functions.

> Winters 현재: `.vcxproj`/`.filters` 손 관리, `WintersAssetConverter` 1차, `Tools/DX12SmokeHost`.
> 통증 박제: "현재 통증: `.vcxproj`와 `.filters`를 손으로 관리하는 게 곧 한계."
> 레퍼런스: `UnrealEngine/Engine/Source/Programs/UnrealBuildTool/`, `Source/Developer/DerivedDataCache/`.

---

## 1. 기초 원리 — Tooling이 안 만들어지면 다음 챕터를 못 짠다

지금까지 챕터들의 의존:
- **Ch12 Editor** ← Reflection 필요 ← **UHT (HeaderTool)**
- **Ch2 RenderGraph** ← shader compile ← **DDC**
- **모든 챕터** ← 빌드 자동화 ← **UBT (BuildTool)**
- **Ch15 Data pipeline** ← asset 변환 ← **Cooker** + **AssetConverter**
- **퍼포먼스** ← **Profiler**

이 5개가 **메타-시스템**이다. 다른 시스템을 만들고 유지하는 시스템.

UE5에서 `Source/Programs/UnrealBuildTool/` 코드량은 약 100만 줄 (C#). UHT가 50만 줄. 즉 UE5의 build infra는 게임 엔진만큼 큰 별도 프로젝트.

---

## 2. 핵심 — 5대 도구

### 2.1 UBT (Build Tool)

`Source/Programs/UnrealBuildTool/`. C# 작성.

UE5는 **vcxproj를 손으로 안 짠다.** 대신:

```cs
// Source/Runtime/MyModule/MyModule.Build.cs
public class MyModule : ModuleRules
{
    public MyModule(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "InputCore" });

        PublicIncludePaths.Add(ModuleDirectory);
        if (Target.Platform == UnrealTargetPlatform.Win64)
            PublicDefinitions.Add("WINTERS_RHI_BACKEND_DX12");
    }
}
```

UBT가:
1. `*.Build.cs` 전부 파싱
2. dependency graph 구성 (순환 검사)
3. 모듈별 IWYU include 검증
4. `.vcxproj` / `.sln` 자동 생성
5. msbuild / clang / xcodebuild 호출
6. 결과 binary copy / pak / sign

또한 target별 빌드:
```cs
// UnrealEditor.Target.cs    Editor + 게임
// UnrealGame.Target.cs      Standalone 게임
// UnrealServer.Target.cs    Dedicated server (UI/Render 제거)
// UnrealClient.Target.cs    Client-only (server logic 제거)
```

UE5는 같은 코드를 5가지 target으로 빌드. UBT가 `#if WITH_EDITOR`, `WITH_SERVER_CODE` 같은 macro 분기를 일관 관리.

### 2.2 UHT (HeaderTool)

`Source/Programs/Shared/UnrealBuildTool.Tests/UnrealHeaderToolTests.cs` 같은 곳 + main UHT는 5.x에서 C# 통합.

원리:
1. UCLASS / UPROPERTY / UFUNCTION / USTRUCT 매크로가 박힌 `.h` 스캔
2. 매크로 의미를 파싱 (lexer/parser, C++ subset)
3. `.generated.h` / `.gen.cpp` 생성:

```cpp
// MyClass.generated.h (UHT 출력 예시)
#define MyClass_RPC_WRAPPERS \
    DECLARE_FUNCTION(execDoSomething);

// MyClass.gen.cpp
UClass* Z_Construct_UClass_UMyClass()
{
    static UE_Cf::FClassParams ClassParams = {
        &UMyClass::StaticClass,
        "MyClass",
        sizeof(UMyClass),
        // property records:
        { { "Health", &UMyClass::OffsetOf_Health, /*meta*/{...} } },
        // function records:
        { { "DoSomething", &UMyClass::execDoSomething } }
    };
    // ...
}
```

이 자동 생성 코드가 reflection을 제공.

### 2.3 DDC (Derived Data Cache)

`Source/Developer/DerivedDataCache/`. 빌드 결과물 캐시.

원리:
- Shader compile, Texture mip generate, Mesh distance field, Lighting build 같은 **deterministic input → deterministic output** 결과를 hash 키로 저장
- 다른 사람이 같은 input을 요청하면 → cache hit → 재계산 안 함
- 팀이 shared DDC를 두면 → "최초 1명만 compile, 나머지 100명은 download"

```text
Input:  shader bytecode "RT01.usf" + define{} + platform{DX12} → SHA256
Output: compiled DXIL binary (50KB)
Key:    "ShaderCache/RT01_DX12_ab3f4e..."
```

DDC backend: local disk, network share, AWS S3 / GCS. 회사 규모에 따라.

### 2.4 Cooker

게임을 배포 가능 형태로 변환.

```text
[원본 asset]                    [Cooked]
.png 텍스처     →               BC7 압축 .pak
.fbx 메시       →               LOD 분할 + bone-baked .pak
.umap level     →               serialized binary + asset chunk
.uasset blueprint → optionally native C++ generation
shader source   →               DX12/Vulkan/Metal binary
```

플랫폼별 cook:
- Windows: DX12 shader, BC7 texture
- iOS: Metal shader, ASTC texture, lower mip
- Switch: NVN shader, BC7

각 플랫폼별 디렉토리 출력. CDN으로 patch 배포.

### 2.5 Profiler

빌드 후 게임이 빠른가? 그것도 도구가 알려준다.

UE5 도구:
- **Insights** (Source/Programs/UnrealInsights/) — CPU trace, frame timing, GPU breakdown
- **Memory profiler** — 메모리 누수 / fragmentation
- **GPU Visualizer** — RenderDoc 등가, 자체 pass-level 분석
- **Network profiler** — bandwidth, replication 비용

샘플 흐름:
```text
게임 실행 with -trace=cpu,gpu,frame,bookmark
→ .utrace 파일 생성
→ UnrealInsights 열기
→ 매 프레임 어떤 함수가 몇 ms 썼는지 timeline
→ 가장 느린 5개 함수 → 최적화 우선순위
```

---

## 3. 심화

### 3.1 Module-based Architecture

UE5는 monolithic이 아니라 module 단위 빌드 (.dll). 모듈 의존성이 build.cs에 명시.

장점:
- Editor 빌드 = Game module + Editor module
- Server 빌드 = Game module만 (Editor/Render module exclude)
- 개별 모듈 hot reload

Winters는 현재 `WintersEngine.dll` 단일. 향후 분리 가능:
```text
WintersCore.dll
WintersRHI.dll
WintersRenderer.dll
WintersAnimation.dll
WintersAudio.dll
WintersNetwork.dll
WintersGAS.dll
WintersAI.dll
```

### 3.2 IWYU (Include What You Use)

각 .h가 자기 사용하는 것만 include. Forward declare 활용. PCH (precompiled header) 최소화.

UE5 Build.cs:
```cs
IWYUSupport = IWYUSupport.Full;
PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
```

→ 헤더 1개 수정에 100개 .cpp 재컴파일하는 일 방지.

### 3.3 Live Coding (Ch12 Stage D)

`Source/Programs/LiveCoding/`. C++ 함수 본문 수정 → 빌드 → process에 .obj patch.

원리:
1. 변경된 .obj만 컴파일
2. 실행 중 process의 메모리 protection 해제
3. 함수 시작에 jmp 또는 detour로 새 코드 가리킴
4. 다시 protect

제약:
- 멤버 변경(layout 바뀜) 불가
- vtable 추가 불가
- 전역 초기화 변경 불가

### 3.4 Distributed Build

UE5 + IncrediBuild / SN-DBS / FastBuild — 컴파일을 여러 머신에 분산. 큰 팀 필수.

### 3.5 CI / CD

- Pull request → 자동 빌드 (Win/Mac/Linux/Switch/PS5)
- 자동 unit test
- Cooker run
- 자동 PIE smoke test
- 결과를 PR 댓글로

Winters memory `project_session_2026_04_30.md`:
> Server.vcxproj `/fp:precise`+`Mswsock.lib` 적용

→ 이런 설정은 BuildTool에 박혀야 한다. 손 관리하면 매번 잊는다.

---

## 4. Winters 매핑

### 4.1 현재 통증 박제

CLAUDE.md / 본 brief의 Ch13:
> 현재 통증: `.vcxproj`와 `.filters`를 손으로 관리하는 게 곧 한계.
> UE5 UBT 등가가 1년 안에 필요.

memory `project_session_2026_04_23.md`:
> WINTERS_ENGINE dllexport + unique_ptr 멤버 = copy ctor/assign 명시 delete 필수.

→ 이런 규칙은 build tool이 강제해야 maintain됨.

### 4.2 Ch13 신규 도구 (제안)

```text
Tools/WintersBuildTool/        (C# 또는 Rust 또는 자체 C++)
  Module.Build.cs              UE5 등가 형식
  Target.Build.cs              빌드 target (Game/Server/Editor)
  ProjectFileGenerator.cs      .vcxproj / .filters 자동 생성
  IWYUValidator.cs             include 정합 검사
  PostbuildCopy.cs             dll/dat copy

Tools/WintersHeaderTool/       C++ 또는 C#
  Lexer / Parser               .h 파싱 (libclang 추천)
  Generator                    .gen.h / .gen.cpp 출력
  MetadataDB                   reflection DB 산출 (Ch12 Editor 입력)

Tools/WintersCooker/           플랫폼별 cook
  .wmesh / .wanim / .wtex pak
  Shader compile (DXC / glslang)
  Asset dependency graph
  Plat-specific compression

Tools/DerivedDataCache/        (이미 일부 있음)
  Local disk + network share
  Hash key DB

Tools/AssetValidator/          cook 전 sanity
  Missing texture refs
  Dead asset detection
  Naming convention check
  Size budget warnings

Tools/WintersProfiler/         (현재 in-game ImGui, 별도 desktop 확장)
  CPU sampling
  GPU trace (D3D12 PIX 통합)
  Memory tracking
  Network bandwidth
  Frame timeline
```

### 4.3 Reflection 매크로 (제안)

```cpp
// 사용
WINTERS_CLASS(meta=Category("Combat"))
class CCombatComponent
{
    WINTERS_BODY()

    WINTERS_PROPERTY(EditAnywhere, ClampMin=0)
    f32_t attackDamage = 50.f;

    WINTERS_PROPERTY(EditAnywhere)
    AbilityID basicAttackAbility;

    WINTERS_FUNCTION(BlueprintCallable)
    void Attack(EntityID target);
};

// HeaderTool 출력 (의사)
// CombatComponent.gen.h
#define CCombatComponent_GEN_BODY \
    static const CTypeInfo TypeInfo; \
    static void* Construct() { return new CCombatComponent(); } \
    void* GetThis() override { return this; }

// CombatComponent.gen.cpp
const CTypeInfo CCombatComponent::TypeInfo = {
    "CCombatComponent",
    sizeof(CCombatComponent),
    {
        { "attackDamage",        offsetof(CCombatComponent, attackDamage),
          TypeOf<f32_t>(), { Edit::Anywhere, ClampMin{0} } },
        { "basicAttackAbility",  offsetof(CCombatComponent, basicAttackAbility),
          TypeOf<AbilityID>(), { Edit::Anywhere } }
    },
    {
        { "Attack", &CCombatComponent::Attack, { Blueprint::Callable } }
    }
};
```

### 4.4 Bot AI / 서버 권위와의 관계

Tooling은 빌드 시 작동. 런타임 Bot AI/Server와 무관. 단:
- BuildTool이 module dependency 분석할 때, `Shared/GameSim/` → `Engine/Public/RHI` include를 **차단**해야 함 (Ch1 Stage 7에서 명시한 제약)
- Cooker가 Server / Client별로 다른 asset set cook 가능 (서버는 visual asset 안 cook)

### 4.5 단계별 도입

```text
Ch13-Stage1  Module.Build.cs 형식 정의 + ProjectFileGenerator
             → .vcxproj 손 관리 탈피
Ch13-Stage2  IWYU validator (include 정합)
Ch13-Stage3  HeaderTool 1차 (reflection metadata 추출 only, 매크로는 수동)
Ch13-Stage4  HeaderTool 매크로 도입 (WINTERS_CLASS/PROPERTY)
Ch13-Stage5  Reflection runtime DB
Ch13-Stage6  DDC (shader/mesh cache)
Ch13-Stage7  Cooker (Win/Mac/Linux/Switch)
Ch13-Stage8  AssetValidator (cook 전 검사)
Ch13-Stage9  Profiler desktop (CPU/GPU/Memory/Net 통합)
Ch13-Stage10 Live coding (Stage D)
Ch13-Stage11 Distributed build (큰 팀 가면)
```

### 4.6 게임별 적용

| 게임 | 필요 Stage |
|------|-----------|
| LoL (현재) | Stage 1, 6, 8, 9 (당장은 이거) |
| 로아 | Stage 1~9 |
| 엘든링 | Stage 1~9 + 일부 10 |
| GTA6 | Stage 1~11 전부 + multi-region build farm |

---

## 5. 검증 명령

```powershell
# Stage 1 도입 후
.\Tools\Bin\Debug\WintersBuildTool.exe regen
# → .sln / *.vcxproj / *.filters 새로 생성

.\Tools\Bin\Debug\WintersHeaderTool.exe scan Engine
# → Engine/.../*.gen.h, *.gen.cpp 생성

.\Tools\Bin\Debug\WintersCooker.exe --platform=Win64 --content=./Content --out=./Cooked/Win64
# → .pak / .wmesh / .shader 산출
```

---

## 6. 다음 챕터로

Ch13 Stage 3까지 가야 **Ch12 Editor Stage C** (Reflection 기반 DetailsPanel)가 가능. Ch13 Stage 7까지 가야 라이브 패치 시스템(Ch14 Services의 CDN과 결합) 가능.
