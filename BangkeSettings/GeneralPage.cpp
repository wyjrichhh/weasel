#include "GeneralPage.h"

#include <QComboBox>
#include <QFile>
#include <QFormLayout>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTextStream>
#include <QVBoxLayout>

#include <WeaselUtility.h>

GeneralPage::GeneralPage(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 16, 24, 16);
  layout->setSpacing(8);

  pageSize_ = new QSpinBox;
  pageSize_->setRange(1, 20);
  pageSize_->setValue(5);

  shiftL_ = new QComboBox;
  shiftR_ = new QComboBox;
  const struct { const char* val; const char* label; } actions[] = {
      {"commit_code", "提交编码"}, {"commit_text", "提交中文"},
      {"inline_ascii", "内嵌英文"}, {"clear", "清除"}, {"noop", "无操作"},
  };
  for (const auto& a : actions) {
    shiftL_->addItem(QString::fromUtf8(a.label), QString::fromLatin1(a.val));
    shiftR_->addItem(QString::fromUtf8(a.label), QString::fromLatin1(a.val));
  }

  auto* form = new QFormLayout;
  form->addRow(QStringLiteral(u"每页候选数"), pageSize_);
  form->addRow(QStringLiteral(u"左 Shift 切换行为"), shiftL_);
  form->addRow(QStringLiteral(u"右 Shift 切换行为"), shiftR_);
  layout->addLayout(form);
  layout->addStretch(1);

  load();
}

QString GeneralPage::defaultCustomYaml() const {
  return QString::fromStdWString(WeaselUserDataPath().wstring()) +
         QStringLiteral("/default.custom.yaml");
}

// 读嵌套键:在 yaml 中找 "  <key>: <value>" 行(任意缩进层级)。
// 精确匹配键名(避免 Shift_L 匹配到 Shift_L_extra)
static QString findKeyLine(const QString& yaml, const QString& key) {
  const QRegularExpression rx(
      QStringLiteral("(^|\\n)\\s*%1:\\s*([^\\n]*)")
          .arg(QRegularExpression::escape(key)));
  auto m = rx.match(yaml);
  return m.hasMatch() ? m.captured(2).trimmed() : QString();
}

static bool replaceKeyLine(QString& yaml, const QString& key,
                           const QString& val) {
  const QRegularExpression rx(
      QStringLiteral("(^|\\n)(\\s*)%1:\\s*[^\\n]*")
          .arg(QRegularExpression::escape(key)));
  auto m = rx.match(yaml);
  if (!m.hasMatch())
    return false;
  yaml.replace(m.capturedStart(), m.capturedLength(),
               m.captured(1) + m.captured(2) + key + QStringLiteral(": ") + val);
  return true;
}

void GeneralPage::load() {
  QFile f(defaultCustomYaml());
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return;
  const QString yaml = QString::fromUtf8(f.readAll());
  f.close();

  initPs_ = findKeyLine(yaml, QStringLiteral("menu/page_size"));
  if (!initPs_.isEmpty())
    pageSize_->setValue(initPs_.toInt());

  initShiftL_ = findKeyLine(yaml, QStringLiteral("ascii_composer/switch_key/Shift_L"));
  initShiftR_ = findKeyLine(yaml, QStringLiteral("ascii_composer/switch_key/Shift_R"));
  for (int i = 0; i < shiftL_->count(); ++i) {
    if (shiftL_->itemData(i).toString() == initShiftL_)
      shiftL_->setCurrentIndex(i);
    if (shiftR_->itemData(i).toString() == initShiftR_)
      shiftR_->setCurrentIndex(i);
  }
}

// 写 patch: 块里的一个路径键:已有则替换,没有则追加(文件缺 patch: 头时先补)
static bool writePatchKey(const QString& file, const QString& path,
                          const QString& val) {
  QFile f(file);
  QString yaml;
  if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    yaml = QString::fromUtf8(f.readAll());
    f.close();
  }
  const QRegularExpression rx(
      QStringLiteral("(^|\n)(\s*)%1:\s*[^\n]*").arg(
          QRegularExpression::escape(path)));
  auto m = rx.match(yaml);
  if (m.hasMatch()) {
    yaml.replace(m.capturedStart(), m.capturedLength(),
                 m.captured(1) + m.captured(2) + path +
                     QStringLiteral(": ") + val);
  } else {
    if (!yaml.contains(
            QRegularExpression(QStringLiteral("(^|\n)patch:\s*(\n|$)")))) {
      if (!yaml.isEmpty() && !yaml.endsWith(QLatin1Char('\n')))
        yaml += QLatin1Char('\n');
      yaml += QStringLiteral("patch:\n");
    }
    yaml += QStringLiteral("  %1: %2\n").arg(path, val);
  }
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    return false;
  f.write(yaml.toUtf8());
  f.close();
  return true;
}

bool GeneralPage::save() {
  // 只写有变化的键:未动过的键不进 patch,保留 rime 默认行为
  struct KV { QString path; QString val; QString* init; };
  KV kvs[] = {
      {QStringLiteral("menu/page_size"), QString::number(pageSize_->value()), &initPs_},
      {QStringLiteral("ascii_composer/switch_key/Shift_L"),
       shiftL_->currentData().toString(), &initShiftL_},
      {QStringLiteral("ascii_composer/switch_key/Shift_R"),
       shiftR_->currentData().toString(), &initShiftR_},
  };
  bool any = false;
  for (auto& kv : kvs) {
    if (kv.val == *kv.init)
      continue;
    if (!writePatchKey(defaultCustomYaml(), kv.path, kv.val))
      return false;
    *kv.init = kv.val;
    any = true;
  }
  return any;
}
