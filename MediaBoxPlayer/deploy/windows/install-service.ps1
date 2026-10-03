#Requires -RunAsAdministrator
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $ExecutablePath,
    [string] $DataDirectory = (Join-Path $env:ProgramData 'MediaBox\Player'),
    [ValidateRange(1, 65535)]
    [int] $Port = 17655,
    [switch] $Start
)

$ErrorActionPreference = 'Stop'
$serviceName = 'MediaBoxPlayer'
$playerExecutable = (Resolve-Path -LiteralPath $ExecutablePath).ProviderPath
if (-not (Test-Path -LiteralPath $playerExecutable -PathType Leaf)) {
    throw 'ExecutablePath must name the installed MediaBoxPlayer.exe file.'
}
$playerDataDirectory = [IO.Path]::GetFullPath($DataDirectory)
if ($playerDataDirectory -eq [IO.Path]::GetPathRoot($playerDataDirectory)) {
    throw 'DataDirectory must be a dedicated directory, not a drive or share root.'
}
$playerDataDirectory = $playerDataDirectory.TrimEnd([char[]]@('\', '/'))
if ($playerDataDirectory.Contains('"') -or $playerExecutable.Contains('"')) {
    throw 'Paths must not contain double quotes.'
}
if (Get-Service -Name $serviceName -ErrorAction SilentlyContinue) {
    throw 'MediaBoxPlayer is already registered. Stop and uninstall its service before reinstalling. Data is preserved.'
}

# Keep the bearer token private to the service and local administrators.
# Reuse only a private directory; never replace ACLs on an existing shared directory.
if (Test-Path -LiteralPath $playerDataDirectory) {
    $directory = Get-Item -LiteralPath $playerDataDirectory
    if (-not $directory.PSIsContainer -or ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'DataDirectory must be a normal directory, not a file or reparse point.'
    }
    $existingAcl = Get-Acl -LiteralPath $playerDataDirectory
    if (-not $existingAcl.AreAccessRulesProtected) {
        throw 'Existing DataDirectory must have private ACLs. Use a new dedicated directory or configure its ACLs as documented.'
    }
    $allowedSids = @('S-1-5-18', 'S-1-5-19', 'S-1-5-32-544')
    foreach ($rule in $existingAcl.Access) {
        $sid = $rule.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value
        if ($rule.AccessControlType -eq 'Allow' -and $sid -notin $allowedSids) {
            throw 'Existing DataDirectory grants access beyond SYSTEM, LocalService, and Administrators. Configure private ACLs before installing.'
        }
    }
} else {
    New-Item -ItemType Directory -Path $playerDataDirectory -Force | Out-Null
    $privateAcl = New-Object Security.AccessControl.DirectorySecurity
    $privateAcl.SetAccessRuleProtection($true, $false)
    $inheritance = [Security.AccessControl.InheritanceFlags]'ContainerInherit, ObjectInherit'
    $propagation = [Security.AccessControl.PropagationFlags]::None
    foreach ($sidText in @('S-1-5-18', 'S-1-5-32-544', 'S-1-5-19')) {
        $sid = New-Object Security.Principal.SecurityIdentifier($sidText)
        $rights = if ($sidText -eq 'S-1-5-19') { 'Modify' } else { 'FullControl' }
        $rule = New-Object Security.AccessControl.FileSystemAccessRule($sid, $rights, $inheritance, $propagation, 'Allow')
        $privateAcl.AddAccessRule($rule)
    }
    Set-Acl -LiteralPath $playerDataDirectory -AclObject $privateAcl
}

function Invoke-ServiceControl {
    param([string[]] $ScArguments)
    # Build the native command line explicitly, preserving the quotes inside binPath
    # on Windows PowerShell 5.1 as well as PowerShell 7.
    $quotedArguments = foreach ($argument in $ScArguments) {
        $escaped = [regex]::Replace($argument, '(\\*)"', '$1$1\"')
        $escaped = [regex]::Replace($escaped, '(\\+)$', '$1$1')
        '"' + $escaped + '"'
    }
    $startInfo = New-Object Diagnostics.ProcessStartInfo
    $startInfo.FileName = Join-Path $env:SystemRoot 'System32\sc.exe'
    $startInfo.Arguments = $quotedArguments -join ' '
    $startInfo.UseShellExecute = $false
    $process = [Diagnostics.Process]::Start($startInfo)
    $process.WaitForExit()
    $exitCode = $process.ExitCode
    $process.Dispose()
    if ($exitCode -ne 0) {
        throw "sc.exe failed with exit code $exitCode."
    }
}

$serviceCommand = '"{0}" --service --data-dir "{1}" --listen 127.0.0.1 --port {2}' -f $playerExecutable, $playerDataDirectory, $Port
Invoke-ServiceControl -ScArguments @('create', $serviceName, 'binPath=', $serviceCommand, 'start=', 'auto', 'obj=', 'NT AUTHORITY\LocalService', 'DisplayName=', 'MediaBoxPlayer')
Invoke-ServiceControl -ScArguments @('description', $serviceName, 'MediaBox background audio engine; controlled through its authenticated local API.')
Invoke-ServiceControl -ScArguments @('failure', $serviceName, 'reset=', '86400', 'actions=', 'restart/5000/restart/15000/restart/60000')
Invoke-ServiceControl -ScArguments @('failureflag', $serviceName, '1')
if ($Start) {
    Start-Service -Name $serviceName
}
Write-Output "MediaBoxPlayer registered as LocalService. Data directory: $playerDataDirectory"
