$ErrorActionPreference = "Stop"

$workspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
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

$outputDirectory = Join-Path $env:TEMP "WintersUdpLoopbackHarness_$PID"
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$executablePath = Join-Path $outputDirectory "UdpLoopbackHarness.exe"
$harnessSource = Join-Path $PSScriptRoot "UdpLoopbackHarness.cpp"
$serverSource = Join-Path $workspaceRoot `
    "Server\Private\Network\UdpIocpCore.cpp"
$clientSource = Join-Path $workspaceRoot `
    "Client\Private\Network\Client\UdpClient.cpp"
$reliabilitySource = Join-Path $workspaceRoot `
    "Shared\Network\UdpReliabilityChannel.cpp"
$serverPublic = Join-Path $workspaceRoot "Server\Public"
$clientUdpPrivate = Join-Path $workspaceRoot `
    "Client\Private\Network\Client"
$engineInclude = Join-Path $workspaceRoot "Engine\Include"

$compileCommand = (
    '"{0}" -no_logo -arch=x64 -host_arch=x64 >nul && ' +
    'cd /d "{1}" && ' +
    'cl.exe /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /MDd ' +
    '/D NOMINMAX /D WIN32_LEAN_AND_MEAN ' +
    '/I"{2}" /I"{3}" /I"{4}" /I"{5}" ' +
    '"{6}" "{7}" "{8}" "{9}" ' +
    '/Fe:"{10}" /link ws2_32.lib bcrypt.lib'
) -f $vsDevCmd, $outputDirectory, $workspaceRoot, $serverPublic, `
    $clientUdpPrivate, $engineInclude, $harnessSource, $serverSource, `
    $clientSource, $reliabilitySource, $executablePath

& $env:ComSpec /d /s /c $compileCommand
if ($LASTEXITCODE -ne 0) {
    throw "UDP loopback harness compilation failed with exit code $LASTEXITCODE."
}

& $executablePath
if ($LASTEXITCODE -ne 0) {
    throw "UDP loopback harness failed with exit code $LASTEXITCODE."
}
