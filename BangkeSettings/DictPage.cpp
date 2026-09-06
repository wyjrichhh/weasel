#include "DictPage.h"

#include <QBoxLayout>
#include <QDir>
#include <QFileDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <windows.h>

#include <rime_api.h>
#include <WeaselUtility.h>

static void RevealInExplorer(const QString& path) {
  QProcess::startDetached("explorer", {"/select,", QDir::toNativeSeparators(path)});
}

DictPage::DictPage(QWidget* parent) : QWidget(parent) {
  api_ = leversApi();

  dictList_ = new QListWidget(this);

  backupBtn_ = new QPushButton(L"备份…", this);
  restoreBtn_ = new QPushButton(L"恢复…", this);
  exportBtn_ = new QPushButton(L"导出为文本…", this);
  importBtn_ = new QPushButton(L"从文本导入…", this);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel(L"用户词典：", this));
  layout->addWidget(dictList_, 1);
  auto* btnRow = new QHBoxLayout();
  btnRow->addWidget(backupBtn_);
  btnRow->addWidget(restoreBtn_);
  btnRow->addStretch();
  btnRow->addWidget(exportBtn_);
  btnRow->addWidget(importBtn_);
  layout->addLayout(btnRow);

  connect(backupBtn_, &QPushButton::clicked, this, &DictPage::backup);
  connect(restoreBtn_, &QPushButton::clicked, this, &DictPage::restore);
  connect(exportBtn_, &QPushButton::clicked, this, &DictPage::exportDict);
  connect(importBtn_, &QPushButton::clicked, this, &DictPage::importDict);
  connect(dictList_, &QListWidget::currentRowChanged, this,
          [this](int) { updateButtons(); });

  updateButtons();
}

void DictPage::load() {
  populate();
}

void DictPage::setSessionActive(bool active) {
  sessionActive_ = active;
  if (active)
    populate();
}

void DictPage::populate() {
  dictList_->clear();
  if (!sessionActive_ || !api_)
    return;
  RimeUserDictIterator iter = {0};
  api_->user_dict_iterator_init(&iter);
  while (const char* dict = api_->next_user_dict(&iter))
    dictList_->addItem(QString::fromStdString(dict));
  api_->user_dict_iterator_destroy(&iter);
  updateButtons();
}

void DictPage::updateButtons() {
  bool hasSel = dictList_->currentRow() >= 0;
  backupBtn_->setEnabled(hasSel);
  exportBtn_->setEnabled(hasSel);
  importBtn_->setEnabled(hasSel);
}

static QString currentDictName(QListWidget* list) {
  if (list->currentRow() < 0)
    return QString();
  return list->currentItem()->text();
}

static std::string userSyncDir() {
  char dir[MAX_PATH] = {0};
  rime_get_api()->get_user_data_sync_dir(dir, _countof(dir));
  return std::string(dir);
}

void DictPage::backup() {
  QString dict = currentDictName(dictList_);
  if (dict.isEmpty())
    return;
  std::string dir = userSyncDir();
  if (dir.empty()) {
    QMessageBox::warning(this, L"蚌壳拼音", L"无法定位同步目录。");
    return;
  }
  QString path = QDir::toNativeSeparators(
      QString::fromStdString(dir) + "\\" + dict + ".userdb.txt");
  if (!api_->backup_user_dict(dict.toStdString().c_str())) {
    QMessageBox::warning(this, L"蚌壳拼音", L"备份失败。");
    return;
  }
  RevealInExplorer(path);
}

void DictPage::restore() {
  QString path = QFileDialog::getOpenFileName(
      this, L"恢复词典快照", QString(),
      L"词典快照 (*.userdb.txt *.userdb.kct.snapshot);;所有文件 (*.*)");
  if (path.isEmpty())
    return;
  if (!api_->restore_user_dict(path.toStdString().c_str()))
    QMessageBox::warning(this, L"蚌壳拼音", L"恢复失败。");
  else
    QMessageBox::information(this, L"蚌壳拼音", L"恢复完成。");
}

void DictPage::exportDict() {
  QString dict = currentDictName(dictList_);
  if (dict.isEmpty())
    return;
  QString path = QFileDialog::getSaveFileName(this, L"导出词典", dict + "_export.txt",
                                              L"文本文件 (*.txt)");
  if (path.isEmpty())
    return;
  int result = api_->export_user_dict(dict.toStdString().c_str(),
                                      path.toStdString().c_str());
  if (result < 0) {
    QMessageBox::warning(this, L"蚌壳拼音", L"导出失败。");
    return;
  }
  QMessageBox::information(this, L"蚌壳拼音",
                           QString(L"导出 %1 条记录。").arg(result));
  RevealInExplorer(path);
}

void DictPage::importDict() {
  QString dict = currentDictName(dictList_);
  if (dict.isEmpty())
    return;
  QString path = QFileDialog::getOpenFileName(this, L"导入词典", dict + "_export.txt",
                                              L"文本文件 (*.txt);;所有文件 (*.*)");
  if (path.isEmpty())
    return;
  int result = api_->import_user_dict(dict.toStdString().c_str(),
                                      path.toStdString().c_str());
  if (result < 0) {
    QMessageBox::warning(this, L"蚌壳拼音", L"导入失败。");
    return;
  }
  QMessageBox::information(this, L"蚌壳拼音",
                           QString(L"导入 %1 条记录。").arg(result));
}
