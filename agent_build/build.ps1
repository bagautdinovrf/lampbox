#Requires -Version 5.1
<#
.SYNOPSIS
Configure, build and test Lampbox with the newest installed Qt 6 MSVC kit.
.EXAMPLE
powershell -ExecutionPolicy Bypass -File .\agent_build\build.ps1
.EXAMPLE
.\agent_build\build.ps1 -Configuration Debug -Clean -Jobs 4
.EXAMPLE
.\agent_build\build.ps1 -Deploy
#>
[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',

    [string]$QtRoot = 'C:\Qt',

    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$QtVersion,

    [ValidatePattern('^msvc\d+_64$')]
    [string]$QtKit,

    [ValidateRange(1, 256)]
    [int]$Jobs = [Math]::Max(1, [Math]::Min(8, [Environment]::ProcessorCount)),

    [string[]]$CMakeArguments = @(),

    [switch]$ConfigureOnly,
    [switch]$SkipTests,
    [switch]$Clean,
    [switch]$Deploy
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$agentDirectory = [IO.Path]::GetFullPath($PSScriptRoot)
$sourceDirectory = [IO.Path]::GetFullPath((Join-Path $agentDirectory '..'))
$buildDirectory = Join-Path $agentDirectory 'build'
$deployDirectory = Join-Path (Join-Path $agentDirectory 'deploy') $Configuration
$runId = '{0}-{1}' -f (Get-Date -Format 'yyyyMMdd-HHmmss'), ([Guid]::NewGuid().ToString('N').Substring(0, 8))
$logDirectory = Join-Path (Join-Path $agentDirectory 'logs') $runId
$null = New-Item -ItemType Directory -Path $logDirectory -Force
$originalEnvironment = [Environment]::GetEnvironmentVariables('Process')
$runInformation = [ordered]@{
    Started = (Get-Date).ToString('o')
    Configuration = $Configuration
    SourceDirectory = $sourceDirectory
    BuildDirectory = $buildDirectory
    LogDirectory = $logDirectory
    Status = 'Running'
}

function Assert-File {
    param([string]$Path, [string]$Description)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Description was not found: $Path"
    }
    return [IO.Path]::GetFullPath($Path)
}

function Remove-BuildDirectory {
    # Never recursively remove arbitrary input or follow a junction/symlink.
    $expectedPath = [IO.Path]::GetFullPath((Join-Path $agentDirectory 'build'))
    $targetPath = [IO.Path]::GetFullPath($buildDirectory)
    $parentPrefix = $agentDirectory.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $targetPath.Equals($expectedPath, [StringComparison]::OrdinalIgnoreCase) -or
        -not $targetPath.StartsWith($parentPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean a directory outside agent_build/build: $targetPath"
    }
    if (-not (Test-Path -LiteralPath $targetPath)) {
        return
    }
    $directory = Get-Item -LiteralPath $targetPath -Force
    if (-not $directory.PSIsContainer -or
        ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Refusing to clean a non-directory or reparse point: $targetPath"
    }
    $reparsePoint = Get-ChildItem -LiteralPath $targetPath -Recurse -Force |
        Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint } |
        Select-Object -First 1
    if ($reparsePoint) {
        throw "Refusing to clean a tree containing a junction or symlink: $($reparsePoint.FullName)"
    }
    Remove-Item -LiteralPath $targetPath -Recurse -Force
}

function Get-QtInstallation {
    $resolvedQtRoot = (Resolve-Path -LiteralPath $QtRoot).ProviderPath
    $versions = @(Get-ChildItem -LiteralPath $resolvedQtRoot -Directory |
        Where-Object { $_.Name -match '^6\.\d+\.\d+$' } |
        Sort-Object { [Version]$_.Name } -Descending)
    if ($QtVersion) {
        $versions = @($versions | Where-Object { $_.Name -eq $QtVersion })
    }
    if ($versions.Count -eq 0) {
        throw "No installed Qt 6 version was found in $resolvedQtRoot."
    }

    # Do not silently fall back to an older Qt version if the newest lacks its kit.
    $latestVersion = $versions[0]
    $kits = @(Get-ChildItem -LiteralPath $latestVersion.FullName -Directory |
        Where-Object { $_.Name -match '^msvc\d+_64$' } |
        Sort-Object { [int]($_.Name -replace '^msvc(\d+)_64$', '$1') } -Descending)
    if ($QtKit) {
        $kits = @($kits | Where-Object { $_.Name -eq $QtKit })
    }
    $kits = @($kits | Where-Object {
        (Test-Path -LiteralPath (Join-Path $_.FullName 'lib\cmake\Qt6\Qt6Config.cmake') -PathType Leaf) -and
        (Test-Path -LiteralPath (Join-Path $_.FullName 'bin\Qt6Core.dll') -PathType Leaf)
    })
    if ($kits.Count -eq 0) {
        throw "Qt $($latestVersion.Name) has no complete MSVC x64 kit. Install msvc2022_64 or specify -QtVersion/-QtKit."
    }
    return [PSCustomObject]@{
        Version = $latestVersion.Name
        Kit = $kits[0].Name
        Path = $kits[0].FullName
        ToolsPath = Join-Path $resolvedQtRoot 'Tools'
    }
}

