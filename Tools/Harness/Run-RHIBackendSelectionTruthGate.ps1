[CmdletBinding()]
param(
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$exePath = Join-Path $repoRoot "Client\Bin\$Configuration\WintersGame.exe"
$workingDirectory = Split-Path -Parent $exePath

if (-not (Test-Path $exePath)) {
    throw "WintersGame.exe not found: $exePath"
}

function Read-ProbeReport {
    param([string]$Path)

    $result = @{}
    foreach ($line in Get-Content -LiteralPath $Path) {
        $parts = $line -split '=', 2
        if ($parts.Count -eq 2) {
            $result[$parts[0]] = $parts[1]
        }
    }
    return $result
}

function Invoke-ProbeCase {
    param(
        [string]$Name,
        [AllowEmptyString()][string]$EnvironmentRHI,
        [string[]]$Arguments,
        [bool]$ExpectReport,
        [string]$ExpectedSource,
        [string]$ExpectedRequested,
        [string]$ExpectedModule,
        [string]$ExpectedSelected,
        [string]$ExpectedStatus,
        [string]$ExpectedReason,
        [string]$ExpectedFallback,
        [string]$ExpectedFallbackReason,
        [bool]$ExpectZeroExit
    )

    $reportPath = Join-Path $env:TEMP (
        "winters_rhi_truth_{0}_{1}.txt" -f $Name, [guid]::NewGuid().ToString('N'))

    $previousRHI = $env:WINTERS_RHI
    $previousProbePath = $env:WINTERS_RHI_PROBE_PATH
    try {
        if ([string]::IsNullOrEmpty($EnvironmentRHI)) {
            Remove-Item Env:WINTERS_RHI -ErrorAction SilentlyContinue
        }
        else {
            $env:WINTERS_RHI = $EnvironmentRHI
        }
        $env:WINTERS_RHI_PROBE_PATH = $reportPath

        $startArgs = @{
            FilePath = $exePath
            WorkingDirectory = $workingDirectory
            PassThru = $true
            WindowStyle = 'Hidden'
        }
        if ($Arguments.Count -gt 0) {
            $startArgs.ArgumentList = $Arguments
        }

        $process = Start-Process @startArgs
        if (-not $process.WaitForExit(15000)) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            if (Test-Path -LiteralPath $reportPath) {
                Write-Host "[RHITruthGate] $Name report before timeout:"
                Get-Content -LiteralPath $reportPath | Write-Host
            }
            throw "$Name timed out after 15 seconds"
        }
        if ($ExpectZeroExit -and $process.ExitCode -ne 0) {
            throw "$Name expected exit 0, got $($process.ExitCode)"
        }
        if (-not $ExpectZeroExit -and $process.ExitCode -eq 0) {
            throw "$Name expected non-zero exit, got 0"
        }

        if ($ExpectReport) {
            if (-not (Test-Path $reportPath)) {
                throw "$Name did not create probe report"
            }
            $report = Read-ProbeReport $reportPath
            foreach ($key in @(
                'source', 'requested', 'module', 'selected',
                'status', 'reason', 'fallback', 'fallback_reason',
                'capability_backend', 'feature_tier',
                'supports_compute', 'supports_async_compute',
                'supports_bindless', 'supports_resource_transitions',
                'api_requires_explicit_states', 'frame_resource_slots',
                'buffer_transition_probe', 'buffer_transition_barriers',
                'buffer_transition_validation',
                'texture_transition_probe', 'texture_transition_barriers',
                'texture_transition_validation')) {
                if (-not $report.ContainsKey($key)) {
                    throw "$Name report missing key: $key"
                }
            }
            if ($report.source -ne $ExpectedSource -or
                $report.requested -ne $ExpectedRequested -or
                $report.module -ne $ExpectedModule -or
                $report.selected -ne $ExpectedSelected -or
                $report.status -ne $ExpectedStatus -or
                $report.reason -ne $ExpectedReason -or
                $report.fallback -ne $ExpectedFallback -or
                $report.fallback_reason -ne $ExpectedFallbackReason) {
                throw "$Name report mismatch: $($report | Out-String)"
            }

            $expectedCapabilities = switch ($ExpectedSelected) {
                'DX11' {
                    @{
                        capability_backend = 'DX11'
                        feature_tier = 'LegacyDX11'
                        supports_compute = 'no'
                        supports_async_compute = 'no'
                        supports_bindless = 'no'
                        supports_resource_transitions = 'no'
                        api_requires_explicit_states = 'no'
                        frame_resource_slots = '1'
                        buffer_transition_probe = 'pass'
                        buffer_transition_barriers = '0'
                        buffer_transition_validation = 'not_applicable'
                        texture_transition_probe = 'pass'
                        texture_transition_barriers = '0'
                        texture_transition_validation = 'not_applicable'
                    }
                }
                'DX12' {
                    @{
                        capability_backend = 'DX12'
                        feature_tier = 'ExplicitDesktop'
                        supports_compute = 'no'
                        supports_async_compute = 'no'
                        supports_bindless = 'no'
                        supports_resource_transitions = 'no'
                        api_requires_explicit_states = 'yes'
                        frame_resource_slots = '2'
                        buffer_transition_probe = 'pass'
                        buffer_transition_barriers = '2'
                        buffer_transition_validation = 'pass'
                        texture_transition_probe = 'pass'
                        texture_transition_barriers = '2'
                        texture_transition_validation = 'pass'
                    }
                }
                default {
                    @{
                        capability_backend = 'None'
                        feature_tier = 'None'
                        supports_compute = 'no'
                        supports_async_compute = 'no'
                        supports_bindless = 'no'
                        supports_resource_transitions = 'no'
                        api_requires_explicit_states = 'no'
                        frame_resource_slots = '0'
                        buffer_transition_probe = 'not_run'
                        buffer_transition_barriers = '0'
                        buffer_transition_validation = 'not_run'
                        texture_transition_probe = 'not_run'
                        texture_transition_barriers = '0'
                        texture_transition_validation = 'not_run'
                    }
                }
            }
            foreach ($key in $expectedCapabilities.Keys) {
                if ($report[$key] -ne $expectedCapabilities[$key]) {
                    throw "$Name capability mismatch for ${key}: expected $($expectedCapabilities[$key]), got $($report[$key])"
                }
            }
        }
        elseif (Test-Path $reportPath) {
            throw "$Name unexpectedly created a probe report"
        }

        Write-Host "[RHITruthGate] PASS $Name"
    }
    finally {
        if ($null -eq $previousRHI) {
            Remove-Item Env:WINTERS_RHI -ErrorAction SilentlyContinue
        }
        else {
            $env:WINTERS_RHI = $previousRHI
        }
        if ($null -eq $previousProbePath) {
            Remove-Item Env:WINTERS_RHI_PROBE_PATH -ErrorAction SilentlyContinue
        }
        else {
            $env:WINTERS_RHI_PROBE_PATH = $previousProbePath
        }
        Remove-Item -LiteralPath $reportPath -Force -ErrorAction SilentlyContinue
    }
}

