#include "stdafx.h"
#include <resource.h>
#include <thread>
#include <shellapi.h>
#include "WeaselTSF.h"
#include "LanguageBar.h"
#include "CandidateList.h"
#include <WeaselUtility.h>

static const DWORD LANGBARITEMSINK_COOKIE = 0x42424242;

static void HMENU2ITfMenu(HMENU hMenu, ITfMenu* pTfMenu) {
  /* NOTE: Only limited functions are supported */
  int N = GetMenuItemCount(hMenu);
  for (int i = 0; i < N; i++) {
    MENUITEMINFO mii;
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
    mii.dwTypeData = NULL;
    if (GetMenuItemInfo(hMenu, i, TRUE, &mii)) {
      UINT id = mii.wID;
      if (mii.fType == MFT_SEPARATOR)
        pTfMenu->AddMenuItem(id, TF_LBMENUF_SEPARATOR, NULL, NULL, NULL, 0,
                             NULL);
      else if (mii.fType == MFT_STRING) {
        mii.dwTypeData = (LPWSTR)malloc(sizeof(WCHAR) * (mii.cch + 1));
        mii.cch++;
        if (GetMenuItemInfo(hMenu, i, TRUE, &mii))
          pTfMenu->AddMenuItem(id, 0, NULL, NULL, mii.dwTypeData, mii.cch,
                               NULL);
        free(mii.dwTypeData);
      }
    }
  }
}

static LPCWSTR GetWeaselRegName() {
  // x64-only:64 位视图即本进程默认视图,无需 WOW6432Node 分支
  return L"Software\\Bangke";
}

namespace {
// 菜单动作的可见反馈:主屏工作区底部居中的小 toast,不抢焦点,1.6s 自毁。
// 独立于候选窗生命周期(菜单点击时候选窗可能尚未创建),纯 Win32 零依赖。
void ShowToast(const std::wstring& text) {
  struct Toast {
    static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
      switch (m) {
        case WM_PAINT: {
          PAINTSTRUCT ps;
          HDC dc = BeginPaint(h, &ps);
          RECT rc;
          GetClientRect(h, &rc);
          SetBkColor(dc, RGB(19, 27, 38));
          SetTextColor(dc, RGB(232, 236, 241));
          SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
          auto* t = (const std::wstring*)GetWindowLongPtrW(h, GWLP_USERDATA);
          if (t)
            DrawTextW(dc, t->c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
          EndPaint(h, &ps);
          return 0;
        }
        case WM_TIMER:
          DestroyWindow(h);
          return 0;
        case WM_DESTROY:
          delete (std::wstring*)GetWindowLongPtrW(h, GWLP_USERDATA);
          PostQuitMessage(0);
          return 0;
      }
      return DefWindowProcW(h, m, w, l);
    }
  };
  std::thread th([text]() {
    static ATOM cls = 0;
    if (!cls) {
      WNDCLASSW wc = {0};
      wc.lpfnWndProc = Toast::WndProc;
      wc.hInstance = GetModuleHandleW(NULL);
      wc.hbrBackground = CreateSolidBrush(RGB(19, 27, 38));
      wc.lpszClassName = L"BangkeToast";
      cls = RegisterClassW(&wc);
    }
    if (!cls)
      return;
    const int w = std::max(160, (int)(text.size() * 14 + 48));
    const int h = 42;
    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    const int x = wa.left + ((wa.right - wa.left) - w) / 2;
    const int y = wa.bottom - h - 48;
    auto* t = new std::wstring(text);
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"BangkeToast",
        L"", WS_POPUP, x, y, w, h, NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!hwnd) {
      delete t;
      return;
    }
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)t);
    SetTimer(hwnd, 1, 1600, NULL);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  });
  th.detach();
}
}  // namespace

static bool open(const std::wstring& path) {
  return (uintptr_t)ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL,
                                  SW_SHOWNORMAL) > 32;
}

