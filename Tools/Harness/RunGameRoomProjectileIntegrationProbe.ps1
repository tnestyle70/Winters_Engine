$ErrorActionPreference = "Stop"

$workspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$sourcePath = Join-Path $PSScriptRoot `
    "GameRoomProjectileIntegrationProbe.cpp"
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
$msbuild = Join-Path $installationPath "MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path -LiteralPath $vsDevCmd) -or
    -not (Test-Path -LiteralPath $msbuild)) {
    throw "Visual Studio build tools are incomplete."
}

$serverProject = Join-Path $workspaceRoot "Server\Include\Server.vcxproj"
& $msbuild $serverProject /m:1 /t:Build `
    /p:Configuration=Debug /p:Platform=x64 /verbosity:minimal
if ($LASTEXITCODE -ne 0) {
    throw "Server Debug x64 freshness build failed with exit code $LASTEXITCODE."
}

$serverIntermediate = Join-Path $workspaceRoot `
    "Server\Bin\Intermediate\Debug"
$serverObjects = @(
    Get-ChildItem -LiteralPath $serverIntermediate -Filter "*.obj" |
        Where-Object { $_.Name -ne "main.obj" } |
        Sort-Object Name
)
if ($serverObjects.Count -eq 0) {
    throw "No Server Debug object files were found."
}

$gameSimLibrary = Join-Path $workspaceRoot `
    "Shared\GameSim\Bin\Debug\WintersGameSim.lib"
$engineLibrary = Join-Path $workspaceRoot `
    "Engine\Bin\Debug\WintersEngine.lib"
if (-not (Test-Path -LiteralPath $gameSimLibrary) -or
    -not (Test-Path -LiteralPath $engineLibrary)) {
    throw "The Debug GameSim or Engine import library is missing."
}

$outputDirectory = Join-Path $env:TEMP `
    "WintersGameRoomProjectileIntegrationProbe"
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$executablePath = Join-Path $outputDirectory `
    "GameRoomProjectileIntegrationProbe.exe"
$serverPublic = Join-Path $workspaceRoot "Server\Public"
$engineSdkInclude = Join-Path $workspaceRoot "EngineSDK\inc"
$flatBuffersInclude = Join-Path $workspaceRoot `
    "Engine\ThirdPartyLib\FlatBuffers\Inc"
$quotedObjects = ($serverObjects | ForEach-Object {
    '"{0}"' -f $_.FullName
}) -join " "

$compileCommand = (
    '"{0}" -no_logo -arch=x64 -host_arch=x64 >nul && ' +
    'cd /d "{1}" && ' +
    'cl.exe /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /MDd ' +
    '/D NOMINMAX /D WIN32_LEAN_AND_MEAN /D _DEBUG ' +
    '/I"{2}" /I"{3}" /I"{4}" /I"{5}" ' +
    '"{6}" {7} "{8}" "{9}" ws2_32.lib Mswsock.lib /Fe:"{10}"'
) -f $vsDevCmd, $outputDirectory, $workspaceRoot, $serverPublic, `
    $engineSdkInclude, $flatBuffersInclude, $sourcePath, $quotedObjects, `
    $gameSimLibrary, $engineLibrary, $executablePath

& $env:ComSpec /d /s /c $compileCommand
if ($LASTEXITCODE -ne 0) {
    throw "GameRoom projectile integration probe compilation failed with exit code $LASTEXITCODE."
}

$engineRuntime = Join-Path $workspaceRoot "EngineSDK\bin\Debug"
$previousPath = $env:PATH
try {
    $env:PATH = "$engineRuntime;$previousPath"
    & $executablePath
    if ($LASTEXITCODE -ne 0) {
        throw "GameRoom projectile integration probe failed with exit code $LASTEXITCODE."
    }
}
finally {
    $env:PATH = $previousPath
}
