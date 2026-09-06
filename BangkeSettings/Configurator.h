#pragma once

class Configurator {
 public:
  Configurator();
  ~Configurator();

  void Initialize();
  int UpdateWorkspace(bool report_errors = false);
  int SyncUserData();

  // 词典管理以 maintenance 会话包住（与旧 Deployer 的 DictManagement 等价），
  // 窗口切走时必须调 EndDictSession 恢复服务
  bool BeginDictSession();
  void EndDictSession();

 private:
  void* m_hDictMutex = nullptr;
};
