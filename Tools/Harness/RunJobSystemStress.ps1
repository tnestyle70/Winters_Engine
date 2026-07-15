param(
    [ValidateSet("thread", "shell", "fiber", "all")]
    [string]$Mode = "all"
)

$ErrorActionPreference = "Stop"

$workspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$sourcePath = Join-Path $PSScriptRoot "JobSystemStress.cpp"
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
$runId = [Guid]::NewGuid().ToString("N")
$outputDirectory = Join-Path $env:TEMP "WintersJobSystemStress-$runId"
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$executablePath = Join-Path $outputDirectory "WintersJobSystemStress.exe"
$engineInclude = Join-Path $workspaceRoot "Engine\Include"
$enginePublic = Join-Path $workspaceRoot "Engine\Public"
$engineLibrary = Join-Path $workspaceRoot "EngineSDK\lib\Debug"
$engineBinary = Join-Path $workspaceRoot "Engine\Bin\Debug"
$clientBinary = Join-Path $workspaceRoot "Client\Bin\Debug"

$compileCommand = (
    '"{0}" -no_logo -arch=x64 -host_arch=x64 >nul && ' +
    'cd /d "{1}" && ' +
    'cl.exe /nologo /std:c++20 /utf-8 /EHsc /W4 /MDd ' +
    '/I"{2}" /I"{3}" "{4}" /Fe:"{5}" /link /LIBPATH:"{6}" WintersEngine.lib'
) -f $vsDevCmd, $outputDirectory, $engineInclude, $enginePublic, $sourcePath, `
    $executablePath, $engineLibrary

& $env:ComSpec /d /s /c $compileCommand
if ($LASTEXITCODE -ne 0) {
    throw "JobSystem stress compilation failed with exit code $LASTEXITCODE."
}

$previousPath = $env:PATH
try {
    # WintersEngine.dll has Assimp/DirectXTK/FMOD runtime imports deployed beside Client.
    $env:PATH = "$engineBinary;$clientBinary;$previousPath"
    & $executablePath $Mode
    if ($LASTEXITCODE -ne 0) {
        throw "JobSystem stress failed with exit code $LASTEXITCODE."
    }
}
finally {
    $env:PATH = $previousPath
}

Write-Host "Stress binary: $executablePath"
