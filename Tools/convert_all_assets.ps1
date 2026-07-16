param(
    [string]$Mode = ""
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Conv = Join-Path $Root "Bin\Debug\WintersAssetConverter.exe"
$CharSrc = [System.IO.Path]::GetFullPath((Join-Path $Root "..\Client\Bin\Resource\Texture\Character"))
$ResourceSrc = [System.IO.Path]::GetFullPath((Join-Path $Root "..\Client\Bin\Resource\Texture"))
$LegacyCharSrc = [System.IO.Path]::GetFullPath((Join-Path $Root "..\..\LOL_Resource\Character"))
$LegacyResourceSrc = [System.IO.Path]::GetFullPath((Join-Path $Root "..\..\LOL_Resource"))

$script:OK = 0
$script:FAIL = 0

if (-not (Test-Path -LiteralPath $Conv -PathType Leaf)) {
    Write-Output "[ERROR] Converter not found: $Conv"
    exit 1
}

function Get-FullPath([string]$Path) {
    return [System.IO.Path]::GetFullPath($Path)
}

function Test-OutputsCurrent([string]$Source, [string[]]$Outputs) {
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        return $false
    }

    $sourceItem = Get-Item -LiteralPath $Source
    $convItem = Get-Item -LiteralPath $Conv

    foreach ($output in $Outputs) {
        if (-not (Test-Path -LiteralPath $output -PathType Leaf)) {
            return $false
        }

        $outputItem = Get-Item -LiteralPath $output
        if ($sourceItem.LastWriteTimeUtc -gt $outputItem.LastWriteTimeUtc) {
            return $false
        }
        if ($convItem.LastWriteTimeUtc -gt $outputItem.LastWriteTimeUtc) {
            return $false
        }
    }

    return $true
}

function Test-AssetSetCurrent([string]$Source, [string]$Skel, [string]$Mesh, [string]$Mat, [string]$AnimDir) {
    if (-not (Test-OutputsCurrent $Source @($Skel, $Mesh, $Mat))) {
        return $false
    }

    $hasAnim = Test-Path -LiteralPath (Join-Path $AnimDir ".wanim.stamp") -PathType Leaf
    if (-not $hasAnim -and (Test-Path -LiteralPath $AnimDir -PathType Container)) {
        $hasAnim = [bool](Get-ChildItem -LiteralPath $AnimDir -Filter "*.wanim" -File -ErrorAction SilentlyContinue | Select-Object -First 1)
    }

    return $hasAnim
}

function Invoke-Converter([string[]]$ToolArgs) {
    & $Conv @ToolArgs
    return ($LASTEXITCODE -eq 0)
}

function Convert-Static([string]$SourcePath) {
    $source = Get-FullPath $SourcePath
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        Write-Output "[SKIP] $source not found"
        $script:FAIL++
        return
    }

    $outPath = [System.IO.Path]::ChangeExtension($source, ".wmesh")
    $matPath = [System.IO.Path]::ChangeExtension($source, ".wmat")
    if (Test-OutputsCurrent $source @($outPath, $matPath)) {
        Write-Output "[UP-TO-DATE] $source"
        return
    }

    if (Invoke-Converter @("mesh", $source, "-o", $outPath)) {
        $script:OK++
    } else {
        $script:FAIL++
    }
}

