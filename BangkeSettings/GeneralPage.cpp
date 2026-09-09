#include "GeneralPage.h"

#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>

#pragma warning(disable : 4005)
#include <rime_api.h>
#pragma warning(default : 4005)

#include "Levers.h"

GeneralPage::GeneralPage(QWidget* parent) : QWidget(parent) {
  api_ = leversApi();
  settings_ = api_->custom_settings_init("default", "Bangke::GeneralPage");

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

void GeneralPage::load() {
  api_->load_settings(settings_);
  RimeConfig config = {0};
  if (!api_->settings_get_config(settings_, &config))
    return;
  RimeApi* rime = rime_get_api();

  int ps = 5;
  if (rime->config_get_int(&config, "menu/page_size", &ps))
    pageSize_->setValue(ps);
  initPs_ = ps;

  // settings_get_config 只含 custom 补丁,不含合并后的共享默认;
  // 空 = 未打补丁,显示控件默认值
  const char* sl =
      rime->config_get_cstring(&config, "ascii_composer/switch_key/Shift_L");
  const char* sr =
      rime->config_get_cstring(&config, "ascii_composer/switch_key/Shift_R");
  initShiftL_ = sl ? QString::fromUtf8(sl) : QString();
  initShiftR_ = sr ? QString::fromUtf8(sr) : QString();
  for (int i = 0; i < shiftL_->count(); ++i) {
    if (shiftL_->itemData(i).toString() == initShiftL_)
      shiftL_->setCurrentIndex(i);
    if (shiftR_->itemData(i).toString() == initShiftR_)
      shiftR_->setCurrentIndex(i);
  }
}

bool GeneralPage::save() {
  const int ps = pageSize_->value();
  const QString sl = shiftL_->currentData().toString();
  const QString sr = shiftR_->currentData().toString();
  if (ps == initPs_ && sl == initShiftL_ && sr == initShiftR_)
    return false;
  // 重读再写:与 SwitcherPage 共写 default.custom.yaml,不能拿旧树覆写对方
  api_->load_settings(settings_);
  if (ps != initPs_)
    api_->customize_int(settings_, "menu/page_size", ps);
  if (sl != initShiftL_)
    api_->customize_string(settings_, "ascii_composer/switch_key/Shift_L",
                           sl.toUtf8().constData());
  if (sr != initShiftR_)
    api_->customize_string(settings_, "ascii_composer/switch_key/Shift_R",
                           sr.toUtf8().constData());
  if (!api_->save_settings(settings_))
    return false;
  initPs_ = ps;
  initShiftL_ = sl;
  initShiftR_ = sr;
  return true;
}
