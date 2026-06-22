$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$wfxFiles = @(
    "Data/LoL/FX/Object/Minion/ranged_projectile_blue.wfx",
    "Data/LoL/FX/Object/Minion/ranged_projectile_red.wfx"
)

$missing = @()
foreach ($rel in $wfxFiles) {
    $path = Join-Path $repoRoot $rel
    if (-not (Test-Path -LiteralPath $path)) {
        $missing += $rel
        continue
    }

    $json = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    if (-not $json.name) {
        $missing += "${rel}:name"
    }

    foreach ($emitter in $json.emitters) {
        foreach ($field in @("model", "texture", "erode_texture")) {
            $ref = $emitter.$field
            if ([string]::IsNullOrWhiteSpace($ref)) {
                continue
            }
            $refPath = Join-Path $repoRoot $ref
            if (-not (Test-Path -LiteralPath $refPath)) {
                $missing += "$rel -> $field=$ref"
            }
        }
    }
}

$directProjectileTextures = @()
foreach ($dir in @(
    "Client/Bin/Resource/Texture/Object/Minion_Order/Ranged",
    "Client/Bin/Resource/Texture/Object/Minion_Chaos/ranged"
)) {
    $path = Join-Path $repoRoot $dir
    if (Test-Path -LiteralPath $path) {
        $directProjectileTextures += Get-ChildItem -LiteralPath $path -File -Recurse |
            Where-Object { $_.Name -match "(?i)(projectile|missile|bolt)" } |
            Select-Object -ExpandProperty FullName
    }
}

if ($missing.Count -gt 0) {
    Write-Error ("Missing minion projectile asset references:`n" + ($missing -join "`n"))
    exit 1
}

Write-Host "[OK] Minion projectile WFX cues and referenced resources exist."
if ($directProjectileTextures.Count -eq 0) {
    Write-Host "[INFO] No direct original minion projectile texture was found under Object/Minion_* Ranged folders; runtime uses Data/LoL/FX/Object/Minion ranged_projectile_*.wfx."
}
else {
    Write-Host "[INFO] Direct minion projectile texture candidates:"
    $directProjectileTextures | ForEach-Object { Write-Host " - $_" }
}
