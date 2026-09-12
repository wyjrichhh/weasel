#pragma once

#include "resource.h"
#include <resource.h>
#include <BangkeIPC.h>
#include <BangkeUI.h>
#include <BangkeRime.h>
#include <BangkeUtility.h>
#include <filesystem>
#include <functional>
#include <memory>

namespace fs = std::filesystem;

class BangkeServerApp {
 public:
  static bool execute(const fs::path& cmd, const std::wstring& args) {
    // 子进程必须显式给定工作目录：server 若由计划任务等拉起，继承的
    // System32 工作目录会让子进程初始化阶段 0xc0000022
    return (uintptr_t)ShellExecuteW(NULL, NULL, cmd.c_str(), args.c_str(),
                                    cmd.parent_path().c_str(),
                                    SW_SHOWNORMAL) > 32;
  }

  static bool explore(const fs::path& path) {
    std::wstring quoted_path(L"\"" + path.wstring() + L"\"");
    return (uintptr_t)ShellExecuteW(NULL, L"explore", quoted_path.c_str(), NULL,
                                    NULL, SW_SHOWNORMAL) > 32;
  }

  static bool open(const fs::path& path) {
    return (uintptr_t)ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL,
                                    SW_SHOWNORMAL) > 32;
  }

  static fs::path install_dir() {
    WCHAR exe_path[MAX_PATH] = {0};
    GetModuleFileNameW(GetModuleHandle(NULL), exe_path, _countof(exe_path));
    return fs::path(exe_path).remove_filename();
  }

 public:
  BangkeServerApp();
  ~BangkeServerApp();
  int Run();

 protected:
  bangke::Server m_server;
  bangke::UI m_ui;
  std::unique_ptr<BangkeRimeHandler> m_handler;
};
