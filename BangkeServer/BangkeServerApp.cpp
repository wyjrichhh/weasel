#include "stdafx.h"
#include "BangkeServerApp.h"
#include <filesystem>

BangkeServerApp::BangkeServerApp()
    : m_handler(std::make_unique<BangkeRimeHandler>(&m_ui)) {
  m_server.SetRequestHandler(m_handler.get());
}

BangkeServerApp::~BangkeServerApp() {}

// 托盘图标与菜单均已收敛到 TSF 语言栏（dll 内实现），server 不再承载 UI 入口
int BangkeServerApp::Run() {
  if (!m_server.Start())
    return -1;

  m_ui.Create(m_server.GetHWnd());

  m_handler->Initialize();
  m_handler->OnUpdateUI([] {});

  int ret = m_server.Run();

  m_handler->Finalize();
  m_ui.Destroy();

  return ret;
}
