<#
.SYNOPSIS
  Builds OBS Stream Marker for Windows x64 and creates a ready-to-copy package in .\dist.

.DESCRIPTION
  Needs: Visual Studio 2022 (or Build Tools) with "Desktop development with C++", and CMake 3.28+.
  On first configure CMake downloads (with SHA-256 verification) the OBS 31 sources, pre-built
  obs-deps and Qt6 into .deps\ - roughly 600 MB, one time only.

.PARAMETER Config   RelWithDebInfo (default), Release or Debug
.PARAMETER Test     Also build and run the headless core test (no OBS required).
.PARAMETER Install  Copy the finished package into %ProgramData%\obs-studio\plugins.
#>
param(
    [ValidateSet('RelWithDebInfo', 'Release', 'Debug')] [string]$Config = 'RelWithDebInfo',
    [switch]$Test,
    [switch]$Install
)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)

function Find-CMake {
    $c = Get-Command cmake -ErrorAction SilentlyContinue
    if ($c) { return $c.Source }
    foreach ($p in "$env:ProgramFiles\CMake\bin\cmake.exe", "${env:ProgramFiles(x86)}\CMake\bin\cmake.exe") {
        if (Test-Path $p) { return $p }
    }
    throw "CMake not found. Install it with: winget install Kitware.CMake"
}
$cmake = Find-CMake
Write-Host "Using $cmake"

# Visual Studio generator check
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere) -or -not (& $vswhere -latest -products * -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)) {
    throw "Visual Studio 2022 with the C++ x64 toolset was not found. Install: winget install Microsoft.VisualStudio.2022.BuildTools --override `"--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended`""
}

& $cmake --preset windows-x64
if ($LASTEXITCODE) { throw "configure failed" }
& $cmake --build --preset windows-x64 --config $Config
if ($LASTEXITCODE) { throw "build failed" }

if (Test-Path dist) { Remove-Item dist -Recurse -Force }
& $cmake --install build_x64 --config $Config --prefix dist
if ($LASTEXITCODE) { throw "install step failed" }
Write-Host "`nPackage created:" -ForegroundColor Green
Get-ChildItem dist -Recurse -File | ForEach-Object { "  " + $_.FullName.Substring((Get-Location).Path.Length + 1) }

if ($Test) {
    & $cmake -S . -B build_test -G "Visual Studio 17 2022" -A x64 -DBUILD_SMOKE_TEST=ON
    if ($LASTEXITCODE) { throw "test configure failed" }
    & $cmake --build build_test --config $Config --target core-smoke-test
    if ($LASTEXITCODE) { throw "test build failed" }
    $qtbin = Get-ChildItem .deps -Recurse -Filter Qt6Core.dll -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($qtbin) { $env:PATH = "$($qtbin.DirectoryName);$env:PATH" }
    & ".\build_test\$Config\core-smoke-test.exe"
    if ($LASTEXITCODE) { throw "core tests failed" }
}

if ($Install) { & "$PSScriptRoot\install.ps1" }
