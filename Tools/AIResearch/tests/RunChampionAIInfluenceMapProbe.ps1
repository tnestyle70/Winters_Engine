param()

$ErrorActionPreference = "Stop"

$workspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$sourcePath = Join-Path $PSScriptRoot "ChampionAIInfluenceMapProbe.cpp"
$vsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

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

$outputDirectory = Join-Path $env:TEMP "WintersAIInfluenceMapProbe"
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$objectPath = Join-Path $outputDirectory "ChampionAIInfluenceMapProbe.obj"
$executablePath = Join-Path $outputDirectory "ChampionAIInfluenceMapProbe.exe"

$compileCommand = (
    '"{0}" -no_logo -arch=x64 -host_arch=x64 >nul && ' +
    'cl.exe /nologo /std:c++17 /utf-8 /EHsc /W4 /WX ' +
    '/I"{1}" "{2}" /Fo:"{3}" /Fe:"{4}"'
) -f $vsDevCmd, $workspaceRoot, $sourcePath, $objectPath, $executablePath

& $env:ComSpec /d /s /c $compileCommand
if ($LASTEXITCODE -ne 0) {
    throw "ChampionAIInfluenceMapProbe compilation failed with exit code $LASTEXITCODE."
}

& $executablePath
if ($LASTEXITCODE -ne 0) {
    throw "ChampionAIInfluenceMapProbe failed with exit code $LASTEXITCODE."
}
