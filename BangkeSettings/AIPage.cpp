#include "AIPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTextStream>
#include <QRegularExpression>
#include <QVBoxLayout>

#include <WeaselUtility.h>

// schema 名写死 luna_pinyin:目前唯一带 ai_predict 的方案。
// 多方案启用 AI 时应改为读取当前选中方案(留 TODO)。
static const char* kSchemaId = "luna_pinyin";

AIPage::AIPage(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 16, 24, 16);
  layout->setSpacing(8);

  enabled_ = new QCheckBox(QStringLiteral(u"启用 AI 预测"));
  layout->addWidget(enabled_);
  layout->addSpacing(8);

  device_ = new QComboBox;
  device_->addItem(QStringLiteral(u"CPU"), QStringLiteral("cpu"));
  device_->addItem(QStringLiteral(u"CUDA (GPU)"), QStringLiteral("cuda"));
  maxTokens_ = new QSpinBox;
  maxTokens_->setRange(1, 1024);
  debounce_ = new QSpinBox;
  debounce_->setRange(0, 5000);
  debounce_->setSuffix(QStringLiteral(u" ms"));
  minInput_ = new QSpinBox;
  minInput_->setRange(1, 100);
  minHanzi_ = new QSpinBox;
  minHanzi_->setRange(1, 10);

  auto* form = new QFormLayout;
  form->addRow(QStringLiteral(u"设备"), device_);
  form->addRow(QStringLiteral(u"最大 token 数"), maxTokens_);
  form->addRow(QStringLiteral(u"防抖"), debounce_);
  form->addRow(QStringLiteral(u"最小输入长度"), minInput_);
  form->addRow(QStringLiteral(u"最少汉字数"), minHanzi_);
  layout->addLayout(form);

  quality_ = new QDoubleSpinBox;
  quality_->setRange(-1.0, 100.0);
  quality_->setSingleStep(0.1);
  quality_->setDecimals(2);
  targetIndex_ = new QSpinBox;
  targetIndex_->setRange(0, 20);
  searchRange_ = new QSpinBox;
  searchRange_->setRange(1, 100);
  contextWindow_ = new QSpinBox;
  contextWindow_->setRange(1, 100);
  minContextPrompt_ = new QSpinBox;
  minContextPrompt_->setRange(1, 20);
  modelPath_ = new QLineEdit;
  modelPath_->setReadOnly(true);

  auto* adv = new QGroupBox(QStringLiteral(u"高级选项"));
  auto* advForm = new QFormLayout(adv);
  advForm->setContentsMargins(10, 8, 10, 10);
  advForm->addRow(QStringLiteral(u"候选质量 (quality)"), quality_);
  advForm->addRow(QStringLiteral(u"AI 候选显示位置 (target_index)"), targetIndex_);
  advForm->addRow(QStringLiteral(u"去重扫描范围 (search_range)"), searchRange_);
  advForm->addRow(QStringLiteral(u"上下文窗口 (context_window_size)"), contextWindow_);
  advForm->addRow(QStringLiteral(u"上下文最小拼音数"), minContextPrompt_);
  advForm->addRow(QStringLiteral(u"模型路径"), modelPath_);
  layout->addSpacing(16);
  layout->addWidget(adv);
  layout->addStretch(1);

  load();
}

QString AIPage::schemaCustomYaml() const {
  return QString::fromStdWString(WeaselUserDataPath().wstring()) +
         QStringLiteral("/%1.custom.yaml").arg(QLatin1String(kSchemaId));
}

// 逐行扫描 yaml:只取 ai_predict: 块下已知键的值,其余行不管。
// 这是读;写用 save() 里的"替换已知行"策略——与 fcitx5-rime config-ui 同款。
static QString findValue(const QString& yaml, const QString& key) {
  for (const auto& line : yaml.split(QLatin1Char('\n'))) {
    if (line.trimmed().startsWith(key + QLatin1Char(':')))
      return line.mid(line.indexOf(QLatin1Char(':')) + 1).trimmed();
  }
  return QString();
}

