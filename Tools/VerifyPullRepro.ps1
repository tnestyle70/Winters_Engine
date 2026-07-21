[CmdletBinding()]
param(
    [switch]$RequireTracked
)

$ErrorActionPreference = "Stop"

function Assert-FileExists {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required file is missing: $Path"
    }
}

function Assert-GitNotIgnored {
    param([string]$Path)

    git check-ignore -q -- $Path
    if ($LASTEXITCODE -eq 0) {
        throw "Required pull/build input is ignored by git: $Path"
    }
    if ($LASTEXITCODE -ne 1) {
        throw "git check-ignore failed for: $Path"
    }
}

function Assert-GitIgnored {
    param([string]$Path)

    git check-ignore -q -- $Path
    if ($LASTEXITCODE -ne 0) {
        throw "Expected local-only path is not ignored by git: $Path"
    }
}

function Assert-GitTracked {
    param([string]$Path)

    $tracked = @(git ls-files -- $Path)
    if (-not ($tracked -contains $Path)) {
        throw "Required pull/build input is not tracked yet: $Path"
    }
}

$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path

Push-Location $root
try {
    $requiredFiles = @(
        "Tools/Bin/flatc.exe",
        "Engine/ThirdPartyLib/Assimp/Lib/Debug/assimp-vc143-mtd.lib",
        "Engine/ThirdPartyLib/Assimp/Lib/Release/assimp-vc143-mt.lib",
        "Engine/ThirdPartyLib/Assimp/Bin/Debug/assimp-vc143-mtd.dll",
        "Engine/ThirdPartyLib/Assimp/Bin/Debug/kubazip.dll",
        "Engine/ThirdPartyLib/Assimp/Bin/Debug/minizip.dll",
        "Engine/ThirdPartyLib/Assimp/Bin/Debug/poly2tri.dll",
        "Engine/ThirdPartyLib/Assimp/Bin/Debug/pugixml.dll",
        "Engine/ThirdPartyLib/Assimp/Bin/Debug/zlibd1.dll",
        "Engine/ThirdPartyLib/Assimp/Bin/Release/assimp-vc143-mt.dll",
        "Engine/ThirdPartyLib/Assimp/Bin/Release/kubazip.dll",
        "Engine/ThirdPartyLib/Assimp/Bin/Release/minizip.dll",
        "Engine/ThirdPartyLib/Assimp/Bin/Release/poly2tri.dll",
        "Engine/ThirdPartyLib/Assimp/Bin/Release/pugixml.dll",
        "Engine/ThirdPartyLib/Assimp/Bin/Release/zlib1.dll",
        "Engine/ThirdPartyLib/DirectXTK/Lib/Debug/DirectXTK.lib",
        "Engine/ThirdPartyLib/DirectXTK/Lib/Release/DirectXTK.lib",
        "Engine/ThirdPartyLib/DirectXTK/Bin/Debug/DirectXTK.dll",
        "Engine/ThirdPartyLib/DirectXTK/Bin/Release/DirectXTK.dll",
        "Engine/ThirdPartyLib/FMOD/Lib/fmod_vc.lib",
        "Engine/ThirdPartyLib/FMOD/Bin/fmod.dll"
    )

    foreach ($file in $requiredFiles) {
        Assert-FileExists $file
        Assert-GitNotIgnored $file
        if ($RequireTracked) {
            Assert-GitTracked $file
        }
    }

    $trackedContractFiles = @(
        "EngineSDK/inc/ECS/Entity.h",
        "EngineSDK/inc/ECS/World.h",
        "Shared/Schemas/Generated/cpp/Command_generated.h",
        "Shared/Schemas/Generated/cpp/Event_generated.h",
        "Shared/Schemas/Generated/cpp/Snapshot_generated.h"
    )

    foreach ($file in $trackedContractFiles) {
        Assert-FileExists $file
        Assert-GitTracked $file
    }

    Assert-GitIgnored "Client/Bin/Resource/example.asset"
    Assert-GitIgnored "Client/Bin/Resource.zip"
    Assert-GitIgnored "Client/Bin/Debug/Resource/example.asset"
    Assert-GitIgnored "Client/Bin/Release/Resource/example.asset"

    $expectedTrackedRuntimeResources = @(
        "Client/Bin/Resource/Config/Practice/attack_speed_tuning.json",
        "Client/Bin/Resource/Config/Practice/practice_balance_overrides.json"
    ) | Sort-Object
    $trackedRuntimeResources = @(git ls-files -- Client/Bin/Resource) | Sort-Object
    $runtimeResourceDrift = @(Compare-Object -ReferenceObject $expectedTrackedRuntimeResources -DifferenceObject $trackedRuntimeResources)
    if ($runtimeResourceDrift.Count -ne 0) {
        throw "Client/Bin/Resource tracked exact-set drift: $($runtimeResourceDrift | Out-String)"
    }

    Write-Host "PASS: pull reproducibility inputs and local asset boundary are valid."
}
finally {
    Pop-Location
}
