# 蚌壳拼音 · Bangke Pinyin

Windows 平台的 [Rime](https://rime.im) 输入法,基于 [rime/weasel](https://github.com/rime/weasel)(小狼毫)深度定制的 x64-only 分支。

## 特性

- **AI 候选预测**:内置 [librime-ai-predict](librime/plugins/librime-ai-predict) 插件,CTranslate2 本地推理,长句输入时 AI 候选实时浮现(带 `AI` 标记),按键序号直选上屏
- **内置方案**:明月拼音(`luna_pinyin`)与雾凇拼音(`rime_ice`,含词库与 librime-lua),安装即用
- **现代候选面板**:纯 Direct2D 绘制,胶囊高亮、圆角、环阴影、逐像素半透明,跟随系统深浅色;8 套精选配色 + 完全自定义
- **Qt 设置程序**:通用 / 界面样式 / AI / 高级四页,微信式行式排版与滑动开关,保存即热部署
- **WiX MSI 安装器**:带 Qt 前端(装/修/卸/升级),全新安装自动播种用户配置与 AI 模型
- **版本化二进制协议**:服务端↔TSF 前端走帧协议,按会话共享内存推送 AI 快照

## 构建

见 [docs/win-build.md](docs/win-build.md)(Visual Studio 2022 + boost + Qt6 + WiX,x64-only)。

```bat
build.bat          :: weasel 主件(dll/server/tests)
build.bat settings :: Qt 设置程序
build.bat data     :: 数据(plum + 雾凇离线铺设)
build_msi.bat      :: MSI 安装包
```

## 目录

| 目录 | 内容 |
|---|---|
| `BangkeTSF/` | TSF 文本服务(前端,候选窗在应用进程内绘制) |
| `BangkeServer/` | 算法服务进程 |
| `BangkeIPC/` `BangkeIPCServer/` | 管道通信与二进制协议 |
| `BangkeUI/` | Direct2D 候选面板与布局 |
| `BangkeRime/` | Rime 集成层 |
| `BangkeSettings/` | Qt 设置程序 |
| `installer/` | WiX MSI + 安装器前端 |
| `librime/plugins/librime-ai-predict/` | AI 预测插件 |

> 兼容性说明:rime 配置节点沿用了上游的 `weasel.yaml` / `weasel.custom.yaml` 文件名,以保持用户配置习惯与文档通用性。

## 致谢

- [rime/weasel](https://github.com/rime/weasel) 及其贡献者 —— 本项目的基础
- [rime](https://github.com/rime/librime) 输入法引擎
- [雾凇拼音 rime-ice](https://github.com/iDvel/rime-ice)
- [CTranslate2](https://github.com/OpenNMT/CTranslate2)

## 许可

GPL-3.0,见 [LICENSE.txt](LICENSE.txt)。
