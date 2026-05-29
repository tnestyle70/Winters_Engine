Session - A: Windows/MSVC/GitHub Actions 기준으로, 현재 `Winters.sln`과 MSBuild를 CI의 1차 진실로 삼는 로컬 Build Tools 진입점을 만든다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Tools/Build/README.md

새 파일:

```md
# Winters Build Tools

이 폴더는 로컬 개발자와 CI가 같은 명령으로 Winters를 검증하기 위한 진입점이다.

## 기본 원칙

- 기본 빌드는 현재 Visual Studio 솔루션인 `Winters.sln`을 사용한다.
- CMake/Ninja는 Engine bootstrap부터 확장 중이므로, 전체 CI의 필수 경로는 MSBuild로 먼저 고정한다.
- `Shared/Schemas/run_codegen.bat`는 컴파일 전에 실행한다.
- 무거운 전체 asset cook은 기본 CI에서 제외하고, 수동 또는 nightly 검증으로 분리한다.
- 모든 로그는 `out/logs` 아래에 남긴다.

## 자주 쓰는 명령

```powershell
.\Tools\Build\Invoke-WintersBuild.ps1 -Configuration Debug -Target Solution
.\Tools\Build\Invoke-WintersBuild.ps1 -Configuration Release -Target Solution
.\Tools\Build\Invoke-WintersVerify.ps1 -Level Quick
.\Tools\Build\Invoke-WintersVerify.ps1 -Level CI -Configuration Debug,Release,Debug-DX12 -SkipAssetCook
```

## 단계

1. `Invoke-WintersBuild.ps1`로 codegen과 MSBuild/CMake 빌드를 실행한다.
2. `Invoke-WintersVerify.ps1`로 diff sanity, codegen, 빌드 매트릭스를 묶어 실행한다.
3. `Package-WintersArtifacts.ps1`로 검증된 산출물을 zip artifact로 만든다.

## 확인 필요

- 실제 배포 대상은 아직 정하지 않는다. GitHub Release, 내부 QA 머신, object storage, Steam branch 중 무엇으로 갈지는 별도 세션에서 확정한다.
- self-hosted runner는 속도에는 좋지만 PR 코드 실행 위험이 있으므로 격리 정책을 먼저 정한다.
```

1-2. C:/Users/user/Desktop/Winters/Tools/Build/Invoke-WintersBuild.ps1

새 파일:

```powershell
[CmdletBinding()]
param(
    [ValidateSet("MSBuild", "CMake")]
    [string]$Backend = "MSBuild",

    [ValidateSet("Debug", "Release", "Debug-DX12", "Release-DX12")]
    [string[]]$Configuration = @("Debug"),

    [ValidateSet("x64")]
    [string]$Platform = "x64",

    [ValidateSet("Solution", "Engine", "Server", "Client", "Tools", "DX12SmokeHost")]
    [string[]]$Target = @("Solution"),

    [switch]$Clean,
    [switch]$SkipCodegen,
    [switch]$CI,
    [string]$LogDir = ""
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = [System.IO.Path]::GetFullPath((Join-Path $ScriptDir "..\.."))

if ([string]::IsNullOrWhiteSpace($LogDir)) {
    $LogDir = Join-Path $Root "out\logs\build"
}

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

function Write-Section([string]$Message) {
    Write-Host ""
    Write-Host "== $Message =="
}

function Invoke-Native([string]$Name, [string]$FilePath, [string[]]$Arguments) {
    Write-Section $Name
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE"
    }
}

function Find-MSBuild {
    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $installPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($installPath)) {
            $amd64MsBuild = Join-Path $installPath "MSBuild\Current\Bin\amd64\MSBuild.exe"
            if (Test-Path -LiteralPath $amd64MsBuild -PathType Leaf) {
                return $amd64MsBuild
            }
        }
    }

    throw "MSBuild.exe was not found. Run from Developer PowerShell or install Visual Studio Build Tools."
}

function Convert-TargetToMSBuildTarget([string]$Name) {
    switch ($Name) {
        "Solution" { return "Build" }
        "Engine" { return "Engine" }
        "Server" { return "Server" }
        "Client" { return "Client" }
        "Tools" { return "WintersAssetConverter" }
        "DX12SmokeHost" { return "DX12SmokeHost" }
    }
}

function Convert-ConfigurationToCMakePreset([string]$Name) {
    switch ($Name) {
        "Debug" { return "engine-debug" }
        "Release" { return "engine-release" }
        "Debug-DX12" { return "engine-debug-dx12" }
        default { throw "CMake preset for $Name is not defined yet." }
    }
}

if (-not $SkipCodegen) {
    $codegen = Join-Path $Root "Shared\Schemas\run_codegen.bat"
    Invoke-Native "FlatBuffers codegen" $codegen @()
}

if ($Backend -eq "MSBuild") {
    $msbuild = Find-MSBuild
    $solution = Join-Path $Root "Winters.sln"

    foreach ($config in $Configuration) {
        foreach ($targetName in $Target) {
            $msTarget = Convert-TargetToMSBuildTarget $targetName
            if ($Clean) {
                $msTarget = "Rebuild"
            }

            $safeTarget = $targetName -replace "[^A-Za-z0-9_.-]", "_"
            $safeConfig = $config -replace "[^A-Za-z0-9_.-]", "_"
            $log = Join-Path $LogDir "msbuild-$safeTarget-$safeConfig.log"

            $args = @(
                $solution,
                "/m",
                "/nr:false",
                "/t:$msTarget",
                "/p:Configuration=$config",
                "/p:Platform=$Platform",
                "/v:m",
                "/flp:logfile=$log;verbosity=normal;encoding=UTF-8"
            )

            Invoke-Native "MSBuild $targetName $config|$Platform" $msbuild $args
        }
    }
}
else {
    foreach ($targetName in $Target) {
        if ($targetName -ne "Engine") {
            throw "CMake backend currently supports only Engine. Use MSBuild until Client/Server/Tools CMake targets are added."
        }
    }

    foreach ($config in $Configuration) {
        $preset = Convert-ConfigurationToCMakePreset $config
        Invoke-Native "CMake build preset $preset" "cmake" @("--build", "--preset", $preset)
    }
}

if ($CI) {
    Write-Host "CI build completed."
}
```

