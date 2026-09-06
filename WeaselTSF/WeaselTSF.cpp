#include "stdafx.h"

#include <WeaselIPCData.h>
#include <thread>
#include <shellapi.h>
#include <tlhelp32.h>
#include "WeaselTSF.h"
#include "CandidateList.h"
#include "LanguageBar.h"
#include "Compartment.h"
#include "ResponseParser.h"
#include "EditSession.h"
#include <sstream>

static void error_message(const WCHAR* msg) {
  static DWORD next_tick = 0;
  DWORD now = GetTickCount();
  if (now > next_tick) {
    next_tick = now + 10000;  // (ms)
    MessageBox(NULL, msg, get_weasel_ime_name().c_str(), MB_ICONERROR | MB_OK);
  }
}

WeaselTSF::WeaselTSF() {
  _cRef = 1;

  _dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;

  _dwTextEditSinkCookie = TF_INVALID_COOKIE;
  _dwTextLayoutSinkCookie = TF_INVALID_COOKIE;
  _dwThreadFocusSinkCookie = TF_INVALID_COOKIE;
  _fTestKeyDownPending = FALSE;
  _fTestKeyUpPending = FALSE;

  _fCUASWorkaroundTested = _fCUASWorkaroundEnabled = FALSE;

  _cand = new CCandidateList(this);

  DllAddRef();
}

WeaselTSF::~WeaselTSF() {
  DllRelease();
}

STDMETHODIMP WeaselTSF::QueryInterface(REFIID riid, void** ppvObject) {
  if (ppvObject == NULL)
    return E_INVALIDARG;

  *ppvObject = NULL;

  if (IsEqualIID(riid, IID_IUnknown) ||
      IsEqualIID(riid, IID_ITfTextInputProcessor))
    *ppvObject = (ITfTextInputProcessor*)this;
  else if (IsEqualIID(riid, IID_ITfTextInputProcessorEx))
    *ppvObject = (ITfTextInputProcessorEx*)this;
  else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink))
    *ppvObject = (ITfThreadMgrEventSink*)this;
  else if (IsEqualIID(riid, IID_ITfTextEditSink))
    *ppvObject = (ITfTextEditSink*)this;
  else if (IsEqualIID(riid, IID_ITfTextLayoutSink))
    *ppvObject = (ITfTextLayoutSink*)this;
  else if (IsEqualIID(riid, IID_ITfKeyEventSink))
    *ppvObject = (ITfKeyEventSink*)this;
  else if (IsEqualIID(riid, IID_ITfCompositionSink))
    *ppvObject = (ITfCompositionSink*)this;
  else if (IsEqualIID(riid, IID_ITfEditSession))
    *ppvObject = (ITfEditSession*)this;
  else if (IsEqualIID(riid, IID_ITfThreadFocusSink))
    *ppvObject = (ITfThreadFocusSink*)this;
  else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider))
    *ppvObject = (ITfDisplayAttributeProvider*)this;

  if (*ppvObject) {
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) WeaselTSF::AddRef() {
  return ++_cRef;
}

STDMETHODIMP_(ULONG) WeaselTSF::Release() {
  LONG cr = --_cRef;

  assert(_cRef >= 0);

  if (_cRef == 0)
    delete this;

  return cr;
}

STDMETHODIMP WeaselTSF::Activate(ITfThreadMgr* pThreadMgr,
                                 TfClientId tfClientId) {
  return ActivateEx(pThreadMgr, tfClientId, 0U);
}

STDMETHODIMP WeaselTSF::Deactivate() {
  m_client.EndSession();

  _InitTextEditSink(com_ptr<ITfDocumentMgr>());

  _UninitThreadMgrEventSink();

  _UninitKeyEventSink();
  _UninitPreservedKey();

  _UninitLanguageBar();

  _UninitCompartment();

  _UninitThreadMgrEventSink();

  _pThreadMgr = NULL;

  _tfClientId = TF_CLIENTID_NULL;

  _cand->DestroyAll();

  return S_OK;
}

