#include "AIPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QStringList>
#include <QTextStream>
#include <QRegularExpression>
#include <QVBoxLayout>

#include "Ui.h"
#include <WeaselUtility.h>

// schema 名写死 luna_pinyin:目前唯一带 ai_predict 的方案。
// 多方案启用 AI 时应改为读取当前选中方案(留 TODO)。
static const char* kSchemaId = "luna_pinyin";

AIPage::AIPage(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(12);

  enabled_ = new QCheckBox(QStringLiteral(u"启用 AI 预测"));
  auto* basic = new QVBoxLayout();
  basic->addWidget(enabled_);

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
  form->setSpacing(8);
  form->addRow(QStringLiteral(u"设备"), device_);
  form->addRow(QStringLiteral(u"最大 token 数"), maxTokens_);
  form->addRow(QStringLiteral(u"防抖"), debounce_);
  form->addRow(QStringLiteral(u"最小输入长度"), minInput_);
  form->addRow(QStringLiteral(u"最少汉字数"), minHanzi_);
  basic->addLayout(form);
  layout->addWidget(makeCard(QStringLiteral(u"基本"), basic));

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

  initState_ = stateSignature();
}

QString AIPage::stateSignature() const {
  return QStringList{
             enabled_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"),
             device_->currentData().toString(),
             QString::number(maxTokens_->value()),
             QString::number(debounce_->value()),
             QString::number(minInput_->value()),
             QString::number(minHanzi_->value()),
             QString::number(quality_->value()),
             QString::number(targetIndex_->value()),
             QString::number(searchRange_->value()),
             QString::number(contextWindow_->value()),
             QString::number(minContextPrompt_->value()),
             modelPath_->text(),
         }
      .join(QLatin1Char('|'));
}

// 保存:读全文,逐行替换已知键的值;文件或 ai_predict: 块不存在则追加。
// 未管理的行(含注释、engine 配置等)原样保留——与 fcitx5-rime config-ui 同款纪律。
bool AIPage::save() {
  const QString sig = stateSignature();
  if (sig == initState_)
    return false;

  QFile f(schemaCustomYaml());
  QString yaml;
  if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    yaml = QString::fromUtf8(f.readAll());
    f.close();
  }

  // 键值对;写入时必须在 ai_predict: 块内(4 空格缩进,patch:→ai_predict:→键)
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

  // 定位 ai_predict: 块:在 patch: 下的 "  ai_predict:" 行
  // 块内键的缩进是 4 空格( "    key: value" )
  const int apStart = yaml.indexOf(QStringLiteral("  ai_predict:"));
  if (apStart < 0)
    return false;  // 无 ai_predict 块则不写(避免在错误位置追加)

  // 找块尾:下一个缩进 ≤2 空格的非空行
  int apEnd = yaml.length();
  int pos = yaml.indexOf(QLatin1Char('\n'), apStart);
  while (pos >= 0) {
    const int nextNL = yaml.indexOf(QLatin1Char('\n'), pos + 1);
    if (nextNL < 0)
      break;
    const QString line = yaml.mid(pos + 1, nextNL - pos - 1);
    if (!line.isEmpty() && !line.startsWith(QStringLiteral("    "))) {
      apEnd = pos + 1;
      break;
    }
    pos = nextNL;
  }
  const QString block = yaml.mid(apStart, apEnd - apStart);

  // 逐键替换或追加到块内
  QString newBlock = block;
  for (const auto& kv : kvs) {
    const QString key = QString::fromLatin1(kv.key);
    const QRegularExpression rx(
        QStringLiteral("(^|\n)    %1:\s*[^\n]*").arg(
            QRegularExpression::escape(key)));
    auto m2 = rx.match(newBlock);
    if (m2.hasMatch()) {
      newBlock.replace(m2.capturedStart(), m2.capturedLength(),
                       m2.captured(1) + QStringLiteral("    ") + key +
                           QStringLiteral(": ") + kv.val);
    } else {
      // 追加到块内最后一行后
      if (!newBlock.endsWith(QLatin1Char('\n')))
        newBlock += QLatin1Char('\n');
      newBlock += QStringLiteral("    %1: %2\n").arg(key, kv.val);
    }
  }
  yaml.replace(apStart, apEnd - apStart, newBlock);

  if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    return false;
  f.write(yaml.toUtf8());
  f.close();
  initState_ = sig;
  return true;
}
