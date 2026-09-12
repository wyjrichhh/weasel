// codec 往返与截断健壮性测试(p4 协议帧)
// 项目名暂沿用 TestResponseParser,p6 清扫时改名为 TestSnapshotCodec

#include "stdafx.h"
#include <BangkeProtocol.h>
#include <string>
#include <vector>

static int g_failed = 0;

#define CHECK(cond)                                             \
  do {                                                          \
    if (!(cond)) {                                              \
      printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);     \
      ++g_failed;                                               \
    }                                                           \
  } while (0)

static bangke::Context MakeContext() {
  bangke::Context ctx;
  ctx.preedit.str = L"ni'hao'蚌壳";
  ctx.preedit.attributes.push_back(
      bangke::TextAttribute(0, 2, bangke::HIGHLIGHTED));
  ctx.aux.str = L"提示文本";
  bangke::Text candy;
  candy.str = L"你好";
  candy.attributes.push_back(bangke::TextAttribute(0, 2, bangke::LAST_TYPE));
  ctx.cinfo.candies.push_back(candy);
  ctx.cinfo.candies.push_back(bangke::Text(L"逆 Hoy"));
  ctx.cinfo.comments.push_back(bangke::Text(L"注释"));
  ctx.cinfo.labels.push_back(bangke::Text(L"1."));
  ctx.cinfo.currentPage = 2;
  ctx.cinfo.totalPages = 7;
  ctx.cinfo.highlighted = 1;
  ctx.cinfo.is_last_page = true;
  return ctx;
}

static void RoundTrip() {
  const std::wstring commit = L"上屏文本";
  bangke::Context ctx = MakeContext();
  bangke::Status status;
  status.schema_name = L"朙月拼音";
  status.schema_id = L"luna_pinyin";
  status.ascii_mode = true;
  status.composing = true;
  status.full_shape = false;
  status.type = bangke::FULL_SHAPE;
  bangke::Config config;
  config.inline_preedit = true;
  bangke::UIStyle style;
  style.font_face = L"Microsoft YaHei";
  style.font_point = 14;
  style.hover_type = bangke::UIStyle::HILITE;
  style.layout_type = bangke::UIStyle::LAYOUT_VERTICAL;
  style.text_color = 0x12345678;
  style.hilited_mark_color = -1;
  style.client_caps = 7;

  auto frame = bangke::BuildFrame(0xABCD, 42, &commit, &ctx, &status, &config,
                                  &style);
  CHECK(frame.size() > sizeof(bangke::FrameHeader));

  bangke::FrameHeader hdr{};
  std::wstring commit2;
  bangke::Context ctx2;
  bangke::Status status2;
  bangke::Config config2;
  bangke::UIStyle style2;
  const bool ok =
      bangke::ParseFrame(frame.data(), frame.size(), &hdr, &commit2, &ctx2,
                         &status2, &config2, &style2);
  CHECK(ok);
  CHECK(hdr.magic == bangke::kFrameMagic);
  CHECK(hdr.version == bangke::kProtoVersion);
  CHECK(hdr.flags == (bangke::SNAP_HAS_COMMIT | bangke::SNAP_CTX |
                      bangke::SNAP_STATUS | bangke::SNAP_CONFIG |
                      bangke::SNAP_STYLE));
  CHECK(hdr.ipc_sid == 0xABCD);
  CHECK(hdr.key_serial == 42);
  CHECK(commit2 == commit);
  CHECK(ctx2.preedit.str == ctx.preedit.str);
  CHECK(ctx2.preedit.attributes.size() == 1);
  CHECK(ctx2.preedit.attributes[0].type == bangke::HIGHLIGHTED);
  CHECK(ctx2.cinfo.candies.size() == 2);
  CHECK(ctx2.cinfo.candies[1].str == L"逆 Hoy");
  CHECK(ctx2.cinfo.currentPage == 2 && ctx2.cinfo.totalPages == 7);
  CHECK(ctx2.cinfo.highlighted == 1 && ctx2.cinfo.is_last_page);
  CHECK(status2.schema_name == L"朙月拼音");
  CHECK(status2.ascii_mode && status2.composing);
  CHECK(status2.type == bangke::FULL_SHAPE);
  CHECK(config2.inline_preedit);
  CHECK(style2.font_face == L"Microsoft YaHei");
  CHECK(style2.font_point == 14);
  CHECK(style2.layout_type == bangke::UIStyle::LAYOUT_VERTICAL);
  CHECK(style2.text_color == 0x12345678);
  CHECK(style2.hilited_mark_color == -1);
  CHECK(style2.client_caps == 7);
}

static void EmptyFrame() {
  auto frame = bangke::BuildFrame(1, 0, nullptr, nullptr, nullptr, nullptr,
                                  nullptr);
  bangke::FrameHeader hdr{};
  CHECK(bangke::ParseFrame(frame.data(), frame.size(), &hdr, nullptr, nullptr,
                           nullptr, nullptr, nullptr));
  CHECK(hdr.flags == 0);
  CHECK(hdr.payload_len == 0);
}

static void TruncationFuzz() {
  bangke::Context ctx = MakeContext();
  bangke::Status status;
  status.schema_name = L"x";
  auto frame = bangke::BuildFrame(3, 9, nullptr, &ctx, &status, nullptr,
                                  nullptr);
  bangke::Context ctx2;
  bangke::Status status2;
  bangke::FrameHeader hdr{};
  // 任何截断都必须解析失败(且不得崩溃)
  for (size_t cut = 0; cut < frame.size(); ++cut) {
    const bool ok = bangke::ParseFrame(frame.data(), cut, &hdr, nullptr, &ctx2,
                                       &status2, nullptr, nullptr);
    CHECK(!ok);
  }
  // 尾部粘垃圾:长度不匹配,拒绝
  std::vector<uint8_t> tampered = frame;
  tampered.push_back(0xEE);
  CHECK(!bangke::ParseFrame(tampered.data(), tampered.size(), &hdr, nullptr,
                            &ctx2, &status2, nullptr, nullptr));
  // 完整帧仍可解析
  CHECK(bangke::ParseFrame(frame.data(), frame.size(), &hdr, nullptr, &ctx2,
                           &status2, nullptr, nullptr));
}

static void OutParamSubset() {
  // 只取部分输出参数也要正确消费流
  bangke::Status status;
  status.composing = true;
  auto frame = bangke::BuildFrame(5, 6, nullptr, nullptr, &status, nullptr,
                                  nullptr);
  bangke::FrameHeader hdr{};
  CHECK(bangke::ParseFrame(frame.data(), frame.size(), &hdr, nullptr, nullptr,
                           nullptr, nullptr, nullptr));
  CHECK(hdr.flags == bangke::SNAP_STATUS);
}

int main() {
  RoundTrip();
  EmptyFrame();
  TruncationFuzz();
  OutParamSubset();
  if (g_failed == 0) {
    printf("ALL PASS\n");
    return 0;
  }
  printf("%d FAILURES\n", g_failed);
  return 1;
}