CLangBarItemButton::CLangBarItemButton(com_ptr<WeaselTSF> pTextService,
                                       REFGUID guid,
                                       weasel::UIStyle& style)
    : _status(0),
      _style(style),
      _current_schema_zhung_icon(),
      _current_schema_ascii_icon() {
  DllAddRef();

  _pLangBarItemSink = NULL;
  _cRef = 1;
  _pTextService = pTextService;
  _guid = guid;
  ascii_mode = false;
}

CLangBarItemButton::~CLangBarItemButton() {
  DllRelease();
}

STDMETHODIMP CLangBarItemButton::QueryInterface(REFIID riid, void** ppvObject) {
  if (ppvObject == NULL)
    return E_INVALIDARG;

  *ppvObject = NULL;
  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfLangBarItem) ||
      IsEqualIID(riid, IID_ITfLangBarItemButton))
    *ppvObject = (ITfLangBarItemButton*)this;
  else if (IsEqualIID(riid, IID_ITfSource))
    *ppvObject = (ITfSource*)this;

  if (*ppvObject) {
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CLangBarItemButton::AddRef() {
  return ++_cRef;
}

STDMETHODIMP_(ULONG) CLangBarItemButton::Release() {
  LONG cr = --_cRef;
  assert(_cRef >= 0);
  if (_cRef == 0)
    delete this;
  return cr;
}

STDMETHODIMP CLangBarItemButton::GetInfo(TF_LANGBARITEMINFO* pInfo) {
  pInfo->clsidService = c_clsidTextService;
  pInfo->guidItem = _guid;
  // TF_LBI_STYLE_SHOWNINTRAY 会在系统输入指示器之外再占一个托盘位，
  // 与 Win11 输入指示器重复显示两个图标，故不启用
  pInfo->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_BTN_MENU;
  pInfo->ulSort = 1;
  lstrcpyW(pInfo->szDescription, get_weasel_ime_name().c_str());
  return S_OK;
}

STDMETHODIMP CLangBarItemButton::GetStatus(DWORD* pdwStatus) {
  *pdwStatus = _status;
  return S_OK;
}

STDMETHODIMP CLangBarItemButton::Show(BOOL fShow) {
  SetLangbarStatus(TF_LBI_STATUS_HIDDEN, fShow ? FALSE : TRUE);
  return S_OK;
}

static LANGID GetActiveProfileLangId() {
  CComPtr<ITfInputProcessorProfileMgr> pInputProcessorProfileMgr;
  HRESULT hr = pInputProcessorProfileMgr.CoCreateInstance(
      CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_ALL);
  if (FAILED(hr))
    return MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);

  TF_INPUTPROCESSORPROFILE profile;
  hr = pInputProcessorProfileMgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,
                                                   &profile);
  if (FAILED(hr))
    return MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
  return profile.langid;
}

STDMETHODIMP CLangBarItemButton::GetTooltipString(BSTR* pbstrToolTip) {
  LANGID langid = get_language_id();
  if (langid == TEXTSERVICE_LANGID_HANS) {
    *pbstrToolTip = SysAllocString(L"左键切换模式，右键打开菜单");
  } else if (langid == TEXTSERVICE_LANGID_HANT) {
    *pbstrToolTip = SysAllocString(L"左鍵切換模式，右鍵打開菜單");
  } else {
    *pbstrToolTip = SysAllocString(
        L"Left-click to switch modes\n\nRight-click for more options");
  }

  return (*pbstrToolTip == NULL) ? E_OUTOFMEMORY : S_OK;
}

