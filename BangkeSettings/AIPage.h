#pragma once
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QSpinBox;

// AI 预测配置页:读写 schema custom.yaml 的 ai_predict: 块(12 个键)。
// 保存时由 MainWindow 统一触发重新部署。
class AIPage : public QWidget {
  Q_OBJECT
 public:
  explicit AIPage(QWidget* parent = nullptr);

  void load();
  void save();

 private:
  QCheckBox* enabled_ = nullptr;
  QComboBox* device_ = nullptr;
  QSpinBox* maxTokens_ = nullptr;
  QSpinBox* debounce_ = nullptr;
  QSpinBox* minInput_ = nullptr;
  QSpinBox* minHanzi_ = nullptr;
  QDoubleSpinBox* quality_ = nullptr;
  QSpinBox* targetIndex_ = nullptr;
  QSpinBox* searchRange_ = nullptr;
  QSpinBox* contextWindow_ = nullptr;
  QSpinBox* minContextPrompt_ = nullptr;
  class QLineEdit* modelPath_ = nullptr;

  QString schemaCustomYaml() const;
};
