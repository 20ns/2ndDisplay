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

# Check network connectivity to Android device
Write-Host ""
Write-Host "Testing Android device connectivity..." -ForegroundColor Yellow

$testIPs = @("192.168.238.1", "192.168.238.129", "192.168.42.129", "192.168.43.1")
$foundDevice = $false

foreach ($ip in $testIPs) {
    try {
        $udpClient = New-Object System.Net.Sockets.UdpClient
        $udpClient.Connect($ip, 5004)
        $data = [System.Text.Encoding]::ASCII.GetBytes("PING")
        $udpClient.Send($data, $data.Length) | Out-Null
        $udpClient.Close()
        Write-Host "✓ Android device responsive at $ip" -ForegroundColor Green
        $foundDevice = $true
        break
    } catch {
        Write-Host "✗ No response from $ip" -ForegroundColor Red
    }
}

if ($foundDevice) {
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
} else {
    Write-Host ""
    Write-Host "ERROR: No Android device found!" -ForegroundColor Red
    Write-Host "Make sure:" -ForegroundColor Yellow
    Write-Host "1. Android tablet is connected via USB"
    Write-Host "2. USB tethering is enabled on Android"
    Write-Host "3. TabDisplay Android app is running"
    Write-Host "4. Android allows USB debugging"
}

Write-Host ""
Write-Host "Current build status: Basic functionality only" -ForegroundColor Yellow
Write-Host "To get full extended desktop support, the build needs to be completed." -ForegroundColor Yellow