void AIPage::load() {
  QFile f(schemaCustomYaml());
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return;
  const QString yaml = QString::fromUtf8(f.readAll());
  f.close();

  enabled_->setChecked(findValue(yaml, QStringLiteral("enabled")) != QStringLiteral("false"));
  const QString dev = findValue(yaml, QStringLiteral("device"));
  device_->setCurrentIndex(dev == QStringLiteral("cuda") ? 1 : 0);
  maxTokens_->setValue(findValue(yaml, QStringLiteral("max_tokens")).toInt());
  debounce_->setValue(findValue(yaml, QStringLiteral("debounce_ms")).toInt());
  minInput_->setValue(findValue(yaml, QStringLiteral("min_input_length")).toInt());
  minHanzi_->setValue(findValue(yaml, QStringLiteral("min_hanzi")).toInt());
  quality_->setValue(findValue(yaml, QStringLiteral("quality")).toDouble());
  targetIndex_->setValue(findValue(yaml, QStringLiteral("target_index")).toInt());
  searchRange_->setValue(findValue(yaml, QStringLiteral("search_range")).toInt());
  contextWindow_->setValue(findValue(yaml, QStringLiteral("context_window_size")).toInt());
  minContextPrompt_->setValue(findValue(yaml, QStringLiteral("min_context_prompt_length")).toInt());
  modelPath_->setText(findValue(yaml, QStringLiteral("model_path")));
}

// 保存:读全文,逐行替换已知键的值;文件或 ai_predict: 块不存在则追加。
// 未管理的行(含注释、engine 配置等)原样保留——与 fcitx5-rime config-ui 同款纪律。
void AIPage::save() {
  QFile f(schemaCustomYaml());
  QString yaml;
  if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    yaml = QString::fromUtf8(f.readAll());
    f.close();
  }

  struct KV { const char* key; QString val; };
  const KV kvs[] = {
      {"enabled", enabled_->isChecked() ? QStringLiteral("true") : QStringLiteral("false")},
      {"device", device_->currentData().toString()},
      {"max_tokens", QString::number(maxTokens_->value())},
      {"debounce_ms", QString::number(debounce_->value())},
      {"min_input_length", QString::number(minInput_->value())},
      {"min_hanzi", QString::number(minHanzi_->value())},
      {"quality", QString::number(quality_->value(), 'f', 2)},
      {"target_index", QString::number(targetIndex_->value())},
      {"search_range", QString::number(searchRange_->value())},
      {"context_window_size", QString::number(contextWindow_->value())},
      {"min_context_prompt_length", QString::number(minContextPrompt_->value())},
      {"model_path", modelPath_->text()},
  };

  // 确保 ai_predict: 块存在
  if (!yaml.contains(QStringLiteral("ai_predict:"))) {
    yaml += QStringLiteral("\nai_predict:\n");
  }
  // 确保 patch 块存在(custom.yaml 必须有 patch: 根)
  if (!yaml.contains(QStringLiteral("patch:"))) {
    yaml.prepend(QStringLiteral("patch:\n"));
  }

  for (const auto& kv : kvs) {
    const QString key = QString::fromLatin1(kv.key);
    QString line;
    // 查找 "  key: oldval" 形式(缩进 2 空格)
    const QRegularExpression rx(
        QStringLiteral("(^|\\n)(\\s*)%1:\\s*[^\\n]*").arg(
            QRegularExpression::escape(key)));
    auto m = rx.match(yaml);
    if (m.hasMatch()) {
      yaml.replace(m.capturedStart(), m.capturedLength(),
                   m.captured(1) + m.captured(2) + key + QStringLiteral(": ") +
                       kv.val);
    } else {
      // 追加到 ai_predict: 块末(找下一个非缩进行前)
      const int ap = yaml.indexOf(QStringLiteral("ai_predict:"));
      int insert = yaml.indexOf(QLatin1Char('\n'), ap);
      // 找块内最后一行的末尾
      while (insert >= 0) {
        const int next = yaml.indexOf(QLatin1Char('\n'), insert + 1);
        if (next < 0)
          break;
        const QString nextLine = yaml.mid(insert + 1, next - insert - 1);
        if (!nextLine.startsWith(QLatin1Char(' ')) && !nextLine.isEmpty())
          break;
        insert = next;
      }
      if (insert >= 0) {
        yaml.insert(insert + 1,
                    QStringLiteral("  %1: %2\n").arg(key, kv.val));
      }
    }
  }

  if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
    f.write(yaml.toUtf8());
    f.close();
  }
}
