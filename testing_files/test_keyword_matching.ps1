$interfaceName = "Remote NDIS based Internet Sharing Device"
$lowerName = $interfaceName.ToLower()

Write-Host "Testing interface: '$interfaceName'" -ForegroundColor Green
Write-Host "Lowercase: '$lowerName'" -ForegroundColor Yellow

$androidKeywords = @("android", "adb", "rndis", "remote ndis", "usb ethernet", "samsung", "galaxy", "pixel", "nexus")

Write-Host "`nKeyword matching:" -ForegroundColor Cyan
$found = $false
foreach ($keyword in $androidKeywords) {
    if ($lowerName.Contains($keyword)) {
        Write-Host "  ✅ MATCH: '$keyword'" -ForegroundColor Green
        $found = $true
    } else {
        Write-Host "  ❌ No match: '$keyword'" -ForegroundColor Red
    }
}

# Test secondary ethernet pattern
Write-Host "`nSecondary ethernet pattern test:" -ForegroundColor Cyan
if ($lowerName.Contains("ethernet") -and ($lowerName.Contains("2") -or $lowerName.Contains("3") -or $lowerName.Contains("usb"))) {
    Write-Host "  ✅ MATCH: Secondary ethernet pattern" -ForegroundColor Green
    $found = $true
} else {
    Write-Host "  ❌ No match: Secondary ethernet pattern" -ForegroundColor Red
}

Write-Host "`nResult:" -ForegroundColor Yellow
if ($found) {
    Write-Host "✅ This interface SHOULD be detected as Android device" -ForegroundColor Green
} else {
    Write-Host "❌ This interface would NOT be detected" -ForegroundColor Red
}

# Test the interface name "Ethernet 3" separately
Write-Host "`n" + "="*50
$ethernetName = "Ethernet 3"
$ethernetLower = $ethernetName.ToLower()
Write-Host "Testing interface name: '$ethernetName'" -ForegroundColor Green
Write-Host "Lowercase: '$ethernetLower'" -ForegroundColor Yellow

if ($ethernetLower.Contains("ethernet") -and ($ethernetLower.Contains("2") -or $ethernetLower.Contains("3") -or $ethernetLower.Contains("usb"))) {
    Write-Host "✅ 'Ethernet 3' SHOULD match secondary ethernet pattern" -ForegroundColor Green
} else {
    Write-Host "❌ 'Ethernet 3' would NOT match secondary ethernet pattern" -ForegroundColor Red
}
