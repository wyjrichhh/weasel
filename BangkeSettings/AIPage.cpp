#include "AIPage.h"

#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QStringList>
#include <QTextStream>
#include <QRegularExpression>
#include <QVBoxLayout>

#include "Ui.h"
#include <WeaselUtility.h>

AIPage::AIPage(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 12, 16, 12);
  layout->setSpacing(10);

  enabled_ = new ToggleSwitch;
  schemaLabel_ = new QLabel;
  schemaLabel_->setObjectName(QStringLiteral("hint"));

  device_ = new QComboBox;
  device_->setFixedWidth(150);
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
  for (auto* spin : {maxTokens_, debounce_, minInput_, minHanzi_})
    spin->setFixedWidth(96);

  auto* basic = new QVBoxLayout();
  basic->addWidget(schemaLabel_);
  basic->addLayout(makeSettingRows({
      makeSettingRow(QStringLiteral(u"启用 AI 预测"), enabled_,
                     QStringLiteral(u"开关作用于当前方案")),
      makeSettingRow(QStringLiteral(u"设备"), device_),
      makeSettingRow(QStringLiteral(u"最大 token 数"), maxTokens_),
      makeSettingRow(QStringLiteral(u"防抖"), debounce_),
      makeSettingRow(QStringLiteral(u"最小输入长度"), minInput_),
      makeSettingRow(QStringLiteral(u"最少汉字数"), minHanzi_),
  }));
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
  quality_->setFixedWidth(96);
  for (auto* spin :
       {targetIndex_, searchRange_, contextWindow_, minContextPrompt_})
    spin->setFixedWidth(96);
  modelPath_ = new QLineEdit;
  modelPath_->setReadOnly(true);
  modelPath_->setFixedWidth(280);

  layout->addWidget(makeCard(
      QStringLiteral(u"高级选项"),
      makeSettingRows({
          makeSettingRow(QStringLiteral(u"候选质量 (quality)"), quality_),
          makeSettingRow(QStringLiteral(u"AI 候选显示位置 (target_index)"),
                         targetIndex_),
          makeSettingRow(QStringLiteral(u"去重扫描范围 (search_range)"),
                         searchRange_),
          makeSettingRow(QStringLiteral(u"上下文窗口 (context_window_size)"),
                         contextWindow_),
          makeSettingRow(QStringLiteral(u"上下文最小拼音数"), minContextPrompt_),
          makeSettingRow(QStringLiteral(u"模型路径"), modelPath_),
      })));
  layout->addStretch(1);

  load();
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

// 当前方案:rime 把最近选择的方案记在 user.yaml(previously_selected_schema)。
// 取不到时退到方案列表第一个。
QString AIPage::currentSchemaId() const {
  const QDir dir(QString::fromStdWString(WeaselUserDataPath().wstring()));
  QFile u(dir.filePath(QStringLiteral("user.yaml")));
  if (u.open(QIODevice::ReadOnly | QIODevice::Text)) {
    const QString yaml = QString::fromUtf8(u.readAll());
    u.close();
    static const QRegularExpression rx(
        QStringLiteral("(?m)^\\s*previously_selected_schema:\\s*(\\S+)"));
    auto m = rx.match(yaml);
    if (m.hasMatch())
      return m.captured(1);
  }
  QFile d(dir.filePath(QStringLiteral("default.custom.yaml")));
  if (d.open(QIODevice::ReadOnly | QIODevice::Text)) {
    const QString yaml = QString::fromUtf8(d.readAll());
    d.close();
    static const QRegularExpression rx(QStringLiteral("schema:\\s*(\\S+)"));
    auto m = rx.match(yaml);
    if (m.hasMatch())
      return m.captured(1);
  }
  return QStringLiteral("luna_pinyin");
}

QString AIPage::schemaCustomYaml(const QString& schemaId) const {
  const QDir dir(QString::fromStdWString(WeaselUserDataPath().wstring()));
  return dir.filePath(schemaId + QStringLiteral(".custom.yaml"));
}

// 展示名取共享数据目录 <id>.schema.yaml 的顶层 name:;取不到用 id
QString AIPage::schemaDisplayName(const QString& schemaId) const {
  const QDir dir(QString::fromStdWString(WeaselSharedDataPath().wstring()));
  QFile f(dir.filePath(schemaId + QStringLiteral(".schema.yaml")));
  if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    const QString v =
        findValue(QString::fromUtf8(f.readAll()), QStringLiteral("name"));
    f.close();
    if (!v.isEmpty())
      return v;
  }
  return schemaId;
}

void AIPage::load() {
  const QString schema = currentSchemaId();
  schemaLabel_->setText(QStringLiteral(u"当前方案:%1").arg(
      schemaDisplayName(schema)));
  QFile f(schemaCustomYaml(schema));
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

// 保存:读全文,逐行替换已知键的值;块内缺的键追加。
// 未管理的行(含注释、engine 配置等)原样保留——与 fcitx5-rime config-ui 同款纪律。
// 只写当前方案的 custom yaml。
bool AIPage::save() {
  const QString sig = stateSignature();
  if (sig == initState_)
    return false;

  QFile f(schemaCustomYaml(currentSchemaId()));
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
