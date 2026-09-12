#pragma once
#include <BangkeIPC.h>
#include <BangkeUI.h>
#include <map>
#include <string>
#include <mutex>

#include <rime_api.h>

struct CaseInsensitiveCompare {
  bool operator()(const std::string& str1, const std::string& str2) const {
    std::string str1Lower, str2Lower;
    std::transform(str1.begin(), str1.end(), std::back_inserter(str1Lower),
                   [](char c) { return std::tolower(c); });
    std::transform(str2.begin(), str2.end(), std::back_inserter(str2Lower),
                   [](char c) { return std::tolower(c); });
    return str1Lower < str2Lower;
  }
};

typedef std::map<std::string, bool> AppOptions;
typedef std::map<std::string, AppOptions, CaseInsensitiveCompare>
    AppOptionsByAppName;

struct SessionStatus {
  SessionStatus()
      : style(bangke::UIStyle()),
        __synced(false),
        session_id(0) {
    RIME_STRUCT(RimeStatus, status);
  }
  bangke::UIStyle style;
  RimeStatus status;
  bool __synced;
  RimeSessionId session_id;
  std::string client_app;  // 部署后按它恢复 ipc 会话
};
typedef std::map<DWORD, SessionStatus> SessionStatusMap;
typedef DWORD BangkeSessionId;
class BangkeRimeHandler : public bangke::RequestHandler {
 public:
  // rime API 与会话表的串行锁：管道路径(ServerImpl)与延迟事件消费
  // (OnDeferredEvent)共用。必须递归：process_key→Compose→filter 会在同一线程
  // 内同步触发属性通知。OnNotify 本身绝不取此锁——它可能跑在 rime 内部线程上，
  // 一旦等锁即可与销毁路径的 worker join 互等成死锁
  static std::recursive_mutex& ApiMutex() {
    static std::recursive_mutex m;
    return m;
  }

  BangkeRimeHandler(bangke::UI* ui);
  virtual ~BangkeRimeHandler();
  virtual void Initialize();
  virtual void Finalize();
  virtual DWORD FindSession(BangkeSessionId ipc_id);
  virtual DWORD AddSession(LPWSTR buffer, EatLine eat = 0);
  virtual DWORD RemoveSession(BangkeSessionId ipc_id);
  virtual BOOL ProcessKeyEvent(bangke::KeyEvent keyEvent,
                               BangkeSessionId ipc_id,
                               EatLine eat);
  virtual void CommitComposition(BangkeSessionId ipc_id);
  virtual void ClearComposition(BangkeSessionId ipc_id);
  virtual void SelectCandidateOnCurrentPage(size_t index,
                                            BangkeSessionId ipc_id);
  virtual bool HighlightCandidateOnCurrentPage(size_t index,
                                               BangkeSessionId ipc_id,
                                               EatLine eat);
  virtual bool ChangePage(bool backward, BangkeSessionId ipc_id, EatLine eat);
  virtual void FocusIn(DWORD param, BangkeSessionId ipc_id);
  virtual void FocusOut(DWORD param, BangkeSessionId ipc_id);
  virtual void UpdateInputPosition(RECT const& rc, BangkeSessionId ipc_id);
  virtual void StartMaintenance();
  virtual void EndMaintenance();
  virtual void SetOption(BangkeSessionId ipc_id,
                         const std::string& opt,
                         bool val);
  virtual void UpdateColorTheme(BOOL darkMode);
  void SetEventWindow(HWND wnd) override;
  void OnDeferredEvent(int event, uintptr_t rime_session_id) override;

  void OnUpdateUI(std::function<void()> const& cb);

 private:
  void _Setup();
  bool _IsDeployerRunning();
  void _UpdateUI(BangkeSessionId ipc_id);
  void _LoadSchemaSpecificSettings(BangkeSessionId ipc_id,
                                   const std::string& schema_id);
  void _LoadAppInlinePreeditSet(BangkeSessionId ipc_id,
                                bool ignore_app_name = false);
  bool _ShowMessage(bangke::Context& ctx, bangke::Status& status);
  bool _Respond(BangkeSessionId ipc_id, EatLine eat, bool include_commit = true);
  bool _RespondFrame(BangkeSessionId ipc_id, EatLine eat, bool include_commit);
  void _ReadClientInfo(BangkeSessionId ipc_id, LPWSTR buffer);
  void _RestoreSession(BangkeSessionId ipc_id, const std::string& client_app);
  void _GetCandidateInfo(bangke::CandidateInfo& cinfo, RimeContext& ctx);
  void _GetStatus(bangke::Status& stat,
                  BangkeSessionId ipc_id,
                  bangke::Context& ctx);
  void _GetContext(bangke::Context& ctx, RimeSessionId session_id);
  void _UpdateShowNotifications(RimeConfig* config, bool initialize = false);

  void _UpdateInlinePreeditStatus(BangkeSessionId ipc_id);

  RimeSessionId to_session_id(BangkeSessionId ipc_id) {
    return m_session_status_map[ipc_id].session_id;
  }
  SessionStatus& get_session_status(BangkeSessionId ipc_id) {
    return m_session_status_map[ipc_id];
  }
  SessionStatus& new_session_status(BangkeSessionId ipc_id) {
    return m_session_status_map[ipc_id] = SessionStatus();
  }

  AppOptionsByAppName m_app_options;
  bangke::UI* m_ui;  // reference
  DWORD m_active_session;
  HWND m_event_wnd = nullptr;  // OnNotify 投递 WM_BK_RIME_EVENT 用
  bool m_disabled;
  std::string m_last_schema_id;
  std::string m_last_app_name;
  bangke::UIStyle m_base_style;
  std::map<std::string, bool> m_show_notifications;
  std::map<std::string, bool> m_show_notifications_base;
  std::function<void()> _UpdateUICallback;

  static void OnNotify(void* context_object,
                       uintptr_t session_id,
                       const char* message_type,
                       const char* message_value);
  void _PushAiSnapshot(uintptr_t rime_sid);
  static std::string m_message_type;
  static std::string m_message_value;
  static std::string m_message_label;
  static std::string m_option_name;
  static std::mutex m_notifier_mutex;
  SessionStatusMap m_session_status_map;
  // 部署窗口期的 ipc↔client_app 快照,EndMaintenance 据此重建会话
  std::map<DWORD, std::string> m_pending_restore;
  bool m_current_dark_mode;
  bool m_global_ascii_mode;
  int m_show_notifications_time;
  DWORD m_pid;
};
