param(
    [string]$Root = "",
    [switch]$NoWrite
)

$ErrorActionPreference = "Stop"

if ($Root.Length -eq 0) {
    $Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}

$generator = Join-Path $Root "Tools\LoLData\Build-LoLDefinitionPack.py"
& python $generator --root $Root --test-draft-roundtrip
exit $LASTEXITCODE
