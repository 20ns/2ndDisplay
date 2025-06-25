# Windows App SDK Installation Script
# This script properly installs Windows App SDK for development

Write-Host "Installing Windows App SDK Development Environment..." -ForegroundColor Green

# Check if running as administrator
if (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Host "This script requires administrator privileges. Restarting as administrator..." -ForegroundColor Yellow
    Start-Process PowerShell -Verb RunAs "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""
    exit
}

# Function to download and install MSI
function Install-MSI {
    param($Url, $Name, $Args = "/quiet")
    
    Write-Host "Downloading $Name..." -ForegroundColor Yellow
    $tempFile = Join-Path $env:TEMP "$Name.msi"
    
    try {
        Invoke-WebRequest -Uri $Url -OutFile $tempFile -UseBasicParsing
        Write-Host "Installing $Name..." -ForegroundColor Yellow
        Start-Process -FilePath "msiexec.exe" -ArgumentList "/i", $tempFile, $Args -Wait
        Write-Host "$Name installed successfully!" -ForegroundColor Green
    }
    catch {
        Write-Host "Failed to install ${Name}: $($_.Exception.Message)" -ForegroundColor Red
    }
    finally {
        if (Test-Path $tempFile) {
            Remove-Item $tempFile -Force
        }
    }
}

try {
    # Install Windows App SDK Runtime
    Write-Host "Step 1: Installing Windows App SDK Runtime..." -ForegroundColor Cyan
    Install-MSI -Url "https://aka.ms/windowsappsdk/1.5/1.5.240627000/windowsappsdk-runtime-win10-x64.msix" -Name "WindowsAppSDK-Runtime"
    
    # For the development package, we need to use the .exe installer
    Write-Host "Step 2: Installing Windows App SDK Development Package..." -ForegroundColor Cyan
    $devUrl = "https://github.com/microsoft/WindowsAppSDK/releases/download/v1.5.240627000/WindowsAppSDK_1.5.240627000_x64.msi"
    Install-MSI -Url $devUrl -Name "WindowsAppSDK-Dev"
    
    # Install Visual C++ Redistributable (required for Windows App SDK)
    Write-Host "Step 3: Installing Visual C++ Redistributable..." -ForegroundColor Cyan
    $vcRedistUrl = "https://aka.ms/vs/17/release/vc_redist.x64.exe"
    $vcRedistFile = Join-Path $env:TEMP "vc_redist.x64.exe"
    
    Invoke-WebRequest -Uri $vcRedistUrl -OutFile $vcRedistFile -UseBasicParsing
    Start-Process -FilePath $vcRedistFile -ArgumentList "/quiet", "/norestart" -Wait
    Remove-Item $vcRedistFile -Force
    
    Write-Host "Windows App SDK installation completed!" -ForegroundColor Green
    Write-Host "Please restart Visual Studio and your development environment." -ForegroundColor Yellow
    
    # Check if Windows App SDK is now available
    $winAppSDKPath = "C:\Program Files (x86)\Microsoft SDKs\WindowsAppSDK"
    if (Test-Path $winAppSDKPath) {
        Write-Host "Windows App SDK found at: $winAppSDKPath" -ForegroundColor Green
    }
    
} catch {
    Write-Host "Error during installation: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Please try installing manually from: https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/downloads" -ForegroundColor Yellow
}

Write-Host "Installation complete. Press any key to continue..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
