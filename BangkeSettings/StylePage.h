#pragma once

#include <QWidget>

#include "UIStyleSettings.h"

class QComboBox;
class QLabel;
class QSpinBox;

class StylePage : public QWidget {
  Q_OBJECT

 public:
  StylePage(QWidget* parent = nullptr);

  void load();
  // 部署后强制重读（含未保存改动的丢弃由调用方决定时机）
  void forceLoad();
  bool save();

 private:
  void updatePreview();

  UIStyleSettings settings_;
  QComboBox* schemeCombo_ = nullptr;
  QLabel* preview_ = nullptr;
  QSpinBox* fontSize_ = nullptr;
  std::string activeScheme_;
  int activeFontSize_ = 0;
  bool loaded_ = false;
};
