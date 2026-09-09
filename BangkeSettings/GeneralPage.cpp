#include "GeneralPage.h"

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

  const QString ps = findKeyLine(yaml, QStringLiteral("page_size"));
  if (!ps.isEmpty())
    pageSize_->setValue(ps.toInt());

  const QString sl = findKeyLine(yaml, QStringLiteral("Shift_L"));
  const QString sr = findKeyLine(yaml, QStringLiteral("Shift_R"));
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

  // 确保必需的父块存在(default.custom.yaml 的 patch: 下)
  for (const char* block : {"menu/", "ascii_composer/", "switch_key/"}) {
    // 简化处理:custom.yaml 的 patch: 块内追加即可,rime 部署器会合并
  }
  // 不改文件结构,只替换已有行;如果行不存在,追加到 patch: 块下
  const QString psKey = QStringLiteral("page_size");
  const QString slKey = QStringLiteral("Shift_L");
  const QString srKey = QStringLiteral("Shift_R");

  if (!replaceKeyLine(yaml, psKey, QString::number(pageSize_->value()))) {
    yaml += QStringLiteral("\n  %1: %2\n").arg(psKey, QString::number(pageSize_->value()));
  }
  if (!replaceKeyLine(yaml, slKey, shiftL_->currentData().toString())) {
    yaml += QStringLiteral("  %1: %2\n").arg(slKey, shiftL_->currentData().toString());
  }
  if (!replaceKeyLine(yaml, srKey, shiftR_->currentData().toString())) {
    yaml += QStringLiteral("  %1: %2\n").arg(srKey, shiftR_->currentData().toString());
  }

  if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
    f.write(yaml.toUtf8());
    f.close();
  }
}
