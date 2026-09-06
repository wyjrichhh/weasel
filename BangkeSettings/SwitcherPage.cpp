#include "SwitcherPage.h"

#include <QBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTextBrowser>
#include <windows.h>

#include <cstring>
#include <set>
#include <string>
#include <vector>

#include <rime_levers_api.h>
#include <WeaselUtility.h>

// rime-install.bat 由 build.bat data 复制到安装目录
static std::wstring GetBangkeRoot() {
  std::wstring dir;
  RegGetStringValue(HKEY_LOCAL_MACHINE, L"Software\\Bangke", L"BangkeRoot", dir);
  return dir;
}

SwitcherPage::SwitcherPage(QWidget* parent) : QWidget(parent) {
  api_ = leversApi();

  schemaList_ = new QListWidget(this);
  schemaList_->setSelectionMode(QAbstractItemView::SingleSelection);

  description_ = new QTextBrowser(this);
  description_->setOpenExternalLinks(false);

  hotkeys_ = new QLineEdit(this);
  hotkeys_->setReadOnly(true);

  auto* refreshBtn = new QPushButton(L"刷新", this);
  auto* moreBtn = new QPushButton(L"获取更多方案…", this);
  auto* btnRow = new QHBoxLayout();
  btnRow->addWidget(refreshBtn);
  btnRow->addWidget(moreBtn);
  btnRow->addStretch();

  auto* leftLayout = new QVBoxLayout();
  leftLayout->addWidget(new QLabel(L"已选方案置顶并勾选，勾选即启用：", this));
  leftLayout->addWidget(schemaList_, 1);
  leftLayout->addLayout(btnRow);

  auto* rightLayout = new QVBoxLayout();
  rightLayout->addWidget(new QLabel(L"方案说明：", this));
  rightLayout->addWidget(description_, 1);
  rightLayout->addWidget(new QLabel(L"方案选单快捷键：", this));
  rightLayout->addWidget(hotkeys_);

  auto* layout = new QHBoxLayout(this);
  auto* left = new QWidget(this);
  left->setLayout(leftLayout);
  auto* right = new QWidget(this);
  right->setLayout(rightLayout);
  layout->addWidget(left, 3);
  layout->addWidget(right, 2);

  connect(refreshBtn, &QPushButton::clicked, this, &SwitcherPage::forceLoad);
  connect(moreBtn, &QPushButton::clicked, this, &SwitcherPage::getMoreSchemas);
  connect(schemaList_, &QListWidget::currentItemChanged, this,
          [this](QListWidgetItem* current, QListWidgetItem*) {
            if (current)
              showDetails((RimeSchemaInfo*)current->data(Qt::UserRole).toULongLong());
          });
  connect(schemaList_, &QListWidget::itemChanged, this,
          [this](QListWidgetItem*) { modified_ = true; });
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

  schemaList_->blockSignals(true);
  schemaList_->clear();
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
        auto* row = new QListWidgetItem(QString::fromStdString(item.name));
        row->setFlags(row->flags() | Qt::ItemIsUserCheckable);
        row->setCheckState(Qt::Checked);
        row->setData(Qt::UserRole, (qulonglong)info);
        schemaList_->insertItem(k++, row);
        break;
      }
    }
  }
  for (size_t i = 0; i < available_.size; ++i) {
    RimeSchemaListItem& item(available_.list[i]);
    RimeSchemaInfo* info = (RimeSchemaInfo*)item.reserved;
    if (recruited.find(info) == recruited.end()) {
      recruited.insert(info);
      auto* row = new QListWidgetItem(QString::fromStdString(item.name));
      row->setFlags(row->flags() | Qt::ItemIsUserCheckable);
      row->setCheckState(Qt::Unchecked);
      row->setData(Qt::UserRole, (qulonglong)info);
      schemaList_->insertItem(k++, row);
    }
  }
  schemaList_->blockSignals(false);
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

void SwitcherPage::getMoreSchemas() {
  std::wstring root = GetBangkeRoot();
  if (root.empty())
    return;
  QProcess::startDetached(
      "cmd", {"/k", QString::fromStdWString(root) + "\\rime-install.bat"},
      QString::fromStdWString(root));
  QMessageBox::information(this, L"获取更多方案",
                           L"在弹出的命令行窗口中按提示安装方案，\n完成后点击「刷新」重新加载列表。");
}

bool SwitcherPage::save() {
  if (!modified_ || !settings_ || schemaList_->count() == 0)
    return false;
  std::vector<const char*> selection;
  for (int i = 0; i < schemaList_->count(); ++i) {
    if (schemaList_->item(i)->checkState() != Qt::Checked)
      continue;
    if (auto* info =
            (RimeSchemaInfo*)schemaList_->item(i)->data(Qt::UserRole).toULongLong())
      selection.push_back(api_->get_schema_id(info));
  }
  if (selection.empty()) {
    QMessageBox::warning(this, L"蚌壳拼音", L"至少要选用一项方案。");
    return false;
  }
  api_->select_schemas(settings_, selection.data(), (int)selection.size());
  return api_->save_settings((RimeCustomSettings*)settings_);
}