function Convert-Map([string]$SourcePath) {
    $source = Get-FullPath $SourcePath
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        Write-Output "[SKIP] $source not found"
        $script:FAIL++
        return
    }

    $dir = Split-Path -Parent $source
    $base = [System.IO.Path]::GetFileNameWithoutExtension($source)
    $visualMesh = Join-Path $dir "$base.wmesh"
    $visualMat = Join-Path $dir "$base.wmat"
    $surfaceMesh = Join-Path $dir "${base}_surface.wmesh"
    $surfaceMat = Join-Path $dir "${base}_surface.wmat"
    if (Test-OutputsCurrent $source @($visualMesh, $visualMat, $surfaceMesh, $surfaceMat)) {
        Write-Output "[UP-TO-DATE] $source"
        return
    }

    $foliageMaterial = "Maps/KitPieces/SRX/Base/Models/LevelProp/Materials/VertexDeform_inst"
    $foliageTexture = "Texture/MAP/output/textures/assets/maps/kitpieces/srx/textures/sru_brush.png"
    $materialRemap = "$foliageMaterial=$foliageTexture"

    if (-not (Invoke-Converter @(
        "mesh", $source,
        "--pretransform",
        "--material-remap", $materialRemap,
        "-o", $visualMesh))) {
        $script:FAIL++
        return
    }

    if (-not (Invoke-Converter @(
        "mesh", $source,
        "--pretransform",
        "--exclude-material", $foliageMaterial,
        "-o", $surfaceMesh))) {
        $script:FAIL++
        return
    }

    $script:OK++
}

function Convert-SkinnedModel([string]$ModelPath) {
    $model = Get-FullPath $ModelPath
    if (-not (Test-Path -LiteralPath $model -PathType Leaf)) {
        Write-Output "[SKIP] $model not found"
        $script:FAIL++
        return
    }

    $dir = Split-Path -Parent $model
    $base = [System.IO.Path]::GetFileNameWithoutExtension($model)
    $skel = Join-Path $dir "$base.wskel"
    $mesh = Join-Path $dir "$base.wmesh"
    $mat = Join-Path $dir "$base.wmat"
    $animDir = Join-Path $dir "anims"

    if (Test-AssetSetCurrent $model $skel $mesh $mat $animDir) {
        Write-Output "[UP-TO-DATE] $model"
        return
    }

    if (-not (Invoke-Converter @("skel", $model, "-o", $skel))) {
        $script:FAIL++
        return
    }

    if (-not (Invoke-Converter @("mesh", $model, "--skel", $skel, "-o", $mesh))) {
        $script:FAIL++
        return
    }

    New-Item -ItemType Directory -Force -Path $animDir | Out-Null
    Get-ChildItem -LiteralPath $animDir -Filter "*.wanim" -File -ErrorAction SilentlyContinue |
        Remove-Item -Force -ErrorAction SilentlyContinue
    if (Invoke-Converter @("anim", $model, "--skel", $skel, "-o", $animDir)) {
        New-Item -ItemType File -Force -Path (Join-Path $animDir ".wanim.stamp") | Out-Null
        $script:OK++
    } else {
        $script:FAIL++
    }
}

function Convert-Champ([string]$ChampRoot, [string]$Champion, [string]$Fbx) {
    $dir = Join-Path $ChampRoot $Champion
    Convert-SkinnedModel (Join-Path $dir $Fbx)
}

function Convert-StaticTree([string]$TreeRoot) {
    if (-not (Test-Path -LiteralPath $TreeRoot -PathType Container)) {
        return
    }

    Get-ChildItem -LiteralPath $TreeRoot -Recurse -Filter "*.fbx" -File | ForEach-Object {
        Convert-Static $_.FullName
    }
}

function Convert-ParticleFbxTree([string]$TreeRoot) {
    if (-not (Test-Path -LiteralPath $TreeRoot -PathType Container)) {
        return
    }

    Get-ChildItem -LiteralPath $TreeRoot -Recurse -Filter "*.fbx" -File | Where-Object {
        $_.FullName -match "\\particles\\fbx\\"
    } | ForEach-Object {
        Convert-Static $_.FullName
    }
}