2. 검증

미검증:
- 새 wrapper 파일은 이 세션에서 추가한 뒤 PowerShell syntax parse를 확인해야 한다.
- 기존 dirty worktree가 많으므로 구현 시에는 새 `Tools/Build` 파일 외 변경이 섞이지 않았는지 확인해야 한다.

검증 명령:
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\Build\Invoke-WintersBuild.ps1 -Configuration Debug -Target Engine -SkipCodegen`
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\Build\Invoke-WintersBuild.ps1 -Configuration Debug -Target Solution`

Session - B: 로컬과 CI가 함께 쓰는 검증 게이트를 만들고, codegen stale 상태와 기본 빌드 실패를 한 명령으로 잡는다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Tools/Build/Invoke-WintersVerify.ps1

새 파일:

```powershell
[CmdletBinding()]
param(
    [ValidateSet("Quick", "CI", "Full")]
    [string]$Level = "Quick",

    [ValidateSet("Debug", "Release", "Debug-DX12", "Release-DX12")]
    [string[]]$Configuration = @(),

    [switch]$SkipAssetCook
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = [System.IO.Path]::GetFullPath((Join-Path $ScriptDir "..\.."))
$LogDir = Join-Path $Root "out\logs\verify"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

function Write-Section([string]$Message) {
    Write-Host ""
    Write-Host "== $Message =="
}

function Invoke-Native([string]$Name, [string]$FilePath, [string[]]$Arguments) {
    Write-Section $Name
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE"
    }
}

if ($Configuration.Count -eq 0) {
    switch ($Level) {
        "Quick" { $Configuration = @("Debug") }
        "CI" { $Configuration = @("Debug", "Release", "Debug-DX12") }
        "Full" { $Configuration = @("Debug", "Release", "Debug-DX12", "Release-DX12") }
    }
}

Push-Location $Root
try {
    Invoke-Native "git diff --check" "git" @("diff", "--check")

    $codegen = Join-Path $Root "Shared\Schemas\run_codegen.bat"
    Invoke-Native "FlatBuffers codegen" $codegen @()

    $buildScript = Join-Path $Root "Tools\Build\Invoke-WintersBuild.ps1"
    $buildArgs = @(
        "-Backend", "MSBuild",
        "-Target", "Solution",
        "-Configuration"
    ) + $Configuration + @(
        "-SkipCodegen",
        "-CI",
        "-LogDir", (Join-Path $Root "out\logs\build")
    )

    Write-Section "Winters build matrix"
    & powershell -NoProfile -ExecutionPolicy Bypass -File $buildScript @buildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Winters build matrix failed with exit code $LASTEXITCODE"
    }

    if ($Level -eq "Full" -and -not $SkipAssetCook) {
        $assetScript = Join-Path $Root "Tools\convert_all_assets.ps1"
        if (Test-Path -LiteralPath $assetScript -PathType Leaf) {
            Invoke-Native "Asset conversion smoke" "powershell" @(
                "-NoProfile",
                "-ExecutionPolicy", "Bypass",
                "-File", $assetScript,
                "champions"
            )
        }
    }
}
finally {
    Pop-Location
}

Write-Host "Winters verification completed."
```

