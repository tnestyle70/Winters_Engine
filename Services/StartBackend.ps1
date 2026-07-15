# Winters backend bring-up: docker infra(postgres/redis/kafka) + auth/profile/shop Go services.
# Usage: powershell -ExecutionPolicy Bypass -File Services\StartBackend.ps1
# Note: fresh pgdata volume requires migrations once — see Services/Makefile `migrate`
#       (or: Get-Content migrations\*.up.sql | docker exec -i winters-postgres psql -U winters -d winters)
$ErrorActionPreference = 'Stop'
$servicesDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $servicesDir

docker compose up -d

for ($i = 0; $i -lt 30; $i++) {
    $health = docker inspect -f '{{.State.Health.Status}}' winters-postgres 2>$null
    if ($health -eq 'healthy') { break }
    Start-Sleep -Seconds 2
}
if ($health -ne 'healthy') { Write-Error 'winters-postgres did not become healthy'; exit 1 }

# S030 passwordless ID auth (/auth/id/*) is gated behind this dev-only flag.
Start-Process powershell -ArgumentList '-NoExit', '-Command', "`$env:WINTERS_DEV_AUTH_ENABLED='true'; Set-Location '$servicesDir'; go run ./cmd/auth"
Start-Process powershell -ArgumentList '-NoExit', '-Command', "Set-Location '$servicesDir'; go run ./cmd/profile"
Start-Process powershell -ArgumentList '-NoExit', '-Command', "Set-Location '$servicesDir'; go run ./cmd/shop"

Write-Host 'launching: auth :8081 / profile :8084 / shop :8086 (separate windows)'
