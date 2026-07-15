param(
    [string]$HostName = "127.0.0.1",
    [ValidateRange(1, 65535)]
    [int]$Port = 9000
)

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

$outputDirectory = Join-Path $env:TEMP "WintersUdpF5SessionSmoke_$PID"
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$executablePath = Join-Path $outputDirectory "UdpF5SessionSmoke.exe"
$harnessSource = Join-Path $PSScriptRoot "UdpF5SessionSmoke.cpp"
$clientSource = Join-Path $workspaceRoot `
    "Client\Private\Network\Client\UdpClient.cpp"
$reliabilitySource = Join-Path $workspaceRoot `
    "Shared\Network\UdpReliabilityChannel.cpp"
$clientUdpPrivate = Join-Path $workspaceRoot `
    "Client\Private\Network\Client"
$engineInclude = Join-Path $workspaceRoot "Engine\Include"

$compileCommand = (
    '"{0}" -no_logo -arch=x64 -host_arch=x64 >nul && ' +
    'cd /d "{1}" && ' +
    'cl.exe /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /MDd ' +
    '/D NOMINMAX /D WIN32_LEAN_AND_MEAN ' +
    '/I"{2}" /I"{3}" /I"{4}" ' +
    '"{5}" "{6}" "{7}" ' +
    '/Fe:"{8}" /link ws2_32.lib bcrypt.lib'
) -f $vsDevCmd, $outputDirectory, $workspaceRoot, $clientUdpPrivate, `
    $engineInclude, $harnessSource, $clientSource, $reliabilitySource, `
    $executablePath

& $env:ComSpec /d /s /c $compileCommand
if ($LASTEXITCODE -ne 0) {
    throw "UDP F5 smoke compilation failed with exit code $LASTEXITCODE."
}

& $executablePath $HostName $Port
if ($LASTEXITCODE -ne 0) {
    throw "UDP F5 smoke failed with exit code $LASTEXITCODE."
}
