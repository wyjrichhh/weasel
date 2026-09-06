// 蚌壳拼音安装器前端：UI 由本程序承担，MSI 静默执行
// 真实进度通过 MsiSetExternalUIRecord 回调获取
#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <windows.h>
#include <msi.h>
#include <msiquery.h>

#include <atomic>
#include <string>

static const wchar_t* kUpgradeCode = L"{8F1D4B33-9C2A-4E6D-B0F7-3A5C8E21D940}";

static QString FindMsi() {
  QDir dir(QApplication::applicationDirPath());
  const auto msis = dir.entryList({QStringLiteral("BangkeSetup-*.msi")}, QDir::Files, QDir::Name);
  return msis.isEmpty() ? QString() : dir.absoluteFilePath(msis.last());
}

static QString InstalledProductCode() {
  wchar_t code[40] = {0};
  for (DWORD i = 0;; ++i) {
    DWORD sz = 40;
    UINT r = MsiEnumRelatedProductsW(kUpgradeCode, 0, i, code);
    if (r != ERROR_SUCCESS)
      break;
    if (MsiQueryProductState(code) == INSTALLSTATE_DEFAULT)
      return QString::fromWCharArray(code);
  }
  return QString();
}

static QString InstalledVersion(const QString& code) {
  wchar_t v[64] = {0};
  DWORD sz = 64;
  if (MsiGetProductInfoW(code.toStdWString().c_str(), INSTALLPROPERTY_VERSIONSTRING, v, &sz) == ERROR_SUCCESS)
    return QString::fromWCharArray(v);
  return QStringLiteral(u"未知");
}

// ---------------- MSI 执行（msiexec 子进程，最成熟路径） ----------------

struct MsiJob {
  enum Op { Install, Repair, Uninstall } op;
  QString msiPath, productCode, installDir;
};

static QString MsiLogPath(const QString& msi) { return msi + ".log"; }

static QStringList BuildArgs(const MsiJob& job) {
  const QString log = QDir::toNativeSeparators(MsiLogPath(job.msiPath));
  QStringList args;
  switch (job.op) {
    case MsiJob::Install: {
      args << "/i" << QDir::toNativeSeparators(job.msiPath);
      QString dir = job.installDir;
      while (dir.endsWith('\\') || dir.endsWith('/'))
        dir.chop(1);
      if (!dir.isEmpty())
        args << ("INSTALLDIR=\"" + dir + "\"");
      break;
    }
    case MsiJob::Repair:
      args << "/f" << QDir::toNativeSeparators(job.msiPath);
      break;
    case MsiJob::Uninstall:
      args << "/x" << QDir::toNativeSeparators(job.msiPath);
      break;
  }
  args << "/qn" << "/l*v" << log;
  return args;
}

// ---------------- UI ----------------

class MainWindow : public QWidget {
  Q_OBJECT

 public:
  MainWindow() {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(600, 420);

    m_msiPath = FindMsi();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto* panel = new QFrame(this);
    panel->setObjectName("panel");
    root->addWidget(panel);

    auto* box = new QVBoxLayout(panel);
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(0);

    // 标题栏
    auto* title = new QHBoxLayout();
    auto* titleText = new QLabel(QStringLiteral(u"蚌壳拼音 · 安装"), panel);
    titleText->setObjectName("title");
    auto* closeBtn = new QPushButton(QStringLiteral(u"✕"), panel);
    closeBtn->setObjectName("close");
    closeBtn->setFixedSize(32, 32);
    connect(closeBtn, &QPushButton::clicked, qApp, &QApplication::quit);
    title->addSpacing(24);
    title->addWidget(titleText);
    title->addStretch();
    title->addWidget(closeBtn);
    box->addLayout(title);

    m_stack = new QStackedWidget(panel);
    box->addWidget(m_stack, 1);
    buildWelcomePage();
    buildProgressPage();
    buildFinishPage();

    refreshState();
  }

