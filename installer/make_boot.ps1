# 生成单文件分发 exe:图形安装器 + 最新 MSI + Qt 运行库 全部嵌进
# 一个原生引导壳(installer\BangkeBoot)。
# 用法: 在仓库根,先完成 build.bat settings / build_installer.bat / build_msi.bat,
#       再 powershell -File installer\make_boot.ps1
# 产物: dist\BangkeSetup-<版本>-win64.exe —— 单文件,双击即图形安装
$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [Text.Encoding]::UTF8
$root = Split-Path $PSScriptRoot -Parent
$output = Join-Path $root "output"

$msi = Get-ChildItem $output -Filter "BangkeSetup-*.msi" |
  Sort-Object Name | Select-Object -Last 1
if (-not $msi) { throw "output 下没有 BangkeSetup-*.msi,先跑 installer\build_msi.bat" }
$version = $msi.BaseName -replace '^BangkeSetup-', '' -replace '-x64$', ''

# ---- 收集载荷清单(引导壳按相对路径解包) ----
$payload = [System.Collections.Generic.List[string]]::new()
$payload.Add("BangkeInstaller.exe")
$payload.Add($msi.Name)
foreach ($n in @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Network.dll")) {
  if (Test-Path (Join-Path $output $n)) { $payload.Add($n) }
}
foreach ($d in @("platforms", "styles", "imageformats", "tls", "networkinformation", "generic")) {
  $p = Join-Path $output $d
  if (Test-Path $p) {
    Get-ChildItem $p -Recurse -File | ForEach-Object {
      $payload.Add($_.FullName.Substring($output.Length + 1))
    }
  }
}
# VC 运行库(Qt 为 /MD)
Get-ChildItem $output -File | Where-Object {
  $_.Name -match '^(msvcp|vcruntime|vccorlib|concrt|ucrtbase)' } | ForEach-Object {
  $payload.Add($_.Name)
}
if ($payload -notcontains "BangkeInstaller.exe") { throw "output\BangkeInstaller.exe 缺失" }

# ---- 生成资源脚本:IDR_MANIFEST(100) = 清单,101.. = 文件 ----
$build = Join-Path $PSScriptRoot "BangkeBoot\build"
New-Item $build -ItemType Directory -Force | Out-Null
$id = 100
$manifest = New-Object System.Text.StringBuilder
$rc = New-Object System.Text.StringBuilder
[void]$rc.AppendLine("// 自动生成:make_boot.ps1,勿手改")
foreach ($rel in $payload) {
  $id++
  [void]$manifest.AppendLine("$id|$rel")
  [void]$rc.AppendLine("$id RCDATA `"$($output.Replace('\', '\\'))\\$($rel.Replace('\', '\\'))`"")
}
[void]$rc.AppendLine("100 RCDATA `"$build\payload.manifest`"")
[IO.File]::WriteAllText("$build\payload.manifest", $manifest.ToString(),
                        (New-Object Text.UTF8Encoding($false)))
[IO.File]::WriteAllText("$build\boot_payload.rc", $rc.ToString(),
                        (New-Object Text.UTF8Encoding($false)))

# ---- 编译引导壳 ----
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
$devcmd = Join-Path $vs "Common7\Tools\VsDevCmd.bat"
$src = Join-Path $PSScriptRoot "BangkeBoot\main.cpp"
$exe = Join-Path $build "BangkeBoot.exe"
$cmd = "`"$devcmd`" -arch=amd64 -host_arch=amd64 && cl /nologo /O2 /MT /DUNICODE /D_UNICODE " +
       "$($src.Replace('\', '\')) $($build.Replace('\', '\'))\boot_payload.rc /Fo`"$($build.Replace('\', '\'))\`" " +
       "/Fe`"$($exe.Replace('\', '\'))`" /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib shlwapi.lib"
cmd /c $cmd | Out-Null
if (-not (Test-Path $exe)) { throw "引导壳编译失败(需 VsDevCmd 环境,或看上方 cl 输出)" }

New-Item (Join-Path $root "dist") -ItemType Directory -Force | Out-Null
$dist = Join-Path $root "dist\BangkeSetup-$version-win64.exe"
Copy-Item $exe $dist -Force
Write-Output ("dist: " + $dist + "  " + [math]::Round((Get-Item $dist).Length / 1MB, 1) + "MB (单文件,双击即装)")
