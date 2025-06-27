# Test to see if we can find the TabDisplay window and simulate the menu
Add-Type -AssemblyName System.Windows.Forms

Write-Host "TabDisplay Menu Test" -ForegroundColor Green
Write-Host "===================" 

# Try to find TabDisplay tray icon
$processes = Get-Process -Name "TabDisplay" -ErrorAction SilentlyContinue
if ($processes) {
    Write-Host "✅ TabDisplay process found (PID: $($processes.Id))" -ForegroundColor Green
} else {
    Write-Host "❌ TabDisplay process not found" -ForegroundColor Red
    exit
}

# Check if we can see the tray icon area
Write-Host "`nTesting tray icon access..." -ForegroundColor Yellow

# Get notification area
$trayWnd = [System.Windows.Forms.Screen]::PrimaryScreen
Write-Host "Primary screen resolution: $($trayWnd.Bounds.Width)x$($trayWnd.Bounds.Height)"

Write-Host "`nTo test the menu manually:" -ForegroundColor Cyan
Write-Host "1. Look in the system tray (bottom-right corner)" 
Write-Host "2. You should see a TabDisplay icon" 
Write-Host "3. Right-click on it"
Write-Host "4. Click 'Connect to Android'"
Write-Host "5. Tell us what popup message appears"

Write-Host "`nIf you don't see the tray icon, the app might not be running properly." -ForegroundColor Yellow
