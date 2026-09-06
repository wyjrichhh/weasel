#include <QApplication>
#include <QMessageBox>
#include <windows.h>

#include "MainWindow.h"
#include "Configurator.h"
#include <WeaselUtility.h>

namespace {

enum class Mode { Gui, Deploy, Sync, Install, Dict, Help };

Mode parseMode(const QString& arg) {
  QString a = arg;
  if (a.startsWith('/') || a.startsWith('-'))
    a.remove(0, 1);
  a = a.toLower();
  if (a == "deploy")
    return Mode::Deploy;
  if (a == "sync")
    return Mode::Sync;
  if (a == "install")
    return Mode::Install;
  if (a == "dict")
    return Mode::Dict;
  if (a == "?" || a == "help")
    return Mode::Help;
  return Mode::Gui;
}

void showUsage(QWidget* parent) {
  QMessageBox::information(
      parent, L"蚌壳拼音·设置",
      L"用法: BangkeSettings.exe [选项]\n"
      L"/deploy  - 重新部署\n"
      L"/dict    - 词典管理\n"
      L"/sync    - 同步用户数据\n"
      L"/install - 静默首次部署（安装器调用）\n"
      L"/?       - 显示本帮助");
}

}  // namespace

int main(int argc, char* argv[]) {
  // WeaselTSF 以此互斥量探测部署进程是否在运行，勿改名
  HANDLE hMutex = CreateMutexW(NULL, TRUE, L"BangkeDeployerExclusiveMutex");
  if (!hMutex || GetLastError() == ERROR_ALREADY_EXISTS)
    return 1;

  int ret = 0;
  {
    QApplication app(argc, argv);
    QApplication::setApplicationName(L"蚌壳拼音·设置");
    QApplication::setOrganizationName(L"Bangke");

    Mode mode = argc > 1 ? parseMode(QString::fromLocal8Bit(argv[1])) : Mode::Gui;

    Configurator configurator;
    configurator.Initialize();

    switch (mode) {
      case Mode::Help:
        showUsage(nullptr);
        break;
      case Mode::Deploy:
      case Mode::Install:
        ret = configurator.UpdateWorkspace();
        break;
      case Mode::Sync:
        ret = configurator.SyncUserData();
        break;
      case Mode::Dict:
      case Mode::Gui: {
        MainWindow window(&configurator, mode == Mode::Dict);
        window.show();
        ret = app.exec();
        break;
      }
    }
  }

  if (hMutex)
    CloseHandle(hMutex);
  return ret;
}
