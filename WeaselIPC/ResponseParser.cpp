#include "stdafx.h"
#include <BangkeProtocol.h>
#include <cstring>

#include <ResponseParser.h>

using namespace weasel;

ResponseParser::ResponseParser(std::wstring* commit,
                               Context* context,
                               Status* status,
                               Config* config,
                               UIStyle* style)
    : p_commit(commit),
      p_context(context),
      p_status(status),
      p_config(config),
      p_style(style) {}

bool ResponseParser::operator()(LPWSTR buffer, UINT length) {
  // length 是 wchar 容量;帧自描述长度,尾部残留无害
  if (length < sizeof(bangke::FrameHeader) / sizeof(wchar_t))
    return false;
  uint32_t magic = 0;
  std::memcpy(&magic, buffer, sizeof(magic));
  if (magic != bangke::kFrameMagic)
    return false;
  return bangke::ParseFramePrefix(reinterpret_cast<const uint8_t*>(buffer),
                                  length * sizeof(wchar_t), nullptr, p_commit,
                                  p_context, p_status, p_config, p_style);
}
