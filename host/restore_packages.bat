@echo off
echo Installing Windows App SDK via NuGet...

REM Download NuGet.exe if it doesn't exist
if not exist nuget.exe (
    echo Downloading NuGet.exe...
    powershell -Command "Invoke-WebRequest -Uri 'https://dist.nuget.org/win-x86-commandline/latest/nuget.exe' -OutFile 'nuget.exe'"
)

REM Restore packages
echo Restoring packages...
nuget.exe restore packages.config -PackagesDirectory packages

echo Done! Windows App SDK should now be available in the packages directory.
pause
