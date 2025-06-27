# Compile the direct connector using Visual Studio tools
Write-Host "Setting up Visual Studio environment..."

# Try to find Visual Studio installation
$vsPath = ""
$possiblePaths = @(
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat", 
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
)

foreach ($path in $possiblePaths) {
    if (Test-Path $path) {
        $vsPath = $path
        break
    }
}

if ($vsPath -eq "") {
    Write-Host "Visual Studio not found. Trying simple compilation..."
    
    # Try direct compilation without VS environment
    try {
        Write-Host "Attempting direct compilation..."
        $env:PATH += ";C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.30.30705\bin\Hostx64\x64"
        $env:PATH += ";C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64"
        
        cl.exe direct_connector.cpp /Fe:direct_connector.exe ws2_32.lib iphlpapi.lib /EHsc
        
        if (Test-Path "direct_connector.exe") {
            Write-Host "✅ Compilation successful!"
            Write-Host "Running direct connector..."
            .\direct_connector.exe
        } else {
            Write-Host "❌ Compilation failed"
        }
    } catch {
        Write-Host "❌ Direct compilation failed: $($_.Exception.Message)"
    }
} else {
    Write-Host "Found Visual Studio at: $vsPath"
    
    # Create a batch script to set up environment and compile
    $batchScript = @"
@echo off
call "$vsPath"
cl.exe direct_connector.cpp /Fe:direct_connector.exe ws2_32.lib iphlpapi.lib /EHsc
if exist direct_connector.exe (
    echo ✅ Compilation successful!
    echo Running direct connector...
    direct_connector.exe
) else (
    echo ❌ Compilation failed
)
pause
"@
    
    $batchScript | Out-File -FilePath "compile_and_run.bat" -Encoding ASCII
    Write-Host "Created compile_and_run.bat - executing..."
    Start-Process -FilePath "compile_and_run.bat" -Wait
}
