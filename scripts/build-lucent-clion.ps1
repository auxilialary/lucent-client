# Use CLion's selected profile and existing File API/cache, never configure a new tree.
[CmdletBinding()]
param(
    [string]$Profile,
    [string[]]$Targets = @(),
    [ValidateRange(1, 256)][int]$Parallel = 14,
    [switch]$Test,
    [string]$TestFilter = '*',
    [switch]$Inspect
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$repoRoot = Split-Path -Parent $PSScriptRoot

function Same-Path([string]$Left, [string]$Right) {
    return [string]::Equals([IO.Path]::GetFullPath($Left).TrimEnd('\', '/'), [IO.Path]::GetFullPath($Right).TrimEnd('\', '/'), [StringComparison]::OrdinalIgnoreCase)
}
function Read-Cache([string]$Directory) {
    $values = @{}
    foreach ($line in Get-Content -LiteralPath (Join-Path $Directory 'CMakeCache.txt')) {
        if ($line -match '^([^/#][^:]*):[^=]+=(.*)$') { $values[$Matches[1]] = $Matches[2] }
    }
    return $values
}
function Read-Model([string]$Directory) {
    $reply = Join-Path $Directory '.cmake/api/v1/reply'
    $indexFile = Get-ChildItem -LiteralPath $reply -Filter 'index-*.json' | Sort-Object Name -Descending | Select-Object -First 1
    if (!$indexFile) { throw "CLion File API index is missing: $Directory" }
    $index = Get-Content -Raw -LiteralPath $indexFile.FullName | ConvertFrom-Json
    $modelRef = @($index.objects | Where-Object kind -EQ 'codemodel')
    $toolsRef = @($index.objects | Where-Object kind -EQ 'toolchains')
    if ($modelRef.Count -ne 1 -or $toolsRef.Count -ne 1) { throw 'Ambiguous CLion File API model/toolchains.' }
    return @{
        Index = $index
        Model = (Get-Content -Raw -LiteralPath (Join-Path $reply $modelRef[0].jsonFile) | ConvertFrom-Json)
        Tools = (Get-Content -Raw -LiteralPath (Join-Path $reply $toolsRef[0].jsonFile) | ConvertFrom-Json)
        Reply = $reply
    }
}

[xml]$workspace = Get-Content -Raw -LiteralPath (Join-Path $repoRoot '.idea/workspace.xml')
$profiles = @($workspace.project.component | Where-Object name -EQ 'CMakeSettings' | ForEach-Object { $_.configurations.configuration })
if (!$Profile) {
    $selection = @($workspace.project.component | Where-Object name -EQ 'ExecutionTargetManager')
    if ($selection.Count -ne 1 -or $selection[0].SELECTED_TARGET -notmatch '^CMakeBuildProfile:(.+)$') {
        throw 'CLion has no unambiguous selected CMake profile. Select one in CLion or pass -Profile.'
    }
    $Profile = $Matches[1]
}
$selected = @($profiles | Where-Object { $_.PROFILE_NAME -eq $Profile -and $_.ENABLED -eq 'true' })
if ($selected.Count -ne 1) { throw "CLion profile is missing, disabled or ambiguous: $Profile" }
$configuration = $selected[0]
# Refuse unsupported overrides instead of silently constructing a different environment.
$supported = @('PROFILE_NAME', 'ENABLED', 'CONFIG_NAME', 'TOOLCHAIN_NAME', 'GENERATION_DIR', 'GENERATOR')
foreach ($attribute in $configuration.Attributes) {
    if ($attribute.Name -notin $supported -and $attribute.Value) { throw "Unsupported CLion profile override: $($attribute.Name). Resolve explicitly; no VS fallback is used." }
}
if ($configuration.HasChildNodes) { throw 'Custom CLion profile environment/options require explicit support; refusing to ignore them.' }
if (!$configuration.CONFIG_NAME -or !$configuration.TOOLCHAIN_NAME) { throw 'CLion profile must name a build type and toolchain.' }

if ($configuration.GENERATION_DIR) {
    $generation = $configuration.GENERATION_DIR.Replace('$PROJECT_DIR$', $repoRoot)
    $buildRoot = if ([IO.Path]::IsPathRooted($generation)) { [IO.Path]::GetFullPath($generation) } else { [IO.Path]::GetFullPath((Join-Path $repoRoot $generation)) }
} else {
    # Discover existing CLion trees by their marker and cached build type, not VS presets.
    $candidates = @(Get-ChildItem -LiteralPath $repoRoot -Directory | Where-Object {
        (Test-Path -LiteralPath (Join-Path $_.FullName 'CMakeCache.txt')) -and
        (Test-Path -LiteralPath (Join-Path $_.FullName 'CMakeFiles/clion-environment.txt'))
    } | Where-Object { (Read-Cache $_.FullName).CMAKE_BUILD_TYPE -eq $configuration.CONFIG_NAME })
    if ($candidates.Count -ne 1) { throw "Cannot uniquely identify the existing CLion tree for $Profile. Set its generation directory in CLion; no tree will be created." }
    $buildRoot = $candidates[0].FullName
}
$cache = Read-Cache $buildRoot
$api = Read-Model $buildRoot
if (!(Same-Path $cache.CMAKE_HOME_DIRECTORY $repoRoot) -or !(Same-Path $api.Model.paths.source $repoRoot) -or !(Same-Path $api.Model.paths.build $buildRoot)) {
    throw 'CLion cache/model belongs to a different source or build directory.'
}
if ($cache.CMAKE_BUILD_TYPE -ne $configuration.CONFIG_NAME -or $cache.CMAKE_GENERATOR -ne 'Ninja' -or $api.Index.cmake.generator.name -ne $cache.CMAKE_GENERATOR -or $api.Index.cmake.generator.multiConfig) {
    throw 'CLion profile, cache and File API disagree, or the generator is unsupported.'
}
if ($configuration.GENERATOR -and $configuration.GENERATOR -ne $cache.CMAKE_GENERATOR) { throw 'CLion generator override disagrees with the cache.' }
$modelConfiguration = @($api.Model.configurations | Where-Object name -EQ $configuration.CONFIG_NAME)
if ($modelConfiguration.Count -ne 1) { throw 'CLion codemodel configuration is ambiguous.' }
$modelTargets = @($modelConfiguration[0].targets)
$lucentTests = @($modelTargets | Where-Object name -Match '^lucent-.+-test$' | Sort-Object name)
if (!$lucentTests.Count -or 'game-client' -notin $modelTargets.name -or 'testrunner' -notin $modelTargets.name) { throw 'CLion model lacks required client/test targets.' }
$requiredTargets = @('game-client', 'testrunner') + @($lucentTests.name)
if (!$Targets.Count) { $Targets = @('all') + $requiredTargets }
if ($Test) { $Targets += $requiredTargets }
$Targets = @($Targets | Select-Object -Unique)
foreach ($target in $Targets) {
    if ($target -notmatch '^[a-zA-Z0-9_.-]+$' -or ($target -ne 'all' -and $target -notin $modelTargets.name)) { throw "Unknown or invalid CLion target: $target" }
}
$cmake = $cache.CMAKE_COMMAND
$ninja = $cache.CMAKE_MAKE_PROGRAM
if (!(Same-Path $cmake $api.Index.cmake.paths.cmake)) { throw 'CLion cache and File API use different CMake executables.' }
$normalizedCmake = $cmake.Replace('\', '/')
if ($normalizedCmake -notmatch '^(.*)/bin/cmake/[^/]+/[^/]+/bin/cmake.exe$') { throw 'Cannot identify CLion installation from its cached bundled CMake.' }
$installation = $Matches[1]
$product = Get-Content -Raw -LiteralPath (Join-Path $installation 'product-info.json') | ConvertFrom-Json
if ($product.productCode -ne 'CL') { throw 'Cached CMake does not belong to CLion.' }
$toolchainsFile = Join-Path $env:APPDATA "JetBrains/$($product.dataDirectoryName)/options/windows/toolchains.xml"
[xml]$toolchains = Get-Content -Raw -LiteralPath $toolchainsFile
$toolchain = @($toolchains.application.component.toolchains.toolchain | Where-Object name -EQ $configuration.TOOLCHAIN_NAME)
if ($toolchain.Count -ne 1 -or $toolchain[0].toolSetKind -ne 'MSVC') { throw 'Expected the actual configured CLion MSVC toolchain; refusing a compiler substitution.' }
foreach ($attribute in $toolchain[0].Attributes) {
    if ($attribute.Name -notin @('name', 'toolSetKind', 'toolSetPath', 'debuggerKind') -and $attribute.Value) { throw "Unsupported CLion toolchain override: $($attribute.Name)." }
}
if ($toolchain[0].HasChildNodes) { throw 'Custom CLion toolchain environment requires explicit support.' }
$compiler = $cache.CMAKE_CXX_COMPILER.Replace('\', '/')
if ($compiler -notmatch '^(?<root>.+)/VC/Tools/MSVC/(?<version>[0-9.]+)/bin/Hostx64/x64/cl.exe$') { throw 'Expected the cached MSVC host-x64/target-x64 compiler.' }
$msvcRoot = $Matches.root
$msvcVersion = $Matches.version
if (!(Same-Path $msvcRoot $toolchain[0].toolSetPath) -or !(Same-Path $compiler $cache.CMAKE_C_COMPILER)) { throw 'CLion toolchain installation and cached C/C++ compilers disagree.' }
$cxx = @($api.Tools.toolchains | Where-Object language -EQ 'CXX')
if ($cxx.Count -ne 1 -or $cxx[0].compiler.id -ne 'MSVC' -or !(Same-Path $cxx[0].compiler.path $compiler)) { throw 'File API compiler differs from the cached CLion compiler.' }
$resourceCompiler = $cache.CMAKE_RC_COMPILER.Replace('\', '/')
if ($resourceCompiler -notmatch '/bin/([0-9.]+)/x64/rc.exe$') { throw 'Cannot identify the cached CLion Windows SDK.' }
$sdkVersion = $Matches[1]
$vcvars = Join-Path $msvcRoot 'VC/Auxiliary/Build/vcvarsall.bat'
$marker = Get-Content -Raw -LiteralPath (Join-Path $buildRoot 'CMakeFiles/clion-environment.txt')
if ($marker -notmatch [regex]::Escape($msvcRoot.Replace('/', '\')) -or $marker.Replace('\', '/') -notmatch [regex]::Escape($ninja.Replace('\', '/'))) {
    throw 'CLion environment marker disagrees with cached MSVC/Ninja.'
}
foreach ($path in @($cmake, $ninja, $compiler, $resourceCompiler, $vcvars)) {
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing configured CLion tool: $path" }
}
# Only this verified Microsoft batch path crosses cmd; build/test arguments use native arrays.
if ($vcvars -match '["&|<>^%!]') { throw 'Unsupported batch path characters.' }
$savedEnvironment = @{}
Push-Location $repoRoot
try {
    $envCommand = '"{0}" amd64 {1} -vcvars_ver={2} >nul && set' -f $vcvars, $sdkVersion, $msvcVersion
    $preparedEnvironment = & $env:ComSpec /d /s /c $envCommand
    if ($LASTEXITCODE -ne 0) { throw 'CLion MSVC environment initialization failed.' }
    foreach ($line in $preparedEnvironment) {
        if ($line -notmatch '^([^=]+)=(.*)$') { continue }
        $key, $value = $Matches[1], $Matches[2]
        $previous = [Environment]::GetEnvironmentVariable($key, 'Process')
        if ($previous -cne $value) {
            $savedEnvironment[$key] = $previous
            [Environment]::SetEnvironmentVariable($key, $value, 'Process')
        }
    }
    $actualCompiler = (Get-Command cl.exe -CommandType Application).Source
    if (!(Same-Path $actualCompiler $compiler) -or $env:VCToolsVersion.TrimEnd('\') -ne $msvcVersion -or $env:WindowsSDKVersion.TrimEnd('\') -ne $sdkVersion -or $env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
        throw 'Prepared CLI environment does not match the CLion compiler, SDK or architecture.'
    }
    if ((Get-Item -LiteralPath $actualCompiler).VersionInfo.FileVersion -ne $cxx[0].compiler.version) {
        throw 'Compiler binary version differs from the CLion File API. Reload explicitly in CLion.'
    }
    $cmakeVersion = & $cmake --version | Select-Object -First 1
    if ($LASTEXITCODE -ne 0 -or $cmakeVersion -ne "cmake version $($api.Index.cmake.version.string)") { throw 'CMake executable version differs from the CLion File API. Reload explicitly in CLion.' }
    $ninjaVersion = & $ninja --version
    if ($LASTEXITCODE -ne 0 -or $marker -notmatch '(?m)^Ninja:\s+([^@]+)@' -or $ninjaVersion -ne $Matches[1]) { throw 'Ninja version differs from the CLion environment marker.' }
    $summary = [pscustomobject]@{
        Profile = $Profile; BuildType = $cache.CMAKE_BUILD_TYPE; BuildDirectory = $buildRoot
        Generator = $cache.CMAKE_GENERATOR; CMake = $cmake; CMakeVersion = $cmakeVersion; Ninja = $ninja; NinjaVersion = $ninjaVersion
        CLionVersion = $product.version; Toolchain = $configuration.TOOLCHAIN_NAME; ToolchainsFile = $toolchainsFile
        Compiler = $actualCompiler; CompilerVersion = $cxx[0].compiler.version; Toolset = $msvcVersion; SDK = $sdkVersion
        EnvironmentInitializer = $vcvars; EnvironmentArguments = "amd64 $sdkVersion -vcvars_ver=$msvcVersion"
        Architecture = $env:VSCMD_ARG_TGT_ARCH; Parallel = $Parallel; Targets = $Targets; LucentTests = @($lucentTests.name)
    }
    if ($Inspect) { return $summary }
    Write-Host "CLion: $Profile / $($cache.CMAKE_BUILD_TYPE) / $buildRoot"
    Write-Host "Toolchain: $($configuration.TOOLCHAIN_NAME) / MSVC $msvcVersion / x64 / SDK $sdkVersion"
    & $cmake --build $buildRoot --target @Targets --parallel $Parallel
    if ($LASTEXITCODE -ne 0) { throw "CLion build failed with exit code $LASTEXITCODE" }
    if ($Test) {
        # Ninja may have regenerated the existing tree. Resolve current artifact
        # filenames instead of retaining File API reply files that CMake removed.
        $api = Read-Model $buildRoot
        $modelConfiguration = @($api.Model.configurations | Where-Object name -EQ $configuration.CONFIG_NAME)
        if ($modelConfiguration.Count -ne 1) { throw 'CLion configuration changed during the build.' }
        $modelTargets = @($modelConfiguration[0].targets)
        $failures = @()
        foreach ($name in @('testrunner') + @($lucentTests.name)) {
            $targetRef = @($modelTargets | Where-Object name -EQ $name)[0]
            $targetModel = Get-Content -Raw -LiteralPath (Join-Path $api.Reply $targetRef.jsonFile) | ConvertFrom-Json
            $artifacts = @($targetModel.artifacts | Where-Object path -Match '\.exe$')
            if ($artifacts.Count -ne 1) { throw "Cannot resolve executable artifact for $name" }
            $exe = Join-Path $buildRoot $artifacts[0].path
            $testArguments = @()
            if ($name -eq 'testrunner') { $testArguments = @("--gtest_filter=$TestFilter", '--gtest_color=no', '--gtest_brief=1') }
            & $exe @testArguments
            $result = $LASTEXITCODE
            Write-Host "TEST $name : exit $result"
            if ($result -ne 0) { $failures += "$name (exit $result)" }
        }
        if ('run_rust_tests' -in $modelTargets.name) {
            & $cmake --build $buildRoot --target run_rust_tests --parallel $Parallel
            $result = $LASTEXITCODE
            Write-Host "TEST run_rust_tests : exit $result"
            if ($result -ne 0) { $failures += "run_rust_tests (exit $result)" }
        }
        if ($failures.Count) { throw "CLion tests failed: $($failures -join ', '). See individual results; no failures are silently ignored." }
    }
} finally {
    foreach ($key in $savedEnvironment.Keys) {
        # PowerShell can coerce $null to an empty string at this .NET boundary.
        # A previously absent variable must be removed, not restored as empty.
        $value = if ($null -eq $savedEnvironment[$key]) { [NullString]::Value } else { $savedEnvironment[$key] }
        [Environment]::SetEnvironmentVariable($key, $value, 'Process')
    }
    Pop-Location
}
