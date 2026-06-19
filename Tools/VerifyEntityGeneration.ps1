[CmdletBinding()]
param(
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

function Find-ExistingPath {
    param([string[]]$Candidates)

    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    return $null
}

function Invoke-EntitySmoke {
    param(
        [string]$Root,
        [string]$VcVars
    )

    $tempDir = Join-Path ([System.IO.Path]::GetTempPath()) ("WintersEntityGenerationSmoke_" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $tempDir | Out-Null

    try {
        $cpp = Join-Path $tempDir "EntityGenerationSmoke.cpp"
        $obj = Join-Path $tempDir "EntityGenerationSmoke.obj"
        $exe = Join-Path $tempDir "EntityGenerationSmoke.exe"

        @'
#include "ECS/Entity.h"

#include <cstdio>

#define CHECK(expr) do { if (!(expr)) { std::printf("FAIL: %s\n", #expr); return 1; } } while (0)

int main()
{
    CEntityManager entities;

    CHECK(entities.GetAliveCount() == 0);

    const EntityID first = entities.Create();
    CHECK(first != NULL_ENTITY);
    CHECK(entities.IsAlive(first));
    CHECK(entities.GetAliveCount() == 1);

    const EntityHandle firstHandle = entities.GetHandle(first);
    CHECK(firstHandle.IsValid());
    CHECK(firstHandle.GetIndex() == first);
    CHECK(entities.IsAlive(firstHandle));
    CHECK(entities.Resolve(firstHandle) == first);

    entities.Destroy(first);
    CHECK(!entities.IsAlive(first));
    CHECK(!entities.IsAlive(firstHandle));
    CHECK(entities.Resolve(firstHandle) == NULL_ENTITY);
    CHECK(entities.GetAliveCount() == 0);

    EntityID resolved = first;
    CHECK(!entities.TryResolve(firstHandle, resolved));

    const EntityID reused = entities.Create();
    const EntityHandle reusedHandle = entities.GetHandle(reused);
    CHECK(reused == first);
    CHECK(reusedHandle.IsValid());
    CHECK(reusedHandle.GetIndex() == reused);
    CHECK(reusedHandle.GetGeneration() != firstHandle.GetGeneration());
    CHECK(entities.IsAlive(reused));
    CHECK(entities.IsAlive(reusedHandle));
    CHECK(entities.GetAliveCount() == 1);

    CHECK(!entities.Destroy(firstHandle));
    CHECK(entities.IsAlive(reused));
    CHECK(entities.IsAlive(reusedHandle));
    CHECK(entities.GetAliveCount() == 1);

    CHECK(entities.Destroy(reusedHandle));
    CHECK(!entities.IsAlive(reused));
    CHECK(!entities.IsAlive(reusedHandle));
    CHECK(entities.GetAliveCount() == 0);

    std::printf("PASS\n");
    return 0;
}
'@ | Set-Content -Encoding ASCII -NoNewline -Path $cpp

        $includeDir = Join-Path $Root "Engine\Public"
        $compileCmd = "`"$VcVars`" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /I`"$includeDir`" `"$cpp`" /Fo`"$obj`" /Fe`"$exe`""
        cmd /c $compileCmd
        if ($LASTEXITCODE -ne 0) {
            throw "Entity generation smoke compile failed."
        }

        & $exe
        if ($LASTEXITCODE -ne 0) {
            throw "Entity generation smoke failed."
        }
    }
    finally {
        Remove-Item -LiteralPath $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$vcVars = Find-ExistingPath @(
    "$env:ProgramFiles\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
)

if (-not $vcVars) {
    throw "vcvars64.bat was not found."
}

$msBuild = Find-ExistingPath @(
    "$env:ProgramFiles\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
)

Push-Location $root
try {
    $diffTargets = @(
        "Engine/Public/ECS/Entity.h",
        "EngineSDK/inc/ECS/Entity.h",
        "Engine/Public/ECS/World.h",
        "EngineSDK/inc/ECS/World.h",
        ".md/plan/refactor/06_ENTITY_ID_GENERATION_ATOMIC_DIRECTION.md",
        "Tools/VerifyEntityGeneration.ps1"
    )

    git diff --check -- @diffTargets
    if ($LASTEXITCODE -ne 0) {
        throw "git diff --check failed."
    }

    $entityHeaders = @("Engine/Public/ECS/Entity.h", "EngineSDK/inc/ECS/Entity.h")

    rg -q "m_vecAlive|m_vecGenerations|m_vecFreeList|m_iNextID" @entityHeaders
    $legacySearchExit = $LASTEXITCODE
    if ($legacySearchExit -eq 0) {
        throw "Legacy EntityManager storage fields are still present."
    }
    if ($legacySearchExit -ne 1) {
        throw "Legacy field search failed with exit code $legacySearchExit."
    }

    rg -q "m_vecSlots|m_iFreeHead|NextAliveGeneration|NextDeadGeneration" @entityHeaders
    if ($LASTEXITCODE -ne 0) {
        throw "EntitySlot storage fields were not found."
    }

    Invoke-EntitySmoke -Root $root -VcVars $vcVars

    if (-not $SkipBuild) {
        if (-not $msBuild) {
            throw "MSBuild.exe was not found."
        }

        & $msBuild "Winters.sln" /m /p:Configuration=$Configuration /p:Platform=$Platform
        if ($LASTEXITCODE -ne 0) {
            throw "Winters.sln build failed."
        }
    }
}
finally {
    Pop-Location
}
