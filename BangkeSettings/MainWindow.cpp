#include "MainWindow.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFrame>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QUrl>

#include "AIPage.h"
#include "Configurator.h"
#include "DictPage.h"
#include "GeneralPage.h"
#include "StylePage.h"
#include "SwitcherPage.h"
#include "Theme.h"
#include "Ui.h"
#include <WeaselUtility.h>
#include <windows.h>

// 紧凑窗口下内容高出视口时滚动,不高时撑满
static QScrollArea* wrapInScroll(QWidget* page) {
  auto* scroll = new QScrollArea;
  scroll->setObjectName(QStringLiteral("pageScroll"));
  scroll->setWidget(page);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  return scroll;
}

MainWindow::MainWindow(Configurator* configurator, bool openDictPage,
                       QWidget* parent)
    : QMainWindow(parent), configurator_(configurator) {
  setWindowTitle(QStringLiteral(u"蚌壳拼音 · 设置"));
  resize(680, 460);
  setMinimumSize(600, 400);

  switcherPage_ = new SwitcherPage(this);
  generalPage_ = new GeneralPage(this);
  stylePage_ = new StylePage(this);
  aiPage_ = new AIPage(this);
  dictPage_ = new DictPage(this);

  // 高级页 = 方案选单 + 用户词典:两页内容都偏稀,合并不再各占一屏
  auto* advanced = new QWidget(this);
  auto* advLayout = new QVBoxLayout(advanced);
  advLayout->setContentsMargins(16, 12, 16, 12);
  advLayout->setSpacing(10);
  advLayout->addWidget(switcherPage_);
  advLayout->addWidget(dictPage_);
  advLayout->addStretch(1);

  stack_ = new QStackedWidget(this);
  stack_->addWidget(wrapInScroll(generalPage_));
  stack_->addWidget(wrapInScroll(stylePage_));
  stack_->addWidget(wrapInScroll(aiPage_));
  stack_->addWidget(wrapInScroll(advanced));

  nav_ = new QListWidget(this);
  nav_->setObjectName(QStringLiteral("nav"));
  nav_->setFixedWidth(112);
  nav_->setFrameShape(QFrame::NoFrame);
  nav_->setSpacing(2);
  nav_->addItems({QStringLiteral(u"通用设置"), QStringLiteral(u"界面样式"), QStringLiteral(u"AI 预测"), QStringLiteral(u"高级")});
  nav_->setCurrentRow(openDictPage ? 3 : 0);

  auto* saveBtn = new QPushButton(QStringLiteral(u"保存"), this);
  saveBtn->setObjectName(QStringLiteral("primary"));
  saveBtn->setDefault(true);
  auto* userDirBtn = new QPushButton(QStringLiteral(u"打开用户文件夹"), this);
  auto* logDirBtn = new QPushButton(QStringLiteral(u"打开日志文件夹"), this);

  auto* bottomRow = new QHBoxLayout();
  bottomRow->addWidget(userDirBtn);
  bottomRow->addWidget(logDirBtn);
  bottomRow->addStretch();
  bottomRow->addWidget(saveBtn);

  auto* body = new QWidget(this);
  body->setObjectName(QStringLiteral("pageRoot"));
  auto* bodyLayout = new QVBoxLayout(body);
  bodyLayout->setContentsMargins(16, 12, 16, 10);
  bodyLayout->setSpacing(10);
  auto* topRow = new QHBoxLayout();
  topRow->setSpacing(12);
  topRow->addWidget(nav_);
  topRow->addWidget(stack_, 1);
  bodyLayout->addLayout(topRow, 1);
  auto* line = new QFrame(body);
  line->setObjectName(QStringLiteral("line"));
  line->setFrameShape(QFrame::HLine);
  bodyLayout->addWidget(line);
  bodyLayout->addLayout(bottomRow);
  setCentralWidget(body);

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

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message,
                             qintptr* result) {
  MSG* msg = (MSG*)message;
  // 系统深浅切换:即时重应用主题(样式表+调色板+标题栏)
  if (msg->message == WM_SETTINGCHANGE &&
      msg->lParam &&
      wcscmp((const wchar_t*)msg->lParam, L"ImmersiveColorSet") == 0) {
    Theme::RefreshIfChanged();
  }
  return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::onPageChanged(int index) {
  stack_->setCurrentIndex(index);
  dictPage_->setSessionActive(false);
  // 离开高级页立即结束部署会话:BeginDictSession 拿住的 BangkeDeployerMutex
  // 若握到关窗,之后点「保存并重新部署」必撞"另一项部署任务"
  configurator_->EndDictSession();
  // 四页布局: 0=通用 1=界面样式 2=AI 3=高级(方案+词典)
  // 进入即重读配置(含外部/部署后的变更),未保存的改动随之丢弃
  if (index == 3) {
    switcherPage_->load();
    if (configurator_->BeginDictSession())
      dictPage_->setSessionActive(true);
  } else if (index == 0) {
    generalPage_->load();
  } else if (index == 1) {
    stylePage_->forceLoad();
  } else if (index == 2) {
    aiPage_->load();
  }
}

void MainWindow::saveAndDeploy() {
  bool modified = switcherPage_->save();
  modified = stylePage_->save() || modified;
  modified = generalPage_->save() || modified;
  modified = aiPage_->save() || modified;

  if (!modified) {
    makeToast(QStringLiteral(u"没有需要保存的修改"), this);
    return;
  }

  // 部署期间禁用保存,防连点撞部署互斥
  QPushButton* primaryBtn = nullptr;
  for (auto* b : findChildren<QPushButton*>()) {
    if (b->objectName() == QStringLiteral("primary")) {
      primaryBtn = b;
      b->setEnabled(false);
    }
  }
  QApplication::processEvents();

  // UpdateWorkspace(true) 内部对'已有部署在跑'等场景已弹提示,
  // 这里只对 >1 的意外失败码追加告警(1=mutex 冲突,信息已展示)
  const int ret = configurator_->UpdateWorkspace(true);
  if (primaryBtn)
    primaryBtn->setEnabled(true);
  if (ret > 1) {
    QMessageBox::warning(this, QStringLiteral(u"蚌壳拼音"),
                         QStringLiteral(u"保存失败(错误码 %1)。\n"
                                        u"配置可能写入有误,请检查用户文件夹中的 yaml 文件。")
                             .arg(ret));
    return;
  }
  if (ret == 0) {
    makeToast(QStringLiteral(u"已保存,输入法即刻生效"), this);
  }
}

void MainWindow::closeEvent(QCloseEvent* event) {
  dictPage_->setSessionActive(false);
  QMainWindow::closeEvent(event);
}