2. 검증

미검증:
- `Full` asset conversion은 raw asset 존재 여부에 따라 실패할 수 있으므로 CI 기본값에서는 `-SkipAssetCook`을 사용한다.
- `run_codegen.bat` 실행 후 generated 파일 diff를 강제 실패시킬지는 팀 정책 확인이 필요하다.

검증 명령:
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\Build\Invoke-WintersVerify.ps1 -Level Quick -SkipAssetCook`
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\Build\Invoke-WintersVerify.ps1 -Level CI -Configuration Debug,Release -SkipAssetCook`

Session - C: GitHub Actions에서 PR/push마다 Windows MSBuild 매트릭스를 실행하고 실패 로그를 artifact로 남긴다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/.github/workflows/winters-ci.yml

새 파일:

```yaml
name: Winters CI

on:
  pull_request:
  push:
    branches:
      - main
      - master
      - develop
  workflow_dispatch:

permissions:
  contents: read

concurrency:
  group: winters-ci-${{ github.ref }}
  cancel-in-progress: true

defaults:
  run:
    shell: pwsh

jobs:
  verify:
    name: MSBuild ${{ matrix.configuration }}
    runs-on: windows-2022
    timeout-minutes: 90

    strategy:
      fail-fast: false
      matrix:
        configuration:
          - Debug
          - Release
          - Debug-DX12

    steps:
      - name: Checkout
        uses: actions/checkout@v6
        with:
          lfs: true

      - name: Setup MSBuild
        uses: microsoft/setup-msbuild@v2
        with:
          msbuild-architecture: x64

      - name: Print toolchain
        run: |
          msbuild -version
          cmake --version

      - name: Verify Winters
        run: |
          .\Tools\Build\Invoke-WintersVerify.ps1 `
            -Level CI `
            -Configuration ${{ matrix.configuration }} `
            -SkipAssetCook

      - name: Upload logs
        if: always()
        uses: actions/upload-artifact@v7
        with:
          name: winters-logs-${{ matrix.configuration }}
          path: |
            out/logs/**
          if-no-files-found: warn
          retention-days: 14
```

2. 검증

미검증:
- GitHub 저장소 기본 브랜치가 `main`, `master`, `develop` 중 무엇인지 확인해야 한다.
- Git LFS를 실제로 쓰는지 확인해야 한다. 쓰지 않으면 checkout 옵션은 유지해도 무해하지만 느릴 수 있다.
- 운영 단계에서는 external action을 tag가 아니라 full commit SHA로 pinning할지 결정해야 한다.

검증 명령:
- `git diff --check`
- GitHub Actions `workflow_dispatch`로 `Winters CI` 수동 실행.
- PR에서 `Debug`, `Release`, `Debug-DX12` job이 각각 독립 artifact를 남기는지 확인.

Session - D: 검증된 빌드 산출물을 release candidate zip으로 패키징하고, CD를 실제 배포 전 artifact 승격 단계까지 구축한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Tools/Build/Package-WintersArtifacts.ps1

새 파일:

```powershell
[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "Debug-DX12", "Release-DX12")]
    [string]$Configuration = "Release",

    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = [System.IO.Path]::GetFullPath((Join-Path $ScriptDir "..\.."))

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $Root "out\artifacts"
}

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$packageName = "Winters-$Configuration-$stamp"
$packageRoot = Join-Path $OutputDir $packageName
$zipPath = Join-Path $OutputDir "$packageName.zip"

New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null

function Copy-PathIfExists([string]$Source, [string]$Destination) {
    if (Test-Path -LiteralPath $Source) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Destination) | Out-Null
        Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
    }
    else {
        Write-Host "[SKIP] Missing $Source"
    }
}

Copy-PathIfExists (Join-Path $Root "Engine\Bin\$Configuration") (Join-Path $packageRoot "Engine\Bin\$Configuration")
Copy-PathIfExists (Join-Path $Root "Client\Bin\$Configuration") (Join-Path $packageRoot "Client\Bin\$Configuration")
Copy-PathIfExists (Join-Path $Root "Server\Bin\$Configuration") (Join-Path $packageRoot "Server\Bin\$Configuration")
Copy-PathIfExists (Join-Path $Root "Tools\Bin\$Configuration") (Join-Path $packageRoot "Tools\Bin\$Configuration")
Copy-PathIfExists (Join-Path $Root "out\logs") (Join-Path $packageRoot "logs")
Copy-PathIfExists (Join-Path $Root "Shared\Schemas\Generated") (Join-Path $packageRoot "Shared\Schemas\Generated")

$manifest = @{
    name = $packageName
    configuration = $Configuration
    createdUtc = (Get-Date).ToUniversalTime().ToString("o")
    gitCommit = (& git rev-parse HEAD)
    gitStatus = (& git status --short)
} | ConvertTo-Json -Depth 4

$manifest | Set-Content -LiteralPath (Join-Path $packageRoot "manifest.json") -Encoding UTF8

Compress-Archive -LiteralPath (Join-Path $packageRoot "*") -DestinationPath $zipPath -Force
Write-Host "Package created: $zipPath"
```

1-2. C:/Users/user/Desktop/Winters/.github/workflows/winters-package.yml

새 파일:

```yaml
name: Winters Package

on:
  workflow_dispatch:
    inputs:
      configuration:
        description: Build configuration to package
        required: true
        default: Release
        type: choice
        options:
          - Release
          - Debug
          - Debug-DX12
          - Release-DX12
  push:
    tags:
      - "v*"

permissions:
  contents: read

defaults:
  run:
    shell: pwsh

jobs:
  package:
    name: Package ${{ inputs.configuration || 'Release' }}
    runs-on: windows-2022
    timeout-minutes: 120

    steps:
      - name: Checkout
        uses: actions/checkout@v6
        with:
          lfs: true

      - name: Setup MSBuild
        uses: microsoft/setup-msbuild@v2
        with:
          msbuild-architecture: x64

      - name: Resolve configuration
        id: config
        run: |
          $config = "${{ inputs.configuration }}"
          if ([string]::IsNullOrWhiteSpace($config)) {
            $config = "Release"
          }
          "configuration=$config" >> $env:GITHUB_OUTPUT

      - name: Verify release candidate
        run: |
          .\Tools\Build\Invoke-WintersVerify.ps1 `
            -Level CI `
            -Configuration "${{ steps.config.outputs.configuration }}" `
            -SkipAssetCook

      - name: Package artifacts
        run: |
          .\Tools\Build\Package-WintersArtifacts.ps1 `
            -Configuration "${{ steps.config.outputs.configuration }}"

      - name: Upload package
        uses: actions/upload-artifact@v7
        with:
          name: winters-package-${{ steps.config.outputs.configuration }}
          path: |
            out/artifacts/*.zip
          if-no-files-found: error
          retention-days: 30
```

2. 검증

미검증:
- 이 단계의 CD는 실제 서버 배포가 아니라 artifact 승격까지다.
- 배포 대상이 정해지면 `permissions`, secrets, environment approvals, remote copy 방식을 별도 세션에서 확정해야 한다.

검증 명령:
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\Build\Package-WintersArtifacts.ps1 -Configuration Release`
- GitHub Actions `Winters Package`를 수동 실행하고 zip artifact 다운로드 확인.

Session - E: CMake/Ninja를 전체 빌드 그래프로 확장하되, 기존 MSBuild/F5 흐름을 깨지 않는 병렬 경로로 만든다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/CMakeLists.txt

기존 코드:

```cmake
include(WintersCompilerOptions)
include(WintersThirdParty)
include(WintersEngine)
```

아래에 추가:

```cmake
include(WintersFlatbuffers)
include(WintersSharedGameSim)
include(WintersServer)
include(WintersClient)
include(WintersTools)
```

1-2. C:/Users/user/Desktop/Winters/cmake/WintersFlatbuffers.cmake

새 파일:

```cmake
set(WINTERS_SCHEMA_DIR "${WINTERS_ROOT_DIR}/Shared/Schemas")
set(WINTERS_FLATC_EXE "${WINTERS_ROOT_DIR}/Tools/Bin/flatc.exe")

set(WINTERS_SCHEMA_FILES
    "${WINTERS_SCHEMA_DIR}/Command.fbs"
    "${WINTERS_SCHEMA_DIR}/Snapshot.fbs"
    "${WINTERS_SCHEMA_DIR}/Event.fbs"
    "${WINTERS_SCHEMA_DIR}/Hello.fbs"
    "${WINTERS_SCHEMA_DIR}/LobbyTypes.fbs"
    "${WINTERS_SCHEMA_DIR}/LobbyState.fbs"
    "${WINTERS_SCHEMA_DIR}/LobbyCommand.fbs"
)

WintersRequireFile("${WINTERS_FLATC_EXE}" "FlatBuffers compiler")

add_custom_target(WintersFlatbuffersCodegen
    COMMAND "${WINTERS_SCHEMA_DIR}/run_codegen.bat"
    WORKING_DIRECTORY "${WINTERS_ROOT_DIR}"
    SOURCES ${WINTERS_SCHEMA_FILES}
    COMMENT "Generating Winters FlatBuffers code"
    VERBATIM
)

set_target_properties(WintersFlatbuffersCodegen PROPERTIES
    FOLDER "Generated"
)
```

1-3. C:/Users/user/Desktop/Winters/cmake/WintersSharedGameSim.cmake

CONFIRM_NEEDED:
- 현재 dirty worktree에서 `Shared/GameSim` 파일 추가/삭제/rename이 많다.
- 구현 세션 시작 시 `Server/Include/Server.vcxproj`, `Client/Include/Client.vcxproj`, `Shared/GameSim/**/*.h`, `Shared/GameSim/**/*.cpp`를 다시 읽고 실제 source ownership을 확정해야 한다.
- `OBJECT` library로 분리할지, Client/Server가 각각 shared sources를 compile할지 결정해야 한다.

1-4. C:/Users/user/Desktop/Winters/cmake/WintersServer.cmake

CONFIRM_NEEDED:
- `Server.vcxproj`의 include, link, post-build DLL copy, FlatcCodegen target을 CMake로 정확히 이식해야 한다.
- 현재 Server project 파일도 dirty 상태이므로 구현 직전 실제 anchor를 다시 확인해야 한다.

1-5. C:/Users/user/Desktop/Winters/cmake/WintersClient.cmake

CONFIRM_NEEDED:
- Client는 EngineSDK, Shared generated headers, network/backend, champion modules, shaders/runtime assets와 얽혀 있다.
- `Client.vcxproj`의 전체 include/link/post-build/FlatcCodegen 규칙을 다시 확인한 뒤 target body를 작성해야 한다.

1-6. C:/Users/user/Desktop/Winters/cmake/WintersTools.cmake

CONFIRM_NEEDED:
- `WintersAssetConverter.vcxproj`의 Assimp DLL copy, AssetFormat source list, Resource helper source list를 다시 확인해야 한다.
- Tools에 추가될 validator/cook/checker가 같은 module에 들어갈지 별도 executable로 갈지 결정해야 한다.

1-7. C:/Users/user/Desktop/Winters/CMakePresets.json

CONFIRM_NEEDED:
- Session E에서 실제 CMake target이 생긴 뒤 `server-debug`, `client-debug`, `tools-debug`, `all-debug`, `all-release` presets를 추가한다.
- 현재 `CMakePresets.json`은 Engine 중심 preset만 있으므로, target 이름 확정 전에는 JSON 교체 블록을 쓰지 않는다.

2. 검증

미검증:
- CMake 전체 그래프는 MSBuild CI가 안정화된 뒤 별도 세션에서 진행한다.
- 목표는 MSBuild를 즉시 제거하는 것이 아니라, CI 빌드 시간을 줄이고 generated Visual Studio 프로젝트 경로를 열어두는 것이다.

검증 명령:
- `cmake --preset msvc-ninja`
- `cmake --build --preset engine-debug`
- `cmake --build --preset engine-release`
- Session E 구현 후 추가: `cmake --build --preset all-debug`

Session - F: 실제 배포 대상이 확정되면 CD를 artifact 업로드에서 환경 승격으로 확장한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/.github/workflows/winters-deploy.yml

CONFIRM_NEEDED:
- 배포 대상이 아직 없다.
- 선택지가 GitHub Release draft, 내부 QA 공유 폴더, 원격 Windows test machine, cloud object storage, Steam branch 중 무엇인지 확인해야 한다.
- 대상에 따라 필요한 `permissions`, environment approval, secret 이름, credential 방식, rollback 방식이 달라진다.

1-2. C:/Users/user/Desktop/Winters/Tools/Build/Deploy-WintersArtifacts.ps1

CONFIRM_NEEDED:
- 배포 대상 URI와 인증 방식이 정해져야 complete file body를 쓸 수 있다.
- deploy script는 package zip을 입력으로 받고, build를 다시 수행하지 않는 방식으로 작성한다.

2. 검증

미검증:
- CD의 성공 기준은 "새 build를 다시 만드는 것"이 아니라 "CI에서 검증된 artifact를 같은 checksum으로 대상 환경에 승격하는 것"이다.

확인 필요:
- 배포 대상.
- secrets 저장 위치.
- rollback 기준.
- release naming 규칙.
- PDB/symbol 보관 기간.

