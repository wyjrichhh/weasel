# 从 output\ 递归生成 Files.wxs（BangkeFiles ComponentGroup）
# 排除项由 $exclude 定义；bangkex64.dll 由主 wxs 单独装到 System32
param(
  [string]$OutputDir = "output",
  [string]$OutFile = "installer\Files.wxs"
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path $OutputDir).Path

# 兜底：Qt(/MD) 依赖 VC 运行库，随程序目录捆绑，防目标机 System32 CRT 异常
if (-not (Test-Path "$root\msvcp140.dll")) {
  $crt = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT\msvcp140.dll' -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1
  if ($crt) {
    Copy-Item (Join-Path $crt.Directory '*.dll') $root
    "harvest: bundled VC runtime from $($crt.Directory)"
  } else {
    "harvest: WARNING - no VC runtime found in output and none to copy"
  }
}

$exclude = @('bangkex64.dll', 'weasel.log', 'BangkeDeployer.exe')
$excludeExt = @('.log', '.pdb', '.old', '.msi')

$files = Get-ChildItem $root -Recurse -File | Where-Object {
  ($exclude -notcontains $_.Name) -and ($excludeExt -notcontains $_.Extension.ToLower())
}

function DirId([string]$rel) {
  if ($rel -eq '.') { return 'INSTALLDIR' }
  return 'dir_' + ($rel -replace '[\\./ ]', '_')
}

# 目录树登记：rel 为 output 下的相对路径（'.' 为根）
$dirs = @{}
function EnsureDir([string]$rel) {
  if ($rel -eq '.') { return }
  $did = DirId $rel
  if (-not $dirs.ContainsKey($did)) {
    $pp = Split-Path $rel -Parent
    if (-not $pp) { $pp = '.' }
    EnsureDir $pp
    $dirs[$did] = @{ Name = (Split-Path $rel -Leaf); Parent = (DirId $pp) }
  }
}

function Esc([string]$s) { return [System.Security.SecurityElement]::Escape($s) }

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('<?xml version="1.0" encoding="utf-8"?>')
[void]$sb.AppendLine('<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">')
[void]$sb.AppendLine('  <Fragment>')
[void]$sb.AppendLine('    <DirectoryRef Id="INSTALLDIR">')

# 深度优先：先父后子输出
$emitted = @{}
function EmitDir([string]$did) {
  if (-not $did -or $did -eq 'INSTALLDIR' -or $emitted.ContainsKey($did)) { return }
  $d = $dirs[$did]
  EmitDir $d.Parent
  [void]$script:sb.AppendLine("      <Directory Id=""$did"" Name=""$(Esc $d.Name)"" />")
  $script:emitted[$did] = $true
}

$comps = New-Object System.Text.StringBuilder
$i = 0
foreach ($f in $files) {
  $rel = $f.FullName.Substring($root.Length + 1)
  $parent = Split-Path $rel -Parent
  if (-not $parent) { $parent = '.' }
  EnsureDir $parent
  $i++
  $src = $f.FullName -replace '/', '\'
  $did = DirId $parent
  [void]$comps.AppendLine("      <Component Id=""comp$i"" Directory=""$did"" Guid=""*"">")
  [void]$comps.AppendLine("        <File Id=""file$i"" Source=""$(Esc $src)"" Name=""$(Esc $f.Name)"" KeyPath=""yes"" />")
  [void]$comps.AppendLine("      </Component>")
}
foreach ($did in @($dirs.Keys)) { EmitDir $did }
[void]$sb.AppendLine('    </DirectoryRef>')
[void]$sb.AppendLine("    <ComponentGroup Id=""BangkeFiles"">")
[void]$sb.Append($comps.ToString())
[void]$sb.AppendLine('    </ComponentGroup>')
[void]$sb.AppendLine('  </Fragment>')
[void]$sb.AppendLine('</Wix>')

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText($OutFile, $sb.ToString(), $utf8NoBom)
"harvest: $($files.Count) files -> $OutFile"
