#include "SwitcherPage.h"

#include <QBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <windows.h>

#include <cstring>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include <rime_levers_api.h>
#include <BangkeUtility.h>

#include "Ui.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <vector>

// 方案行:整行可点(看说明),右侧滑块控启用
class SchemaRowWidget : public QWidget {
 public:
  SchemaRowWidget(const QString& name, std::function<void()> onActivate,
                   QWidget* parent)
      : QWidget(parent), onActivate_(std::move(onActivate)) {
    setObjectName(QStringLiteral("schemaRow"));
    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(2, 9, 2, 9);
    h->setSpacing(12);
    auto* title = new QLabel(name, this);
    title->setObjectName(QStringLiteral("rowTitle"));
    h->addWidget(title);
    h->addStretch(1);
    sw_ = new ToggleSwitch(this);
    h->addWidget(sw_, 0, Qt::AlignRight | Qt::AlignVCenter);
  }
  ToggleSwitch* sw() const { return sw_; }

 protected:
  void mousePressEvent(QMouseEvent*) override { onActivate_(); }

 private:
  ToggleSwitch* sw_ = nullptr;
  std::function<void()> onActivate_;
};

// rime-install.bat 由 build.bat data 复制到安装目录
SwitcherPage::SwitcherPage(QWidget* parent) : QWidget(parent) {
  api_ = leversApi();

  listHost_ = new QWidget(this);
  auto* rowsLayout = new QVBoxLayout(listHost_);
  rowsLayout->setContentsMargins(0, 0, 0, 0);
  rowsLayout->setSpacing(0);
  rowsLayout->addStretch(1);

  description_ = new QTextBrowser(this);
  description_->setOpenExternalLinks(false);

  hotkeys_ = new QLineEdit(this);
  hotkeys_->setReadOnly(true);

  auto* refreshBtn = new QPushButton(QStringLiteral(u"刷新"), this);
  auto* btnRow = new QHBoxLayout();
  btnRow->addWidget(refreshBtn);
  btnRow->addStretch();

  auto* leftLayout = new QVBoxLayout();
  leftLayout->addWidget(
      new QLabel(QStringLiteral(u"已启用方案置顶；点击行查看说明。"), this));
  leftLayout->addWidget(listHost_, 1);
  leftLayout->addLayout(btnRow);

  auto* rightLayout = new QVBoxLayout();
  rightLayout->addWidget(new QLabel(QStringLiteral(u"方案说明："), this));
  rightLayout->addWidget(description_, 1);
  rightLayout->addWidget(new QLabel(QStringLiteral(u"方案选单快捷键："), this));
  rightLayout->addWidget(hotkeys_);

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(10);
  auto* leftCard = makeCard(QString(), leftLayout, this);
  auto* rightCard = makeCard(QString(), rightLayout, this);
  layout->addWidget(leftCard, 3);
  layout->addWidget(rightCard, 2);

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
  auto* row = new SchemaRowWidget(
      QString::fromStdString(item.name),
      [this, info] { showDetails(info); }, listHost_);
  row->sw()->setChecked(checked);
  connect(row->sw(), &ToggleSwitch::toggled, this,
          [this](bool) { modified_ = true; });
  rowsLayout->insertWidget((int)index * 2, row);
  rows_.push_back({info, row->sw(), row});
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
  std::vector<bool> checked(available_.size, false);
  for (size_t i = 0; i < selected.size; ++i) {
    const char* schema_id = selected.list[i].schema_id;
    for (size_t j = 0; j < available_.size; ++j) {
      RimeSchemaListItem& item(available_.list[j]);
      RimeSchemaInfo* info = (RimeSchemaInfo*)item.reserved;
      if (!strcmp(item.schema_id, schema_id) &&
          recruited.find(info) == recruited.end()) {
        recruited.insert(info);
        checked[j] = true;
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

  if (const char* hotkeys = api_->get_hotkeys(settings_))
    hotkeys_->setText(QString::fromStdString(hotkeys));
  modified_ = false;
}

void SwitcherPage::showDetails(RimeSchemaInfo* info) {
  if (!info)
    return;
  std::string details;
  if (const char* name = api_->get_schema_name(info))
    details += name;
  if (const char* author = api_->get_schema_author(info))
    (details += "\n\n") += author;
  if (const char* description = api_->get_schema_description(info))
    (details += "\n\n") += description;
  description_->setText(QString::fromStdString(details));
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