function Import-MsvcEnvironment {
    $installerDirectory = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer'
    $vswhere = Join-Path $installerDirectory 'vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        $command = Get-Command 'vswhere.exe' -CommandType Application -ErrorAction SilentlyContinue
        if ($command) {
            $vswhere = $command.Source
        } else {
            throw 'vswhere.exe was not found. Install Visual Studio 2022 or its Build Tools with Desktop development with C++.'
        }
    }
    # C++23 requires a modern toolset; VS 2022 can also use older compatible MSVC Qt kits.
    $visualStudio = @(& $vswhere -latest -products '*' -version '[17.0,)' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
    if ($LASTEXITCODE -ne 0 -or $visualStudio.Count -eq 0 -or [string]::IsNullOrWhiteSpace($visualStudio[0])) {
        throw 'Visual Studio 2022+ with the MSVC x64 tools and Windows SDK was not found.'
    }
    $visualStudioPath = $visualStudio[0].Trim()
    $vsDevCmd = Assert-File (Join-Path $visualStudioPath 'Common7\Tools\VsDevCmd.bat') 'VsDevCmd.bat'
    $commandProcessor = Assert-File $env:ComSpec 'cmd.exe'

    # Invoke cmd directly to preserve a quoted batch path on Windows PowerShell 5.1.
    # Only the environment is imported; it is restored in the outer finally block.
    $startInfo = New-Object Diagnostics.ProcessStartInfo
    $startInfo.FileName = $commandProcessor
    $startInfo.Arguments = '/d /s /c ""' + $vsDevCmd + '" -no_logo -arch=x64 -host_arch=x64 && set"'
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $startInfo
    try {
        $null = $process.Start()
        $outputTask = $process.StandardOutput.ReadToEndAsync()
        $errorTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $output = $outputTask.GetAwaiter().GetResult()
        $errorOutput = $errorTask.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) {
            throw "VsDevCmd failed with exit code $($process.ExitCode). $errorOutput $output"
        }
        foreach ($line in ($output -split '\r?\n')) {
            $separator = $line.IndexOf('=')
            if ($separator -gt 0) {
                [Environment]::SetEnvironmentVariable($line.Substring(0, $separator), $line.Substring($separator + 1), 'Process')
            }
        }
    } finally {
        $process.Dispose()
    }
    $compiler = Get-Command 'cl.exe' -CommandType Application -ErrorAction Stop
    return [PSCustomObject]@{
        VisualStudio = $visualStudioPath
        Compiler = $compiler.Source
    }
}