Invoke-ProbeCase -Name 'default_auto' -EnvironmentRHI '' -Arguments @() `
    -ExpectReport $true -ExpectedSource 'default' -ExpectedRequested 'Auto' `
    -ExpectedModule 'DX11' -ExpectedSelected 'DX11' -ExpectedStatus 'success' `
    -ExpectedReason 'device_created' -ExpectedFallback 'no' `
    -ExpectedFallbackReason 'none' -ExpectZeroExit $true

Invoke-ProbeCase -Name 'env_dx11' -EnvironmentRHI 'dx11' -Arguments @() `
    -ExpectReport $true -ExpectedSource 'environment' -ExpectedRequested 'DX11' `
    -ExpectedModule 'DX11' -ExpectedSelected 'DX11' -ExpectedStatus 'success' `
    -ExpectedReason 'device_created' -ExpectedFallback 'no' `
    -ExpectedFallbackReason 'none' -ExpectZeroExit $true

Invoke-ProbeCase -Name 'env_dx12' -EnvironmentRHI 'dx12' -Arguments @() `
    -ExpectReport $true -ExpectedSource 'environment' -ExpectedRequested 'DX12' `
    -ExpectedModule 'DX12' -ExpectedSelected 'DX12' -ExpectedStatus 'success' `
    -ExpectedReason 'device_created' -ExpectedFallback 'no' `
    -ExpectedFallbackReason 'none' -ExpectZeroExit $true

Invoke-ProbeCase -Name 'env_vulkan' -EnvironmentRHI 'vulkan' -Arguments @() `
    -ExpectReport $true -ExpectedSource 'environment' -ExpectedRequested 'Vulkan' `
    -ExpectedModule 'None' -ExpectedSelected 'None' `
    -ExpectedStatus 'module_not_registered' `
    -ExpectedReason 'backend_module_not_registered' -ExpectedFallback 'no' `
    -ExpectedFallbackReason 'none' -ExpectZeroExit $false

Invoke-ProbeCase -Name 'cli_precedence' -EnvironmentRHI 'dx11' -Arguments @('--rhi=dx12') `
    -ExpectReport $true -ExpectedSource 'command-line' -ExpectedRequested 'DX12' `
    -ExpectedModule 'DX12' -ExpectedSelected 'DX12' -ExpectedStatus 'success' `
    -ExpectedReason 'device_created' -ExpectedFallback 'no' `
    -ExpectedFallbackReason 'none' -ExpectZeroExit $true

Invoke-ProbeCase -Name 'empty_cli' -EnvironmentRHI 'dx11' -Arguments @('--rhi=') `
    -ExpectReport $false -ExpectedSource '' -ExpectedRequested '' `
    -ExpectedModule '' -ExpectedSelected '' -ExpectedStatus '' -ExpectedReason '' `
    -ExpectedFallback '' -ExpectedFallbackReason '' -ExpectZeroExit $false

Invoke-ProbeCase -Name 'duplicate_cli' -EnvironmentRHI '' `
    -Arguments @('--rhi=dx11', '--rhi=dx12') -ExpectReport $false `
    -ExpectedSource '' -ExpectedRequested '' -ExpectedModule '' `
    -ExpectedSelected '' -ExpectedStatus '' -ExpectedReason '' `
    -ExpectedFallback '' -ExpectedFallbackReason '' -ExpectZeroExit $false

Invoke-ProbeCase -Name 'invalid_env' -EnvironmentRHI 'd3d11' -Arguments @() `
    -ExpectReport $false -ExpectedSource '' -ExpectedRequested '' `
    -ExpectedModule '' -ExpectedSelected '' -ExpectedStatus '' -ExpectedReason '' `
    -ExpectedFallback '' -ExpectedFallbackReason '' -ExpectZeroExit $false

Write-Host '[RHITruthGate] ALL PASS'
