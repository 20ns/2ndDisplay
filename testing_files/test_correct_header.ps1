# Test with CORRECT packet header format for Android app
$AndroidIP = "192.168.238.161"
$AndroidPort = 5004

Write-Host "Testing with CORRECT packet header format to Android device at ${AndroidIP}:${AndroidPort}"

# Create UDP client  
$client = New-Object System.Net.Sockets.UdpClient

try {
    # Create proper JSON control packet
    $controlPacket = @{
        type = "keepalive"
        width = 1920
        height = 1080
        fps = 30
        bitrate = 5000000
        touchPort = 5005
    } | ConvertTo-Json -Compress
    
    Write-Host "Control packet JSON: $controlPacket"
    
    # Convert JSON to bytes
    $jsonBytes = [System.Text.Encoding]::UTF8.GetBytes($controlPacket)
    
    # Create proper header (12 bytes total):
    # sequenceId (2 bytes) + frameId (2 bytes) + totalChunks (2 bytes) + chunkIndex (2 bytes) + flags (4 bytes)
    $header = New-Object byte[] 12
    
    # sequenceId = 1 (little endian, 2 bytes)
    $header[0] = 1
    $header[1] = 0
    
    # frameId = 0 (little endian, 2 bytes) 
    $header[2] = 0
    $header[3] = 0
    
    # totalChunks = 1 (little endian, 2 bytes)
    $header[4] = 1
    $header[5] = 0
    
    # chunkIndex = 0 (little endian, 2 bytes)
    $header[6] = 0
    $header[7] = 0
    
    # flags = FLAG_CONTROL_PACKET = 4 (little endian, 4 bytes)
    $header[8] = 4
    $header[9] = 0
    $header[10] = 0
    $header[11] = 0
    
    # Combine header + JSON payload into single packet
    $fullPacket = New-Object byte[] ($header.Length + $jsonBytes.Length)
    $header.CopyTo($fullPacket, 0)
    $jsonBytes.CopyTo($fullPacket, $header.Length)
    
    Write-Host "Sending packet: Header(12 bytes) + JSON($($jsonBytes.Length) bytes) = $($fullPacket.Length) total bytes"
    
    # Send as single UDP packet
    $bytes = $client.Send($fullPacket, $fullPacket.Length, $AndroidIP, $AndroidPort)
    Write-Host "SUCCESS: Sent $bytes bytes to $AndroidIP`:$AndroidPort"
    Write-Host ""
    Write-Host "*** CHECK YOUR ANDROID DEVICE NOW ***"
    Write-Host "Status should change from 'Surface Ready' to 'Connected to /192.168.238.58'"
    
    # Wait to see response
    Start-Sleep -Seconds 3
    
} catch {
    Write-Host "ERROR: $($_.Exception.Message)"
} finally {
    $client.Close()
}

Write-Host ""
Write-Host "Test completed. Did the Android status change?"