STDMETHODIMP WeaselTSF::ActivateEx(ITfThreadMgr* pThreadMgr,
                                   TfClientId tfClientId,
                                   DWORD dwFlags) {
  com_ptr<ITfDocumentMgr> pDocMgrFocus;
  _activateFlags = dwFlags;

  _pThreadMgr = pThreadMgr;
  _tfClientId = tfClientId;

  if (!_InitThreadMgrEventSink())
    goto ExitError;

  if ((_pThreadMgr->GetFocus(&pDocMgrFocus) == S_OK) &&
      (pDocMgrFocus != NULL)) {
    _InitTextEditSink(pDocMgrFocus);
  }

  if (!_InitKeyEventSink())
    goto ExitError;

  // if (!_InitDisplayAttributeGuidAtom())
  //	goto ExitError;
  //	some app might init failed because it not provide DisplayAttributeInfo,
  // like some opengl stuff
  _InitDisplayAttributeGuidAtom();

  if (!_InitPreservedKey())
    goto ExitError;

  if (!_InitLanguageBar())
    goto ExitError;

  if (!_IsKeyboardOpen())
    _SetKeyboardOpen(TRUE);

  if (!_InitCompartment())
    goto ExitError;
  if (!_InitThreadFocusSink())
    goto ExitError;

  _EnsureServerConnected();

  return S_OK;

ExitError:
  Deactivate();
  return E_FAIL;
}

STDMETHODIMP WeaselTSF::OnSetThreadFocus() {
  std::wstring _ToggleImeOnOpenClose{};
  RegGetStringValue(HKEY_CURRENT_USER, L"Software\\Bangke",
                    L"ToggleImeOnOpenClose", _ToggleImeOnOpenClose);
  _isToOpenClose = (_ToggleImeOnOpenClose == L"yes");
  if (m_client.Echo()) {
    m_client.ProcessKeyEvent(0);
    weasel::ResponseParser parser(NULL, NULL, &_status, NULL, &_cand->style());
    bool ok = m_client.GetResponseData(std::ref(parser));
    if (ok)
      _UpdateLanguageBar(_status);
  }
  return S_OK;
}
STDMETHODIMP WeaselTSF::OnKillThreadFocus() {
  _AbortComposition();
  return S_OK;
}
BOOL WeaselTSF::_InitThreadFocusSink() {
  com_ptr<ITfSource> pSource;
  if (FAILED(_pThreadMgr->QueryInterface(&pSource)))
    return FALSE;
  if (FAILED(pSource->AdviseSink(IID_ITfThreadFocusSink,
                                 (ITfThreadFocusSink*)this,
                                 &_dwThreadFocusSinkCookie)))
    return FALSE;
  return TRUE;
}
void WeaselTSF::_UninitThreadFocusSink() {
  com_ptr<ITfSource> pSource;
  if (FAILED(_pThreadMgr->QueryInterface(&pSource)))
    return;
  if (FAILED(pSource->UnadviseSink(_dwThreadFocusSinkCookie)))
    return;
}

STDMETHODIMP WeaselTSF::OnActivated(REFCLSID clsid,
                                    REFGUID guidProfile,
                                    BOOL isActivated) {
  if (!IsEqualCLSID(clsid, c_clsidTextService)) {
    return S_OK;
  }

  if (isActivated) {
    _ShowLanguageBar(TRUE);
    _UpdateLanguageBar(_status);
  } else {
    _DeleteCandidateList();
    _ShowLanguageBar(FALSE);
  }
  return S_OK;
}

// 异步刷新链路诊断日志（验证后移除）
static void BkTrace(const char* tag, const char* detail) {
  WCHAR path[MAX_PATH] = {0};
  GetEnvironmentVariableW(L"TEMP", path, MAX_PATH);
  wcscat_s(path, L"\\bk_refresh.log");
  FILE* f = _wfsopen(path, L"a", _SH_DENYNO);
  if (f) {
    fprintf(f, "[%u] %s %s\n", GetCurrentProcessId(), tag, detail ? detail : "");
    fclose(f);
  }
}

