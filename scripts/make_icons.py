#!/usr/bin/env python3
"""生成蚌壳拼音托盘模式图标（bangke_zh.ico / bangke_en.ico）。

设计目标：16px 下仍可与 profile 图标（bangke.ico 的「蚌」描边徽章）区分。
模式图标采用「色块 + 粗壮单字符」：中文=蓝色圆角块+白色「中」，英文=深灰
圆角块+白色「A」。字符笔画数少，缩到 16px 仍可读；色相差异保证一眼区分。

用法：python3 scripts/make_icons.py   （需 Pillow；在 Mac 或 Windows 构建机上均可）
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
RESOURCE = ROOT / "resource"

SIZES = [16, 20, 24, 32, 48, 64, 128, 256]
MASTER = 256

# 中文/英文模式块的颜色（zh 沿用安装器 accent 蓝，en 用中性深灰蓝）
ZH_FILL = (58, 138, 232)
ZH_FILL_DARK = (47, 124, 214)
EN_FILL = (74, 85, 107)
EN_FILL_DARK = (58, 67, 86)

FONT_CANDIDATES_ZH = [
    "/System/Library/Fonts/PingFang.ttc",            # mac
    "/System/Library/Fonts/Hiragino Sans GB.ttc",    # mac fallback
    "C:/Windows/Fonts/msyhbd.ttc",                   # 微软雅黑 Bold
    "C:/Windows/Fonts/simhei.ttf",                   # 黑体
]
FONT_CANDIDATES_EN = [
    "/System/Library/Fonts/Helvetica.ttc",
    "/Library/Fonts/Arial Bold.ttf",
    "C:/Windows/Fonts/arialbd.ttf",
    "C:/Windows/Fonts/seguisb.ttf",
]


def load_font(candidates, size):
    for path in candidates:
        if Path(path).exists():
            try:
                return ImageFont.truetype(path, size)
            except OSError:
                continue
    return ImageFont.load_default()


def make_tile(fill, fill_dark, glyph, font_candidates):
    """圆角色块 + 居中粗体字符。master 尺寸绘制，靠 LANCZOS 缩小保锐度。"""
    img = Image.new("RGBA", (MASTER, MASTER), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([8, 8, MASTER - 8, MASTER - 8], radius=56,
                        fill=fill_dark)
    # 顶部略亮的内层，制造极简立体感（不依赖细描边，小尺寸下不糊）
    d.rounded_rectangle([8, 8, MASTER - 8, MASTER - 20], radius=56,
                        fill=fill)
    font = load_font(font_candidates, 150)
    bbox = d.textbbox((0, 0), glyph, font=font)
    w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
    d.text(((MASTER - w) / 2 - bbox[0], (MASTER - h) / 2 - bbox[1] - 4),
           glyph, font=font, fill=(255, 255, 255, 255))
    return img


def main():
    icons = {
        "bangke_zh.ico": make_tile(ZH_FILL, ZH_FILL_DARK, "中", FONT_CANDIDATES_ZH),
        "bangke_en.ico": make_tile(EN_FILL, EN_FILL_DARK, "A", FONT_CANDIDATES_EN),
    }
    for name, img in icons.items():
        out = RESOURCE / name
        img.save(out, format="ICO",
                 sizes=[(s, s) for s in SIZES])
        print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
