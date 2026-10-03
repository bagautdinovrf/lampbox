#Requires -RunAsAdministrator
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$serviceName = 'MediaBoxPlayer'
$service = Get-Service -Name $serviceName -ErrorAction SilentlyContinue
if (-not $service) {
    Write-Output 'MediaBoxPlayer is not registered.'
    return
}
if ($service.Status -ne 'Stopped') {
    Stop-Service -Name $serviceName
    $service.WaitForStatus('Stopped', [TimeSpan]::FromSeconds(30))
}
& (Join-Path $env:SystemRoot 'System32\sc.exe') delete $serviceName
if ($LASTEXITCODE -ne 0) {
    throw "sc.exe delete failed with exit code $LASTEXITCODE."
}
Write-Output 'MediaBoxPlayer service removed. Player files, data directory, and token were preserved.'
