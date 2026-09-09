#include "stdafx.h"

#include <PipeChannel.h>

using namespace weasel;
using namespace std;
using namespace boost;

#define _ThrowLastError throw ::GetLastError()
#define _ThrowCode(__c) throw __c
#define _ThrowIfNot(__c)                 \
  {                                      \
    DWORD err;                           \
    if ((err = ::GetLastError()) != __c) \
      throw err;                         \
  }

PipeChannelBase::PipeChannelBase(std::wstring&& pn_cmd,
                                 size_t bs = 4 * 1024,
                                 SECURITY_ATTRIBUTES* s = NULL)
    : pname(pn_cmd), buff_size(bs), sa(s) {};

PipeChannelBase::~PipeChannelBase() {
  // Thread-specific pointers are cleaned up automatically
}

bool PipeChannelBase::_Ensure() {
  try {
    HANDLE* phandle = _GetPipeHandle();
    if (_Invalid(*phandle)) {
      *phandle = _Connect(pname.c_str());
      return !_Invalid(*phandle);
    }
  } catch (...) {
    return false;
  }

  return true;
}

// 管道读写超时:server 挂死时 UI 线程最多卡 kPipeTimeoutMs 而非永久冻死。
// 同步句柄上用 CancelIo + 自身线程等待:发起 overlapped 读写,限时等待,
// 超时则 CancelIo 取消并抛异常让上层断开重连。
static bool s_TimedPipeIO(HANDLE pipe, BOOL is_read, LPVOID buf, DWORD len,
                          DWORD* transferred) {
  OVERLAPPED ov = {};
  ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
  if (!ov.hEvent)
    return false;
  BOOL ok = is_read ? ::ReadFile(pipe, buf, len, transferred, &ov)
                    : ::WriteFile(pipe, buf, len, transferred, &ov);
  if (!ok && GetLastError() != ERROR_IO_PENDING) {
    CloseHandle(ov.hEvent);
    return false;
  }
  if (WaitForSingleObject(ov.hEvent, 2000) != WAIT_OBJECT_0) {
    ::CancelIo(pipe);
    // 等取消落地(管道可能已经完成,取最终状态)
    DWORD dummy = 0;
    GetOverlappedResult(pipe, &ov, &dummy, TRUE);
    CloseHandle(ov.hEvent);
    SetLastError(ERROR_TIMEOUT);
    return false;
  }
  ok = GetOverlappedResult(pipe, &ov, transferred, FALSE);
  CloseHandle(ov.hEvent);
  return ok;
}


HANDLE PipeChannelBase::_Connect(const wchar_t* name) {
  HANDLE pipe = INVALID_HANDLE_VALUE;
  while (_Invalid(pipe = _TryConnect()))
    ::WaitNamedPipe(name, 500);
  DWORD mode = PIPE_READMODE_MESSAGE;
  if (!SetNamedPipeHandleState(pipe, &mode, NULL, NULL)) {
    _ThrowLastError;
  }
  return pipe;
}

void PipeChannelBase::_Reconnect() {
  HANDLE* phandle = _GetPipeHandle();
  _FinalizePipe(*phandle);
  _Ensure();
}

HANDLE PipeChannelBase::_TryConnect() {
  auto pipe = ::CreateFile(pname.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                           OPEN_EXISTING, 0, NULL);
  if (!_Invalid(pipe)) {
    // connected to the pipe
    return pipe;
  }
  // being busy is not really an error since we just need to wait.
  _ThrowIfNot(ERROR_PIPE_BUSY);
  // All pipe instances are busy
  return INVALID_HANDLE_VALUE;
}

size_t PipeChannelBase::_WritePipe(HANDLE pipe, size_t s, char* b) {
  DWORD lwritten = 0;
  if (!s_TimedPipeIO(pipe, FALSE, b, (DWORD)s, &lwritten) || lwritten == 0) {
    _ThrowLastError;
  }
  return lwritten;
}

void PipeChannelBase::_FinalizePipe(HANDLE& p) {
  if (!_Invalid(p)) {
    DisconnectNamedPipe(p);
    CloseHandle(p);
  }
  p = INVALID_HANDLE_VALUE;
}

void PipeChannelBase::_Receive(HANDLE pipe, LPVOID msg, size_t rec_len) {
  DWORD lread = 0;
  if (!s_TimedPipeIO(pipe, TRUE, msg, (DWORD)rec_len, &lread)) {
    _ThrowLastError;
  }
  if (lread < rec_len) {
    // ERROR_MORE_DATA:读剩余体到线程缓冲
    auto ctx = _GetContext();
    memset(ctx->buffer.get(), 0, buff_size);
    DWORD lbody = 0;
    if (!s_TimedPipeIO(pipe, TRUE, ctx->buffer.get(), (DWORD)buff_size,
                       &lbody)) {
      _ThrowLastError;
    }
  }
  _GetContext()->has_body = false;
}

HANDLE PipeChannelBase::_ConnectServerPipe(std::wstring& pn) {
  HANDLE pipe =
      CreateNamedPipe(pn.c_str(), PIPE_ACCESS_DUPLEX,
                      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                      PIPE_UNLIMITED_INSTANCES, buff_size, buff_size, 0, sa);
  if (pipe == INVALID_HANDLE_VALUE || !::ConnectNamedPipe(pipe, NULL)) {
    _ThrowLastError;
  }
  return pipe;
}
