# Windows 构建环境（17 机器）

Phase 0 基线：weasel 0.17.4（d73f629），2026-09-04 全量构建 + 打字验证通过。

## 工具链

| 组件 | 版本/路径 | 说明 |
|---|---|---|
| VS2022 Community | 17.14.39，MSVC 14.44.35207 | workload NativeDesktop + **ATL v143**（WTL 依赖）；不需要 MFC（见下） |
| Boost | 1.84.0，`C:\Libraries\boost_1_84_0` | 必须用 `archives.boost.io` 官方源码包；GitHub Release 的 7z 是无子模块超项目快照，没有 `boost/` 头文件 |
| cmake | 3.31.7 便携版，`C:\Libraries\cmake-3.31.7-windows-x86_64` | **不能用 4.x**（移除 <3.5 兼容，yaml-cpp 等老依赖全挂） |
| Python | 3.12.8，`C:\Libraries\Python312` | librime `data/CMakeLists.txt` 的 find_package(PythonInterp REQUIRED) 硬依赖 |
| Git / 7-Zip | winget 安装 | git-bash 供 plum 数据打包用 |

`env.bat`（仓库根，gitignored）关键项：`BOOST_ROOT` / `BJAM_TOOLSET=msvc-14.3` / `PLATFORM_TOOLSET=v143` / `CMAKE_GENERATOR="Visual Studio 17 2022"` / `DEVTOOLS_PATH`（Python 和 cmake 的便携目录挂最前，sshd 会话不会刷新 PATH，全靠这个变量）。

## 本地补丁（已提交）

1. **b2 msvc.jam**（`C:\Libraries\boost_1_84_0\tools\build\src\tools\msvc.jam`，**不进 git**，重建 boost 环境时重打）：首分支 `if [ MATCH "(14.3)"...]` 改为 `(14.[1-9])`。原因：b2 4.10 的版本正则不认 MSVC 14.44，走老分支拼出不存在的 `bin\Hostx64\vcvarsall.bat` 依赖目标，全部编译目标被 skip。
2. **afxres.h → windows.h**（`WeaselTSF/WeaselTSF.rc`、`WeaselServer/WeaselServer.rc`，已提交）：`.rc` 里的 `afxres.h` 是 MFC 头，官方 CI 的 runner 镜像自带 MFC 所以没暴露；替换成 `windows.h` 后永久去掉 MFC 组件依赖。注意两个 `.rc` 都是 **UTF-16LE**，改完要保编码。

## 构建序列

```bat
cd C:\dev\weasel
build.bat boost     :: b2 双架构静态库，约 10 分钟
build.bat rime      :: librime deps + rime.dll，约 30 分钟
build.bat data opencc  :: plum 配方 + opencc 词库（走 GitHub，网络抖时重跑）
build.bat           :: msbuild 全解决方案（x64 + Win32）
```

## 开发态安装（免 NSIS）

```powershell
# 管理员会话；server 必须在交互桌面会话里启动，不要从 ssh 拉起
cd C:\dev\weasel\output
Copy-Item .\weaselx64.dll C:\Windows\System32\weasel.dll -Force
Copy-Item .\weasel.dll C:\Windows\SysWOW64\weasel.dll -Force
cmd /c "set TEXTSERVICE_PROFILE=hans&& C:\Windows\System32\regsvr32.exe /s C:\Windows\System32\weasel.dll"
cmd /c "set TEXTSERVICE_PROFILE=hans&& C:\Windows\SysWOW64\regsvr32.exe /s C:\Windows\SysWOW64\weasel.dll"
New-Item HKLM:\SOFTWARE\Rime\Weasel -Force
Set-ItemProperty HKLM:\SOFTWARE\Rime\Weasel WeaselRoot 'C:\dev\weasel\output'
Set-ItemProperty HKLM:\SOFTWARE\Rime\Weasel ServerExecutable 'WeaselServer.exe'
New-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Run' WeaselServer -PropertyType String -Value 'C:\dev\weasel\output\WeaselServer.exe' -Force
```

首次部署**不需要**跑 `WeaselDeployer.exe /deploy`（它是先弹方案选择对话框再部署，无桌面会话里会永远挂起等输入）——直接启动 `WeaselServer.exe`，`Initialize()` 的 `start_maintenance` 会自动完成首次部署（编译词库约 1-3 分钟）。

## 远程驱动（Mac → 17-Windows）

`ssh bangke@192.168.101.17`（公钥免密）。执行 PS 用 EncodedCommand：

```bash
ENC=$(iconv -f UTF-8 -t UTF-16LE script.ps1 | base64 | tr -d '\n')
ssh bangke@192.168.101.17 "powershell -NoProfile -EncodedCommand $ENC"
```

勿用 `powershell -Command -` + stdin（多行代码块被逐行执行搞坏）、勿裸引号内联（cmd 抢解析管道符）。
