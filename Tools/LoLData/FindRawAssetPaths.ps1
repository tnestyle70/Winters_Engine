param(
    [switch]$FailOnCandidate
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

$SearchRoots = @(
    "Client",
    "Engine",
    "Server",
    "Shared"
)

$RgArgs = @(
    "--glob", "!Client/Bin/**",
    "--glob", "!EngineSDK/**",
    "--glob", "!Engine/External/**",
    "--glob", "!Engine/ThirdPartyLib/**",
    "--glob", "!**/Generated/**",
    "-g", "*.cpp",
    "-g", "*.h",
    "-g", "*.hpp",
    "-g", "*.inl",
    "-n",
    "--fixed-strings",
    "Client/Bin/Resource"
)

Push-Location $Root
try {
    $Lines = @(rg @RgArgs @SearchRoots)
    $Candidates = @()

    foreach ($Line in $Lines) {
        if ($Line -match "^(?<file>[^:]+):(?<line>\d+):(?<text>.*)$") {
            $File = $Matches["file"].Replace("\", "/")
            $Text = $Matches["text"]
            $Candidates += [pscustomobject]@{
                File = $File
                Line = [int]$Matches["line"]
                Text = $Text.Trim()
            }
        }
    }

    $Groups = $Candidates |
        Group-Object File |
        Sort-Object @{ Expression = "Count"; Descending = $true }, @{ Expression = "Name"; Descending = $false }

    Write-Host "[RawAssetPathAudit] candidate files: $($Groups.Count)"
    Write-Host "[RawAssetPathAudit] candidate occurrences: $($Candidates.Count)"

    foreach ($Group in $Groups) {
        Write-Host ("{0,4} {1}" -f $Group.Count, $Group.Name)
    }

    if ($FailOnCandidate -and $Candidates.Count -gt 0) {
        throw "Raw product asset paths remain in hand-written runtime code."
    }
}
finally {
    Pop-Location
}
