param(
    [string]$FixturePath = ""
)

$ErrorActionPreference = "Stop"

$workspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$sourcePath = Join-Path $PSScriptRoot "ChampionAIResearchTypesProbe.cpp"
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

$outputDirectory = Join-Path $env:TEMP "WintersAIResearchTypesProbe"
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$objectPath = Join-Path $outputDirectory "ChampionAIResearchTypesProbe.obj"
$executablePath = Join-Path $outputDirectory "ChampionAIResearchTypesProbe.exe"
$engineInclude = Join-Path $workspaceRoot "Engine\Include"
$enginePublic = Join-Path $workspaceRoot "Engine\Public"

$compileCommand = (
    '"{0}" -no_logo -arch=x64 -host_arch=x64 >nul && ' +
    'cl.exe /nologo /std:c++17 /utf-8 /EHsc /W4 /WX ' +
    '/I"{1}" /I"{2}" /I"{3}" "{4}" /Fo:"{5}" /Fe:"{6}"'
) -f $vsDevCmd, $workspaceRoot, $engineInclude, $enginePublic, $sourcePath, $objectPath, $executablePath

& $env:ComSpec /d /s /c $compileCommand
if ($LASTEXITCODE -ne 0) {
    throw "ChampionAIResearchTypesProbe compilation failed with exit code $LASTEXITCODE."
}

if ($FixturePath) {
    $fixtureParent = Split-Path -Parent $FixturePath
    if ($fixtureParent) {
        New-Item -ItemType Directory -Force -Path $fixtureParent | Out-Null
    }
    & $executablePath --write-fixture $FixturePath
}
else {
    & $executablePath
}
if ($LASTEXITCODE -ne 0) {
    throw "ChampionAIResearchTypesProbe failed with exit code $LASTEXITCODE."
}
