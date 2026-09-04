# Removes a dev-mode Bangke Pinyin install. Official Weasel is not touched.
# Run from an elevated PowerShell.
$ErrorActionPreference = 'Continue'

Stop-Process -Name BangkeServer -Force -ErrorAction SilentlyContinue

$env:TEXTSERVICE_PROFILE = 'hans'
& "$env:SystemRoot\System32\regsvr32.exe" /s /u C:\Windows\System32\bangke.dll
Write-Host "regsvr32 /u exit: $LASTEXITCODE"

Remove-Item C:\Windows\System32\bangke.dll -Force -ErrorAction SilentlyContinue
Remove-Item 'HKLM:\SOFTWARE\Bangke' -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item 'HKCU:\SOFTWARE\Bangke' -Recurse -Force -ErrorAction SilentlyContinue
Remove-ItemProperty 'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Run' -Name BangkeServer -ErrorAction SilentlyContinue

# COM registration leftovers from DllRegisterServer
Remove-Item 'HKCR:\CLSID\{9D44BD49-B647-4010-9ADC-16DA253F5CCA}' -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item 'HKLM:\SOFTWARE\Classes\CLSID\{9D44BD49-B647-4010-9ADC-16DA253F5CCA}' -Recurse -Force -ErrorAction SilentlyContinue

Write-Host 'uninstalled (user data dir %APPDATA%\Bangke kept).'
