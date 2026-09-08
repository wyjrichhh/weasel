// 蚌壳拼音前端协议 v2:版本化二进制快照帧。
// 管道响应与 AI 推送槽共用同一种帧;取代行文本+boost archive。
// 数据结构本体仍在 WeaselIPCData.h(⑤ 步剥离 boost 后只剩纯结构)。
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <WeaselIPCData.h>

namespace bangke {

constexpr uint32_t kFrameMagic = 0x50534B42;  // "BKSP" (little-endian)
constexpr uint16_t kProtoVersion = 2;

enum FrameFlags : uint16_t {
  SNAP_HAS_COMMIT = 1 << 0,  // 仅按键路径置位;推送帧永不携带
  SNAP_CTX = 1 << 1,
  SNAP_STATUS = 1 << 2,
  SNAP_CONFIG = 1 << 3,
  SNAP_STYLE = 1 << 4,
};

#pragma pack(push, 1)
struct FrameHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t flags;
  uint32_t ipc_sid;      // weasel 会话 id
  uint32_t key_serial;   // server 每 ProcessKeyEvent 递增;帧的全序依据
  uint32_t payload_len;  // 头之后的字节数
};
#pragma pack(pop)

// payload 编码:依 flags 顺序排列字段,每字段 = u32 字节数 + 内容;
// 字符串为 wchar 数组(无终止符),整数为定宽小端。长度前缀兼作字段边界,
// 截断/越界在 Reader 层统一判废。

class Writer {
 public:
  std::vector<uint8_t> buf;

  void U8(uint8_t v) { buf.push_back(v); }
  void U32(uint32_t v) {
    for (int i = 0; i < 4; ++i) buf.push_back((v >> (8 * i)) & 0xFF);
  }
  void I32(int32_t v) { U32(static_cast<uint32_t>(v)); }
  void Bool(bool v) { U8(v ? 1 : 0); }
  void WStr(const std::wstring& s) {
    const uint32_t bytes = static_cast<uint32_t>(s.size() * sizeof(wchar_t));
    U32(bytes);
    const auto* p = reinterpret_cast<const uint8_t*>(s.data());
    buf.insert(buf.end(), p, p + bytes);
  }
};

class Reader {
 public:
  Reader(const uint8_t* data, size_t len)
      : p_(data), end_(data + len), ok_(true) {}

  bool Ok() const { return ok_; }
  size_t Remaining() const { return static_cast<size_t>(end_ - p_); }

  uint8_t U8() {
    if (!Take(1)) return 0;
    return *p_++;
  }
  uint32_t U32() {
    if (!Take(4)) return 0;
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(*p_++) << (8 * i);
    return v;
  }
  int32_t I32() { return static_cast<int32_t>(U32()); }
  bool Bool() { return U8() != 0; }
  std::wstring WStr() {
    const uint32_t bytes = U32();
    if (!ok_ || bytes % sizeof(wchar_t) != 0 || !Take(bytes))
      return std::wstring();
    std::wstring s(reinterpret_cast<const wchar_t*>(p_), bytes / sizeof(wchar_t));
    p_ += bytes;
    return s;
  }

 private:
  bool Take(size_t n) {
    if (!ok_ || static_cast<size_t>(end_ - p_) < n) {
      ok_ = false;
      return false;
    }
    return true;
  }
  const uint8_t* p_;
  const uint8_t* end_;
  bool ok_;
};

// ---- 结构体编解码(逐字段显式;新字段必须两侧同步,版本号随之递增) ----
void PutText(Writer& w, const weasel::Text& t);
bool GetText(Reader& r, weasel::Text& t);
void PutCandidateInfo(Writer& w, const weasel::CandidateInfo& ci);
bool GetCandidateInfo(Reader& r, weasel::CandidateInfo& ci);
void PutContext(Writer& w, const weasel::Context& ctx);
bool GetContext(Reader& r, weasel::Context& ctx);
void PutStatus(Writer& w, const weasel::Status& s);
bool GetStatus(Reader& r, weasel::Status& s);
void PutConfig(Writer& w, const weasel::Config& c);
bool GetConfig(Reader& r, weasel::Config& c);
void PutUIStyle(Writer& w, const weasel::UIStyle& st);
bool GetUIStyle(Reader& r, weasel::UIStyle& st);

// ---- 帧级 API ----
// 组帧;传 nullptr 的部分不写入且不置对应 flag。
std::vector<uint8_t> BuildFrame(uint32_t ipc_sid,
                                uint32_t key_serial,
                                const std::wstring* commit,
                                const weasel::Context* ctx,
                                const weasel::Status* status,
                                const weasel::Config* config,
                                const weasel::UIStyle* style);

// 解帧:校验 magic/version/长度后依 flags 解码;任何不一致返回 false。
// hdr 始终回填(便于诊断);输出结构仅在成功时写入。
bool ParseFrame(const uint8_t* data,
                size_t len,
                FrameHeader* hdr,
                std::wstring* commit,
                weasel::Context* ctx,
                weasel::Status* status,
                weasel::Config* config,
                weasel::UIStyle* style);

// 前缀容错版:cap 是容量上限而非精确长度(管道缓冲按容量传入,帧自描述长度,
// 尾部残留无害)。帧的实际解码边界仍由 payload_len 决定。
bool ParseFramePrefix(const uint8_t* data,
                      size_t cap,
                      FrameHeader* hdr,
                      std::wstring* commit,
                      weasel::Context* ctx,
                      weasel::Status* status,
                      weasel::Config* config,
                      weasel::UIStyle* style);

}  // namespace bangke
