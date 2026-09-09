#pragma once
#include <QWidget>

class QComboBox;
class QSpinBox;

// 通用配置页:读写 default.custom.yaml(menu/page_size、ascii_composer/switch_key)。
class GeneralPage : public QWidget {
  Q_OBJECT
 public:
  explicit GeneralPage(QWidget* parent = nullptr);
  void load();
  void save();

 private:
  QSpinBox* pageSize_ = nullptr;
  QComboBox* shiftL_ = nullptr;
  QComboBox* shiftR_ = nullptr;
  QString defaultCustomYaml() const;
};
