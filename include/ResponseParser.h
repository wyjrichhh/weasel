#pragma once
#include <WeaselIPCData.h>
#include <windows.h>
#include <string>

namespace weasel {

// v2 帧响应解析:管道体以 bangke::kFrameMagic 起始即解码;
// 否则返回失败(不再有文本协议回退)
struct ResponseParser {
  std::wstring* p_commit;
  Context* p_context;
  Status* p_status;
  Config* p_config;
  UIStyle* p_style;
  uint32_t* p_serial;  // 帧序号:调用方用于维护已应用序号(推送按序取舍)

  ResponseParser(std::wstring* commit,
                 Context* context = 0,
                 Status* status = 0,
                 Config* config = 0,
                 UIStyle* style = 0,
                 uint32_t* serial = 0);
  bool operator()(LPWSTR buffer, UINT length);
};

}  // namespace weasel
