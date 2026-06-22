$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot

$assets = @(
    @{ Name = 'cursor'; Path = 'Client/Bin/Resource/Texture/UI/ux/cursors/radialmenucursor_ping.png'; Width = 48; Height = 48; Directional = $false },
    @{ Name = 'default'; Path = 'Client/Bin/Resource/Texture/UI/ux/minimap/pings/ping.png'; Width = 32; Height = 32; Directional = $false },
    @{ Name = 'on_my_way'; Path = 'Client/Bin/Resource/Texture/UI/ux/minimap/pings/on_my_way_new.png'; Width = 32; Height = 32; Directional = $true },
    @{ Name = 'danger'; Path = 'Client/Bin/Resource/Texture/UI/ux/minimap/pings/caution.png'; Width = 32; Height = 32; Directional = $true },
    @{ Name = 'assist'; Path = 'Client/Bin/Resource/Texture/UI/ux/minimap/pings/assist.png'; Width = 32; Height = 32; Directional = $true },
    @{ Name = 'missing'; Path = 'Client/Bin/Resource/Texture/UI/ux/minimap/pings/mia_new.png'; Width = 32; Height = 32; Directional = $true }
)

Add-Type -AssemblyName System.Drawing

function Get-PixelStats {
    param([string]$Path)

    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        $bytes = [System.Collections.Generic.List[byte]]::new($bitmap.Width * $bitmap.Height * 4)
        $visiblePixels = 0
        for ($y = 0; $y -lt $bitmap.Height; ++$y) {
            for ($x = 0; $x -lt $bitmap.Width; ++$x) {
                $pixel = $bitmap.GetPixel($x, $y)
                $bytes.Add($pixel.R)
                $bytes.Add($pixel.G)
                $bytes.Add($pixel.B)
                $bytes.Add($pixel.A)
                if ($pixel.A -ne 0) {
                    ++$visiblePixels
                }
            }
        }

        $sha = [System.Security.Cryptography.SHA256]::Create()
        try {
            $hashBytes = $sha.ComputeHash($bytes.ToArray())
        }
        finally {
            $sha.Dispose()
        }

        return [pscustomobject]@{
            Width = $bitmap.Width
            Height = $bitmap.Height
            VisiblePixels = $visiblePixels
            Hash = ([BitConverter]::ToString($hashBytes) -replace '-', '').ToLowerInvariant()
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

$directionalHashes = @{}

foreach ($asset in $assets) {
    $fullPath = Join-Path $repoRoot $asset.Path
    if (-not (Test-Path -LiteralPath $fullPath)) {
        throw "Missing ping wheel asset: $($asset.Path)"
    }

    $stats = Get-PixelStats -Path $fullPath
    if ($stats.Width -ne $asset.Width -or $stats.Height -ne $asset.Height) {
        throw "Unexpected dimensions for $($asset.Path): $($stats.Width)x$($stats.Height), expected $($asset.Width)x$($asset.Height)"
    }
    if ($stats.VisiblePixels -le 0) {
        throw "Asset has no visible pixels: $($asset.Path)"
    }

    if ($asset.Directional) {
        if ($directionalHashes.ContainsKey($stats.Hash)) {
            throw "Directional ping image duplicates $($directionalHashes[$stats.Hash]): $($asset.Path)"
        }
        $directionalHashes[$stats.Hash] = $asset.Name
    }

    Write-Host ("[PingWheelAssets] {0} {1}x{2} visible={3} sha256={4}" -f `
        $asset.Name, $stats.Width, $stats.Height, $stats.VisiblePixels, $stats.Hash.Substring(0, 16))
}

if ($directionalHashes.Count -ne 4) {
    throw "Expected four unique directional ping images, got $($directionalHashes.Count)"
}

Write-Host '[PingWheelAssets] PASS'