function Test-ConfigureRefreshRequired {
    param([string]$CachePath, [string]$QtPath, [string]$CompilerPath, [string]$Generator = 'Ninja')
    if (-not (Test-Path -LiteralPath $CachePath -PathType Leaf)) {
        return $false
    }
    $cachedValues = @{}
    foreach ($line in (Get-Content -LiteralPath $CachePath)) {
        if ($line -match '^(Qt6_DIR|CMAKE_CXX_COMPILER|CMAKE_GENERATOR):[^=]+=(.*)$') {
            $cachedValues[$Matches[1]] = $Matches[2]
        }
    }
    if ($cachedValues['CMAKE_GENERATOR'] -ne $Generator) {
        return $true
    }
    $expectedPaths = @{
        Qt6_DIR = Join-Path $QtPath 'lib\cmake\Qt6'
        CMAKE_CXX_COMPILER = $CompilerPath
    }
    foreach ($name in $expectedPaths.Keys) {
        if (-not $cachedValues.ContainsKey($name) -or [string]::IsNullOrWhiteSpace($cachedValues[$name])) {
            return $true
        }
        $cachedPath = [IO.Path]::GetFullPath($cachedValues[$name]).TrimEnd('\', '/')
        $expectedPath = [IO.Path]::GetFullPath($expectedPaths[$name]).TrimEnd('\', '/')
        if (-not $cachedPath.Equals($expectedPath, [StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
    }
    return $false
}

function Invoke-LoggedCommand {
    param([string]$Name, [string]$FilePath, [string[]]$Arguments)
    $logPath = Join-Path $logDirectory ($Name + '.log')
    Write-Host "`n[$Name] $FilePath $($Arguments -join ' ')"
    Write-Host "Log: $logPath"
    # Windows PowerShell wraps native stderr in ErrorRecords. Capture it in the log,
    # and use the native exit code rather than PowerShell's pipeline status.
    $previousErrorAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $global:LASTEXITCODE = $null
        & $FilePath @Arguments 2>&1 |
            ForEach-Object {
                if ($_ -is [Management.Automation.ErrorRecord]) { $_.Exception.Message }
                else { $_.ToString() }
            } |
            Tee-Object -FilePath $logPath -ErrorAction Stop
        $commandExitCode = $global:LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorAction
    }
    if ($null -eq $commandExitCode) {
        throw "$Name could not start. See $logPath"
    }
    if ($commandExitCode -ne 0) {
        throw "$Name failed with exit code $commandExitCode. See $logPath"
    }
}

try {
    if ($ConfigureOnly -and $Deploy) {
        throw '-Deploy requires a build and cannot be used with -ConfigureOnly.'
    }
    $null = Assert-File (Join-Path $sourceDirectory 'CMakeLists.txt') 'Root CMakeLists.txt'
    $qt = Get-QtInstallation
    $cmake = Assert-File (Join-Path $qt.ToolsPath 'CMake_64\bin\cmake.exe') 'Qt Tools CMake'
    $ctest = Assert-File (Join-Path $qt.ToolsPath 'CMake_64\bin\ctest.exe') 'Qt Tools CTest'
    $ninja = Assert-File (Join-Path $qt.ToolsPath 'Ninja\ninja.exe') 'Qt Tools Ninja'
    $msvc = Import-MsvcEnvironment
    $env:Path = (Join-Path $qt.Path 'bin') + ';' + (Split-Path -Parent $cmake) + ';' + (Split-Path -Parent $ninja) + ';' + $env:Path

    $runInformation.QtVersion = $qt.Version
    $runInformation.QtKit = $qt.Kit
    $runInformation.QtPath = $qt.Path
    $runInformation.VisualStudio = $msvc.VisualStudio
    $runInformation.Compiler = $msvc.Compiler
    Write-Host "Qt: $($qt.Version) / $($qt.Kit)"
    Write-Host "MSVC: $($msvc.Compiler)"
    Write-Host "Configuration: $Configuration; parallel jobs: $Jobs"

    if ($Clean) {
        Remove-BuildDirectory
    }
    # Preserve generated files during ordinary incremental builds. Refresh the
    # cache only when it would pin a different Qt installation, compiler or generator.
    $configureArguments = @(
        '-S', $sourceDirectory, '-B', $buildDirectory, '-G', 'Ninja',
        "-DCMAKE_BUILD_TYPE=$Configuration",
        "-DCMAKE_MAKE_PROGRAM=$ninja",
        "-DCMAKE_C_COMPILER=$($msvc.Compiler)",
        "-DCMAKE_CXX_COMPILER=$($msvc.Compiler)",
        "-DCMAKE_PREFIX_PATH=$($qt.Path)",
        "-DQt6_ROOT=$($qt.Path)",
        "-DQt6_DIR=$(Join-Path $qt.Path 'lib\cmake\Qt6')",
        '-DBUILD_TESTING=ON'
    ) + $CMakeArguments
    if (Test-ConfigureRefreshRequired (Join-Path $buildDirectory 'CMakeCache.txt') $qt.Path $msvc.Compiler) {
        $configureArguments = @('--fresh') + $configureArguments
        Write-Host 'Qt, MSVC or the CMake generator changed; refreshing the CMake cache.'
    }
    Invoke-LoggedCommand 'configure' $cmake $configureArguments
    if (-not $ConfigureOnly) {
        Invoke-LoggedCommand 'build' $cmake @('--build', $buildDirectory, '--config', $Configuration, '--parallel', "$Jobs")
        if (-not $SkipTests) {
            Invoke-LoggedCommand 'test' $ctest @('--test-dir', $buildDirectory, '-C', $Configuration,
                '--output-on-failure', '--no-tests=error', '--parallel', "$Jobs")
        }
        if ($Deploy) {
            Invoke-LoggedCommand 'deploy' $cmake @('--install', $buildDirectory, '--config', $Configuration, '--prefix', $deployDirectory)
            $runInformation.DeployDirectory = $deployDirectory
            Write-Host "Deployed application: $(Join-Path $deployDirectory 'bin\lampbox.exe')"
        }
        Write-Host "Built application: $(Join-Path $buildDirectory 'bin\lampbox.exe')"
    }
    $runInformation.Status = 'Succeeded'
    Write-Host "`nCompleted successfully. Logs: $logDirectory"
} catch {
    $runInformation.Status = 'Failed'
    $runInformation.Error = $_.Exception.Message
    throw
} finally {
    $runInformation.Finished = (Get-Date).ToString('o')
    # Restore both overwritten variables and variables created by VsDevCmd.
    foreach ($name in @([Environment]::GetEnvironmentVariables('Process').Keys)) {
        if (-not $originalEnvironment.Contains($name)) {
            [Environment]::SetEnvironmentVariable($name, $null, 'Process')
        }
    }
    foreach ($name in $originalEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $originalEnvironment[$name], 'Process')
    }
    $runInformation | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $logDirectory 'run.json') -Encoding UTF8
}
