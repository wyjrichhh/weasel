// 单文件分发引导壳:把图形安装器(BangkeInstaller.exe)+ MSI + Qt 运行库
// 作为 RCDATA 资源嵌入本 exe。运行时解包到 %TEMP%\BangkeSetup\,
// 拉起安装界面,安装器退出后清理临时目录。
// 载荷布局由资源 IDR_MANIFEST(100) 描述:每行 "资源ID|相对路径"。
// 本体 asInvoker(只写 %TEMP%),UAC 由内层安装器触发,只弹一次。
#include <windows.h>
#include <shlwapi.h>

#include <string>
#include <vector>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

static const wchar_t* kWndClass = L"BangkeBoot";
static wchar_t g_msg[128] = L"正在准备安装…";
static HWND g_wnd = NULL;

static std::wstring SlashPath(const std::wstring& p) {
  std::wstring r = p;
  for (auto& c : r)
    if (c == L'/')
      c = L'\\';
  return r;
}

static void EnsureDirFor(const std::wstring& file) {
  std::wstring dir = file.substr(0, file.find_last_of(L'\\'));
  for (size_t i = 3; i < dir.size(); ++i) {
    if (dir[i] == L'\\') {
      CreateDirectoryW(dir.substr(0, i).c_str(), NULL);
    }
  }
  CreateDirectoryW(dir.c_str(), NULL);
}

static void Pump() {
  MSG m;
  while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&m);
    DispatchMessageW(&m);
  }
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
  switch (msg) {
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(h, &ps);
      SetBkColor(dc, RGB(13, 20, 29));
      SetTextColor(dc, RGB(210, 222, 235));
      RECT rc;
      GetClientRect(h, &rc);
      DrawTextW(dc, g_msg, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      EndPaint(h, &ps);
      return 0;
    }
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(h, msg, w, l);
}

static bool ExtractPayload(const std::wstring& dir) {
  HRSRC mres = FindResourceW(NULL, MAKEINTRESOURCEW(100), RT_RCDATA);
  if (!mres)
    return false;
  HGLOBAL mg = LoadResource(NULL, mres);
  const char* manifest = (const char*)LockResource(mg);
  const size_t mlen = SizeofResource(NULL, mres);

  std::vector<std::pair<int, std::wstring>> items;
  std::string line;
  for (size_t i = 0; i <= mlen; ++i) {
    if (i == mlen || manifest[i] == '\n') {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();  // CRLF 清单,\r 混进路径即建文件失败
      const size_t bar = line.find('|');
      if (bar != std::string::npos) {
        items.push_back({atoi(line.substr(0, bar).c_str()),
                         SlashPath(std::wstring(line.begin() + bar + 1,
                                                line.begin() + line.size()))});
      }
      line.clear();
    } else {
      line.push_back(manifest[i]);
    }
  }

  int done = 0;
  for (const auto& [id, rel] : items) {
    swprintf_s(g_msg, L"正在解包安装文件… %d/%d", ++done, (int)items.size());
    InvalidateRect(g_wnd, NULL, FALSE);  // 让小窗文案跟上
    Pump();

    HRSRC r = FindResourceW(NULL, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!r)
      return false;
    HGLOBAL g = LoadResource(NULL, r);
    const void* data = LockResource(g);
    const DWORD size = SizeofResource(NULL, r);
    const std::wstring dst = dir + L"\\" + rel;
    EnsureDirFor(dst);
    HANDLE f = CreateFileW(dst.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
      return false;
    // 264MB 级载荷,分块落盘,进度条不卡
    const BYTE* p = (const BYTE*)data;
    for (DWORD off = 0; off < size;) {
      const DWORD chunk = min(1u << 20, size - off);
      DWORD wrote = 0;
      if (!WriteFile(f, p + off, chunk, &wrote, NULL) || wrote != chunk) {
        CloseHandle(f);
        return false;
      }
      off += chunk;
      Pump();
    }
    CloseHandle(f);
  }
  return true;
}

static void DeleteTree(const std::wstring& dir) {
  std::wstring dbl = dir;
  for (auto& c : dbl)
    if (c == L'\\')
      c = L'\0';
  dbl.push_back(L'\0');
  dbl.push_back(L'\0');
  SHFILEOPSTRUCTW op{};
  op.wFunc = FO_DELETE;
  op.pFrom = dbl.c_str();
  op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
  SHFileOperationW(&op);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int) {
  // 极简进度小窗:安装界面解包就绪后由它接管
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursorW(NULL, IDC_APPSTARTING);
  wc.hbrBackground = CreateSolidBrush(RGB(13, 20, 29));
  wc.lpszClassName = kWndClass;
  RegisterClassExW(&wc);
  const int w = 420, h = 120;
  const int sx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
  const int sy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
  g_wnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, kWndClass,
                             L"蚌壳拼音", WS_POPUP | WS_VISIBLE, sx, sy, w, h,
                             NULL, NULL, inst, NULL);
  Pump();

  wchar_t tmp[MAX_PATH] = {0};
  GetTempPathW(MAX_PATH, tmp);
  const std::wstring dir = std::wstring(tmp) + L"BangkeSetup";
  DeleteTree(dir);  // 上次残留
  CreateDirectoryW(dir.c_str(), NULL);

  bool ok = ExtractPayload(dir);
  if (ok) {
    wcscpy_s(g_msg, L"");
    DestroyWindow(g_wnd);
    const std::wstring exe = dir + L"\\BangkeInstaller.exe";
    // 安装器带 requireAdministrator 清单,CreateProcessW 拉不起来
    // (ERROR_ELEVATION_REQUIRED);ShellExecuteEx 才会触发 UAC 弹窗
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.lpFile = exe.c_str();
    sei.lpDirectory = dir.c_str();
    sei.nShow = SW_SHOWNORMAL;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    if (ShellExecuteExW(&sei)) {
      if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, INFINITE);
        CloseHandle(sei.hProcess);
      }
    } else {
      ok = false;
    }
  }
  if (!ok) {
    wcscpy_s(g_msg, L"解包失败,请重新下载安装包");
    InvalidateRect(g_wnd, NULL, TRUE);
    Pump();
    Sleep(2500);
  }
  DeleteTree(dir);
  return 0;
}
