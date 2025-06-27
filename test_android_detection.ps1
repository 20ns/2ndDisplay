Write-Host "Android Interface Detection Test" -ForegroundColor Green
Write-Host "=" * 40

# Get the Remote NDIS interface
$adapter = Get-NetAdapter | Where-Object { $_.InterfaceDescription -like "*Remote NDIS*" }

if ($adapter) {
    Write-Host "Found Remote NDIS Interface:" -ForegroundColor Yellow
    Write-Host "  Name: $($adapter.Name)"
    Write-Host "  Description: $($adapter.InterfaceDescription)"
    Write-Host "  Status: $($adapter.Status)"
    
    # Test our keyword matching
    $description = $adapter.InterfaceDescription.ToLower()
    Write-Host "`nKeyword Matching Test:" -ForegroundColor Cyan
    
    $androidKeywords = @("android", "adb", "rndis", "remote ndis", "usb ethernet", "samsung", "galaxy", "pixel", "nexus")
    $found = $false
    
    foreach ($keyword in $androidKeywords) {
        if ($description.Contains($keyword)) {
            Write-Host "  ✅ MATCH: '$keyword'" -ForegroundColor Green
            $found = $true
        } else {
            Write-Host "  ❌ No match: '$keyword'" -ForegroundColor Red
        }
    }
    
    # Test secondary ethernet check
    $name = $adapter.Name.ToLower()
    if ($name.Contains("ethernet") -and ($name.Contains("2") -or $name.Contains("3") -or $description.Contains("usb"))) {
        Write-Host "  ✅ MATCH: Secondary ethernet pattern" -ForegroundColor Green
        $found = $true
    }
    
    if ($found) {
        Write-Host "`n✅ This interface SHOULD be detected by TabDisplay" -ForegroundColor Green
    } else {
        Write-Host "`n❌ This interface would NOT be detected by TabDisplay" -ForegroundColor Red
        Write-Host "Need to update the detection logic!" -ForegroundColor Red
    }
    
    # Show IP details
    $ip = Get-NetIPAddress -InterfaceIndex $adapter.ifIndex -AddressFamily IPv4 -ErrorAction SilentlyContinue
    if ($ip) {
        Write-Host "`nIP Configuration:" -ForegroundColor Yellow
        Write-Host "  IP: $($ip.IPAddress)"
        Write-Host "  Expected Android Gateway: $(($ip.IPAddress -split '\.')[0..2] -join '.').1"
    }
    
} else {
    Write-Host "❌ No Remote NDIS interface found" -ForegroundColor Red
}

Write-Host "`nTabDisplay should detect this as an Android device and try to connect to the gateway IP."
