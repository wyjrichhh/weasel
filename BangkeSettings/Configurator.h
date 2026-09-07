#pragma once

class Configurator {
 public:
  Configurator();
  ~Configurator();

  void Initialize();
  int UpdateWorkspace(bool report_errors = false);
  int SyncUserData();
  // 卸载残留清理：以 MSI SYSTEM 令牌运行，不触碰用户数据目录，故为静态
  static int CleanupResidue();
  // 安装前清掉指向本产品文件的挂起重启删除：否则卸载后未重启就重装，
  // 下次重启会删掉新装好的文件
  static int ClearPendingDeletes();

  // 词典管理以 maintenance 会话包住（与旧 Deployer 的 DictManagement 等价），
  // 窗口切走时必须调 EndDictSession 恢复服务
  bool BeginDictSession();
  void EndDictSession();

 private:
  void* m_hDictMutex = nullptr;
};
