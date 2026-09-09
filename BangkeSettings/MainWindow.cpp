#include "MainWindow.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDesktopServices>
#include <QFileDialog>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QUrl>

#include "AIPage.h"
#include "Configurator.h"
#include "DictPage.h"
#include "GeneralPage.h"
#include "StylePage.h"
#include "SwitcherPage.h"
#include <WeaselUtility.h>

MainWindow::MainWindow(Configurator* configurator, bool openDictPage,
                       QWidget* parent)
    : QMainWindow(parent), configurator_(configurator) {
  setWindowTitle(QStringLiteral(u"蚌壳拼音 · 设置"));
  resize(860, 560);

  switcherPage_ = new SwitcherPage(this);
  generalPage_ = new GeneralPage(this);
  stylePage_ = new StylePage(this);
  aiPage_ = new AIPage(this);
  dictPage_ = new DictPage(this);

  stack_ = new QStackedWidget(this);
  stack_->addWidget(switcherPage_);
  stack_->addWidget(generalPage_);
  stack_->addWidget(stylePage_);
  stack_->addWidget(aiPage_);
  stack_->addWidget(dictPage_);

  nav_ = new QListWidget(this);
  nav_->setFixedWidth(120);
  nav_->setFrameShape(QFrame::NoFrame);
  nav_->addItems({QStringLiteral(u"方案选单"), QStringLiteral(u"通用设置"), QStringLiteral(u"界面样式"), QStringLiteral(u"AI 预测"), QStringLiteral(u"词典管理")});
  nav_->setCurrentRow(openDictPage ? 4 : 0);

  auto* saveBtn = new QPushButton(QStringLiteral(u"保存并重新部署"), this);
  saveBtn->setDefault(true);
  auto* userDirBtn = new QPushButton(QStringLiteral(u"打开用户文件夹"), this);
  auto* logDirBtn = new QPushButton(QStringLiteral(u"打开日志文件夹"), this);

  auto* bottomRow = new QHBoxLayout();
  bottomRow->addWidget(userDirBtn);
  bottomRow->addWidget(logDirBtn);
  bottomRow->addStretch();
  bottomRow->addWidget(saveBtn);

  auto* body = new QWidget(this);
  auto* bodyLayout = new QVBoxLayout(body);
  auto* topRow = new QHBoxLayout();
  topRow->addWidget(nav_);
  topRow->addWidget(stack_, 1);
  bodyLayout->addLayout(topRow, 1);
  bodyLayout->addLayout(bottomRow);
  setCentralWidget(body);
  statusBar()->showMessage(QStringLiteral(u"修改后点击「保存并重新部署」生效"));

  connect(nav_, &QListWidget::currentRowChanged, this,
          &MainWindow::onPageChanged);
  connect(saveBtn, &QPushButton::clicked, this, &MainWindow::saveAndDeploy);
  connect(userDirBtn, &QPushButton::clicked, this, [] {
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(QString::fromStdWString(WeaselUserDataPath().wstring())));
  });
  connect(logDirBtn, &QPushButton::clicked, this, [] {
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(QString::fromStdWString(WeaselLogPath().wstring())));
  });

  onPageChanged(nav_->currentRow());
}

MainWindow::~MainWindow() {
  dictPage_->setSessionActive(false);
}

void MainWindow::onPageChanged(int index) {
  stack_->setCurrentIndex(index);
  dictPage_->setSessionActive(false);
  // 五页布局: 0=方案 1=通用 2=界面样式 3=AI 4=词典
  if (index == 4) {
    if (configurator_->BeginDictSession())
      dictPage_->setSessionActive(true);
  } else if (index == 0) {
    switcherPage_->load();
  } else if (index == 2) {
    stylePage_->load();
  }
}

void MainWindow::saveAndDeploy() {
  bool modified = switcherPage_->save();
  modified = stylePage_->save() || modified;
  generalPage_->save();
  aiPage_->save();

  if (!modified)
    return;

  const int ret = configurator_->UpdateWorkspace(true);
  if (ret != 0) {
    QMessageBox::warning(this, QStringLiteral(u"蚌壳拼音"),
                         QStringLiteral(u"重新部署失败(错误码 %1)。\n"
                                        u"配置可能写入有误,请检查用户文件夹中的 yaml 文件。")
                             .arg(ret));
    return;
  }
  statusBar()->showMessage(QStringLiteral(u"已保存并重新部署"), 3000);
}

void MainWindow::closeEvent(QCloseEvent* event) {
  dictPage_->setSessionActive(false);
  QMainWindow::closeEvent(event);
}
