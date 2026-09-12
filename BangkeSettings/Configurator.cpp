#include "Configurator.h"

#include <QMessageBox>
#include <windows.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Levers.h"
#include <BangkeConstants.h>
#include <BangkeIPC.h>
#include <BangkeUtility.h>
#pragma warning(disable : 4005)
#include <rime_api.h>
#pragma warning(default : 4005)

static void CreateFileIfNotExist(std::string filename) {
  std::filesystem::path file_path = BangkeUserDataPath() / u8tow(filename);
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
  RIME_STRUCT(RimeTraits, bangke_traits);
  std::string shared_dir = wtou8(BangkeSharedDataPath().wstring());
  std::string user_dir = wtou8(BangkeUserDataPath().wstring());
  bangke_traits.shared_data_dir = shared_dir.c_str();
  bangke_traits.user_data_dir = user_dir.c_str();
  bangke_traits.prebuilt_data_dir = bangke_traits.shared_data_dir;
  std::string distribution_name = wtou8(get_bangke_ime_name());
  bangke_traits.distribution_name = distribution_name.c_str();
  bangke_traits.distribution_code_name = BANGKE_CODE_NAME;
  bangke_traits.distribution_version = BANGKE_VERSION;
  bangke_traits.app_name = "rime.bangke";
  std::string log_dir = BangkeLogPath().u8string();
  bangke_traits.log_dir = log_dir.c_str();
  RimeApi* rime_api = rime_get_api();
  assert(rime_api);
  rime_api->setup(&bangke_traits);
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

  bangke::Client client;
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

int Configurator::EnsureAiDefaults() {
  return BangkeEnsureUserSeeds();
}

int Configurator::CleanupResidue() {
  // 静态：勿依赖实例状态（构造器会写用户目录，SYSTEM 下路径错误）
  // 被应用进程加载的 TSF dll 删不掉时转由下次重启删除；BangkeServer 自启键
  // 仅在目标文件已成幽灵时移除，避免误伤共存的官方小狼毫
  // TSF 机器级注册(DllRegisterServer 写入 HKLM CTF\TIP)卸载无人反注册,一并清
  {
    HKEY ctf = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\CTF\\TIP", 0,
                      DELETE, &ctf) == ERROR_SUCCESS) {
      RegDeleteTreeW(
          ctf, L"{9D44BD49-B647-4010-9ADC-16DA253F5CCA}");
      RegCloseKey(ctf);
    }
  }
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

int Configurator::ClearPendingDeletes() {
  // 静态：勿依赖实例状态（构造器会写用户目录，SYSTEM 下路径错误）
  // PFRO 条目可能带 "*N"/"!" 等类型前缀（如 "*1\??\C:\..."），按 "\??\" 起始的
  // 路径主体比对
  static const wchar_t* kProductFiles[] = {
      L"\\??\\C:\\Windows\\System32\\bangke.dll",
      L"\\??\\C:\\Windows\\System32\\bangke.dll.old",
  };
  auto isOurPath = [](const std::wstring& e) {
    const size_t at = e.find(L"\\??\\");
    if (at == std::wstring::npos)
      return false;
    const std::wstring body = e.substr(at);
    for (const wchar_t* f : kProductFiles)
      if (_wcsicmp(body.c_str(), f) == 0)
        return true;
    return false;
  };
  const wchar_t* kSessionMgr =
      L"SYSTEM\\CurrentControlSet\\Control\\Session Manager";
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kSessionMgr, 0, KEY_QUERY_VALUE | KEY_SET_VALUE,
                    &key) != ERROR_SUCCESS)
    return 0;
  wchar_t buf[32768] = {0};
  DWORD sz = sizeof(buf);
  if (RegQueryValueExW(key, L"PendingFileRenameOperations", nullptr, nullptr,
                       (LPBYTE)buf, &sz) != ERROR_SUCCESS) {
    RegCloseKey(key);
    return 0;
  }
  // REG_MULTI_SZ 的单个元素可以是空串（删除对的目标），必须按总长遍历，
  // while(*p) 会在第一个空串处提前终止
  std::vector<std::wstring> entries;
  {
    const size_t total = sz / sizeof(wchar_t);
    size_t pos = 0;
    while (pos < total) {
      const std::wstring e(buf + pos);
      entries.push_back(e);
      pos += e.size() + 1;
    }
    if (!entries.empty() && entries.back().empty())
      entries.pop_back();  // 末尾的终止空串不是元素
  }
  bool changed = false;
  std::vector<std::wstring> kept;
  // PFRO 按 (源, 目标) 成对排列，目标为空串表示删除；整对取舍，绝不能拆散别人的对
  for (size_t i = 0; i < entries.size(); i += 2) {
    if (isOurPath(entries[i])) {
      changed = true;
      continue;
    }
    kept.push_back(entries[i]);
    if (i + 1 < entries.size())
      kept.push_back(entries[i + 1]);
  }
  if (changed) {
    std::wstring flat;
    for (const auto& e : kept) {
      flat += e;
      flat += L'\0';  // wstring+L"\0" 会因 wcslen=0 丢分隔符，必须按字符追加
    }
    flat += L'\0';
    RegSetValueExW(key, L"PendingFileRenameOperations", 0, REG_MULTI_SZ,
                   (const BYTE*)flat.c_str(), (DWORD)(flat.size() * sizeof(wchar_t)));
  }
  RegCloseKey(key);
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

  bangke::Client client;
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

  bangke::Client client;
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

  bangke::Client client;
  if (client.Connect())
    client.EndMaintenance();
}
