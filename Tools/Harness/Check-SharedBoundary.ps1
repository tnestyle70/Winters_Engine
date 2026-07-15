# Check-SharedBoundary.ps1
# Shared/GameSim 의존성 경계 lint.
# 컴파일러가 강제하지 못하는 compass 규칙(Shared는 Engine/DX/ImGui/제품 코드를 include하지 않는다)을
# 텍스트 수준에서 검사한다. Phase 7F 어댑터(Shared/GameSim/Core/Ecs/*, Core/World/World.h)만
# Engine ECS 직접 include가 허용된다. 근거: .md/architecture/WINTERS_DEPENDENCY_MAP.md §3.
# 통과 exit 0 / 위반 exit 1 (위반 file:line 출력).

param(
    [string]$RepoRoot = (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent)
)

$sharedDir = Join-Path $RepoRoot "Shared"
if (-not (Test-Path $sharedDir)) {
    Write-Host "[SharedBoundary] SKIP - Shared directory not found at $sharedDir"
    exit 0
}

$violations = @()
$files = Get-ChildItem -Path $sharedDir -Recurse -Include *.h, *.cpp, *.hpp, *.inl -File

foreach ($file in $files) {
    $rel = $file.FullName.Substring($RepoRoot.Length + 1) -replace '\\', '/'
    $isAdapter = ($rel -like 'Shared/GameSim/Core/Ecs/*') -or ($rel -eq 'Shared/GameSim/Core/World/World.h')

    $hits = Select-String -Path $file.FullName -Pattern '#include\s+"(ECS/|Engine_Defines\.h|Client/|Server/|d3d11|dxgi|imgui)'
    foreach ($hit in $hits) {
        $line = $hit.Line.Trim()
        if ($isAdapter -and $line -match '"ECS/') { continue }
        $violations += ("{0}:{1}: {2}" -f $rel, $hit.LineNumber, $line)
    }
}

if ($violations.Count -gt 0) {
    Write-Host "[SharedBoundary] FAIL - $($violations.Count) violation(s):"
    $violations | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host "[SharedBoundary] PASS - Shared has no direct Engine/DX/ImGui/product includes (Phase 7F adapters excluded)."
exit 0
