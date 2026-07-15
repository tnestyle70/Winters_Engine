$ErrorActionPreference = "Stop"

$workspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$sourcePath = Join-Path $PSScriptRoot `
    "ServerProjectileAuthorityContractProbe.cpp"
$authoritySource = Join-Path $workspaceRoot `
    "Server\Private\Game\ServerProjectileAuthority.cpp"
$vsWhere = Join-Path ${env:ProgramFiles(x86)} `
    "Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path -LiteralPath $vsWhere)) {
    throw "vswhere.exe was not found: $vsWhere"
}

$installationPath = & $vsWhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $installationPath) {
    throw "A Visual Studio C++ toolchain was not found."
}

$vsDevCmd = Join-Path $installationPath "Common7\Tools\VsDevCmd.bat"
if (-not (Test-Path -LiteralPath $vsDevCmd)) {
    throw "VsDevCmd.bat was not found: $vsDevCmd"
}

$gameSimLibrary = Join-Path $workspaceRoot `
    "Shared\GameSim\Bin\Debug\WintersGameSim.lib"
$engineLibrary = Join-Path $workspaceRoot `
    "Engine\Bin\Debug\WintersEngine.lib"
if (-not (Test-Path -LiteralPath $gameSimLibrary) -or
    -not (Test-Path -LiteralPath $engineLibrary)) {
    throw "Build Debug x64 GameSim and Engine before running this probe."
}

$outputDirectory = Join-Path $env:TEMP `
    "WintersServerProjectileAuthorityContractProbe"
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$executablePath = Join-Path $outputDirectory `
    "ServerProjectileAuthorityContractProbe.exe"
$serverPublic = Join-Path $workspaceRoot "Server\Public"
$engineSdkInclude = Join-Path $workspaceRoot "EngineSDK\inc"
$flatBuffersInclude = Join-Path $workspaceRoot `
    "Engine\ThirdPartyLib\FlatBuffers\Inc"

$compileCommand = (
    '"{0}" -no_logo -arch=x64 -host_arch=x64 >nul && ' +
    'cd /d "{1}" && ' +
    'cl.exe /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /MDd ' +
    '/D NOMINMAX /D WIN32_LEAN_AND_MEAN /D _DEBUG ' +
    '/I"{2}" /I"{3}" /I"{4}" /I"{5}" ' +
    '"{6}" "{7}" "{8}" "{9}" /Fe:"{10}"'
) -f $vsDevCmd, $outputDirectory, $workspaceRoot, $serverPublic, `
    $engineSdkInclude, $flatBuffersInclude, $sourcePath, $authoritySource, `
    $gameSimLibrary, $engineLibrary, $executablePath

& $env:ComSpec /d /s /c $compileCommand
if ($LASTEXITCODE -ne 0) {
    throw "Server projectile authority contract compilation failed with exit code $LASTEXITCODE."
}

$engineRuntime = Join-Path $workspaceRoot "EngineSDK\bin\Debug"
$previousPath = $env:PATH
try {
    $env:PATH = "$engineRuntime;$previousPath"
    & $executablePath
    if ($LASTEXITCODE -ne 0) {
        throw "Server projectile authority contract failed with exit code $LASTEXITCODE."
    }
}
finally {
    $env:PATH = $previousPath
}
