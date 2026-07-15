param()

$ErrorActionPreference = "Stop"

$workspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$probeDirectory = Join-Path $env:TEMP "WintersAiEpisodeV1CodecProbe"
New-Item -ItemType Directory -Force -Path $probeDirectory | Out-Null

$capturePath = Join-Path $probeDirectory "decision_trace_v1.bin"
$firstOutput = Join-Path $probeDirectory "episode_first.jsonl"
$secondOutput = Join-Path $probeDirectory "episode_second.jsonl"
$metadataPath = Join-Path $workspaceRoot `
    "Tools\AIResearch\fixtures\ai_episode_v1_pending_metadata.json"

& powershell -NoProfile -ExecutionPolicy Bypass `
    -File (Join-Path $PSScriptRoot "RunChampionAIResearchTypesProbe.ps1") `
    -FixturePath $capturePath
if ($LASTEXITCODE -ne 0) {
    throw "AiDecisionTraceV1 C++ fixture generation failed."
}

foreach ($outputPath in @($firstOutput, $secondOutput)) {
    & python -B (Join-Path $workspaceRoot `
        "Tools\AIResearch\ExportAiEpisodeV1.py") `
        --input $capturePath `
        --metadata $metadataPath `
        --output $outputPath
    if ($LASTEXITCODE -ne 0) {
        throw "AiEpisodeV1 export failed: $outputPath"
    }
}

$firstHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $firstOutput).Hash
$secondHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $secondOutput).Hash
if ($firstHash -ne $secondHash) {
    throw "Canonical AiEpisodeV1 export is not byte deterministic."
}

& python -B (Join-Path $workspaceRoot `
    "Tools\AIResearch\ValidateAiEpisode.py") `
    --input $firstOutput
if ($LASTEXITCODE -ne 0) {
    throw "Pending AiEpisodeV1 raw validation failed."
}

& python -B (Join-Path $workspaceRoot `
    "Tools\AIResearch\ValidateAiEpisode.py") `
    --input $firstOutput `
    --promotion
if ($LASTEXITCODE -eq 0) {
    throw "Pending privileged AiEpisodeV1 unexpectedly passed promotion."
}

# Finish on a successful native command so the expected promotion rejection
# cannot leak a non-zero LASTEXITCODE to the caller.
& python -B (Join-Path $workspaceRoot `
    "Tools\AIResearch\ValidateAiEpisode.py") `
    --input $firstOutput
if ($LASTEXITCODE -ne 0) {
    throw "Final AiEpisodeV1 raw validation failed."
}

Write-Host (
    "[AiEpisodeV1Codec] PASS: C++ bytes -> deterministic JSONL; " +
    "pending/privileged promotion rejection confirmed; sha256=$firstHash"
)
