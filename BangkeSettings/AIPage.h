#pragma once
#include <QWidget>

#include <QStringList>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QSpinBox;

// AI 预测配置页:读写 schema custom.yaml 的 ai_predict: 块(12 个键),
// 管辖所有接了 ai_predict 的方案;保存时由 MainWindow 统一触发重新部署。
class AIPage : public QWidget {
  Q_OBJECT
 public:
  explicit AIPage(QWidget* parent = nullptr);

  void load();
  bool save();

 private:
  QString stateSignature() const;

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

  QStringList wiredSchemaCustomYamls() const;
  // 载入时的控件状态签名;保存仅在有差异时写文件
  QString initState_;
};