STDMETHODIMP CLangBarItemButton::OnClick(TfLBIClick click,
                                         POINT pt,
                                         const RECT* prcArea) {
  if (click == TF_LBI_CLK_LEFT) {
    _pTextService->_SetAsciiMode(!ascii_mode);
    ascii_mode = !ascii_mode;
    if (_pLangBarItemSink) {
      _pLangBarItemSink->OnUpdate(TF_LBI_STATUS | TF_LBI_ICON);
    }
  } else if (click == TF_LBI_CLK_RIGHT) {
    /* Open menu */
    HWND hwnd = _pTextService->_GetFocusedContextWindow();
    if (hwnd != NULL) {
      LANGID langid = get_language_id();
      HMENU menu;
      if (langid == TEXTSERVICE_LANGID_HANS) {
        menu = LoadMenuW(g_hInst, MAKEINTRESOURCE(IDR_MENU_POPUP_HANS));
      } else if (langid == TEXTSERVICE_LANGID_HANT) {
        menu = LoadMenuW(g_hInst, MAKEINTRESOURCE(IDR_MENU_POPUP_HANT));
      } else {
        menu = LoadMenuW(g_hInst, MAKEINTRESOURCE(IDR_MENU_POPUP));
      }
      HMENU popupMenu = GetSubMenu(menu, 0);
      UINT wID = TrackPopupMenuEx(
          popupMenu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_HORPOSANIMATION, pt.x,
          pt.y, hwnd, NULL);
      DestroyMenu(menu);
      _pTextService->_ExecuteMenuCommand(wID);
    }
  }
  return S_OK;
}

STDMETHODIMP CLangBarItemButton::InitMenu(ITfMenu* pMenu) {
  HMENU menu = LoadMenuW(g_hInst, MAKEINTRESOURCE(IDR_MENU_POPUP));
  HMENU popupMenu = GetSubMenu(menu, 0);
  HMENU2ITfMenu(popupMenu, pMenu);
  DestroyMenu(menu);
  return S_OK;
}

STDMETHODIMP CLangBarItemButton::OnMenuSelect(UINT wID) {
  _pTextService->_ExecuteMenuCommand(wID);
  return S_OK;
}

STDMETHODIMP CLangBarItemButton::GetIcon(HICON* phIcon) {
  if (ascii_mode) {
    if (_style.current_ascii_icon.empty())
      *phIcon = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_EN), IMAGE_ICON,
                                  GetSystemMetrics(SM_CXSMICON),
                                  GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    else
      *phIcon =
          (HICON)LoadImageW(NULL, _style.current_ascii_icon.c_str(), IMAGE_ICON,
                            GetSystemMetrics(SM_CXSMICON),
                            GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);
  } else {
    if (_style.current_zhung_icon.empty())
      *phIcon = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_ZH), IMAGE_ICON,
                                  GetSystemMetrics(SM_CXSMICON),
                                  GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    else
      *phIcon =
          (HICON)LoadImageW(NULL, _style.current_zhung_icon.c_str(), IMAGE_ICON,
                            GetSystemMetrics(SM_CXSMICON),
                            GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);
  }
  return (*phIcon == NULL) ? E_FAIL : S_OK;
}

STDMETHODIMP CLangBarItemButton::GetText(BSTR* pbstrText) {
  *pbstrText = SysAllocString(get_weasel_ime_name().c_str());
  return (*pbstrText == NULL) ? E_OUTOFMEMORY : S_OK;
}

STDMETHODIMP CLangBarItemButton::AdviseSink(REFIID riid,
                                            IUnknown* punk,
                                            DWORD* pdwCookie) {
  if (!IsEqualIID(riid, IID_ITfLangBarItemSink))
    return CONNECT_E_CANNOTCONNECT;
  if (_pLangBarItemSink != NULL)
    return CONNECT_E_ADVISELIMIT;

  if (punk->QueryInterface(IID_ITfLangBarItemSink,
                           (LPVOID*)&_pLangBarItemSink) != S_OK) {
    _pLangBarItemSink = NULL;
    return E_NOINTERFACE;
  }
  *pdwCookie = LANGBARITEMSINK_COOKIE;
  return S_OK;
}

STDMETHODIMP CLangBarItemButton::UnadviseSink(DWORD dwCookie) {
  if (dwCookie != LANGBARITEMSINK_COOKIE || _pLangBarItemSink == NULL)
    return CONNECT_E_NOCONNECTION;
  _pLangBarItemSink = NULL;
  return S_OK;
}

