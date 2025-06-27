# Comprehensive network debugging for Android connection
$AndroidIP = "192.168.238.161" 
$AndroidPort = 5004

Write-Host "=== COMPREHENSIVE ANDROID CONNECTION DEBUG ===" -ForegroundColor Yellow
Write-Host ""

# Test 1: Basic connectivity
Write-Host "1. Testing basic ping connectivity..." -ForegroundColor Cyan
$pingResult = ping -n 1 $AndroidIP
if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ PING: Android device is reachable" -ForegroundColor Green
} else {
    Write-Host "❌ PING: Cannot reach Android device" -ForegroundColor Red
    exit 1
}

# Test 2: Port scan to check if UDP port is actually listening
Write-Host ""
Write-Host "2. Testing if UDP port 5004 is accessible..." -ForegroundColor Cyan
try {
    $tcpClient = New-Object System.Net.Sockets.TcpClient
    # This will fail for UDP, but let's try anyway to see network behavior
    $tcpClient.ConnectTimeout = 1000
    Write-Host "   TCP connection test (expected to fail for UDP port)..." 
} catch {
    Write-Host "   TCP test failed as expected (UDP port)" -ForegroundColor Yellow
}

# Test 3: Send UDP with logging
Write-Host ""
Write-Host "3. Sending test UDP packets with detailed logging..." -ForegroundColor Cyan

$client = New-Object System.Net.Sockets.UdpClient
try {
    # Bind to a specific local port so we can see responses
    $client.Client.Bind([System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, 0))
    $localPort = $client.Client.LocalEndPoint.Port
    Write-Host "   Local UDP port: $localPort" -ForegroundColor Yellow
    
    # Test 1: Simple UDP packet
    Write-Host ""
    Write-Host "   Test 3a: Simple 'HELLO' packet..." 
    $simpleData = [System.Text.Encoding]::UTF8.GetBytes("HELLO")
    $bytesSent = $client.Send($simpleData, $simpleData.Length, $AndroidIP, $AndroidPort)
    Write-Host "   Sent $bytesSent bytes"
    
    # Test 2: Control packet with correct header
    Write-Host ""
    Write-Host "   Test 3b: Proper control packet..."
    
    $json = '{"type":"keepalive","width":1920,"height":1080,"fps":30,"bitrate":5000000,"touchPort":5005}'
    $jsonBytes = [System.Text.Encoding]::UTF8.GetBytes($json)
    
    # Create header matching Android expectation
    $header = New-Object byte[] 12
    $header[0] = 1; $header[1] = 0     # sequenceId = 1
    $header[2] = 0; $header[3] = 0     # frameId = 0  
    $header[4] = 1; $header[5] = 0     # totalChunks = 1
    $header[6] = 0; $header[7] = 0     # chunkIndex = 0
    $header[8] = 4; $header[9] = 0; $header[10] = 0; $header[11] = 0  # flags = FLAG_CONTROL_PACKET
    
    $fullPacket = New-Object byte[] ($header.Length + $jsonBytes.Length)
    $header.CopyTo($fullPacket, 0)
    $jsonBytes.CopyTo($fullPacket, $header.Length)
    
    Write-Host "   Header: $($header -join ' ')"
    Write-Host "   JSON: $json"
    Write-Host "   Total packet size: $($fullPacket.Length) bytes"
    
    $bytesSent2 = $client.Send($fullPacket, $fullPacket.Length, $AndroidIP, $AndroidPort)
    Write-Host "   Sent $bytesSent2 bytes" -ForegroundColor Green
    
    # Test 3: Multiple rapid packets
    Write-Host ""
    Write-Host "   Test 3c: Multiple rapid packets..."
    for ($i = 1; $i -le 3; $i++) {
        $testMsg = [System.Text.Encoding]::UTF8.GetBytes("TEST_$i")
        $client.Send($testMsg, $testMsg.Length, $AndroidIP, $AndroidPort) | Out-Null
        Start-Sleep -Milliseconds 100
    }
    Write-Host "   Sent 3 rapid test packets"
    
} catch {
    Write-Host "   ERROR: $($_.Exception.Message)" -ForegroundColor Red
} finally {
    $client.Close()
}

# Test 4: Network interface analysis
Write-Host ""
Write-Host "4. Network interface analysis..." -ForegroundColor Cyan
$adapter = Get-NetAdapter | Where-Object {$_.Name -eq "Ethernet 3"}
if ($adapter) {
    Write-Host "   Interface found: $($adapter.InterfaceDescription)" -ForegroundColor Green
    Write-Host "   Status: $($adapter.Status)"
    $ipConfig = Get-NetIPAddress -InterfaceAlias "Ethernet 3" -AddressFamily IPv4
    Write-Host "   PC IP: $($ipConfig.IPAddress)"
    Write-Host "   Android IP (target): $AndroidIP"
} else {
    Write-Host "   ❌ Ethernet 3 interface not found!" -ForegroundColor Red
}

Write-Host ""
Write-Host "=== TEST COMPLETED ===" -ForegroundColor Yellow
Write-Host "Check your Android device now:"
Write-Host "- Look for status change from 'Surface Ready' to 'Connected to...'"
Write-Host "- Check if any packet reception is logged"
Write-Host "- If no change, the UDP socket may not be properly bound in the Android app"