 public slots:
  void msiFinished() {
    if (m_pollTimer) { m_pollTimer->stop(); m_pollTimer->deleteLater(); m_pollTimer = nullptr; }
    int r = (m_exitCode >= 0) ? m_exitCode
             : (m_proc ? (m_proc->error() == QProcess::UnknownError ? m_proc->exitCode() : 1603) : -1);
    if (m_proc) { m_proc->deleteLater(); m_proc = nullptr; }
    m_stack->setCurrentWidget(finishPage_);
    const bool ok = (r == 0);
    if (ok) {
      const wchar_t* t = m_job->op == MsiJob::Uninstall ? L"卸载完成"
                       : m_job->op == MsiJob::Repair ? L"修复完成" : L"安装完成";
      m_finishTitle->setText(QString::fromWCharArray(t));
      m_finishDetail->setText(m_job->op == MsiJob::Uninstall
                                  ? QStringLiteral(u"蚌壳拼音已卸载（用户词库保留）。建议注销一次，输入法列表中的残留图标即会消失。")
                                  : QStringLiteral(u"蚌壳拼音已就绪，按 Win+空格 切换开始使用。"));
    } else {
      m_finishTitle->setText(QStringLiteral(u"操作失败 (代码 0x%1)").arg((uint)r, 8, 16, QChar('0')));
      m_finishDetail->setText(QStringLiteral(u"详细日志：") +
                              QDir::toNativeSeparators(m_msiPath + ".log"));
    }
    m_launchCheck->setVisible(ok && m_job->op != MsiJob::Uninstall);
  }

 protected:
  void mousePressEvent(QMouseEvent* e) override {
    if (e->button() == Qt::LeftButton)
      m_dragPos = e->globalPosition().toPoint();
  }
  void mouseMoveEvent(QMouseEvent* e) override {
    if (!m_dragPos.isNull()) {
      move(pos() + e->globalPosition().toPoint() - m_dragPos);
      m_dragPos = e->globalPosition().toPoint();
    }
  }
  void mouseReleaseEvent(QMouseEvent*) override { m_dragPos = {}; }

