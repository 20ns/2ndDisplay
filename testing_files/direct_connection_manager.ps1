# DIRECT TABDISPLAY CONNECTION MANAGER
# This PowerShell script establishes connection to Android device
# and maintains it until the host application can be rebuilt

param(
    [string]$AndroidIP = "192.168.238.161",
    [int]$AndroidPort = 5004,
    [switch]$KeepAlive
)

Write-Host "=== DIRECT TABDISPLAY CONNECTION MANAGER ===" -ForegroundColor Yellow
Write-Host ""

# Test 1: Verify Android device is reachable
Write-Host "1. Testing connectivity to Android device..." -ForegroundColor Cyan
$pingResult = Test-Connection -ComputerName $AndroidIP -Count 1 -Quiet
if ($pingResult) {
    Write-Host "✅ Android device is reachable at $AndroidIP" -ForegroundColor Green
} else {
    Write-Host "❌ Cannot reach Android device at $AndroidIP" -ForegroundColor Red
    exit 1
}

# Test 2: Send initial control packet
Write-Host ""
Write-Host "2. Sending initial control packet..." -ForegroundColor Cyan

function Send-ControlPacket {
    param($IP, $Port)
    
    $client = New-Object System.Net.Sockets.UdpClient
    try {
        # Create control packet JSON
        $controlData = @{
            type = "keepalive"
            width = 1920
            height = 1080
            fps = 30
            bitrate = 5000000
            touchPort = 5005
        } | ConvertTo-Json -Compress
        
        # Create packet header (12 bytes) - little endian format
        $header = New-Object byte[] 12
        $header[0] = 1; $header[1] = 0      # sequenceId = 1
        $header[2] = 0; $header[3] = 0      # frameId = 0
        $header[4] = 1; $header[5] = 0      # totalChunks = 1
        $header[6] = 0; $header[7] = 0      # chunkIndex = 0
        $header[8] = 4; $header[9] = 0; $header[10] = 0; $header[11] = 0  # flags = FLAG_CONTROL_PACKET
        
        # Convert JSON to bytes
        $jsonBytes = [System.Text.Encoding]::UTF8.GetBytes($controlData)
        
        # Combine header + payload
        $fullPacket = New-Object byte[] ($header.Length + $jsonBytes.Length)
        $header.CopyTo($fullPacket, 0)
        $jsonBytes.CopyTo($fullPacket, $header.Length)
        
        # Send packet
        $bytesSent = $client.Send($fullPacket, $fullPacket.Length, $IP, $Port)
        
        Write-Host "   Sent $bytesSent bytes to $IP`:$Port" -ForegroundColor Green
        Write-Host "   JSON: $controlData" -ForegroundColor Gray
        
        return $true
    } catch {
        Write-Host "   ERROR: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    } finally {
        $client.Close()
    }
}

# Send initial packet
if (Send-ControlPacket -IP $AndroidIP -Port $AndroidPort) {
    Write-Host "✅ Initial control packet sent successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "*** CHECK YOUR ANDROID DEVICE ***" -ForegroundColor Yellow
    Write-Host "Status should show: 'Connected to /[your-pc-ip]'" -ForegroundColor Yellow
} else {
    Write-Host "❌ Failed to send initial control packet" -ForegroundColor Red
    exit 1
}

# Test 3: Keepalive mode
if ($KeepAlive) {
    Write-Host ""
    Write-Host "3. Starting keepalive mode..." -ForegroundColor Cyan
    Write-Host "   Sending control packets every 5 seconds" -ForegroundColor Gray
    Write-Host "   Press Ctrl+C to stop" -ForegroundColor Gray
    Write-Host ""
    
    $counter = 1
    try {
        while ($true) {
            Start-Sleep -Seconds 5
            
            Write-Host "Keepalive #$counter - " -NoNewline -ForegroundColor Cyan
            if (Send-ControlPacket -IP $AndroidIP -Port $AndroidPort) {
                Write-Host "✅ Sent" -ForegroundColor Green
            } else {
                Write-Host "❌ Failed" -ForegroundColor Red
            }
            
            $counter++
        }
    } catch {
        Write-Host ""
        Write-Host "Keepalive stopped by user" -ForegroundColor Yellow
    }
} else {
    Write-Host ""
    Write-Host "Connection established! To start keepalive mode, run:" -ForegroundColor Yellow
    Write-Host "   .\direct_connection_manager.ps1 -KeepAlive" -ForegroundColor Gray
}

Write-Host ""
Write-Host "=== CONNECTION MANAGER COMPLETED ===" -ForegroundColor Yellow
