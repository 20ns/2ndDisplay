# Install Windows App SDK for TabDisplay project
# This script downloads and installs the Windows App SDK

param(
    [switch]$Force = $false
)

Write-Host "Installing Windows App SDK..." -ForegroundColor Green

# Check if running as administrator
if (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Host "This script requires administrator privileges. Restarting as administrator..." -ForegroundColor Yellow
    Start-Process PowerShell -Verb RunAs "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""
    exit
}

# Create temp directory
$tempDir = Join-Path $env:TEMP "WindowsAppSDK"
if (Test-Path $tempDir) {
    Remove-Item $tempDir -Recurse -Force
}
New-Item -ItemType Directory -Path $tempDir | Out-Null

try {
    # Download Windows App SDK
    $downloadUrl = "https://aka.ms/windowsappsdk/1.6/1.6.241114003/windowsappsdk-runtime-win10-x64.msix"
    $runtimeDownload = Join-Path $tempDir "windowsappsdk-runtime.msix"
    
    Write-Host "Downloading Windows App SDK Runtime..." -ForegroundColor Yellow
    Invoke-WebRequest -Uri $downloadUrl -OutFile $runtimeDownload -UseBasicParsing
    
    # Download the development package
    $devDownloadUrl = "https://github.com/microsoft/WindowsAppSDK/releases/download/v1.6.241114003/Microsoft.WindowsAppSDK.ProjectReunion.1.6.241114003-preview2.nupkg"
    $devDownload = Join-Path $tempDir "WindowsAppSDK.nupkg"
    
    Write-Host "Downloading Windows App SDK Development Package..." -ForegroundColor Yellow
    Invoke-WebRequest -Uri $devDownloadUrl -OutFile $devDownload -UseBasicParsing
    
    # Install the runtime
    Write-Host "Installing Windows App SDK Runtime..." -ForegroundColor Yellow
    Add-AppxPackage -Path $runtimeDownload -ForceApplicationShutdown
    
    # Extract the development package
    $extractDir = Join-Path $tempDir "extracted"
    New-Item -ItemType Directory -Path $extractDir | Out-Null
    
    # Rename .nupkg to .zip and extract
    $zipFile = Join-Path $tempDir "WindowsAppSDK.zip"
    Move-Item $devDownload $zipFile
    
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::ExtractToDirectory($zipFile, $extractDir)
    
    Write-Host "Windows App SDK installed successfully!" -ForegroundColor Green
    Write-Host "You may need to restart Visual Studio if it's open." -ForegroundColor Yellow
    
} catch {
    Write-Host "Error installing Windows App SDK: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Please try installing manually from: https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/downloads" -ForegroundColor Yellow
} finally {
    # Cleanup
    if (Test-Path $tempDir) {
        Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Write-Host "Press any key to continue..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
