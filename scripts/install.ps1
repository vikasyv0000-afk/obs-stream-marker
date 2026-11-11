<#
.SYNOPSIS
  Installs the packaged plugin from .\dist into OBS (per-machine plugin folder, no admin needed
  if you can write to %ProgramData%\obs-studio). Close OBS first.
#>
param([string]$Source = (Join-Path (Split-Path $PSScriptRoot -Parent) 'dist\obs-stream-marker'))
$ErrorActionPreference = 'Stop'

if (-not (Test-Path (Join-Path $Source 'bin\64bit\obs-stream-marker.dll'))) {
    throw "Package not found at $Source. Run scripts\build.ps1 first (or point -Source at the package folder)."
}
if (Get-Process obs64 -ErrorAction SilentlyContinue) { throw "OBS is running. Close it and run this again." }

$target = Join-Path $env:ProgramData 'obs-studio\plugins\obs-stream-marker'
New-Item -ItemType Directory -Force $target | Out-Null
Copy-Item (Join-Path $Source '*') $target -Recurse -Force
Write-Host "Installed to $target" -ForegroundColor Green
Write-Host "Start OBS, then enable it under  Docks > Stream Marker."
