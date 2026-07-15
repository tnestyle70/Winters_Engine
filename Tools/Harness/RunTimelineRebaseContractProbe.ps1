$ErrorActionPreference = "Stop"

$workspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$sourcePath = Join-Path $PSScriptRoot "TimelineRebaseContractProbe.cpp"
$inputBufferSource = Join-Path $workspaceRoot `
    "Client\Private\Network\Client\ClientInputBuffer.cpp"
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

$outputDirectory = Join-Path $env:TEMP "WintersTimelineRebaseContractProbe"
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$executablePath = Join-Path $outputDirectory "TimelineRebaseContractProbe.exe"
$clientPublic = Join-Path $workspaceRoot "Client\Public"
$engineInclude = Join-Path $workspaceRoot "Engine\Include"
$enginePublic = Join-Path $workspaceRoot "Engine\Public"
$flatBuffersInclude = Join-Path $workspaceRoot `
    "Engine\ThirdPartyLib\FlatBuffers\Inc"

$compileCommand = (
    '"{0}" -no_logo -arch=x64 -host_arch=x64 >nul && ' +
    'cd /d "{1}" && ' +
    'cl.exe /nologo /std:c++20 /utf-8 /EHsc /W4 /WX ' +
    '/D NOMINMAX /D WIN32_LEAN_AND_MEAN ' +
    '/I"{2}" /I"{3}" /I"{4}" /I"{5}" /I"{6}" ' +
    '"{7}" "{8}" /Fe:"{9}"'
) -f $vsDevCmd, $outputDirectory, $workspaceRoot, $clientPublic, `
    $engineInclude, $enginePublic, $flatBuffersInclude, $sourcePath, `
    $inputBufferSource, $executablePath

& $env:ComSpec /d /s /c $compileCommand
if ($LASTEXITCODE -ne 0) {
    throw "TimelineRebaseContractProbe compilation failed with exit code $LASTEXITCODE."
}

& $executablePath
if ($LASTEXITCODE -ne 0) {
    throw "TimelineRebaseContractProbe failed with exit code $LASTEXITCODE."
}
