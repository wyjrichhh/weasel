// 蚌壳拼音安装器前端：UI 由本程序承担，MSI 以 msiexec /qn 子进程执行。
// 子进程隔离保证 UI 崩溃不影响安装事务（2026-09-06 一次 UI 层 UAF 崩溃验证了这一点）。
// 阶段进度来自对 verbose log 的增量读取，不做假百分比。
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <windows.h>
#include <msi.h>
#include <msiquery.h>

#include <tuple>

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

// 从安装包的 Property 表读 ProductVersion，用于欢迎页的版本对比展示
static QString MsiPackageVersion(const QString& path) {
  MSIHANDLE db = 0;
  if (MsiOpenDatabaseW(path.toStdWString().c_str(), MSIDBOPEN_READONLY, &db) != ERROR_SUCCESS)
    return QString();
  QString result;
  MSIHANDLE view = 0;
  if (MsiDatabaseOpenViewW(db,
                           L"SELECT `Value` FROM `Property` WHERE `Property` = 'ProductVersion'",
                           &view) == ERROR_SUCCESS) {
    if (MsiViewExecute(view, 0) == ERROR_SUCCESS) {
      MSIHANDLE rec = 0;
      if (MsiViewFetch(view, &rec) == ERROR_SUCCESS) {
        wchar_t buf[64] = {0};
        DWORD sz = 64;
        if (MsiRecordGetStringW(rec, 1, buf, &sz) == ERROR_SUCCESS)
          result = QString::fromWCharArray(buf, sz);
        MsiCloseHandle(rec);
      }
      MsiViewClose(view);
    }
    MsiCloseHandle(view);
  }
  MsiCloseHandle(db);
  return result;
}

static std::tuple<int, int, int> ParseVersion(const QString& v) {
  const auto parts = v.split('.');
  int a = 0, b = 0, c = 0;
  if (parts.size() > 0) a = parts[0].toInt();
  if (parts.size() > 1) b = parts[1].toInt();
  if (parts.size() > 2) c = parts[2].toInt();
  return {a, b, c};
}

// ---------------- MSI 执行 ----------------

struct MsiJob {
  enum Op { Install, Repair, Uninstall } op;
  QString msiPath, productCode;
  bool wasInstalled = false;  // 操作开始前产品是否已装（区分安装/升级文案）
};

static QString MsiLogPath(const QString& msi) { return msi + ".log"; }

