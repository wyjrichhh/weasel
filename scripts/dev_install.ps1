# Dev-mode install for Bangke Pinyin (x64-only, coexists with official Weasel).
# Run from an elevated PowerShell in the desktop session.
# Usage: .\scripts\dev_install.ps1 [-UserDir <path>] [-NoStart]
param(
  [string]$UserDir = "$env:APPDATA\Bangke",
  [switch]$NoStart
)

$ErrorActionPreference = 'Stop'
$outputDir = Join-Path $PSScriptRoot '..\output' | Resolve-Path
$dll = Join-Path $outputDir 'bangkex64.dll'
$server = Join-Path $outputDir 'BangkeServer.exe'

foreach ($f in @($dll, $server)) {
  if (-not (Test-Path $f)) { throw "not built: $f (run build.bat first)" }
}

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
  throw 'requires elevation (System32 copy + HKLM writes)'
}

# the dll may be loaded by app processes (TSF in-proc COM): swap via rename
Move-Item C:\Windows\System32\bangke.dll C:\Windows\System32\bangke.dll.old -Force -ErrorAction SilentlyContinue
Copy-Item $dll C:\Windows\System32\bangke.dll -Force
Remove-Item C:\Windows\System32\bangke.dll.old -Force -ErrorAction SilentlyContinue

$env:TEXTSERVICE_PROFILE = 'hans'
# regsvr32 is a GUI-subsystem exe: PowerShell cannot capture its exit code
# directly ($LASTEXITCODE stays null), so go through cmd.
cmd /c "`"$env:SystemRoot\System32\regsvr32.exe`" /s C:\Windows\System32\bangke.dll"
if ($LASTEXITCODE -ne 0) { throw ('regsvr32 failed: 0x{0:X8}' -f $LASTEXITCODE) }

New-Item -Path 'HKLM:\SOFTWARE\Bangke' -Force | Out-Null
Set-ItemProperty 'HKLM:\SOFTWARE\Bangke' -Name BangkeRoot -Value $outputDir.Path
Set-ItemProperty 'HKLM:\SOFTWARE\Bangke' -Name ServerExecutable -Value 'BangkeServer.exe'

New-Item -Path 'HKCU:\SOFTWARE\Bangke' -Force | Out-Null
Set-ItemProperty 'HKCU:\SOFTWARE\Bangke' -Name BangkeUserDir -Value $UserDir

New-ItemProperty 'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Run' -Name BangkeServer `
  -PropertyType String -Value $server -Force | Out-Null

Write-Host "installed. user data dir: $UserDir"
Write-Host 'first launch of BangkeServer performs the initial deployment (1-3 min).'

if (-not $NoStart) {
  Start-Process $server
  Write-Host 'BangkeServer started - switch input methods with Win+Space.'
} else {
  Write-Host 'start BangkeServer.exe manually (do NOT launch it from an ssh session).'
}
