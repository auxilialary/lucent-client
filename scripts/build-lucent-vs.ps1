# Build the configuration saved by Visual Studio, without configuring another tree.
[CmdletBinding()]
param([string[]]$Targets = @('game-client'), [int]$Parallel = 6)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$selectionPath = Join-Path $repoRoot '.vs/ProjectSettings.json'
if (!(Test-Path -LiteralPath $selectionPath)) {
    throw 'Visual Studio configuration is unknown: .vs/ProjectSettings.json is missing.'
}
$selected = (Get-Content -Raw -LiteralPath $selectionPath | ConvertFrom-Json).CurrentProjectSetting
$settings = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'CMakeSettings.json') | ConvertFrom-Json
$config = @($settings.configurations | Where-Object name -EQ $selected)
if ($config.Count -ne 1) { throw "Cannot resolve the saved Visual Studio configuration: $selected" }
$buildRoot = $config[0].buildRoot.Replace('${projectDir}', $repoRoot).Replace('${name}', $selected)
$cachePath = Join-Path $buildRoot 'CMakeCache.txt'
if (!(Test-Path -LiteralPath $cachePath)) { throw "Visual Studio must configure its existing tree first: $buildRoot" }
$cache = @{}
foreach ($line in Get-Content -LiteralPath $cachePath) {
    if ($line -match '^([^/#][^:]*):[^=]+=(.*)$') { $cache[$Matches[1]] = $Matches[2] }
}
if ($cache.CMAKE_GENERATOR -ne $config[0].generator -or $cache.CMAKE_BUILD_TYPE -ne $config[0].configurationType) {
    throw 'The saved Visual Studio configuration and existing cache disagree. Resolve this in Visual Studio.'
}
if ([IO.Path]::GetFullPath($cache.CMAKE_HOME_DIRECTORY) -ne [IO.Path]::GetFullPath($repoRoot)) {
    throw 'The CMake cache belongs to a different source directory.'
}
$compiler = $cache.CMAKE_CXX_COMPILER.Replace('/', '\')
$vcIndex = $compiler.IndexOf('\VC\Tools\MSVC\', [StringComparison]::OrdinalIgnoreCase)
if ($vcIndex -lt 0 -or $compiler -notmatch 'Hostx64\\x64\\cl.exe$') { throw 'Expected the existing MSVC x64 toolchain.' }
$compiler -match '\\MSVC\\([^\\]+)\\' | Out-Null
$msvcVersion = $Matches[1]
$resourceCompiler = $cache.CMAKE_RC_COMPILER.Replace('/', '\')
if ($resourceCompiler -notmatch '\\bin\\([0-9.]+)\\x64\\rc.exe$') { throw 'Cannot determine the configured Windows SDK.' }
$sdkVersion = $Matches[1]
$vcvars = Join-Path $compiler.Substring(0, $vcIndex) 'VC/Auxiliary/Build/vcvars64.bat'
$cmake = $cache.CMAKE_COMMAND
foreach ($tool in @($vcvars, $cmake)) {
    if (!(Test-Path -LiteralPath $tool)) { throw "Missing configured build tool: $tool" }
}
# All command arguments are paths from the local VS cache or validated target names.
foreach ($target in $Targets) {
    if ($target -notmatch '^[a-zA-Z0-9_.-]+$') { throw "Invalid target: $target" }
}
if ($Parallel -lt 1) { throw 'Parallel must be positive.' }
Write-Host "Visual Studio: $selected / $($cache.CMAKE_BUILD_TYPE) / $buildRoot"
$command = '"{0}" {5} -vcvars_ver={6} >nul && "{1}" --build "{2}" --target {3} --parallel {4}' -f $vcvars, $cmake, $buildRoot, ($Targets -join ' '), $Parallel, $sdkVersion, $msvcVersion
& $env:ComSpec /d /s /c $command
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