void CLangBarItemButton::UpdateWeaselStatus(weasel::Status stat) {
  if (stat.ascii_mode != ascii_mode) {
    ascii_mode = stat.ascii_mode;
  }
  if (_current_schema_zhung_icon != _style.current_zhung_icon) {
    _current_schema_zhung_icon = _style.current_zhung_icon;
  }
  if (_current_schema_ascii_icon != _style.current_ascii_icon) {
    _current_schema_ascii_icon = _style.current_ascii_icon;
  }
  if (_pLangBarItemSink) {
    _pLangBarItemSink->OnUpdate(TF_LBI_STATUS | TF_LBI_ICON);
  }
}

void CLangBarItemButton::SetLangbarStatus(DWORD dwStatus, BOOL fSet) {
  BOOL isChange = FALSE;

  if (fSet) {
    if (!(_status & dwStatus)) {
      _status |= dwStatus;
      isChange = TRUE;
    }
  } else {
    if (_status & dwStatus) {
      _status &= ~dwStatus;
      isChange = TRUE;
    }
  }

  if (isChange && _pLangBarItemSink) {
    _pLangBarItemSink->OnUpdate(TF_LBI_STATUS | TF_LBI_ICON);
  }

  return;
}

std::wstring WeaselTSF::_GetRootDir() {
  std::wstring dir{};
  RegGetStringValue(HKEY_LOCAL_MACHINE, GetWeaselRegName(), L"BangkeRoot", dir);
  return dir;
}

// 统一进程拉起：CreateProcessW 而非 ShellExecuteW——后者在 TSF dll 的裸线程
// （无 OLE 初始化）上会静默失败；工作目录必须显式给，继承的 cwd 不可靠
void WeaselTSF::_LaunchDetached(const std::wstring& exe,
                                const std::wstring& args) {
  std::wstring dir = _GetRootDir();
  if (dir.empty())
    return;
  std::thread th([dir, exe, args]() {
    std::wstring cmd = L"\"" + dir + L"\\" + exe + L"\"";
    if (!args.empty())
      cmd += L" " + args;
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    if (CreateProcessW(NULL, cmd.data(), NULL, NULL, FALSE, DETACHED_PROCESS,
                       NULL, dir.c_str(), &si, &pi)) {
      CloseHandle(pi.hProcess);
      CloseHandle(pi.hThread);
    }
  });
  th.detach();
}

void WeaselTSF::_LaunchSettings(const std::wstring& args) {
  _LaunchDetached(L"BangkeSettings.exe", args);
}

