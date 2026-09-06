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

# 收集目录结构（相对路径集合）
$dirs = @{}
foreach ($f in $files) {
  $rel = [IO.Path]::GetRelativePath($root, $f.FullName)
  $parent = Split-Path $rel -Parent
  if (-not $parent) { $parent = '.' }
  $name = Split-Path $rel -Leaf
  $did = DirId $parent
  if (-not $dirs.ContainsKey($did)) {
    $dirs[$did] = @{ Name = if ($parent -eq '.') { $null } else { (Split-Path $parent -Leaf) }; Parent = $null }
  }
  # 祖先目录全部登记
  $cur = $parent
  while ($cur -and $cur -ne '.') {
    $pParent = Split-Path $cur -Parent
    if (-not $pParent) { $pParent = '.' }
    $cid = DirId $cur
    if (-not $dirs.ContainsKey($cid)) {
      $dirs[$cid] = @{ Name = Split-Path $cur -Leaf; Parent = $(DirId $pParent) }
    }
    $cur = $pParent
    if ($pParent -eq '.') { break }
  }
}

function Esc([string]$s) { return [System.Security.SecurityElement]::Escape($s) }

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('<?xml version="1.0" encoding="utf-8"?>')
[void]$sb.AppendLine('<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">')
[void]$sb.AppendLine('  <Fragment>')
[void]$sb.AppendLine('    <DirectoryRef Id="INSTALLDIR">')

# 目录树：先输出父目录再嵌套子目录（用栈保证深度）
$emitted = @{}
function EmitDir([string]$did) {
  if ($did -eq 'INSTALLDIR' -or $emitted.ContainsKey($did)) { return }
  $d = $dirs[$did]
  EmitDir $d.Parent
  $indent = '      '
  [void]$script:sb.AppendLine("$indent<Directory Id=""$did"" Name=""$(Esc $d.Name)"" />")
  $script:emitted[$did] = $true
}
foreach ($did in @($dirs.Keys)) { EmitDir $did }
[void]$sb.AppendLine('    </DirectoryRef>')
[void]$sb.AppendLine('    <ComponentGroup Id="BangkeFiles">')

$i = 0
foreach ($f in $files) {
  $rel = [IO.Path]::GetRelativePath($root, $f.FullName)
  $parent = Split-Path $rel -Parent
  if (-not $parent) { $parent = '.' }
  $did = DirId $parent
  $i++
  $fid = 'file' + $i
  $src = $f.FullName -replace '/', '\'
  [void]$sb.AppendLine("      <Component Id=""comp$i"" Directory=""$did"" Guid=""*"" Win64=""yes"">")
  [void]$sb.AppendLine("        <File Id=""$fid"" Source=""$(Esc $src)"" Name=""$(Esc $f.Name)"" KeyPath=""yes"" />")
  [void]$sb.AppendLine("      </Component>")
}
[void]$sb.AppendLine('    </ComponentGroup>')
[void]$sb.AppendLine('  </Fragment>')
[void]$sb.AppendLine('</Wix>')

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText($OutFile, $sb.ToString(), $utf8NoBom)
"harvest: $($files.Count) files -> $OutFile"
