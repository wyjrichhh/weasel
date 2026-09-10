#pragma once

#include <QWidget>

#include "UIStyleSettings.h"

#include <map>

class QComboBox;
class QPushButton;
class QSlider;
class QSpinBox;
class SchemePreviewWidget;

// 界面样式页:外观(配色/字号/横竖排)+ 布局微调(圆角/边框/间距/阴影)
// + 自定义配色(取色器/不透明度,写入 preset_color_schemes/bangke_custom 补丁)。
// 读 = 补丁 ⊕ 共享默认;写 = levers 补丁;进入页面即重读,双向同步。
class StylePage : public QWidget {
  Q_OBJECT

 public:
  StylePage(QWidget* parent = nullptr);

  void load();
  // 部署后强制重读（含未保存改动的丢弃由调用方决定时机）
  void forceLoad();
  bool save();

 private slots:
  void updatePreview();

 private:
  void btnRefresh(const char* key);
  void onCustomEdited();
  QPushButton* colorButton(const char* key, const QString& label);

  UIStyleSettings settings_;
  QComboBox* schemeCombo_ = nullptr;
  SchemePreviewWidget* preview_ = nullptr;
  QSpinBox* fontSize_ = nullptr;
  QComboBox* layoutCombo_ = nullptr;

  // 布局微调:键名 → 控件
  struct LayoutSpin {
    QSpinBox* spin;
    const char* key;
  };
  std::vector<LayoutSpin> layoutSpins_;
  std::map<std::string, int> initLayout_;

  // 自定义配色
  std::map<std::string, unsigned int> custom_;  // key → 0xAARRGGBB
  std::map<std::string, unsigned int> initCustom_;
  bool customDirty_ = false;
  QPushButton* backColorBtn_ = nullptr;
  QPushButton* hiliteColorBtn_ = nullptr;
  QPushButton* textColorBtn_ = nullptr;
  QSlider* opacitySlider_ = nullptr;

  std::string activeScheme_;
  int activeFontSize_ = 0;
  bool activeHorizontal_ = false;
  bool loaded_ = false;
};
