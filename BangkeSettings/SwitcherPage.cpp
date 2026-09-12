#include "SwitcherPage.h"

#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <cstring>
#include <set>
#include <string>
#include <vector>

#include <rime_levers_api.h>
#include <BangkeUtility.h>

#include "Ui.h"

// rime-install.bat 由 build.bat data 复制到安装目录
SwitcherPage::SwitcherPage(QWidget* parent) : QWidget(parent) {
  api_ = leversApi();

  auto* refreshBtn = new QPushButton(QStringLiteral(u"刷新"), this);
  auto* btnRow = new QHBoxLayout();
  btnRow->addWidget(refreshBtn);
  btnRow->addStretch();

  auto* rowsLayout = new QVBoxLayout();
  rowsLayout->setContentsMargins(0, 0, 0, 0);
  rowsLayout->setSpacing(0);
  rowsLayout->addStretch(1);
  listHost_ = new QWidget(this);
  listHost_->setLayout(rowsLayout);

  auto* cardBody = new QVBoxLayout();
  cardBody->addWidget(
      new QLabel(QStringLiteral(u"已启用的方案置顶显示。"), this));
  cardBody->addLayout(btnRow);
  cardBody->addWidget(listHost_, 1);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(10);
  layout->addWidget(makeCard(QStringLiteral(u"方案选单"), cardBody, this));

  connect(refreshBtn, &QPushButton::clicked, this, &SwitcherPage::forceLoad);
}

SwitcherPage::~SwitcherPage() {
  if (available_.list)
    api_->schema_list_destroy(&available_);
  if (settings_)
    api_->custom_settings_destroy((RimeCustomSettings*)settings_);
}

void SwitcherPage::load() {
  if (loaded_)
    return;
  forceLoad();
}

void SwitcherPage::forceLoad() {
  if (!api_)
    return;
  if (!settings_)
    settings_ = api_->switcher_settings_init();
  loadSettings();
  loaded_ = true;
}

void SwitcherPage::loadSettings() {
  api_->load_settings((RimeCustomSettings*)settings_);
  populate();
}

void SwitcherPage::addRow(RimeSchemaListItem& item,
                           RimeSchemaInfo* info,
                           bool checked,
                           size_t index) {
  // 布局形态:[row0, 发丝线, row1, ..., stretch],全部按位插入
  auto* rowsLayout = static_cast<QVBoxLayout*>(listHost_->layout());
  if (index > 0)
    rowsLayout->insertWidget((int)index * 2 - 1, makeHairline());
  auto* sw = new ToggleSwitch(listHost_);
  sw->setChecked(checked);
  connect(sw, &ToggleSwitch::toggled, this, [this](bool) { modified_ = true; });
  rowsLayout->insertWidget(
      (int)index * 2, makeSettingRow(QString::fromStdString(item.name), sw));
  rows_.push_back({info, sw});
}

void SwitcherPage::populate() {
  if (!settings_)
    return;
  if (available_.list) {
    api_->schema_list_destroy(&available_);
    available_ = {0};
  }
  RimeSchemaList selected = {0};
  api_->get_available_schema_list(settings_, &available_);
  api_->get_selected_schema_list(settings_, &selected);

  rows_.clear();
  auto* rowsLayout = static_cast<QVBoxLayout*>(listHost_->layout());
  while (rowsLayout->count() > 1) {  // 末尾的 stretch 保留
    auto* item = rowsLayout->takeAt(0);
    if (item->widget())
      item->widget()->deleteLater();
    delete item;
  }
  size_t k = 0;
  std::set<void*> recruited;
  for (size_t i = 0; i < selected.size; ++i) {
    const char* schema_id = selected.list[i].schema_id;
    for (size_t j = 0; j < available_.size; ++j) {
      RimeSchemaListItem& item(available_.list[j]);
      RimeSchemaInfo* info = (RimeSchemaInfo*)item.reserved;
      if (!strcmp(item.schema_id, schema_id) &&
          recruited.find(info) == recruited.end()) {
        recruited.insert(info);
        addRow(item, info, true, k++);
        break;
      }
    }
  }
  for (size_t i = 0; i < available_.size; ++i) {
    RimeSchemaListItem& item(available_.list[i]);
    RimeSchemaInfo* info = (RimeSchemaInfo*)item.reserved;
    if (recruited.find(info) == recruited.end()) {
      recruited.insert(info);
      addRow(item, info, false, k++);
    }
  }
  api_->schema_list_destroy(&selected);
  modified_ = false;
}

bool SwitcherPage::save() {
  if (!modified_ || !settings_ || rows_.empty())
    return false;
  std::vector<const char*> selection;
  for (auto& rec : rows_) {
    if (!rec.sw->isChecked())
      continue;
    selection.push_back(api_->get_schema_id(rec.info));
  }
  if (selection.empty()) {
    QMessageBox::warning(this, QStringLiteral(u"蚌壳拼音"), QStringLiteral(u"至少要选用一项方案。"));
    return false;
  }
  // 重读再写:与 GeneralPage 共写 default.custom.yaml,避免旧树覆写对方
  api_->load_settings((RimeCustomSettings*)settings_);
  api_->select_schemas(settings_, selection.data(), (int)selection.size());
  return api_->save_settings((RimeCustomSettings*)settings_);
}