// AI 快照推送头（与服务端 AiPushHeader 对应）
#pragma pack(push, 1)
struct AiPushHeader {
  DWORD magic;
  DWORD seq;
  DWORD bytes;
};
#pragma pack(pop)
static const DWORD kAiPushMagic = 0x314B4220;

void WeaselTSF::_AsyncRefresh(UINT_PTR seq) {
  BkTrace("tsf", "async refresh");
  HANDLE map = OpenFileMappingW(FILE_MAP_READ, FALSE, L"Local\\BangkeAIPush");
  if (!map)
    return;
  auto* view = (BYTE*)MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
  if (!view) {
    CloseHandle(map);
    return;
  }
  auto* h = (AiPushHeader*)view;
  bool ok = h->magic == kAiPushMagic && h->seq == (DWORD)seq &&
            h->bytes > sizeof(wchar_t);
  std::wstring text;
  if (ok)
    text.assign((wchar_t*)(view + sizeof(AiPushHeader)),
                h->bytes / sizeof(wchar_t) - 1);
  UnmapViewOfFile(view);
  CloseHandle(map);
  if (!ok) {
    BkTrace("tsf", "snapshot stale/invalid");
    return;
  }

  // 用既有响应文本协议在本地解析快照，不回服务端拉取（无时机竞态）
  std::wstring commit;
  weasel::Config config;
  auto context = std::make_shared<weasel::Context>();
  weasel::Status status;
  weasel::ResponseParser parser(&commit, context.get(), &status, &config,
                                &_cand->style());
  std::wistringstream iss(text);
  std::wstring line;
  while (std::getline(iss, line)) {
    if (!line.empty() && line.back() == L'\r')
      line.pop_back();
    if (!line.empty() && line != L".")
      parser.Feed(line);
  }

  // 快照尚无候选（组合重建中间态）则不动当前显示
  if (context->cinfo.candies.empty() && context->aux.empty()) {
    BkTrace("tsf", "snapshot empty, skip");
    return;
  }
  {
    char t[96];
    sprintf_s(t, "snapshot ok, %d candies", (int)context->cinfo.candies.size());
    BkTrace("tsf", t);
  }
  _UpdateUI(*context, status);
}

void WeaselTSF::_Reconnect() {
  m_client.Disconnect();
  m_client.Connect(NULL);
  m_client.StartSession();
  weasel::ResponseParser parser(NULL, NULL, &_status, NULL, &_cand->style());
  bool ok = m_client.GetResponseData(std::ref(parser));
  if (ok) {
    _UpdateLanguageBar(_status);
  }
}

static unsigned int retry = 0;

bool WeaselTSF::_EnsureServerConnected() {
  if (!m_client.Echo()) {
    _Reconnect();
    retry++;
    if (retry >= 6) {
      HANDLE hMutex = CreateMutex(NULL, TRUE, L"BangkeDeployerExclusiveMutex");
      const auto count_server_process = []() -> int {
        int count = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE)
          return 0;
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(snap, &pe)) {
          do {
            if (_wcsicmp(pe.szExeFile, L"BangkeServer.exe") == 0)
              count++;
          } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
        return count;
      };
      if (!m_client.Echo() && GetLastError() != ERROR_ALREADY_EXISTS &&
          !count_server_process()) {
        std::wstring dir = _GetRootDir();
        std::thread th([dir, this]() {
          ShellExecuteW(NULL, L"open", (dir + L"\\start_service.bat").c_str(),
                        NULL, dir.c_str(), SW_HIDE);
          // wait 500ms, then reconnect
          std::this_thread::sleep_for(std::chrono::milliseconds(500));
          _Reconnect();
        });
        th.detach();
      }
      if (hMutex) {
        CloseHandle(hMutex);
      }
      retry = 0;
    }
    return (m_client.Echo() != 0);
  } else {
    return true;
  }
}
