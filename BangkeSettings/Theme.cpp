#include "Theme.h"

#include <QApplication>
#include <QFont>
#include <QGuiApplication>
#include <QPalette>
#include <QWidget>
#include <QWindow>
#include <windows.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

namespace Theme {
namespace {

QApplication* g_app = nullptr;
int g_appliedLight = -1;

bool SystemIsLight() {
  HKEY h = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\"
                    L"Personalize",
                    0, KEY_QUERY_VALUE, &h) != ERROR_SUCCESS)
    return true;
  DWORD v = 1, sz = sizeof(v), type = 0;
  const LONG r = RegQueryValueExW(h, L"AppsUseLightTheme", nullptr, &type,
                                  (LPBYTE)&v, &sz);
  RegCloseKey(h);
  return r != ERROR_SUCCESS || v != 0;
}

// DWMWA_USE_IMMERSIVE_DARK_MODE:Win10 1809+ 为 19/20 均可
void ApplyTitleBars(bool light) {
  const BOOL dark = light ? FALSE : TRUE;
  for (auto* widget : QApplication::topLevelWidgets()) {
    if (auto* handle = widget->windowHandle())
      DwmSetWindowAttribute((HWND)handle->winId(), 20, &dark, sizeof(dark));
  }
}

const char* kLightQss = R"(
* { font-family: "Microsoft YaHei UI"; font-size: 9.5pt; }
QMainWindow, QDialog { background: #F5F6F7; }
QFrame#pageRoot { background: transparent; }
QFrame#card { background: #FFFFFF; border: 1px solid #E7E9EC; border-radius: 8px; }
QFrame#line { border: none; border-top: 1px solid #E7E9EC; }
QLabel#cardTitle { font-size: 9.5pt; font-weight: 600; color: #6B7280; }
QLabel#hint { color: #6B7280; }
QLabel#rowTitle { font-weight: 600; color: #1F2329; }
QLabel#rowHint { color: #6B7280; font-size: 8.5pt; }
QSlider::groove:horizontal { height: 4px; border-radius: 2px; background: #E4E6EA; }
QSlider::sub-page:horizontal { background: #0D9488; border-radius: 2px; }
QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; background: #FFFFFF; border: 1px solid #0D9488; }
QSlider::handle:horizontal:hover { border-color: #0B6E64; }
QLabel { color: #1F2329; background: transparent; }

QScrollArea#pageScroll { background: transparent; border: none; }
QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
QScrollBar::handle:vertical { background: #D3D7DC; border-radius: 4px; min-height: 32px; }
QScrollBar::handle:vertical:hover { background: #B9BFC7; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal { background: transparent; height: 8px; margin: 0; }
QScrollBar::handle:horizontal { background: #D3D7DC; border-radius: 4px; min-width: 32px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }

QListWidget#nav {
  background: transparent; border: none; outline: none; padding: 4px 2px;
}
QListWidget#nav::item {
  padding: 7px 10px; margin: 2px 4px; border-radius: 6px; color: #3E434B;
}
QListWidget#nav::item:hover { background: rgba(13, 148, 136, 0.07); }
QListWidget#nav::item:selected {
  background: #DFF0EE; color: #0B6E64; font-weight: 600;
}

QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit {
  background: #FFFFFF; border: 1px solid #D9DCE1; border-radius: 6px;
  padding: 3px 8px; min-height: 20px; color: #1F2329;
}
QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover, QLineEdit:hover {
  border-color: #0D9488;
}
QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QLineEdit:focus {
  border: 1px solid #0D9488;
}
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
  background: #FFFFFF; border: 1px solid #E4E6EA; border-radius: 6px;
  selection-background-color: #DFF0EE; selection-color: #0B6E64; outline: none;
  padding: 4px;
}
QSpinBox::up-button, QSpinBox::down-button,
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
  background: transparent; border: none; width: 16px;
}

QPushButton {
  background: #FFFFFF; border: 1px solid #D9DCE1; border-radius: 6px;
  padding: 5px 14px; color: #1F2329;
}
QPushButton:hover { border-color: #0D9488; color: #0B6E64; }
QPushButton:pressed { background: #F0FAF9; }
QPushButton:disabled { color: #9CA3AF; border-color: #E4E6EA; }
QPushButton#primary {
  background: #0D9488; border: 1px solid #0D9488; color: #FFFFFF;
  font-weight: 600;
}
QPushButton#primary:hover { background: #0B8177; }
QPushButton#primary:pressed { background: #096E66; }
QPushButton#primary:disabled { background: #9DD8D1; border-color: #9DD8D1; color: #F2FFFE; }

QListWidget#dictList, QTextBrowser, QListWidget {
  background: #FFFFFF; border: 1px solid #E4E6EA; border-radius: 6px;
  padding: 4px; color: #1F2329; outline: none;
}
QListWidget::item { padding: 5px 8px; border-radius: 4px; }
QListWidget::item:hover { background: #F2F4F6; }
QListWidget::item:selected { background: #DFF0EE; color: #0B6E64; }

QCheckBox { color: #1F2329; spacing: 8px; }
QLabel#toast {
  background: rgba(31, 35, 41, 232); color: #FFFFFF; border-radius: 8px;
  padding: 8px 18px; font-weight: 600;
}
QToolTip { background: #FFFFFF; color: #1F2329; border: 1px solid #D9DCE1; padding: 4px 8px; }
QMessageBox { background: #FFFFFF; }
)";

const char* kDarkQss = R"(
* { font-family: "Microsoft YaHei UI"; font-size: 9.5pt; }
QMainWindow, QDialog { background: #1B1B1E; }
QFrame#pageRoot { background: transparent; }
QFrame#card { background: #242428; border: 1px solid #33333A; border-radius: 8px; }
QFrame#line { border: none; border-top: 1px solid #2A2A30; }
QLabel#cardTitle { font-size: 9.5pt; font-weight: 600; color: #9AA0A6; }
QLabel#hint { color: #9AA0A6; }
QLabel#rowTitle { font-weight: 600; color: #E6E7EA; }
QLabel#rowHint { color: #9AA0A6; font-size: 8.5pt; }
QSlider::groove:horizontal { height: 4px; border-radius: 2px; background: #3A3A42; }
QSlider::sub-page:horizontal { background: #14B8A6; border-radius: 2px; }
QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; background: #2B2B30; border: 1px solid #14B8A6; }
QSlider::handle:horizontal:hover { border-color: #5EEAD4; }
QLabel { color: #E6E7EA; background: transparent; }

QScrollArea#pageScroll { background: transparent; border: none; }
QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
QScrollBar::handle:vertical { background: #3A3A42; border-radius: 4px; min-height: 32px; }
QScrollBar::handle:vertical:hover { background: #4A4A54; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal { background: transparent; height: 8px; margin: 0; }
QScrollBar::handle:horizontal { background: #3A3A42; border-radius: 4px; min-width: 32px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }

QListWidget#nav {
  background: transparent; border: none; outline: none; padding: 4px 2px;
}
QListWidget#nav::item {
  padding: 7px 10px; margin: 2px 4px; border-radius: 6px; color: #B4B8BE;
}
QListWidget#nav::item:hover { background: rgba(20, 184, 166, 0.10); }
QListWidget#nav::item:selected {
  background: #123F3A; color: #5EEAD4; font-weight: 600;
}

QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit {
  background: #2B2B30; border: 1px solid #3A3A42; border-radius: 6px;
  padding: 3px 8px; min-height: 20px; color: #E6E7EA;
}
QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover, QLineEdit:hover {
  border-color: #14B8A6;
}
QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QLineEdit:focus {
  border: 1px solid #14B8A6;
}
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
  background: #2B2B30; border: 1px solid #3A3A42; border-radius: 6px;
  selection-background-color: #123F3A; selection-color: #5EEAD4; outline: none;
  padding: 4px;
}
QSpinBox::up-button, QSpinBox::down-button,
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
  background: transparent; border: none; width: 16px;
}

QPushButton {
  background: #2B2B30; border: 1px solid #3A3A42; border-radius: 6px;
  padding: 5px 14px; color: #E6E7EA;
}
QPushButton:hover { border-color: #14B8A6; color: #5EEAD4; }
QPushButton:pressed { background: #263230; }
QPushButton:disabled { color: #6B7280; border-color: #33333A; }
QPushButton#primary {
  background: #14B8A6; border: 1px solid #14B8A6; color: #08201C;
  font-weight: 600;
}
QPushButton#primary:hover { background: #19CDB9; }
QPushButton#primary:pressed { background: #0FA394; }
QPushButton#primary:disabled { background: #1E4B45; border-color: #1E4B45; color: #7AB8AE; }

QListWidget#dictList, QTextBrowser, QListWidget {
  background: #2B2B30; border: 1px solid #3A3A42; border-radius: 6px;
  padding: 4px; color: #E6E7EA; outline: none;
}
QListWidget::item { padding: 5px 8px; border-radius: 4px; }
QListWidget::item:hover { background: #33333A; }
QListWidget::item:selected { background: #123F3A; color: #5EEAD4; }

QCheckBox { color: #E6E7EA; spacing: 8px; }
QLabel#toast {
  background: rgba(20, 184, 166, 235); color: #08201C; border-radius: 8px;
  padding: 8px 18px; font-weight: 600;
}
QToolTip { background: #2B2B30; color: #E6E7EA; border: 1px solid #3A3A42; padding: 4px 8px; }
QMessageBox { background: #242428; }
)";

}  // namespace

void Apply(QApplication& app) {
  g_app = &app;
  const bool light = SystemIsLight();
  g_appliedLight = light ? 1 : 0;

  app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
  app.setStyleSheet(QString::fromUtf8(light ? kLightQss : kDarkQss));

  // QSS 覆盖不到的零散颜色(菜单/弹出等)走调色板兜底
  QPalette pal;
  if (light) {
    pal.setColor(QPalette::Window, QColor(0xF5, 0xF6, 0xF7));
    pal.setColor(QPalette::Base, QColor(0xFF, 0xFF, 0xFF));
    pal.setColor(QPalette::Text, QColor(0x1F, 0x23, 0x29));
    pal.setColor(QPalette::WindowText, QColor(0x1F, 0x23, 0x29));
    pal.setColor(QPalette::ButtonText, QColor(0x1F, 0x23, 0x29));
    pal.setColor(QPalette::Highlight, QColor(0x0D, 0x94, 0x88));
    pal.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF));
  } else {
    pal.setColor(QPalette::Window, QColor(0x1B, 0x1B, 0x1E));
    pal.setColor(QPalette::Base, QColor(0x2B, 0x2B, 0x30));
    pal.setColor(QPalette::Text, QColor(0xE6, 0xE7, 0xEA));
    pal.setColor(QPalette::WindowText, QColor(0xE6, 0xE7, 0xEA));
    pal.setColor(QPalette::ButtonText, QColor(0xE6, 0xE7, 0xEA));
    pal.setColor(QPalette::Highlight, QColor(0x14, 0xB8, 0xA6));
    pal.setColor(QPalette::HighlightedText, QColor(0x08, 0x20, 0x1C));
  }
  app.setPalette(pal);

  ApplyTitleBars(light);
  for (auto* widget : QApplication::topLevelWidgets())
    widget->update();
}

void RefreshIfChanged() {
  if (!g_app)
    return;
  const bool light = SystemIsLight();
  if ((light ? 1 : 0) != g_appliedLight)
    Apply(*g_app);
}

}  // namespace Theme
