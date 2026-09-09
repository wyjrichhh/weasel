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

  const QString ps = findKeyLine(yaml, QStringLiteral("menu/page_size"));
  if (!ps.isEmpty())
    pageSize_->setValue(ps.toInt());

  const QString sl = findKeyLine(yaml, QStringLiteral("ascii_composer/switch_key/Shift_L"));
  const QString sr = findKeyLine(yaml, QStringLiteral("ascii_composer/switch_key/Shift_R"));
  for (int i = 0; i < shiftL_->count(); ++i) {
    if (shiftL_->itemData(i).toString() == sl)
      shiftL_->setCurrentIndex(i);
    if (shiftR_->itemData(i).toString() == sr)
      shiftR_->setCurrentIndex(i);
  }
}

void GeneralPage::save() {
  QFile f(defaultCustomYaml());
  QString yaml;
  if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    yaml = QString::fromUtf8(f.readAll());
    f.close();
  }

  // default.custom.yaml 结构:
  // patch:
  //   menu/page_size: 5
  //   ascii_composer/switch_key/Shift_L: commit_code
  //   ascii_composer/switch_key/Shift_R: commit_code
  // 写入用 rime patch 路径键(扁平键名),部署器会合并到对应节点。
  struct KV { QString path; QString val; };
  const KV kvs[] = {
      {QStringLiteral("menu/page_size"), QString::number(pageSize_->value())},
      {QStringLiteral("ascii_composer/switch_key/Shift_L"), shiftL_->currentData().toString()},
      {QStringLiteral("ascii_composer/switch_key/Shift_R"), shiftR_->currentData().toString()},
  };

  for (const auto& kv : kvs) {
    const QRegularExpression rx(
        QStringLiteral("(^|\n)(\s*)%1:\s*[^\n]*").arg(
            QRegularExpression::escape(kv.path)));
    auto m2 = rx.match(yaml);
    if (m2.hasMatch()) {
      yaml.replace(m2.capturedStart(), m2.capturedLength(),
                   m2.captured(1) + m2.captured(2) + kv.path +
                       QStringLiteral(": ") + kv.val);
    } else {
      // 追加到 patch: 块末(2 空格缩进)
      if (!yaml.endsWith(QLatin1Char('\n')))
        yaml += QLatin1Char('\n');
      yaml += QStringLiteral("  %1: %2\n").arg(kv.path, kv.val);
    }
  }

  if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
    f.write(yaml.toUtf8());
    f.close();
  }
}
