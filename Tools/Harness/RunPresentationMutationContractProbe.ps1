$ErrorActionPreference = "Stop"

$workspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$sourcePath = Join-Path $PSScriptRoot `
    "PresentationMutationContractProbe.cpp"
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

$outputDirectory = Join-Path $env:TEMP `
    "WintersPresentationMutationContractProbe"
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$executablePath = Join-Path $outputDirectory `
    "PresentationMutationContractProbe.exe"
$clientPublic = Join-Path $workspaceRoot "Client\Public"

$compileCommand = (
    '"{0}" -no_logo -arch=x64 -host_arch=x64 >nul && ' +
    'cd /d "{1}" && ' +
    'cl.exe /nologo /std:c++20 /utf-8 /EHsc /W4 /WX ' +
    '/I"{2}" "{3}" /Fe:"{4}"'
) -f $vsDevCmd, $outputDirectory, $clientPublic, $sourcePath, `
    $executablePath

& $env:ComSpec /d /s /c $compileCommand
if ($LASTEXITCODE -ne 0) {
    throw "Presentation mutation contract compilation failed with exit code $LASTEXITCODE."
}

& $executablePath
if ($LASTEXITCODE -ne 0) {
    throw "Presentation mutation contract failed with exit code $LASTEXITCODE."
}
