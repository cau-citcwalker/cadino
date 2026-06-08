# Loads the Visual Studio Developer environment and project-specific env vars
# Usage: . .\scripts\dev-shell.ps1   (dot-source so changes apply to current session)

$ErrorActionPreference = 'Stop'

$vsDevShell = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1'
if (-not (Test-Path $vsDevShell)) {
    throw "Launch-VsDevShell.ps1 not found at $vsDevShell"
}

& $vsDevShell -SkipAutomaticLocation -Arch amd64 -HostArch amd64 | Out-Null

$env:VCPKG_ROOT = 'E:\dev\vcpkg'
$env:CMAKE_PREFIX_PATH = 'C:\Qt\6.11.1\msvc2022_64'
$env:VULKAN_SDK = 'C:\VulkanSDK\1.4.350.0'

Write-Host "Dev shell ready:"
Write-Host "  cmake : $((Get-Command cmake -ErrorAction SilentlyContinue).Source)"
Write-Host "  ninja : $((Get-Command ninja -ErrorAction SilentlyContinue).Source)"
Write-Host "  cl    : $((Get-Command cl    -ErrorAction SilentlyContinue).Source)"
Write-Host "  Qt    : $env:CMAKE_PREFIX_PATH"
Write-Host "  Vulkan: $env:VULKAN_SDK"
Write-Host "  vcpkg : $env:VCPKG_ROOT"