// 菜单唯一派发点：全部命令在 dll 内就地处理，server 不再承载任何菜单逻辑
void WeaselTSF::_ExecuteMenuCommand(UINT wID) {
  std::wstring dir{};
  switch (wID) {
    case ID_WEASELTRAY_SETTINGS:
      _LaunchSettings(L"");
      break;
    case ID_WEASELTRAY_DICT_MANAGEMENT:
      _LaunchSettings(L"/dict");
      break;
    case ID_WEASELTRAY_DEPLOY:
      _LaunchSettings(L"/deploy");
      break;
    case ID_WEASELTRAY_SYNC:
      _LaunchSettings(L"/sync");
      break;
    case ID_WEASELTRAY_USERCONFIG:
      if (FAILED(RegGetStringValue(HKEY_CURRENT_USER, L"Software\\Bangke",
                                   L"BangkeUserDir", dir)) ||
          dir.empty()) {
        WCHAR _path[MAX_PATH] = {0};
        ExpandEnvironmentStringsW(L"%AppData%\\Bangke", _path, _countof(_path));
        dir = std::wstring(_path);
      }
      if (!dir.empty() && fs::exists(dir))
        open(dir);
      else
        MessageBoxW(NULL, (L"Not found: " + dir).c_str(), L"BangkeUserDir",
                    MB_ICONERROR | MB_OK);
      break;
    case ID_WEASELTRAY_LOGDIR:
      open(WeaselLogPath().wstring());
      break;
    case ID_WEASELTRAY_RERUN_SERVICE: {
      // 先停后启:旧实例持单实例互斥量,直接拉新实例会立即退出等于没重启
      m_client.ShutdownServer();
      std::wstring dir = _GetRootDir();
      if (!dir.empty()) {
        std::thread th([dir]() {
          Sleep(400);  // 等 WM_QUIT 落地、互斥量释放
          std::wstring cmd = L"\"" + dir + L"\\BangkeServer.exe\"";
          STARTUPINFOW si = {sizeof(si)};
          PROCESS_INFORMATION pi = {};
          if (CreateProcessW(NULL, cmd.data(), NULL, NULL, FALSE,
                             DETACHED_PROCESS, NULL, dir.c_str(), &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
          }
        });
        th.detach();
      }
      ShowToast(L"输入法服务已重启");
      break;
    }
    case ID_WEASELTRAY_QUIT:
      m_client.ShutdownServer();
      ShowToast(L"输入法服务已退出，下次按键时自动恢复");
      break;
  }
}

HWND WeaselTSF::_GetFocusedContextWindow() {
  HWND hwnd = NULL;
  ITfDocumentMgr* pDocMgr;
  if (_pThreadMgr->GetFocus(&pDocMgr) == S_OK && pDocMgr != NULL) {
    ITfContext* pContext;
    if (pDocMgr->GetTop(&pContext) == S_OK && pContext != NULL) {
      ITfContextView* pContextView;
      if (pContext->GetActiveView(&pContextView) == S_OK &&
          pContextView != NULL) {
        pContextView->GetWnd(&hwnd);
        pContextView->Release();
      }
      pContext->Release();
    }
    pDocMgr->Release();
  }

  if (hwnd == NULL) {
    HWND hwndForeground = GetForegroundWindow();
    if (GetWindowThreadProcessId(hwndForeground, NULL) == GetCurrentThreadId())
      hwnd = hwndForeground;
  }

  return hwnd;
}

BOOL WeaselTSF::_InitLanguageBar() {
  com_ptr<ITfLangBarItemMgr> pLangBarItemMgr;
  BOOL fRet = FALSE;

  if (_pThreadMgr->QueryInterface(&pLangBarItemMgr) != S_OK)
    return FALSE;

  if ((_pLangBarButton = new CLangBarItemButton(this, GUID_LBI_INPUTMODE,
                                                _cand->style())) == NULL)
    return FALSE;

  if (pLangBarItemMgr->AddItem(_pLangBarButton) != S_OK) {
    _pLangBarButton = NULL;
    return FALSE;
  }

  _pLangBarButton->Show(TRUE);
  fRet = TRUE;

  return fRet;
}

void WeaselTSF::_UninitLanguageBar() {
  com_ptr<ITfLangBarItemMgr> pLangBarItemMgr;

  if (_pLangBarButton == NULL)
    return;

  if (_pThreadMgr->QueryInterface(&pLangBarItemMgr) == S_OK) {
    pLangBarItemMgr->RemoveItem(_pLangBarButton);
  }

  _pLangBarButton = NULL;
}

void WeaselTSF::_UpdateLanguageBar(weasel::Status stat) {
  if (!_pLangBarButton)
    return;
  DWORD flags;
  _GetCompartmentDWORD(flags, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION);
  if (stat.ascii_mode)
    flags &= (~TF_CONVERSIONMODE_NATIVE);
  else
    flags |= TF_CONVERSIONMODE_NATIVE;
  if (stat.full_shape)
    flags |= TF_CONVERSIONMODE_FULLSHAPE;
  else
    flags &= (~TF_CONVERSIONMODE_FULLSHAPE);
  _SetCompartmentDWORD(flags, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION);

  _pLangBarButton->UpdateWeaselStatus(stat);
}

void WeaselTSF::_ShowLanguageBar(BOOL show) {
  if (!_pLangBarButton)
    return;
  _pLangBarButton->Show(show);
}

void WeaselTSF::_EnableLanguageBar(BOOL enable) {
  if (!_pLangBarButton)
    return;
  _pLangBarButton->SetLangbarStatus(TF_LBI_STATUS_DISABLED, !enable);
}