function Convert-Champions([string]$ChampRoot) {
    Convert-Champ $ChampRoot "Irelia" "irelia_fixed.fbx"
    Convert-Champ $ChampRoot "Yasuo" "yasuo_fixed.fbx"
    Convert-Champ $ChampRoot "Sylas" "sylas.fbx"
    Convert-Champ $ChampRoot "Viego" "viego_fixed.fbx"
    Convert-Champ $ChampRoot "Kalista" "kalista.fbx"
    Convert-Champ $ChampRoot "Garen" "garen.fbx"
    Convert-Champ $ChampRoot "Zed" "zed.fbx"
    Convert-Champ $ChampRoot "Riven" "riven.fbx"
    Convert-Champ $ChampRoot "Ezreal" "ezreal.fbx"
    Convert-Champ $ChampRoot "Fiora" "fiora.fbx"
    Convert-Champ $ChampRoot "Jax" "jax.fbx"
    Convert-Champ $ChampRoot "Annie" "annie.fbx"
    Convert-Champ $ChampRoot "Ashe" "ashe.fbx"
    Convert-Champ $ChampRoot "Yone" "Yone.fbx"
    Convert-Champ $ChampRoot "Kindred" "kindred.fbx"
    Convert-Champ $ChampRoot "MasterYi" "masteryi.fbx"
    Convert-Champ $ChampRoot "LeeSin" "leesin.fbx"
}

function Convert-Minions([string]$ObjRoot) {
    Convert-SkinnedModel (Join-Path $ObjRoot "Minion_Order\Melee\order_melee_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Minion_Order\Ranged\order_ranged_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Minion_Order\Siege\order_siege_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Minion_Order\Super\order_super_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Minion_Chaos\melee\chaos_melee_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Minion_Chaos\ranged\chaos_ranged_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Minion_Chaos\siege\chaos_siege_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Minion_Chaos\super\chaos_super_textured.glb")
}

function Convert-Objects([string]$ObjRoot) {
    Convert-SkinnedModel (Join-Path $ObjRoot "Turret\turret_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Turret\turret_red_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Inhibitor\inhibitor_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Inhibitor\inhibitor_red_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Nexus\nexus_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Nexus\nexus_red_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Jungle\Baron\baron_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Jungle\Blue\blue_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Jungle\Dragon\water\dragon_water_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Jungle\Dragon\air\dragon_air_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Jungle\Red\red_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Jungle\Gromp\gromp_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Jungle\Krug\krug_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Jungle\KrugMini\krugmini_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Jungle\Razorbeak\razorbeak_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Jungle\RazorbeakMini\razorbeakmini_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Jungle\Wolf\wolf_textured.glb")
    Convert-SkinnedModel (Join-Path $ObjRoot "Jungle\WolfMini\wolfmini_textured.glb")
    Convert-Minions $ObjRoot
}

$championsOnly = $Mode -ieq "champions"
$minionsOnly = $Mode -ieq "minions"
$mapsOnly = $Mode -ieq "maps"

if ($mapsOnly) {
    Convert-Map (Join-Path $ResourceSrc "MAP\output\sr_base_flip.glb")
    Write-Output "OK=$script:OK FAIL=$script:FAIL"
    if ($script:FAIL -gt 0) {
        exit 1
    }
    exit 0
}

if ($minionsOnly) {
    Convert-Minions (Join-Path $ResourceSrc "Object")
    if (Test-Path -LiteralPath (Join-Path $LegacyResourceSrc "Object") -PathType Container) {
        Convert-Minions (Join-Path $LegacyResourceSrc "Object")
    }
    Write-Output "OK=$script:OK FAIL=$script:FAIL"
    exit 0
}

Convert-Champions $CharSrc
if (Test-Path -LiteralPath $LegacyCharSrc -PathType Container) {
    Convert-Champions $LegacyCharSrc
}

if (-not $championsOnly) {
    Convert-Map (Join-Path $ResourceSrc "MAP\output\sr_base_flip.glb")
    if (Test-Path -LiteralPath (Join-Path $LegacyResourceSrc "MAP") -PathType Container) {
        Convert-Map (Join-Path $LegacyResourceSrc "MAP\output\sr_base_flip.glb")
    }

    Convert-Objects (Join-Path $ResourceSrc "Object")
    Convert-StaticTree (Join-Path $ResourceSrc "FX")
    Convert-ParticleFbxTree $CharSrc

    if (Test-Path -LiteralPath (Join-Path $LegacyResourceSrc "Object") -PathType Container) {
        Convert-Objects (Join-Path $LegacyResourceSrc "Object")
    }
}

Write-Output "OK=$script:OK FAIL=$script:FAIL"
exit 0
