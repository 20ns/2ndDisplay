#!/usr/bin/env pwsh

Write-Host "TabDisplay Second Monitor Configuration Test" -ForegroundColor Green
Write-Host "==============================================" -ForegroundColor Green
Write-Host ""

# Get current monitor setup
Write-Host "Current Monitor Configuration:" -ForegroundColor Yellow
Add-Type -AssemblyName System.Windows.Forms
$screens = [System.Windows.Forms.Screen]::AllScreens

foreach ($screen in $screens) {
    $bounds = $screen.Bounds
    $primary = if ($screen.Primary) { "(Primary)" } else { "" }
    Write-Host "  Monitor: $($screen.DeviceName) $primary"
    Write-Host "    Resolution: $($bounds.Width) x $($bounds.Height)"
    Write-Host "    Position: ($($bounds.X), $($bounds.Y))"
    Write-Host "    Work Area: $($screen.WorkingArea)"
    Write-Host ""
}

# Calculate virtual second monitor area
$primaryScreen = $screens | Where-Object { $_.Primary }
$primaryBounds = $primaryScreen.Bounds

$virtualX = $primaryBounds.Width
$virtualY = 0
$virtualWidth = 1752   # Tablet resolution width
$virtualHeight = 2800  # Tablet resolution height

Write-Host "Recommended Virtual Second Monitor Configuration:" -ForegroundColor Cyan
Write-Host "  Position: ($virtualX, $virtualY)"
Write-Host "  Size: $virtualWidth x $virtualHeight"
Write-Host "  This creates an extended desktop to the right of your primary monitor"
Write-Host ""

Write-Host "To test extended desktop functionality:" -ForegroundColor Yellow
Write-Host "1. Start TabDisplay.exe from: C:\Users\Nav\Desktop\display\host\build\vs2022-release\"
Write-Host "2. Right-click the tray icon and select 'Connect to Android'"
Write-Host "3. Change capture mode to 'Second Monitor (Extended)' from the tray menu"
Write-Host "4. Move a window to coordinates ($virtualX, $virtualY) or drag to the right edge"
Write-Host "5. The window should appear on your Android tablet"
Write-Host ""

# Test if TabDisplay is running
$tabDisplayProcess = Get-Process -Name "TabDisplay" -ErrorAction SilentlyContinue
if ($tabDisplayProcess) {
    Write-Host "TabDisplay is currently running (PID: $($tabDisplayProcess.Id))" -ForegroundColor Green
} else {
    Write-Host "TabDisplay is not currently running" -ForegroundColor Red
    Write-Host "Start it manually from: C:\Users\Nav\Desktop\display\host\build\vs2022-release\TabDisplay.exe"
}

Write-Host ""
Write-Host "Note: The current build may not have the latest region capture features." -ForegroundColor Yellow
Write-Host "If the menu options are missing, the build needs to be updated with the new code." -ForegroundColor Yellow
