#include "Configurator.h"

#include <QMessageBox>
#include <windows.h>
#include <cassert>
#include <filesystem>
#include <fstream>

#include "Levers.h"
#include <WeaselConstants.h>
#include <WeaselIPC.h>
#include <WeaselUtility.h>
#pragma warning(disable : 4005)
#include <rime_api.h>
#pragma warning(default : 4005)

static void CreateFileIfNotExist(std::string filename) {
  std::filesystem::path file_path = WeaselUserDataPath() / u8tow(filename);
  DWORD dwAttrib = GetFileAttributes(file_path.c_str());
  if (!(INVALID_FILE_ATTRIBUTES != dwAttrib &&
        0 == (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))) {
    std::wofstream o(file_path.c_str(), std::ios::app);
    o.close();
  }
}

Configurator::Configurator() {
  CreateFileIfNotExist("default.custom.yaml");
  CreateFileIfNotExist("weasel.custom.yaml");
}

Configurator::~Configurator() {
  EndDictSession();
}

void Configurator::Initialize() {
  RIME_STRUCT(RimeTraits, weasel_traits);
  std::string shared_dir = wtou8(WeaselSharedDataPath().wstring());
  std::string user_dir = wtou8(WeaselUserDataPath().wstring());
  weasel_traits.shared_data_dir = shared_dir.c_str();
  weasel_traits.user_data_dir = user_dir.c_str();
  weasel_traits.prebuilt_data_dir = weasel_traits.shared_data_dir;
  std::string distribution_name = wtou8(get_weasel_ime_name());
  weasel_traits.distribution_name = distribution_name.c_str();
  weasel_traits.distribution_code_name = WEASEL_CODE_NAME;
  weasel_traits.distribution_version = WEASEL_VERSION;
  weasel_traits.app_name = "rime.bangke";
  std::string log_dir = WeaselLogPath().u8string();
  weasel_traits.log_dir = log_dir.c_str();
  RimeApi* rime_api = rime_get_api();
  assert(rime_api);
  rime_api->setup(&weasel_traits);
  rime_api->deployer_initialize(NULL);
}

int Configurator::UpdateWorkspace(bool report_errors) {
  HANDLE hMutex = CreateMutexW(NULL, TRUE, L"BangkeDeployerMutex");
  if (!hMutex)
    return 1;
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(hMutex);
    if (report_errors)
      QMessageBox::information(nullptr, QStringLiteral(u"蚌壳拼音"),
                               QStringLiteral(u"正在执行另一项部署任务，方才所做的修改将在输入法再次启动后生效。"));
    return 1;
  }

  weasel::Client client;
  if (client.Connect())
    client.StartMaintenance();

  {
    RimeApi* rime = rime_get_api();
    rime->deploy();
    rime->deploy_config_file("weasel.yaml", "config_version");
  }

  CloseHandle(hMutex);

  if (client.Connect())
    client.EndMaintenance();
  return 0;
}

int Configurator::CleanupResidue() {
  // 静态：勿依赖实例状态（构造器会写用户目录，SYSTEM 下路径错误）
  // 被应用进程加载的 TSF dll 删不掉时转由下次重启删除；WeaselServer 自启键
  // 仅在目标文件已成幽灵时移除，避免误伤共存的官方小狼毫
  const wchar_t* residues[] = {L"C:\\Windows\\System32\\bangke.dll",
                               L"C:\\Windows\\System32\\bangke.dll.old"};
  for (const wchar_t* f : residues) {
    if (GetFileAttributesW(f) == INVALID_FILE_ATTRIBUTES)
      continue;
    if (!DeleteFileW(f))
      MoveFileExW(f, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
  }

  HKEY run = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                    KEY_QUERY_VALUE | KEY_SET_VALUE, &run) == ERROR_SUCCESS) {
    wchar_t val[1024] = {0};
    DWORD sz = sizeof(val);
    if (RegQueryValueExW(run, L"WeaselServer", nullptr, nullptr, (LPBYTE)val, &sz) ==
            ERROR_SUCCESS &&
        GetFileAttributesW(val) == INVALID_FILE_ATTRIBUTES)
      RegDeleteValueW(run, L"WeaselServer");
    RegCloseKey(run);
  }
  return 0;
}

int Configurator::SyncUserData() {
  HANDLE hMutex = CreateMutexW(NULL, TRUE, L"BangkeDeployerMutex");
  if (!hMutex)
    return 1;
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(hMutex);
    QMessageBox::information(nullptr, QStringLiteral(u"蚌壳拼音"), QStringLiteral(u"正在执行另一项部署任务，请稍候再试。"));
    return 1;
  }

  weasel::Client client;
  if (client.Connect())
    client.StartMaintenance();

  int ret = 0;
  {
    RimeApi* rime = rime_get_api();
    if (!rime->sync_user_data()) {
      ret = 1;
    }
    rime->join_maintenance_thread();
  }

  CloseHandle(hMutex);

  if (client.Connect())
    client.EndMaintenance();
  return ret;
}

bool Configurator::BeginDictSession() {
  if (m_hDictMutex)
    return true;
  HANDLE hMutex = CreateMutexW(NULL, TRUE, L"BangkeDeployerMutex");
  if (!hMutex)
    return false;
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(hMutex);
    QMessageBox::information(nullptr, QStringLiteral(u"蚌壳拼音"), QStringLiteral(u"正在执行另一项部署任务，请稍候再试。"));
    return false;
  }
  m_hDictMutex = hMutex;

  weasel::Client client;
  if (client.Connect())
    client.StartMaintenance();

  RimeApi* rime = rime_get_api();
  if (RIME_API_AVAILABLE(rime, run_task))
    rime->run_task("installation_update");  // 建立用户数据同步目录
  return true;
}

void Configurator::EndDictSession() {
  if (!m_hDictMutex)
    return;
  CloseHandle(m_hDictMutex);
  m_hDictMutex = nullptr;

  weasel::Client client;
  if (client.Connect())
    client.EndMaintenance();
}