 private:
  void startJob(MsiJob::Op op) {
    if (m_msiPath.isEmpty()) {
      QMessageBox::warning(this, QStringLiteral(u"蚌壳拼音"),
                           QStringLiteral(u"未找到 BangkeSetup-*.msi，请与安装程序放在同一目录。"));
      return;
    }
    m_job = new MsiJob{op, m_msiPath, m_productCode, m_dirEdit->text().trimmed()};
    m_bar->setRange(0, 0);  // 不确定进度：真实完成以 msiexec 退出码为准
    m_actionLabel->setText(QStringLiteral(u"正在准备…"));
    m_stack->setCurrentWidget(progressPage_);

    QFile::remove(MsiLogPath(m_msiPath));
    m_proc = new QProcess(this);
    m_proc->setProgram("msiexec.exe");
    m_proc->setArguments(BuildArgs(*m_job));
    connect(m_proc, &QProcess::finished, this, &MainWindow::msiFinished);
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
      m_exitCode = -1;
      msiFinished();
    });
    m_proc->start();

    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &MainWindow::pollLog);
    m_pollTimer->start(400);
  }

  void pollLog() {
    const QString log = MsiLogPath(m_msiPath);
    QFile f(log);
    if (!f.open(QIODevice::ReadOnly))
      return;
    // 只看尾部一段，找最近的已知 Action
    qint64 sz = f.size();
    f.seek(sz > 8192 ? sz - 8192 : 0);
    const QByteArray tail = f.readAll();
    f.close();
    static const struct { const char* a; const wchar_t* t; } steps[] = {
        {"InstallValidate", L"正在校验安装"},
        {"InstallFiles", L"正在复制文件"},
        {"WriteRegistryValues", L"正在写入注册表"},
        {"RegisterTSF", L"正在注册输入法"},
        {"FirstDeploy", L"正在部署输入方案"},
        {"StartServer", L"正在启动服务"},
        {"RemoveFiles", L"正在移除文件"},
        {"UnregisterTSF", L"正在注销输入法"},
    };
    for (int i = _countof(steps) - 1; i >= 0; --i) {
      if (tail.contains(steps[i].a)) {
        m_actionLabel->setText(QString::fromWCharArray(steps[i].t));
        break;
      }
    }
  }

  void refreshState() {
    m_productCode = InstalledProductCode();
    const bool installed = !m_productCode.isEmpty();
    installedGroup_->setVisible(installed);
    freshGroup_->setVisible(!installed);
    if (installed)
      m_installedLabel->setText(QStringLiteral(u"已安装版本 %1").arg(InstalledVersion(m_productCode)));
  }

  QPushButton* accentButton(const QString& text) {
    auto* b = new QPushButton(text);
    b->setObjectName("accent");
    b->setMinimumHeight(44);
    b->setCursor(Qt::PointingHandCursor);
    return b;
  }

  void buildWelcomePage() {
    welcomePage_ = new QFrame();
    welcomePage_->setObjectName("page");
    auto* box = new QVBoxLayout(welcomePage_);
    box->setContentsMargins(36, 8, 36, 28);
    box->setSpacing(12);

    auto* logo = new QLabel(QStringLiteral(u"蚌"), welcomePage_);
    logo->setObjectName("logo");
    logo->setAlignment(Qt::AlignCenter);
    box->addSpacing(10);
    box->addWidget(logo, 0, Qt::AlignHCenter);
    box->addWidget(new QLabel(QStringLiteral(u"蚌壳拼音输入法"), welcomePage_), 0, Qt::AlignHCenter);
    auto* sub = new QLabel(QStringLiteral(u"Rime 内核 · Windows 原生 TSF · 蚌壳出品"), welcomePage_);
    sub->setObjectName("sub");
    sub->setAlignment(Qt::AlignCenter);
    box->addWidget(sub, 0, Qt::AlignHCenter);
    box->addSpacing(8);

    freshGroup_ = new QFrame(welcomePage_);
    auto* fresh = new QVBoxLayout(freshGroup_);
    fresh->setContentsMargins(0, 0, 0, 0);
    fresh->setSpacing(10);
    auto* dirRow = new QHBoxLayout();
    m_dirEdit = new QLineEdit(QStringLiteral(u"C:\\Program Files\\Bangke Pinyin"), freshGroup_);
    auto* browse = new QPushButton(QStringLiteral(u"浏览"), freshGroup_);
    connect(browse, &QPushButton::clicked, this, [this] {
      QString d = QFileDialog::getExistingDirectory(this, QStringLiteral(u"选择安装目录"),
                                                    m_dirEdit->text());
      if (!d.isEmpty())
        m_dirEdit->setText(d);
    });
    dirRow->addWidget(m_dirEdit, 1);
    dirRow->addWidget(browse);
    fresh->addLayout(dirRow);
    auto* installBtn = accentButton(QStringLiteral(u"立即安装"));
    connect(installBtn, &QPushButton::clicked, this, [this] { startJob(MsiJob::Install); });
    fresh->addWidget(installBtn);
    box->addWidget(freshGroup_);

    installedGroup_ = new QFrame(welcomePage_);
    auto* inst = new QVBoxLayout(installedGroup_);
    inst->setContentsMargins(0, 0, 0, 0);
    inst->setSpacing(10);
    m_installedLabel = new QLabel(installedGroup_);
    m_installedLabel->setAlignment(Qt::AlignCenter);
    inst->addWidget(m_installedLabel);
    auto* btnRow = new QHBoxLayout();
    auto* repairBtn = new QPushButton(QStringLiteral(u"修复安装"));
    auto* reinstallBtn = new QPushButton(QStringLiteral(u"重新安装"));
    auto* uninstallBtn = new QPushButton(QStringLiteral(u"卸载"));
    for (auto* b : {repairBtn, reinstallBtn, uninstallBtn}) {
      b->setMinimumHeight(40);
      b->setCursor(Qt::PointingHandCursor);
    }
    connect(repairBtn, &QPushButton::clicked, this, [this] { startJob(MsiJob::Repair); });
    connect(reinstallBtn, &QPushButton::clicked, this, [this] { startJob(MsiJob::Install); });
    connect(uninstallBtn, &QPushButton::clicked, this, [this] {
      if (QMessageBox::question(this, QStringLiteral(u"卸载"),
                                QStringLiteral(u"确定卸载蚌壳拼音吗？用户词库会保留。")) == QMessageBox::Yes)
        startJob(MsiJob::Uninstall);
    });
    btnRow->addWidget(repairBtn);
    btnRow->addWidget(reinstallBtn);
    btnRow->addWidget(uninstallBtn);
    inst->addLayout(btnRow);
    box->addWidget(installedGroup_);
    box->addStretch();

    m_stack->addWidget(welcomePage_);
  }

  void buildProgressPage() {
    progressPage_ = new QFrame();
    progressPage_->setObjectName("page");
    auto* box = new QVBoxLayout(progressPage_);
    box->setContentsMargins(48, 48, 48, 48);
    box->setSpacing(16);
    m_actionLabel = new QLabel(QStringLiteral(u"正在准备…"), progressPage_);
    m_actionLabel->setAlignment(Qt::AlignCenter);
    m_bar = new QProgressBar(progressPage_);
    m_bar->setTextVisible(false);
    m_bar->setFixedHeight(6);
    box->addStretch();
    box->addWidget(m_actionLabel);
    box->addWidget(m_bar);
    box->addStretch();
    m_stack->addWidget(progressPage_);
  }

  void buildFinishPage() {
    finishPage_ = new QFrame();
    finishPage_->setObjectName("page");
    auto* box = new QVBoxLayout(finishPage_);
    box->setContentsMargins(48, 48, 48, 36);
    box->setSpacing(12);
    m_finishTitle = new QLabel(finishPage_);
    m_finishTitle->setObjectName("finishTitle");
    m_finishTitle->setAlignment(Qt::AlignCenter);
    m_finishDetail = new QLabel(finishPage_);
    m_finishDetail->setObjectName("sub");
    m_finishDetail->setAlignment(Qt::AlignCenter);
    m_finishDetail->setWordWrap(true);
    m_launchCheck = new QCheckBox(QStringLiteral(u"立即启动输入法服务"), finishPage_);
    m_launchCheck->setChecked(true);
    auto* doneBtn = accentButton(QStringLiteral(u"完成"));
    connect(doneBtn, &QPushButton::clicked, this, [this] {
      if (m_launchCheck->isChecked() && m_launchCheck->isVisible()) {
        QString dir = m_dirEdit->text().trimmed();
        if (dir.isEmpty())
          dir = QStringLiteral(u"C:\\Program Files\\Bangke Pinyin");
        // explorer 拉起 = 用户会话、非提升，避免服务继承管理员上下文
        QProcess::startDetached("explorer",
                                {QDir::toNativeSeparators(dir + "\\BangkeServer.exe")});
      }
      qApp->quit();
    });
    box->addStretch();
    box->addWidget(m_finishTitle);
    box->addWidget(m_finishDetail);
    box->addWidget(m_launchCheck, 0, Qt::AlignHCenter);
    box->addSpacing(8);
    box->addWidget(doneBtn);
    box->addStretch();
    m_stack->addWidget(finishPage_);
  }

  QString m_msiPath, m_productCode;
  QStackedWidget* m_stack = nullptr;
  QFrame *welcomePage_ = nullptr, *progressPage_ = nullptr, *finishPage_ = nullptr;
  QFrame *freshGroup_ = nullptr, *installedGroup_ = nullptr;
  QLabel *m_installedLabel = nullptr, *m_actionLabel = nullptr, *m_finishTitle = nullptr,
         *m_finishDetail = nullptr;
  QLineEdit* m_dirEdit = nullptr;
  QProgressBar* m_bar = nullptr;
  QCheckBox* m_launchCheck = nullptr;
  MsiJob* m_job = nullptr;
  QProcess* m_proc = nullptr;
  QTimer* m_pollTimer = nullptr;
  int m_exitCode = -1;
  QPoint m_dragPos;
};

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral(u"蚌壳拼音安装器"));

  app.setStyleSheet(QStringLiteral(uR"(
    QWidget { background: transparent; color: #e8ecf1; font-family: "Microsoft YaHei UI"; font-size: 14px; }
    #panel { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #131b26, stop:1 #0d141d);
             border-radius: 14px; border: 1px solid #243244; }
    #title { font-size: 15px; color: #9fb6cd; }
    #close { background: transparent; border: none; color: #7e93a8; font-size: 15px; }
    #close:hover { background: #e81123; color: white; border-radius: 4px; }
    #logo { min-width: 92px; min-height: 92px; max-width: 92px; max-height: 92px;
            background: qradialgradient(cx:0.5, cy:0.35, radius:1.1, stop:0 #35618f, stop:1 #16233a);
            border-radius: 24px; color: #eaf2fb; font-size: 52px; font-weight: 600;
            border: 1px solid #3d5a82; }
    #sub { color: #7e93a8; font-size: 12px; }
    QLineEdit { background: #0b1119; border: 1px solid #2c3d52; border-radius: 6px; padding: 8px 10px; color: #dfe7ef; }
    QPushButton { background: #1b2736; border: 1px solid #2c3d52; border-radius: 8px; color: #dfe7ef; padding: 0 18px; }
    QPushButton:hover { border-color: #4a9df0; color: #ffffff; }
    #accent { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #2f7cd6, stop:1 #4aa8f0);
              border: none; color: white; font-size: 15px; font-weight: 600; border-radius: 8px; }
    #accent:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #3a8ae8, stop:1 #5cb5f7); }
    QProgressBar { background: #0b1119; border: none; border-radius: 3px; }
    QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #2f7cd6, stop:1 #4aa8f0); border-radius: 3px; }
    QCheckBox { color: #9fb6cd; }
    #finishTitle { font-size: 22px; font-weight: 600; color: #eaf2fb; }
  )"));

  MainWindow w;
  w.show();
  return app.exec();
}

#include "main.moc"