static QStringList BuildArgs(const MsiJob& job) {
  const QString log = QDir::toNativeSeparators(MsiLogPath(job.msiPath));
  QStringList args;
  switch (job.op) {
    case MsiJob::Install:
      args << "/i" << QDir::toNativeSeparators(job.msiPath);
      break;
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
    resize(600, 440);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto* panel = new QFrame(this);
    panel->setObjectName("panel");
    root->addWidget(panel);

    auto* box = new QVBoxLayout(panel);
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(0);

    auto* title = new QHBoxLayout();
    auto* titleText = new QLabel(QStringLiteral(u"蚌壳拼音 · 安装"), panel);
    titleText->setObjectName("title");
    m_closeBtn = new QPushButton(QStringLiteral(u"✕"), panel);
    m_closeBtn->setObjectName("close");
    m_closeBtn->setFixedSize(32, 32);
    connect(m_closeBtn, &QPushButton::clicked, this, [this] { close(); });
    title->addSpacing(24);
    title->addWidget(titleText);
    title->addStretch();
    title->addWidget(m_closeBtn);
    box->addLayout(title);

    m_stack = new QStackedWidget(panel);
    box->addWidget(m_stack, 1);
    buildWelcomePage();
    buildProgressPage();
    buildFinishPage();

    refreshState();
  }

 protected:
  // 任务进行中禁止关闭：中断 msiexec 并不可靠（客户端进程被杀后服务端事务照跑），
  // 且安装中途退出 UI 曾引发 Qt 内部 UAF 崩溃，索性从结构上禁止该路径。
  void closeEvent(QCloseEvent* e) override {
    if (m_running) {
      e->ignore();
      m_progressHint->setText(QStringLiteral(u"操作正在进行，请稍候…"));
      QTimer::singleShot(2000, this, [this] {
        if (m_running)
          m_progressHint->setText(progressHint());
      });
      return;
    }
    e->accept();
  }
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
  static QString progressHint() { return QStringLiteral(u"过程中请勿关闭本窗口"); }

  void startJob(MsiJob::Op op) {
    if (m_msiPath.isEmpty()) {
      QMessageBox::warning(this, QStringLiteral(u"蚌壳拼音"),
                           QStringLiteral(u"未找到 BangkeSetup-*.msi，请与安装程序放在同一目录。"));
      return;
    }
    m_job = MsiJob{op, m_msiPath, m_productCode, !m_productCode.isEmpty()};
    m_bar->setRange(0, 0);  // 不确定进度：以 msiexec 退出码为准
    m_actionLabel->setText(QStringLiteral(u"正在准备…"));
    m_progressTitle->setText(op == MsiJob::Uninstall ? QStringLiteral(u"正在卸载 蚌壳拼音")
                                : op == MsiJob::Repair  ? QStringLiteral(u"正在修复 蚌壳拼音")
                                                        : QStringLiteral(u"正在安装 蚌壳拼音"));
    m_progressHint->setText(progressHint());
    m_running = true;
    m_stack->setCurrentWidget(progressPage_);

    QFile::remove(MsiLogPath(m_msiPath));
    m_proc = new QProcess(this);
    m_proc->setProgram("msiexec.exe");
    m_proc->setArguments(BuildArgs(m_job));
    connect(m_proc, &QProcess::finished, this,
            [this](int code, QProcess::ExitStatus st) {
              m_exitCode = (st == QProcess::NormalExit) ? code : -1;
              msiFinished();
            });
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
      m_exitCode = -1;
      msiFinished();
    });
    m_proc->start();
    startLogWatch();
  }

  // ---- 阶段进度：增量读 verbose log，按出现顺序取最后的标记 ----

  void startLogWatch() {
    m_logPos = 0;
    m_logFile.setFileName(MsiLogPath(m_msiPath));
    m_watcher = new QFileSystemWatcher(this);
    // log 文件由 msiexec 创建，先盯目录等它出现
    m_watcher->addPath(QFileInfo(m_logFile).absolutePath());
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { readLogDelta(); });
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { readLogDelta(); });
    // msiexec 独占写时 watcher 事件可能吞掉，低频轮询兜底
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, [this] { readLogDelta(); });
    m_pollTimer->start(250);
  }

  void stopLogWatch() {
    if (m_pollTimer) { m_pollTimer->stop(); m_pollTimer->deleteLater(); m_pollTimer = nullptr; }
    if (m_watcher) { m_watcher->deleteLater(); m_watcher = nullptr; }
    if (m_logFile.isOpen())
      m_logFile.close();
  }

  void readLogDelta() {
    if (!m_logFile.isOpen()) {
      if (!m_logFile.exists())
        return;
      if (!m_logFile.open(QIODevice::ReadOnly))
        return;
      m_logPos = 0;
    }
    const qint64 size = m_logFile.size();
    if (size < m_logPos) {  // 文件被重建（重试场景）
      m_logFile.seek(0);
      m_logPos = 0;
    }
    if (m_logFile.pos() != m_logPos)
      m_logFile.seek(m_logPos);
    const QByteArray chunk = m_logFile.readAll();
    m_logPos = m_logFile.pos();
    if (chunk.isEmpty())
      return;

    static const struct { const char* a; const wchar_t* t; } steps[] = {
        {"StopServer", L"正在停止输入法服务"},
        {"InstallValidate", L"正在校验安装"},
        {"InstallFiles", L"正在复制文件"},
        {"WriteRegistryValues", L"正在写入注册表"},
        {"RegisterTSF", L"正在注册输入法"},
        {"FirstDeploy", L"正在部署输入方案"},
        {"StartServer", L"正在启动服务"},
        {"RemoveFiles", L"正在移除文件"},
        {"UnregisterTSF", L"正在注销输入法"},
        {"Cleanup", L"正在清理残留"},
    };
    int bestPos = -1;
    const wchar_t* bestText = nullptr;
    for (const auto& s : steps) {
      const int p = chunk.lastIndexOf(s.a);
      if (p > bestPos) {
        bestPos = p;
        bestText = s.t;
      }
    }
    if (bestText)
      m_actionLabel->setText(QString::fromWCharArray(bestText));
  }

  void msiFinished() {
    stopLogWatch();
    m_running = false;
    const int r = m_exitCode;
    if (m_proc) { m_proc->deleteLater(); m_proc = nullptr; }
    m_stack->setCurrentWidget(finishPage_);

    const bool ok = (r == 0);
    if (ok) {
      QString title, detail;
      switch (m_job.op) {
        case MsiJob::Uninstall:
          title = QStringLiteral(u"卸载完成");
          detail = QStringLiteral(u"蚌壳拼音已卸载（用户词库保留）。\n建议注销一次，输入指示器中的残留图标即会消失。");
          break;
        case MsiJob::Repair:
          title = QStringLiteral(u"修复完成");
          detail = QStringLiteral(u"蚌壳拼音已恢复就绪。");
          break;
        case MsiJob::Install:
          if (m_job.wasInstalled) {
            title = QStringLiteral(u"升级完成");
            detail = QStringLiteral(u"已更新到 %1，按 Win+空格 即可继续使用。").arg(m_pkgVersion);
          } else {
            title = QStringLiteral(u"安装完成");
            detail = QStringLiteral(u"蚌壳拼音已就绪，按 Win+空格 切换到蚌壳拼音开始使用。");
          }
          break;
      }
      m_finishTitle->setText(title);
      m_finishDetail->setText(detail);
    } else {
      m_finishTitle->setText(QStringLiteral(u"操作失败 (代码 0x%1)").arg((uint)r, 8, 16, QChar('0')));
      m_finishDetail->setText(QStringLiteral(u"详细日志：") +
                              QDir::toNativeSeparators(MsiLogPath(m_msiPath)));
    }
    m_launchCheck->setVisible(ok && m_job.op != MsiJob::Uninstall);
    m_logoffBtn->setVisible(ok && m_job.op == MsiJob::Uninstall);
    m_openLogBtn->setVisible(!ok && !m_msiPath.isEmpty());
  }

  void refreshState() {
    m_msiPath = FindMsi();
    m_pkgVersion = MsiPackageVersion(m_msiPath);
    m_productCode = InstalledProductCode();
    const bool installed = !m_productCode.isEmpty();
    const bool haveMsi = !m_msiPath.isEmpty();

    m_msiMissing->setVisible(!haveMsi);
    freshGroup_->setVisible(haveMsi && !installed);
    installedGroup_->setVisible(haveMsi && installed);

    if (haveMsi)
      m_pkgLabel->setText(QStringLiteral(u"安装包版本 %1").arg(
          m_pkgVersion.isEmpty() ? QStringLiteral(u"未知") : m_pkgVersion));

    if (installed) {
      const QString cur = InstalledVersion(m_productCode);
      m_installedLabel->setText(QStringLiteral(u"已安装版本 %1").arg(cur));
      // 升级与重装都走 /i：MajorUpgrade 自动按 UpgradeCode 接管旧版本
      m_primaryBtn->setText(ParseVersion(m_pkgVersion) > ParseVersion(cur)
                                ? QStringLiteral(u"升级到 %1").arg(m_pkgVersion)
                                : QStringLiteral(u"重新安装"));
    }
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
    m_pkgLabel = new QLabel(welcomePage_);
    m_pkgLabel->setObjectName("sub");
    m_pkgLabel->setAlignment(Qt::AlignCenter);
    box->addWidget(m_pkgLabel, 0, Qt::AlignHCenter);
    box->addSpacing(8);

    m_msiMissing = new QLabel(
        QStringLiteral(u"未找到 BangkeSetup-*.msi。\n请将本程序与安装包放在同一目录后重新运行。"), welcomePage_);
    m_msiMissing->setObjectName("warn");
    m_msiMissing->setAlignment(Qt::AlignCenter);
    m_msiMissing->setWordWrap(true);
    box->addWidget(m_msiMissing);

    freshGroup_ = new QFrame(welcomePage_);
    auto* fresh = new QVBoxLayout(freshGroup_);
    fresh->setContentsMargins(0, 0, 0, 0);
    fresh->setSpacing(10);
    auto* pathNote = new QLabel(QStringLiteral(u"将安装到 C:\\Program Files\\Bangke Pinyin"), freshGroup_);
    pathNote->setObjectName("sub");
    pathNote->setAlignment(Qt::AlignCenter);
    fresh->addWidget(pathNote);
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
    m_primaryBtn = accentButton(QStringLiteral(u"重新安装"));
    connect(m_primaryBtn, &QPushButton::clicked, this, [this] { startJob(MsiJob::Install); });
    inst->addWidget(m_primaryBtn);
    auto* btnRow = new QHBoxLayout();
    auto* repairBtn = new QPushButton(QStringLiteral(u"修复安装"));
    auto* uninstallBtn = new QPushButton(QStringLiteral(u"卸载"));
    uninstallBtn->setObjectName("danger");
    for (auto* b : {repairBtn, uninstallBtn}) {
      b->setMinimumHeight(36);
      b->setCursor(Qt::PointingHandCursor);
    }
    connect(repairBtn, &QPushButton::clicked, this, [this] { startJob(MsiJob::Repair); });
    connect(uninstallBtn, &QPushButton::clicked, this, [this] {
      if (QMessageBox::question(this, QStringLiteral(u"卸载"),
                                QStringLiteral(u"确定卸载蚌壳拼音吗？用户词库会保留。")) == QMessageBox::Yes)
        startJob(MsiJob::Uninstall);
    });
    btnRow->addWidget(repairBtn, 1);
    btnRow->addWidget(uninstallBtn, 1);
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
    box->setSpacing(12);
    m_progressTitle = new QLabel(progressPage_);
    m_progressTitle->setAlignment(Qt::AlignCenter);
    m_actionLabel = new QLabel(QStringLiteral(u"正在准备…"), progressPage_);
    m_actionLabel->setAlignment(Qt::AlignCenter);
    m_bar = new QProgressBar(progressPage_);
    m_bar->setTextVisible(false);
    m_bar->setFixedHeight(6);
    m_progressHint = new QLabel(progressPage_);
    m_progressHint->setObjectName("sub");
    m_progressHint->setAlignment(Qt::AlignCenter);
    box->addStretch();
    box->addWidget(m_progressTitle);
    box->addWidget(m_actionLabel);
    box->addWidget(m_bar);
    box->addSpacing(4);
    box->addWidget(m_progressHint);
    box->addStretch();
    m_stack->addWidget(progressPage_);
  }

  void buildFinishPage() {
    finishPage_ = new QFrame();
    finishPage_->setObjectName("page");
    auto* box = new QVBoxLayout(finishPage_);
    box->setContentsMargins(48, 44, 48, 32);
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

    m_logoffBtn = new QPushButton(QStringLiteral(u"立即注销（清除输入法残留）"));
    m_logoffBtn->setMinimumHeight(36);
    m_logoffBtn->setVisible(false);
    connect(m_logoffBtn, &QPushButton::clicked, this, [] {
      QProcess::startDetached(QStringLiteral("shutdown"), {QStringLiteral("/l")});
    });

    m_openLogBtn = new QPushButton(QStringLiteral(u"打开日志文件"));
    m_openLogBtn->setMinimumHeight(36);
    m_openLogBtn->setVisible(false);
    connect(m_openLogBtn, &QPushButton::clicked, this, [this] {
      const QString log = QDir::toNativeSeparators(MsiLogPath(m_msiPath));
      QProcess::startDetached(QStringLiteral("explorer"),
                              {QStringLiteral("/select,"), log});
    });

    auto* doneBtn = accentButton(QStringLiteral(u"完成"));
    connect(doneBtn, &QPushButton::clicked, this, [this] {
      if (m_launchCheck->isChecked() && m_launchCheck->isVisible()) {
        // explorer 拉起 = 用户会话、非提升，避免服务继承管理员上下文
        QProcess::startDetached(
            QStringLiteral("explorer"),
            {QDir::toNativeSeparators(QStringLiteral(u"C:\\Program Files\\Bangke Pinyin\\BangkeServer.exe"))});
      }
      close();
    });
    box->addStretch();
    box->addWidget(m_finishTitle);
    box->addWidget(m_finishDetail);
    box->addWidget(m_launchCheck, 0, Qt::AlignHCenter);
    box->addSpacing(8);
    box->addWidget(m_logoffBtn);
    box->addWidget(m_openLogBtn);
    box->addSpacing(8);
    box->addWidget(doneBtn);
    box->addStretch();
    m_stack->addWidget(finishPage_);
  }

  QString m_msiPath, m_productCode, m_pkgVersion;
  QStackedWidget* m_stack = nullptr;
  QFrame *welcomePage_ = nullptr, *progressPage_ = nullptr, *finishPage_ = nullptr;
  QFrame *freshGroup_ = nullptr, *installedGroup_ = nullptr;
  QLabel *m_installedLabel = nullptr, *m_pkgLabel = nullptr, *m_msiMissing = nullptr,
         *m_progressTitle = nullptr, *m_actionLabel = nullptr, *m_progressHint = nullptr,
         *m_finishTitle = nullptr, *m_finishDetail = nullptr;
  QProgressBar* m_bar = nullptr;
  QCheckBox* m_launchCheck = nullptr;
  QPushButton *m_primaryBtn = nullptr, *m_closeBtn = nullptr,
              *m_logoffBtn = nullptr, *m_openLogBtn = nullptr;
  MsiJob m_job;
  bool m_running = false;
  QProcess* m_proc = nullptr;
  QTimer* m_pollTimer = nullptr;
  QFileSystemWatcher* m_watcher = nullptr;
  QFile m_logFile;
  qint64 m_logPos = 0;
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
    #warn { color: #e0a75e; font-size: 13px; }
    QPushButton { background: #1b2736; border: 1px solid #2c3d52; border-radius: 8px; color: #dfe7ef; padding: 0 18px; }
    QPushButton:hover { border-color: #4a9df0; color: #ffffff; }
    QPushButton:disabled { color: #55637a; }
    #accent { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #2f7cd6, stop:1 #4aa8f0);
              border: none; color: white; font-size: 15px; font-weight: 600; border-radius: 8px; }
    #accent:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #3a8ae8, stop:1 #5cb5f7); }
    #danger { color: #e08080; }
    #danger:hover { border-color: #e05656; color: #ff9c9c; }
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
