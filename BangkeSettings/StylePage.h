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
  bool save();

 private:
  void updatePreview();

  UIStyleSettings settings_;
  QComboBox* schemeCombo_ = nullptr;
  QLabel* preview_ = nullptr;
  QSpinBox* fontSize_ = nullptr;
  std::string activeScheme_;
  int activeFontSize_ = 0;
};
