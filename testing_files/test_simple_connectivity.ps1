Write-Host "TabDisplay Extended Desktop Test" -ForegroundColor Green
Write-Host "===================================" -ForegroundColor Green
Write-Host ""

# Check if TabDisplay is running
$tabDisplay = Get-Process -Name "TabDisplay" -ErrorAction SilentlyContinue
if (-not $tabDisplay) {
    Write-Host "ERROR: TabDisplay is not running!" -ForegroundColor Red
    Write-Host "Please start TabDisplay.exe first." -ForegroundColor Red
    exit 1
}

Write-Host "✓ TabDisplay is running (PID: $($tabDisplay.Id))" -ForegroundColor Green

# Test connectivity using our existing tool
Write-Host ""
Write-Host "Testing Android device connectivity..." -ForegroundColor Yellow
& ".\test_connectivity.exe"

Write-Host ""
Write-Host "Instructions for Extended Desktop Mode:" -ForegroundColor Cyan
Write-Host "1. Right-click the TabDisplay tray icon"
Write-Host "2. Select 'Connect to Android'"
Write-Host "3. Once connected, your tablet should show your primary monitor"
Write-Host ""
Write-Host "For extended desktop functionality:" -ForegroundColor Yellow
Write-Host "4. Move a window far to the right (beyond x=1920)"
Write-Host "5. The ideal capture region is: x=1920, y=0, width=1752, height=2800"
Write-Host "6. Unfortunately, the current build doesn't have region capture"
Write-Host "7. You'll see the full primary monitor mirrored for now"
Write-Host ""
Write-Host "Next steps needed:" -ForegroundColor Red
Write-Host "- Build the updated TabDisplay with region capture support"
Write-Host "- Or use the current version as a starting point for testing"

Write-Host ""
Write-Host "Current build status: Basic functionality only" -ForegroundColor Yellow
Write-Host "To get full extended desktop support, the build needs to be completed." -ForegroundColor Yellow
