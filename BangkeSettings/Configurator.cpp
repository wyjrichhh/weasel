#include "Configurator.h"

#include <QMessageBox>
#include <windows.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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

int Configurator::EnsureAiDefaults() {
  // 与插件契约对齐:model_path 相对路径按 user_data_dir 解析,
  // 因此模型必须落到用户目录;接线走 luna_pinyin.custom.yaml patch
  const std::filesystem::path user_dir = WeaselUserDataPath();

  // 宽字面量经 wtou8 落盘:窄字面量受源文件编码影响,不冒险
  static const wchar_t kAiWiringYaml[] =
      L"patch:\n"
      L"  # ai-predict: translator 必须列首位(MergedTranslation::Elect 顺序),\n"
      L"  # 因此整列重写朙月拼音原生 translator 并在最前加 ai_predict_translator\n"
      L"  engine/translators:\n"
      L"    - ai_predict_translator\n"
      L"    - punct_translator\n"
      L"    - table_translator@custom_phrase\n"
      L"    - reverse_lookup_translator\n"
      L"    - script_translator\n"
      L"  # filter 用 @next 追加,保留原生 simplifier/uniquifier 链\n"
      L"  'engine/filters/@next': ai_predict_filter\n"
      L"  # 默认简体(CT2 模型简体训练)\n"
      L"  switches/@2/reset: 1\n"
      L"  ai_predict:\n"
      L"    model_path: predict_models/zh-base-ct2-int8\n"
      L"    enabled: true\n"
      L"    device: cpu\n"
      L"    min_input_length: 12\n"
      L"    min_context_prompt_length: 2\n"
      L"    context_window_size: 10\n"
      L"    debounce_ms: 200\n"
      L"    max_tokens: 64\n"
      L"    quality: 0.00\n"
      L"    target_index: 1\n"
      L"    search_range: 10\n"
      L"    min_hanzi: 2\n";

  // 全新安装的方案集:明月 + 雾凇(对齐 Linux 版),仅缺文件时写入
  static const wchar_t kDefaultSchemasYaml[] =
      L"patch:\n"
      L"  schema_list:\n"
      L"    - schema: luna_pinyin\n"
      L"    - schema: rime_ice\n";
  {
    std::error_code dec;
    const std::filesystem::path dcustom = user_dir / L"default.custom.yaml";
    if (!std::filesystem::exists(dcustom, dec)) {
      std::ofstream o(dcustom, std::ios::binary);
      o << wtou8(kDefaultSchemasYaml);
    }
  }

  const std::filesystem::path custom = user_dir / L"luna_pinyin.custom.yaml";
  std::error_code ec;
  if (!std::filesystem::exists(custom, ec)) {
    std::ofstream o(custom, std::ios::binary);
    o << wtou8(kAiWiringYaml);
  }

  // 雾凇:translator 链整列重写(明月同款纪律),仅缺文件时写入
  static const wchar_t kIceWiringYaml[] =
      L"patch:\n"
      L"  # ai-predict: translator 必须列首位(MergedTranslation::Elect 顺序)\n"
      L"  engine/translators:\n"
      L"    - ai_predict_translator\n"
      L"    - punct_translator\n"
      L"    - script_translator\n"
      L"    - lua_translator@*date_translator\n"
      L"    - lua_translator@*lunar\n"
      L"    - lua_translator@*uuid\n"
      L"    - table_translator@custom_phrase\n"
      L"    - table_translator@melt_eng\n"
      L"    - table_translator@cn_en\n"
      L"    - table_translator@radical_lookup\n"
      L"    - lua_translator@*unicode\n"
      L"    - lua_translator@*number_translator\n"
      L"    - lua_translator@*calc_translator\n"
      L"    - lua_translator@*force_gc\n"
      L"  'engine/filters/@next': ai_predict_filter\n"
      L"  ai_predict:\n"
      L"    model_path: predict_models/zh-base-ct2-int8\n"
      L"    enabled: true\n"
      L"    device: cpu\n"
      L"    min_input_length: 12\n"
      L"    min_context_prompt_length: 2\n"
      L"    context_window_size: 10\n"
      L"    debounce_ms: 200\n"
      L"    max_tokens: 64\n"
      L"    quality: 0.00\n"
      L"    target_index: 1\n"
      L"    search_range: 10\n"
      L"    min_hanzi: 2\n";
  const std::filesystem::path iceCustom = user_dir / L"rime_ice.custom.yaml";
  if (!std::filesystem::exists(iceCustom, ec)) {
    std::ofstream o(iceCustom, std::ios::binary);
    o << wtou8(kIceWiringYaml);
  }

  // 模型随安装器铺在安装根目录 predict_models(与 data\ 平级);
  // 首部署拷入用户目录,按哨兵文件判缺,不重复拷
  const std::filesystem::path install_root = WeaselSharedDataPath().parent_path();
  const std::filesystem::path src = install_root / L"predict_models";
  const std::filesystem::path dst = user_dir / L"predict_models";
  if (std::filesystem::exists(src / L"zh-base-ct2-int8", ec) &&
      !std::filesystem::exists(dst / L"zh-base-ct2-int8", ec)) {
    std::filesystem::copy(src, dst, std::filesystem::copy_options::recursive,
                          ec);
  }
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
