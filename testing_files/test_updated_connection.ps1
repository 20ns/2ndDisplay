# Quick test with updated IPs
$AndroidIP = "192.168.238.161"  # Android device IP (unchanged)
$AndroidPort = 5004

Write-Host "Testing connection with updated IPs:"
Write-Host "PC IP: 192.168.238.172 (new)"  
Write-Host "Android IP: $AndroidIP (unchanged)"
Write-Host ""

$client = New-Object System.Net.Sockets.UdpClient
try {
    # Test JSON control packet
    $json = '{"type":"keepalive","width":1920,"height":1080,"fps":30,"bitrate":5000000,"touchPort":5005}'
    $jsonBytes = [System.Text.Encoding]::UTF8.GetBytes($json)
    
    # Create header 
    $header = New-Object byte[] 12
    $header[0] = 1; $header[1] = 0     # sequenceId = 1
    $header[2] = 0; $header[3] = 0     # frameId = 0  
    $header[4] = 1; $header[5] = 0     # totalChunks = 1
    $header[6] = 0; $header[7] = 0     # chunkIndex = 0
    $header[8] = 4; $header[9] = 0; $header[10] = 0; $header[11] = 0  # flags = FLAG_CONTROL_PACKET
    
    $fullPacket = New-Object byte[] ($header.Length + $jsonBytes.Length)
    $header.CopyTo($fullPacket, 0)
    $jsonBytes.CopyTo($fullPacket, $header.Length)
    
    $bytes = $client.Send($fullPacket, $fullPacket.Length, $AndroidIP, $AndroidPort)
    Write-Host "✅ SUCCESS: Sent $bytes bytes" -ForegroundColor Green
    Write-Host "Check Android tablet - should show 'Connected to /192.168.238.172'"
    
} catch {
    Write-Host "❌ ERROR: $($_.Exception.Message)" -ForegroundColor Red
} finally {
    $client.Close()
}
