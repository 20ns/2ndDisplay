# Direct connection test to Android device
Write-Host "Direct Android Connection Test" -ForegroundColor Green
Write-Host "==============================="

# Known Android device IP from our analysis
$androidIP = "192.168.238.1"
$port = 5004

Write-Host "Testing direct connection to Android device..." -ForegroundColor Yellow
Write-Host "Target: $androidIP`:$port"

try {
    # Create UDP client
    $udpClient = New-Object System.Net.Sockets.UdpClient
    
    # Test message
    $message = "TEST_FROM_PC"
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($message)
    
    # Send to Android device
    $result = $udpClient.Send($bytes, $bytes.Length, $androidIP, $port)
    
    Write-Host "✅ UDP packet sent successfully! ($result bytes)" -ForegroundColor Green
    Write-Host "If Android app is running, it should receive this message." -ForegroundColor Cyan
    
    $udpClient.Close()
    
    Write-Host "`nNext steps:" -ForegroundColor Yellow
    Write-Host "1. Make sure Android TabDisplay app is running"
    Write-Host "2. Check if Android app shows any connection activity"
    Write-Host "3. If Android app receives the packet, the network connection works!"
    
} catch {
    Write-Host "❌ Failed to send UDP packet: $($_.Exception.Message)" -ForegroundColor Red
} finally {
    if ($udpClient) { $udpClient.Dispose() }
}

Write-Host "`nFor the TabDisplay host app to work, we need to:" -ForegroundColor Cyan
Write-Host "1. Build a version with working Android device discovery"
Write-Host "2. Or manually configure it to connect to $androidIP"
