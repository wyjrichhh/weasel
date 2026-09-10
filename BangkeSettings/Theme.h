#pragma once

class QApplication;

// 跟随系统深浅的 QSS 主题。
// 启动时 Apply 一次;MainWindow 的 nativeEvent 收到 ImmersiveColorSet
// 再调 Apply 即时切换(样式表+调色板+DWM 标题栏)。
namespace Theme {

void Apply(QApplication& app);
// 与上次应用时相比系统主题是否变化,变则重应用
void RefreshIfChanged();

}  // namespace Theme
