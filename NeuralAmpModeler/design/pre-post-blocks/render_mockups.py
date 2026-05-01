"""Render PRE/POST block redesign mockups for VoLum.

Produces four full-app PNG mockups (variant A-D) plus a comparison sheet,
all sharing the same surrounding chrome so only the PRE/POST treatment
varies between them. Uses the real VoLum palette (VoLumColorHelpers.h)
and the Josefin Sans + Michroma fonts shipped with the plugin.

Run:
    python NeuralAmpModeler/design/pre-post-blocks/render_mockups.py
"""

from __future__ import annotations

import math
import os
import random
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, List, Tuple

from PIL import Image, ImageDraw, ImageFilter, ImageFont

# ---------------------------------------------------------------------------
# Paths & resolution
# ---------------------------------------------------------------------------

ROOT = Path(__file__).resolve().parents[3]
FONT_DIR = ROOT / "NeuralAmpModeler" / "resources" / "fonts"
FONT_DIR_WIN = ROOT / "NeuralAmpModeler" / "resources" / "fonts-win"
OUT_DIR = Path(__file__).resolve().parent

# Render at native target size; PIL antialiases truetype text already.
CW, CH = 1600, 1144

# ---------------------------------------------------------------------------
# Palette (matches VoLumColorHelpers.h)
# ---------------------------------------------------------------------------

BG = (17, 17, 24)
SIDEBAR_BG = (17, 17, 24)
SIDEBAR_BG2 = (12, 12, 20)
HERO_BG = (12, 12, 18)
TEAL = (91, 196, 196)
TEAL_DIM = (75, 162, 162)
GOLD = (252, 222, 145)
GOLD_DIM = (235, 210, 145)
AMBER = (232, 168, 92)
CREAM = (237, 227, 208)
CREAM_DIM = (166, 149, 124)
TEXT_BRIGHT = (255, 248, 238)
TEXT_MED = (245, 232, 218)
TEXT_DIM = (232, 218, 200)
FRAME = (200, 162, 78)  # alpha ~72 in app, we'll blend manually
DIVIDER = (200, 162, 78)
METER_GREEN = (42, 138, 42)
BTN_AMP_ON_BG = (48, 125, 118)
BTN_AMP_ON_BORDER = (165, 230, 220)
BTN_CAB_ON_BG = (115, 88, 52)
BTN_CAB_ON_BORDER = (225, 195, 115)


def rgba(rgb, a):
    return (rgb[0], rgb[1], rgb[2], int(a))


# ---------------------------------------------------------------------------
# Fonts
# ---------------------------------------------------------------------------


def _font(path: Path, size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(path), size)


def font_bold(size):
    p = FONT_DIR_WIN / "JosefinSans-Bold.ttf"
    if not p.exists():
        p = FONT_DIR / "JosefinSans-SemiBold.ttf"
    return _font(p, size)


def font_semi(size):
    return _font(FONT_DIR / "JosefinSans-SemiBold.ttf", size)


def font_reg(size):
    return _font(FONT_DIR / "JosefinSans-Regular.ttf", size)


def font_disp(size):
    return _font(FONT_DIR / "PoiretOne-Regular.ttf", size)


def font_mono(size):
    return _font(FONT_DIR / "Michroma-Regular.ttf", size)


# ---------------------------------------------------------------------------
# Drawing primitives
# ---------------------------------------------------------------------------


def corner_accent(d: ImageDraw.ImageDraw, x, y, size, flip_h, flip_v, color, width=2):
    dx = -size if flip_h else size
    dy = -size if flip_v else size
    d.line([(x, y), (x + dx, y)], fill=color, width=width)
    d.line([(x, y), (x, y + dy)], fill=color, width=width)


def diagonal_hatch(canvas: Image.Image, rect, color=(100, 180, 200, 30), step=14):
    """Mimic the dormant strip's clipped diagonal crosshatch."""
    layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    L, T, R, B = rect
    w, h = R - L, B - T
    for d_off in range(int(-h), int(w), step):
        x1, y1, x2, y2 = L + d_off, T, L + d_off + h, B
        if x1 < L:
            y1 += L - x1
            x1 = L
        if x2 > R:
            y2 -= x2 - R
            x2 = R
        if y1 < B and y2 > T:
            ld.line([(x1, y1), (x2, y2)], fill=color, width=1)
        x1, y1, x2, y2 = R - d_off, T, R - d_off - h, B
        if x1 > R:
            y1 += x1 - R
            x1 = R
        if x2 < L:
            y2 -= L - x2
            x2 = L
        if y1 < B and y2 > T:
            ld.line([(x1, y1), (x2, y2)], fill=color, width=1)
    canvas.alpha_composite(layer)


def text_centered(d, xy, text, font, fill):
    bbox = d.textbbox((0, 0), text, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    cx, cy = xy
    d.text((cx - tw / 2 - bbox[0], cy - th / 2 - bbox[1]), text, font=font, fill=fill)


def text_left(d, xy, text, font, fill):
    bbox = d.textbbox((0, 0), text, font=font)
    d.text((xy[0] - bbox[0], xy[1] - bbox[1]), text, font=font, fill=fill)


def text_tracking(d, xy, text, font, fill, tracking=2):
    """Letter-spaced text starting at xy (left baseline-ish)."""
    x, y = xy
    for ch in text:
        d.text((x, y), ch, font=font, fill=fill)
        bbox = d.textbbox((0, 0), ch, font=font)
        x += (bbox[2] - bbox[0]) + tracking


def stroke_round_rect(d, rect, radius, color, width=2):
    d.rounded_rectangle(rect, radius=radius, outline=color, width=width)


# ---------------------------------------------------------------------------
# Effect motif renderers (echo VoLumTriptych.h _DrawFractalArt at small scale)
# ---------------------------------------------------------------------------


def draw_motif_comp(d, rect, active=True):
    """Compressor: nested elliptical orbits."""
    L, T, R, B = rect
    cx, cy = (L + R) / 2, (T + B) / 2
    w, h = R - L, B - T
    base_alpha = 220 if active else 60
    teal_a = rgba(TEAL, base_alpha)
    blue_a = rgba((145, 220, 245), int(base_alpha * 0.85))
    radii = [(w * 0.20, h * 0.10), (w * 0.30, h * 0.18), (w * 0.40, h * 0.26)]
    for i, (rx, ry) in enumerate(radii):
        col = teal_a if i % 2 == 0 else blue_a
        d.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], outline=col, width=1)
    pin = teal_a if active else rgba(TEAL_DIM, 90)
    for px, py in [(-0.30, 0.10), (-0.10, -0.10), (0.15, 0.10), (0.30, -0.05)]:
        d.ellipse(
            [cx + w * px - 1.5, cy + h * py - 1.5, cx + w * px + 1.5, cy + h * py + 1.5],
            fill=pin,
        )


def draw_motif_nam_lattice(d, rect, active=True):
    """Pre-NAM: rect lattice with inner subdivisions, mimics PRE_NAM1 fractal."""
    L, T, R, B = rect
    cx, cy = (L + R) / 2, (T + B) / 2
    w, h = R - L, B - T
    base_alpha = 220 if active else 70
    teal_a = rgba(TEAL, base_alpha)
    blue_a = rgba((145, 220, 245), int(base_alpha * 0.7))
    cell = min(w * 0.14, h * 0.19)
    cols, rows = 5, 3
    grid_w, grid_h = cell * cols * 1.16, cell * rows * 1.10
    left, top = cx - grid_w / 2, cy - grid_h / 2
    for y in range(rows):
        for x in range(cols):
            px = left + x * cell * 1.16
            py = top + y * cell * 1.10
            col = blue_a if (x + y) % 2 else teal_a
            d.rectangle([px, py, px + cell, py + cell], outline=col, width=1)
            if (x + y) % 3 == 0:
                sub = cell * 0.32
                d.rectangle(
                    [px + sub, py + sub, px + cell - sub, py + cell - sub],
                    outline=rgba(TEAL_DIM, int(base_alpha * 0.5)),
                    width=1,
                )


def draw_motif_nam_circles(d, rect, active=True):
    """Pre-NAM2: dual concentric circles (echoes PRE_NAM2 fractal)."""
    L, T, R, B = rect
    cx, cy = (L + R) / 2, (T + B) / 2
    w, h = R - L, B - T
    base_alpha = 220 if active else 70
    teal_a = rgba(TEAL, base_alpha)
    blue_a = rgba((145, 220, 245), int(base_alpha * 0.85))
    cxA = cx - w * 0.13
    cxB = cx + w * 0.13
    for i in range(4):
        rad = h * (0.10 + i * 0.07)
        d.ellipse(
            [cxA - rad, cy - rad, cxA + rad, cy + rad],
            outline=rgba(TEAL, int(base_alpha * (0.4 + i * 0.15))),
            width=1,
        )
        d.ellipse(
            [cxB - rad, cy - rad, cxB + rad, cy + rad],
            outline=rgba((145, 220, 245), int(base_alpha * (0.35 + i * 0.13))),
            width=1,
        )


def draw_motif_delay(d, rect, active=True):
    """Delay: decaying tap sine wave train."""
    L, T, R, B = rect
    cx, cy = (L + R) / 2, (T + B) / 2
    w, h = R - L, B - T
    base = 220 if active else 60
    taps = 4
    tap_w = w / taps
    for t in range(taps):
        decay = 1 - t / taps * 0.7
        amp = h * 0.30 * decay
        a = int(base * decay)
        base_x = L + t * tap_w
        prev = (base_x, cy)
        segs = 24
        for j in range(1, segs + 1):
            t1 = j / segs
            x = base_x + t1 * tap_w
            env = math.sin(t1 * math.pi)
            y = cy + math.sin(t1 * math.pi * 2 * 3) * amp * env
            d.line([prev, (x, y)], fill=rgba(TEAL, a), width=1)
            prev = (x, y)
    d.line([(L, cy), (R, cy)], fill=rgba(TEAL, int(base * 0.2)), width=1)


def draw_motif_reverb(d, rect, active=True):
    """Reverb: radiating arc fan."""
    L, T, R, B = rect
    cx, cy = (L + R) / 2, (T + B) / 2
    w, h = R - L, B - T
    base = 220 if active else 60
    teal_a = rgba(TEAL, base)
    blue_a = rgba((145, 220, 245), int(base * 0.8))
    for i in range(5):
        rad_x = w * (0.15 + i * 0.07)
        rad_y = h * (0.10 + i * 0.13)
        col = teal_a if i % 2 else blue_a
        d.arc([cx - rad_x, cy - rad_y, cx + rad_x, cy + rad_y], 200, 340, fill=col, width=1)
    d.line([(cx - w * 0.30, cy + h * 0.25), (cx + w * 0.30, cy + h * 0.25)], fill=rgba(TEAL_DIM, base), width=1)


def draw_motif_eq(d, rect, active=True):
    """Spare 'extra slot' motif: vertical EQ-band bars."""
    L, T, R, B = rect
    base = 220 if active else 50
    bars = 6
    pad = 4
    bw = (R - L - pad * 2) / bars
    for i in range(bars):
        bx = L + pad + i * bw
        scale = [0.4, 0.7, 1.0, 0.85, 0.55, 0.3][i]
        bh = (B - T - pad * 2) * scale
        by = B - pad - bh
        col = rgba(TEAL, base) if i % 2 else rgba((145, 220, 245), int(base * 0.85))
        d.rectangle([bx + 1, by, bx + bw - 1, B - pad], outline=col, width=1)


# ---------------------------------------------------------------------------
# Background fractal art for hero (the Diezel "lightning" tree)
# ---------------------------------------------------------------------------


def draw_hero_fractal(canvas: Image.Image, rect):
    """Random branching tree — visual echo of the Diezel Herbert Mk1 hero."""
    layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    L, T, R, B = rect
    cx, cy = (L + R) / 2, (T + B) / 2
    w, h = R - L, B - T
    rng = random.Random(42)
    pts: List[Tuple[float, float]] = []
    for f_pct in range(5, 95, 14):
        pts.append((L + w * f_pct / 100, B - 4))
    for f_pct in range(10, 90, 18):
        pts.append((L + w * f_pct / 100, cy + h * 0.10))
    for f_pct in range(15, 85, 22):
        pts.append((L + w * f_pct / 100, cy - h * 0.10))
    count = 8000
    for i in range(count):
        idx = rng.randrange(len(pts))
        ang = rng.random() * 2 * math.pi
        ln = 2 + rng.random() * 4
        nx = pts[idx][0] + ln * math.cos(ang)
        ny = pts[idx][1] + ln * math.sin(ang)
        if L <= nx <= R and T <= ny <= B:
            if i < 200:
                col = (120, 210, 220, 150)
                tk = 2
            elif i < 1200:
                col = (100, 180, 200, 80)
                tk = 1
            else:
                col = (80, 150, 170, 45)
                tk = 1
            d.line([pts[idx], (nx, ny)], fill=col, width=tk)
            pts.append((nx, ny))
    canvas.alpha_composite(layer)


# ---------------------------------------------------------------------------
# Layout constants — matches the actual app screenshot proportions
# ---------------------------------------------------------------------------


@dataclass
class Layout:
    canvas_w: int
    canvas_h: int
    sidebar_w: int = 290
    titlebar_h: int = 48
    speaker_row_h: int = 92
    knob_row_h: int = 220
    toggle_row_h: int = 60
    footer_h: int = 50
    triptych_w: int = 1100
    triptych_h: int = 360


# ---------------------------------------------------------------------------
# Sidebar (amp list)
# ---------------------------------------------------------------------------

AMP_NAMES = [
    ("Ampete One", False),
    ("Bad Cat mini Cat", False),
    ("Brunetti XL 2", False),
    ("Diezel Herbert Mk1", True),
    ("Fryette Deliv. 120", False),
    ("H&K TriAmp Mk2", False),
    ("Lichtlaerm Prom.", False),
    ("Marshall 2204", False),
    ("Marshall JMP 2203", False),
    ("Marshall JVM", False),
    ("Orange OD120", False),
    ("Orange ORS100", False),
    ("Sebago Texas Fl.", False),
    ("Soldano SLO100", False),
    ("THC Sunset", False),
]


def draw_sidebar(canvas, draw, layout):
    L = 0
    R = layout.sidebar_w
    T = layout.titlebar_h
    B = layout.canvas_h
    draw.rectangle([L, T, R, B], fill=SIDEBAR_BG)
    draw.line([(R, T), (R, B)], fill=rgba(DIVIDER, 50), width=1)

    # Header "VoLum NAM PLAYER"
    fnt_v = font_disp(58)
    fnt_sub = font_mono(13)
    text_centered(draw, ((L + R) / 2, T + 60), "VoLum", fnt_v, GOLD)
    # Tracked subtitle
    sub = "NAM PLAYER"
    bbox = draw.textbbox((0, 0), sub, font=fnt_sub)
    sub_w = (bbox[2] - bbox[0]) + (len(sub) - 1) * 4
    text_tracking(draw, ((L + R) / 2 - sub_w / 2, T + 100), sub, fnt_sub, CREAM_DIM, tracking=4)

    # Items
    f_item = font_semi(22)
    item_h = 56
    list_top = T + 160
    for i, (name, sel) in enumerate(AMP_NAMES):
        y = list_top + i * item_h
        if y + item_h > B - 4:
            break
        row = (L + 8, y, R - 8, y + item_h - 4)
        if sel:
            draw.rounded_rectangle(row, radius=6, fill=rgba(TEAL, 30), outline=rgba(TEAL, 90), width=1)
        # icon glyph (small geometric thumbnail)
        icon_box = (row[0] + 12, row[1] + 14, row[0] + 38, row[3] - 14)
        col = TEAL if sel else CREAM_DIM
        draw_amp_thumb(draw, icon_box, i, col)
        text_left(draw, (row[0] + 50, (row[1] + row[3]) / 2 - 11), name, f_item, CREAM if sel else TEXT_DIM)


def draw_amp_thumb(d, box, kind, col):
    L, T, R, B = box
    cx, cy = (L + R) / 2, (T + B) / 2
    if kind % 5 == 0:
        d.polygon([(L, B), (cx, T), (R, B)], outline=col, width=1)
    elif kind % 5 == 1:
        d.ellipse([L, T, R, B], outline=col, width=1)
        d.ellipse([L + 4, T + 4, R - 4, B - 4], outline=col, width=1)
    elif kind % 5 == 2:
        d.rectangle([L, T, R, B], outline=col, width=1)
        d.line([(L, cy), (R, cy)], fill=col, width=1)
    elif kind % 5 == 3:
        # lightning bolt-ish (selected Diezel)
        d.line([(L + 4, T), (cx, cy - 2), (L + 4, B)], fill=col, width=1)
        d.line([(R - 4, T), (cx, cy + 2), (R - 4, B)], fill=col, width=1)
    else:
        d.line([(L, B), (cx, T), (R, B)], fill=col, width=1)
        d.line([(L + 6, cy), (R - 6, cy)], fill=col, width=1)


# ---------------------------------------------------------------------------
# Top bar (titlebar + speaker tabs + icon row)
# ---------------------------------------------------------------------------


def draw_titlebar(canvas, d, layout):
    d.rectangle([0, 0, layout.canvas_w, layout.titlebar_h], fill=(28, 28, 34))
    fnt = font_semi(20)
    text_left(d, (16, 14), "VoLum", fnt, CREAM)
    fnt2 = font_reg(16)
    text_left(d, (90, 16), "File   Help", fnt2, CREAM_DIM)
    # window controls
    text_left(d, (layout.canvas_w - 110, 14), "—   ▢   ✕", fnt2, CREAM_DIM)


def draw_speaker_row(canvas, d, layout):
    T = layout.titlebar_h
    R = layout.canvas_w
    L = layout.sidebar_w + 40
    main_b = T + layout.speaker_row_h
    # Three center groups: DIRECT/AMP/CABINET, G12/G65/V30, then icons right
    fnt = font_bold(22)
    grp1 = ["DIRECT", "AMP", "CABINET"]
    grp2 = ["G12", "G65", "V30"]
    sel1, sel2 = 1, 2  # AMP selected, V30 selected
    cx = (L + R) / 2 - 60

    # group 1
    x = cx - 230
    for i, t in enumerate(grp1):
        bbox = d.textbbox((0, 0), t, font=fnt)
        tw = bbox[2] - bbox[0] + 28
        rect = (x, T + 28, x + tw, T + 64)
        if i == sel1:
            d.rounded_rectangle(rect, radius=6, fill=BTN_AMP_ON_BG, outline=BTN_AMP_ON_BORDER, width=2)
            text_centered(d, ((rect[0] + rect[2]) / 2, (rect[1] + rect[3]) / 2 - 1), t, fnt, TEXT_BRIGHT)
        else:
            text_centered(d, ((rect[0] + rect[2]) / 2, (rect[1] + rect[3]) / 2 - 1), t, fnt, TEXT_DIM)
        x += tw + 6

    # group 2
    x += 30
    for i, t in enumerate(grp2):
        bbox = d.textbbox((0, 0), t, font=fnt)
        tw = bbox[2] - bbox[0] + 26
        rect = (x, T + 28, x + tw, T + 64)
        if i == sel2:
            d.rounded_rectangle(rect, radius=6, fill=BTN_CAB_ON_BG, outline=BTN_CAB_ON_BORDER, width=2)
            text_centered(d, ((rect[0] + rect[2]) / 2, (rect[1] + rect[3]) / 2 - 1), t, fnt, TEXT_BRIGHT)
        else:
            text_centered(d, ((rect[0] + rect[2]) / 2, (rect[1] + rect[3]) / 2 - 1), t, fnt, TEXT_DIM)
        x += tw + 6

    # icons on the right (tuner, metronome, settings)
    icon_y = T + 36
    for i, glyph in enumerate(["⏷", "♩", "⚙"]):
        ix = R - 110 + i * 36
        d.rounded_rectangle((ix, icon_y - 14, ix + 28, icon_y + 18), radius=4, outline=rgba(TEAL, 50), width=1)
        text_centered(d, (ix + 14, icon_y + 2), glyph, font_reg(20), CREAM_DIM)


# ---------------------------------------------------------------------------
# Hero / amp art block (the central rectangle with fractal + Diezel name)
# ---------------------------------------------------------------------------


def draw_hero_block(canvas, d, hero_rect, name="Diezel Herbert Mk1"):
    L, T, R, B = hero_rect
    d.rectangle(hero_rect, fill=HERO_BG)
    d.rectangle(hero_rect, outline=rgba(FRAME, 80), width=1)
    cs = 14
    corner_accent(d, L + 4, T + 4, cs, False, False, rgba(TEAL_DIM, 200))
    corner_accent(d, R - 4, T + 4, cs, True, False, rgba(TEAL_DIM, 200))
    corner_accent(d, L + 4, B - 4, cs, False, True, rgba(TEAL_DIM, 200))
    corner_accent(d, R - 4, B - 4, cs, True, True, rgba(TEAL_DIM, 200))
    # fractal art (separate alpha layer)
    inner = (L + 8, T + 8, R - 8, B - 8)
    draw_hero_fractal(canvas, inner)


def draw_amp_name_block(canvas, d, rect, name="Diezel Herbert Mk1"):
    L, T, R, B = rect
    cx = (L + R) / 2
    cy = (T + B) / 2
    fnt = font_disp(40)
    text_centered(d, (cx, cy - 6), name, fnt, GOLD)
    # divider with two diamond pips
    d.line([(cx - 90, cy + 18), (cx + 90, cy + 18)], fill=rgba(TEAL_DIM, 200), width=1)
    # diamond center
    pts = [(cx, cy + 14), (cx + 4, cy + 18), (cx, cy + 22), (cx - 4, cy + 18)]
    d.polygon(pts, outline=rgba(TEAL_DIM, 220))


# ---------------------------------------------------------------------------
# Knob row (CHANNEL <4>, INPUT, GATE, BASS, MID, TREBLE, OUTPUT) + meters
# ---------------------------------------------------------------------------


def draw_knob(d, cx, cy, r, value=0.7, label="", value_text="", color=TEAL, dim=False):
    # outer ring
    ring_col = rgba(color, 90 if not dim else 40)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(8, 8, 14), outline=ring_col, width=1)
    # tick arc
    arc_col = rgba(color, 220 if not dim else 80)
    d.arc([cx - r - 4, cy - r - 4, cx + r + 4, cy + r + 4], 135, 135 + 270, fill=rgba(color, 30), width=1)
    d.arc(
        [cx - r - 4, cy - r - 4, cx + r + 4, cy + r + 4],
        135,
        int(135 + 270 * value),
        fill=arc_col,
        width=2,
    )
    # indicator line
    ang = math.radians(135 + 270 * value)
    ix = cx + math.cos(ang) * (r - 4)
    iy = cy + math.sin(ang) * (r - 4)
    d.line([(cx, cy), (ix, iy)], fill=rgba(color, 240 if not dim else 120), width=2)
    # center dot
    d.ellipse([cx - 2, cy - 2, cx + 2, cy + 2], fill=rgba(color, 240 if not dim else 100))


def draw_knob_row(canvas, d, rect):
    L, T, R, B = rect
    cy = (T + B) / 2 + 14
    knobs = [
        ("INPUT", "-20.0 dB", 0.5),
        ("GATE", "-80.0 dB", 0.0),
        ("BASS", "5.0", 0.5),
        ("MID", "5.0", 0.5),
        ("TREBLE", "5.0", 0.5),
        ("OUTPUT", "-40.0 dB", 0.3),
    ]
    fnt_lbl = font_bold(15)
    fnt_val = font_reg(14)

    # Channel selector on the left
    ch_cx = L + 110
    text_centered(d, (ch_cx, cy - 60), "CHANNEL", fnt_lbl, CREAM)
    text_centered(d, (ch_cx - 30, cy - 10), "<", font_reg(28), TEAL)
    text_centered(d, (ch_cx, cy - 10), "4", font_disp(34), GOLD)
    text_centered(d, (ch_cx + 30, cy - 10), ">", font_reg(28), TEAL)

    # IN meter (left), OUT meter (right)
    meter_w = 14
    meter_h = 130
    in_rect = (L + 22, cy - meter_h / 2, L + 22 + meter_w, cy + meter_h / 2)
    out_rect = (R - 22 - meter_w, cy - meter_h / 2, R - 22, cy + meter_h / 2)
    for mr, label_text, fill_pct in [(in_rect, "I N", 0.5), (out_rect, "O U T", 0.4)]:
        d.rectangle(mr, fill=(8, 8, 14), outline=rgba(TEAL_DIM, 100), width=1)
        # meter fill bottom up
        fh = (mr[3] - mr[1] - 4) * fill_pct
        d.rectangle(
            (mr[0] + 2, mr[3] - 2 - fh, mr[2] - 2, mr[3] - 2),
            fill=METER_GREEN,
        )
        # vertical label (one char per row)
        chs = label_text.split(" ")
        for i, ch in enumerate(chs):
            text_centered(d, ((mr[0] + mr[2]) / 2, mr[1] - 22 + i * 12), ch, font_reg(12), CREAM_DIM)

    # Knob row
    knob_area_l = ch_cx + 80
    knob_area_r = R - 60
    available = knob_area_r - knob_area_l
    n = len(knobs)
    spacing = available / n
    knob_r = 36
    for i, (lbl, val, v) in enumerate(knobs):
        cx = knob_area_l + spacing * (i + 0.5)
        text_centered(d, (cx, cy - 60), lbl, fnt_lbl, CREAM)
        draw_knob(d, cx, cy - 10, knob_r, value=v, color=TEAL)
        text_centered(d, (cx, cy + 40), val, fnt_val, CREAM_DIM)


def draw_toggle_row(canvas, d, rect):
    L, T, R, B = rect
    cy = (T + B) / 2
    cx = (L + R) / 2

    def toggle(x, on, label):
        # pill switch
        pw, ph = 44, 22
        rect = (x - pw / 2, cy - ph / 2, x + pw / 2, cy + ph / 2)
        bg = rgba(GOLD_DIM, 180) if on else rgba(CREAM_DIM, 60)
        d.rounded_rectangle(rect, radius=ph / 2, fill=bg, outline=None)
        knob_x = rect[2] - 11 if on else rect[0] + 11
        d.ellipse([knob_x - 8, cy - 8, knob_x + 8, cy + 8], fill=(20, 20, 28))
        # label
        text_left(d, (x + pw / 2 + 12, cy - 9), label, font_bold(15), CREAM)

    toggle(cx - 130, True, "NOISE GATE")
    toggle(cx + 60, True, "EQ")


def draw_footer(canvas, d, rect):
    L, T, R, B = rect
    cx = (L + R) / 2
    cy = (T + B) / 2
    text_centered(d, (cx, cy), "V30-Herb-4.nam", font_reg(14), CREAM_DIM)


# ---------------------------------------------------------------------------
# PRE/POST variant renderers
# ---------------------------------------------------------------------------


PRE_EFFECTS = [
    ("COMP", draw_motif_comp, False, "1.5:1 . 0.5"),
    ("NAM 1", draw_motif_nam_lattice, True, "POSEY-21"),
    ("NAM 2", draw_motif_nam_circles, False, "RECT-FUZZ"),
]

POST_EFFECTS = [
    ("DELAY", draw_motif_delay, True, "DIGITAL . 380 ms"),
    ("REVERB", draw_motif_reverb, False, "HALL . 50 %"),
    # 3rd "ghost" slot for future
    ("EQ", draw_motif_eq, None, "—"),
]


# ----------------------------------------------------------------------- A

VARIANT_PRE_HEIGHT = {
    "a": 0,
    "b": 86,
    "c": 96,
    "d": 0,
}
VARIANT_POST_HEIGHT = {
    "a": 0,
    "b": 0,
    "c": 96,
    "d": 0,
}


def variant_a(canvas, d, layout, hero_rect, triptych_rect, pre_band, post_band):
    """Flanking-compact: shorter, wider blocks left & right. Vertical slot list."""
    L, T, R, B = triptych_rect
    hL, hT, hR, hB = hero_rect
    block_w = 178
    block_h = 240
    by = (hT + hB) / 2 - block_h / 2

    # left block (PRE)
    left_block = (hL - block_w - 18, by, hL - 18, by + block_h)
    draw_effect_block(canvas, d, left_block, "PRE", PRE_EFFECTS)
    # right block (POST) — show 2 slots only (active layout) but with a hint of expandability
    right_block = (hR + 18, by, hR + 18 + block_w, by + block_h)
    draw_effect_block(canvas, d, right_block, "POST", POST_EFFECTS[:2])


def draw_effect_block(canvas, d, rect, header, effects):
    L, T, R, B = rect
    # subtle frame
    d.rectangle(rect, fill=HERO_BG, outline=rgba(FRAME, 70), width=1)
    cs = 10
    corner_accent(d, L + 4, T + 4, cs, False, False, rgba(TEAL_DIM, 200))
    corner_accent(d, R - 4, T + 4, cs, True, False, rgba(TEAL_DIM, 200))
    corner_accent(d, L + 4, B - 4, cs, False, True, rgba(TEAL_DIM, 200))
    corner_accent(d, R - 4, B - 4, cs, True, True, rgba(TEAL_DIM, 200))
    # header label with navigation chevron (clickable: opens this section, focuses active/first effect)
    fnt_hdr = font_bold(14)
    bbox = d.textbbox((0, 0), header, font=fnt_hdr)
    hdr_w = bbox[2] - bbox[0]
    cx = (L + R) / 2
    hdr_x = cx - (hdr_w + 12) / 2  # leave room for chevron
    text_left(d, (hdr_x, T + 9), header, fnt_hdr, GOLD_DIM)
    # chevron ›
    chev_x = hdr_x + hdr_w + 6
    chev_y = T + 16
    d.line([(chev_x, chev_y - 4), (chev_x + 4, chev_y), (chev_x, chev_y + 4)], fill=rgba(GOLD_DIM, 220), width=1)
    d.line([(L + 16, T + 28), (R - 16, T + 28)], fill=rgba(TEAL_DIM, 100), width=1)

    n = len(effects)
    inner_top = T + 36
    inner_bot = B - 12
    slot_h = (inner_bot - inner_top) / n
    for i, (name, motif, active, _) in enumerate(effects):
        sy = inner_top + i * slot_h
        slot_rect = (L + 8, sy + 4, R - 8, sy + slot_h - 4)
        draw_slot(canvas, d, slot_rect, name, motif, active)


def draw_mini_pill(canvas, d, center, on, w=24, h=11):
    """Mini horizontal toggle pill (echoes NOISE GATE / EQ pills at smaller scale).

    On: gold-dim filled track with teal LED 'eye' on the right.
    Off: cream-dim outlined track with dark knob on the left.
    """
    cx, cy = center
    rect = (cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2)
    if on:
        d.rounded_rectangle(rect, radius=h / 2, fill=rgba(GOLD_DIM, 200), outline=None)
        # knob right + teal LED inside
        knob_x = rect[2] - h / 2
        d.ellipse([knob_x - h / 2 + 1.5, cy - h / 2 + 1.5, knob_x + h / 2 - 1.5, cy + h / 2 - 1.5],
                  fill=(20, 20, 28))
        # tiny teal LED inside the knob
        d.ellipse([knob_x - 2, cy - 2, knob_x + 2, cy + 2], fill=TEAL)
    else:
        d.rounded_rectangle(rect, radius=h / 2, fill=(18, 18, 26), outline=rgba(CREAM_DIM, 110), width=1)
        knob_x = rect[0] + h / 2
        d.ellipse([knob_x - h / 2 + 1.5, cy - h / 2 + 1.5, knob_x + h / 2 - 1.5, cy + h / 2 - 1.5],
                  fill=rgba(CREAM_DIM, 80))


def draw_slot(canvas, d, rect, name, motif, active):
    L, T, R, B = rect
    sub_bg = rgba((20, 20, 32), 255) if active else (10, 10, 16)
    d.rectangle(rect, fill=sub_bg)
    border_col = rgba(TEAL, 220) if active else rgba(FRAME, 60)
    d.rectangle(rect, outline=border_col, width=1)
    if active is False:
        diagonal_hatch(canvas, rect, color=(100, 180, 200, 24), step=10)
    elif active is None:  # ghost / future slot
        diagonal_hatch(canvas, rect, color=(150, 130, 90, 22), step=12)

    # Hit-zone split: navigation zone (left ~70%) | toggle zone (right ~30%)
    toggle_zone_w = 34
    nav_right = R - toggle_zone_w
    # hairline divider between zones
    d.line([(nav_right, T + 4), (nav_right, B - 4)], fill=rgba(FRAME, 70), width=1)

    # Navigation zone: motif + label
    motif_rect = (L + 6, T + 4, L + 6 + (nav_right - L - 12) * 0.55, B - 4)
    motif(d, motif_rect, active=bool(active))
    label_x = motif_rect[2] + 6
    label_y = (T + B) / 2 - 8
    text_left(d, (label_x, label_y), name, font_bold(13), CREAM if active else CREAM_DIM)

    # Toggle zone: mini pill
    toggle_cx = (nav_right + R) / 2
    toggle_cy = (T + B) / 2
    if active is None:
        # ghost: empty placeholder
        d.ellipse([toggle_cx - 3, toggle_cy - 3, toggle_cx + 3, toggle_cy + 3],
                  outline=rgba(GOLD_DIM, 140), width=1)
    else:
        draw_mini_pill(canvas, d, (toggle_cx, toggle_cy), bool(active))


# ----------------------------------------------------------------------- B

def variant_b(canvas, d, layout, hero_rect, triptych_rect, pre_band, post_band):
    """Horizontal chip row above AMP art, spanning full triptych width."""
    L, T, R, B = triptych_rect
    hL, hT, hR, hB = hero_rect
    row_h = 64
    # use the pre_band space allocated above hero
    row_y = pre_band[1] + (pre_band[3] - pre_band[1] - row_h) / 2 if pre_band else hT - row_h - 18
    # background bar
    bar_rect = (hL, row_y, hR, row_y + row_h)
    d.rectangle(bar_rect, fill=(10, 10, 16), outline=rgba(FRAME, 50), width=1)

    # Section labels above bar
    text_left(d, (hL + 4, row_y - 22), "PRE", font_bold(13), GOLD_DIM)
    text_left(d, (hR - 50, row_y - 22), "POST", font_bold(13), GOLD_DIM)
    # divider center diamond
    cx_div = (hL + hR) / 2
    DIVIDER_Y = row_y + row_h / 2
    pts = [(cx_div, DIVIDER_Y - 5), (cx_div + 5, DIVIDER_Y), (cx_div, DIVIDER_Y + 5), (cx_div - 5, DIVIDER_Y)]
    d.line([(cx_div, row_y + 6), (cx_div, row_y + row_h - 6)], fill=rgba(TEAL_DIM, 100), width=1)
    d.polygon(pts, fill=HERO_BG, outline=rgba(TEAL, 220))

    # left half: 3 chips
    half_l = (bar_rect[0] + 8, row_y + 6, cx_div - 12, row_y + row_h - 6)
    half_r = (cx_div + 12, row_y + 6, bar_rect[2] - 8, row_y + row_h - 6)
    draw_chip_row(canvas, d, half_l, PRE_EFFECTS, show_ghost=False)
    draw_chip_row(canvas, d, half_r, POST_EFFECTS[:3], show_ghost=True)


def draw_chip_row(canvas, d, rect, effects, show_ghost=False):
    L, T, R, B = rect
    n = len(effects)
    gap = 10
    chip_w = (R - L - gap * (n - 1)) / n
    for i, (name, motif, active, summary) in enumerate(effects):
        cx_l = L + i * (chip_w + gap)
        chip_rect = (cx_l, T, cx_l + chip_w, B)
        draw_chip(canvas, d, chip_rect, name, motif, active, summary)


def draw_chip(canvas, d, rect, name, motif, active, summary):
    L, T, R, B = rect
    radius = 6
    if active is None:
        # ghost / future
        d.rounded_rectangle(rect, radius=radius, fill=(8, 8, 14), outline=rgba(GOLD_DIM, 60), width=1)
        diagonal_hatch(canvas, rect, color=(150, 130, 90, 18), step=10)
        text_centered(d, ((L + R) / 2, (T + B) / 2 - 2), "+ slot", font_reg(12), rgba(GOLD_DIM, 180))
        return
    if active:
        d.rounded_rectangle(rect, radius=radius, fill=(20, 20, 32), outline=rgba(TEAL, 220), width=1)
    else:
        d.rounded_rectangle(rect, radius=radius, fill=(10, 10, 18), outline=rgba(FRAME, 50), width=1)
        diagonal_hatch(canvas, rect, color=(100, 180, 200, 22), step=10)

    # motif on left, label + LED on right
    motif_box = (L + 6, T + 4, L + 50, B - 4)
    motif(d, motif_box, active=bool(active))
    # label and summary stacked
    text_left(d, (motif_box[2] + 8, T + 8), name, font_bold(13), CREAM if active else CREAM_DIM)
    text_left(d, (motif_box[2] + 8, T + 26), summary, font_reg(11), CREAM_DIM if active else rgba(CREAM_DIM, 140))
    # LED upper-right
    led_x, led_y = R - 12, T + 12
    if active:
        for r_h, alpha in [(6, 60), (4, 120)]:
            d.ellipse([led_x - r_h, led_y - r_h, led_x + r_h, led_y + r_h], fill=rgba(TEAL, alpha))
        d.ellipse([led_x - 2.5, led_y - 2.5, led_x + 2.5, led_y + 2.5], fill=TEAL)
    else:
        d.ellipse([led_x - 2.5, led_y - 2.5, led_x + 2.5, led_y + 2.5], fill=(60, 50, 50))


# ----------------------------------------------------------------------- C

def variant_c(canvas, d, layout, hero_rect, triptych_rect, pre_band, post_band):
    """PRE row above hero, POST row below the amp-name divider."""
    hL, hT, hR, hB = hero_rect
    pad = 8

    # PRE sits in the band allocated above the hero
    pre_rect = (hL, pre_band[1] + pad, hR, pre_band[3] - pad)
    # POST sits in the band allocated below (placed by render_mockup beneath amp name)
    post_rect = (hL, post_band[1] + pad, hR, post_band[3] - pad)

    draw_horizontal_section(canvas, d, pre_rect, "PRE", PRE_EFFECTS, show_ghost=False)
    draw_horizontal_section(canvas, d, post_rect, "POST", POST_EFFECTS, show_ghost=True)


def draw_horizontal_section(canvas, d, rect, header, effects, show_ghost=False):
    L, T, R, B = rect
    d.rectangle(rect, fill=HERO_BG, outline=rgba(FRAME, 60), width=1)
    cs = 10
    corner_accent(d, L + 4, T + 4, cs, False, False, rgba(TEAL_DIM, 180))
    corner_accent(d, R - 4, T + 4, cs, True, False, rgba(TEAL_DIM, 180))
    corner_accent(d, L + 4, B - 4, cs, False, True, rgba(TEAL_DIM, 180))
    corner_accent(d, R - 4, B - 4, cs, True, True, rgba(TEAL_DIM, 180))
    # header on left
    text_left(d, (L + 14, (T + B) / 2 - 14), header, font_bold(20), GOLD_DIM)
    # vertical divider
    d.line([(L + 90, T + 8), (L + 90, B - 8)], fill=rgba(TEAL_DIM, 80), width=1)

    inner_l = L + 102
    inner_r = R - 14
    n = len(effects)
    gap = 10
    slot_w = (inner_r - inner_l - gap * (n - 1)) / n
    for i, (name, motif, active, summary) in enumerate(effects):
        sx = inner_l + i * (slot_w + gap)
        sr = (sx, T + 10, sx + slot_w, B - 10)
        draw_horizontal_slot(canvas, d, sr, name, motif, active, summary)


def draw_horizontal_slot(canvas, d, rect, name, motif, active, summary):
    L, T, R, B = rect
    if active is None:
        d.rounded_rectangle(rect, radius=4, fill=(8, 8, 14), outline=rgba(GOLD_DIM, 60), width=1)
        diagonal_hatch(canvas, rect, color=(150, 130, 90, 18), step=10)
        text_centered(d, ((L + R) / 2, (T + B) / 2 - 2), "+ slot", font_reg(12), rgba(GOLD_DIM, 180))
        return
    if active:
        d.rounded_rectangle(rect, radius=4, fill=(18, 18, 28), outline=rgba(TEAL, 220), width=1)
    else:
        d.rounded_rectangle(rect, radius=4, fill=(10, 10, 18), outline=rgba(FRAME, 60), width=1)
        diagonal_hatch(canvas, rect, color=(100, 180, 200, 22), step=10)

    # motif large on top, label + LED on bottom
    motif_box = (L + 6, T + 4, R - 6, T + (B - T) * 0.55)
    motif(d, motif_box, active=bool(active))
    # label
    text_left(d, (L + 10, B - 24), name, font_bold(13), CREAM if active else CREAM_DIM)
    # LED bottom-right
    led_x, led_y = R - 14, B - 14
    if active:
        for r_h, alpha in [(7, 50), (5, 110)]:
            d.ellipse([led_x - r_h, led_y - r_h, led_x + r_h, led_y + r_h], fill=rgba(TEAL, alpha))
        d.ellipse([led_x - 3, led_y - 3, led_x + 3, led_y + 3], fill=TEAL)
    else:
        d.ellipse([led_x - 3, led_y - 3, led_x + 3, led_y + 3], fill=(60, 50, 50))


# ----------------------------------------------------------------------- D

def variant_d(canvas, d, layout, hero_rect, triptych_rect, pre_band, post_band):
    """Hybrid pedalboard mini-rack: side flanking, but slots are pedal-card echoes."""
    hL, hT, hR, hB = hero_rect
    block_w = 178
    # PRE: 3 mini-pedal cards stacked
    by = hT + 8
    bh = (hB - hT) - 16
    n_pre = 3
    gap = 8
    card_h = (bh - gap * (n_pre - 1)) / n_pre
    # Header outside the cards (above)
    pre_x_l = hL - block_w - 18
    pre_x_r = hL - 18
    text_left(d, (pre_x_l + 4, hT - 22), "PRE", font_bold(13), GOLD_DIM)
    for i, (name, motif, active, summary) in enumerate(PRE_EFFECTS):
        cy = by + i * (card_h + gap)
        rect = (pre_x_l, cy, pre_x_r, cy + card_h)
        draw_pedal_mini(canvas, d, rect, name, motif, active, summary)

    # POST: 2 cards, taller (or 3 with a ghost). We'll draw 2 + 1 ghost for symmetry.
    post_x_l = hR + 18
    post_x_r = hR + 18 + block_w
    text_left(d, (post_x_l + 4, hT - 22), "POST", font_bold(13), GOLD_DIM)
    n_post = 3
    card_h_post = (bh - gap * (n_post - 1)) / n_post
    for i, (name, motif, active, summary) in enumerate(POST_EFFECTS):
        cy = by + i * (card_h_post + gap)
        rect = (post_x_l, cy, post_x_r, cy + card_h_post)
        draw_pedal_mini(canvas, d, rect, name, motif, active, summary)


def draw_pedal_mini(canvas, d, rect, name, motif, active, summary):
    L, T, R, B = rect
    if active is None:
        d.rounded_rectangle(rect, radius=6, fill=(8, 8, 14), outline=rgba(GOLD_DIM, 60), width=1)
        cs = 8
        corner_accent(d, L + 4, T + 4, cs, False, False, rgba(GOLD_DIM, 80))
        corner_accent(d, R - 4, T + 4, cs, True, False, rgba(GOLD_DIM, 80))
        corner_accent(d, L + 4, B - 4, cs, False, True, rgba(GOLD_DIM, 80))
        corner_accent(d, R - 4, B - 4, cs, True, True, rgba(GOLD_DIM, 80))
        diagonal_hatch(canvas, rect, color=(150, 130, 90, 18), step=10)
        text_centered(d, ((L + R) / 2, (T + B) / 2 - 8), "+ slot", font_reg(12), rgba(GOLD_DIM, 180))
        text_centered(d, ((L + R) / 2, (T + B) / 2 + 8), "(future)", font_reg(10), rgba(GOLD_DIM, 100))
        return

    border_col = rgba(TEAL, 220) if active else rgba(FRAME, 80)
    fill_col = (20, 20, 32) if active else (10, 10, 18)
    d.rounded_rectangle(rect, radius=6, fill=fill_col, outline=border_col, width=1)
    cs = 8
    corner_accent(d, L + 4, T + 4, cs, False, False, border_col)
    corner_accent(d, R - 4, T + 4, cs, True, False, border_col)
    corner_accent(d, L + 4, B - 4, cs, False, True, border_col)
    corner_accent(d, R - 4, B - 4, cs, True, True, border_col)
    # motif fills upper region
    art_rect = (L + 6, T + 6, R - 6, B - 28)
    motif(d, art_rect, active=bool(active))
    if not active:
        # darken art region a bit
        ov = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
        od = ImageDraw.Draw(ov)
        od.rectangle(art_rect, fill=(12, 12, 18, 170))
        canvas.alpha_composite(ov)
    # name + summary bottom-left, LED bottom-right
    text_left(d, (L + 8, B - 22), name, font_bold(12), CREAM if active else CREAM_DIM)
    text_left(d, (L + 8, B - 9), summary, font_reg(9), CREAM_DIM if active else rgba(CREAM_DIM, 140))
    led_x, led_y = R - 12, B - 12
    if active:
        for r_h, alpha in [(6, 60), (4, 120)]:
            d.ellipse([led_x - r_h, led_y - r_h, led_x + r_h, led_y + r_h], fill=rgba(TEAL, alpha))
        d.ellipse([led_x - 2.5, led_y - 2.5, led_x + 2.5, led_y + 2.5], fill=TEAL)
    else:
        d.ellipse([led_x - 2.5, led_y - 2.5, led_x + 2.5, led_y + 2.5], fill=(60, 50, 50))


# ---------------------------------------------------------------------------
# Compose a full mockup (chrome + hero + variant insert)
# ---------------------------------------------------------------------------


# ===========================================================================
# Style explorations — same Variant A skeleton, four distinct visual treatments
# ===========================================================================
#
# Shared spec across all four:
#   * Block ~178x240 px flanking the hero, vertically centered.
#   * 3 PRE slots + 2 POST slots, same effects, same active state.
#   * Header label with navigation chevron at the top.
#   * Each slot has: motif art + label (navigate target) | divider | toggle (bypass).
# Only the visual language varies.


# ----------------------------------------------------------------- STYLE 1
# "Engraved Brass"  —  Art Deco maximalist. Heavy gold filigree, brass plates,
# corner rivets, brass lever toggles. Reads like the engraved facing of a
# Bakelite / 1930s broadcast console.

BRASS_PLATE = (38, 30, 22)
BRASS_PLATE_LIGHT = (54, 42, 28)
BRASS_FOIL = (228, 188, 110)
BRASS_FOIL_DIM = (158, 128, 78)
BRASS_INK = (52, 36, 22)


def _engraved_hatch(canvas, rect, color=(220, 180, 100, 22), step=4, tilt=1):
    layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    L, T, R, B = rect
    h = B - T
    for d_off in range(int(-h), int(R - L), step):
        if tilt > 0:
            ld.line([(L + d_off, T), (L + d_off + h, B)], fill=color, width=1)
        else:
            ld.line([(R - d_off, T), (R - d_off - h, B)], fill=color, width=1)
    # clip to rect
    mask = Image.new("L", canvas.size, 0)
    md = ImageDraw.Draw(mask)
    md.rectangle(rect, fill=255)
    canvas.paste(layer, (0, 0), mask)


def _rivet(d, cx, cy, r=3, color=BRASS_FOIL, dark=BRASS_INK):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=dark)
    d.ellipse([cx - r + 1, cy - r + 1, cx + r - 1, cy + r - 1], fill=color)
    d.line([(cx - 1, cy), (cx + 1, cy)], fill=dark, width=1)


def _draw_filigree_corner(d, x, y, size, flip_h, flip_v, color):
    dx = -size if flip_h else size
    dy = -size if flip_v else size
    # Outer corner L
    d.line([(x, y), (x + dx, y)], fill=color, width=1)
    d.line([(x, y), (x, y + dy)], fill=color, width=1)
    # Inner parallel pinstripe
    d.line([(x + dx / 4, y + dy / 5), (x + dx * 0.85, y + dy / 5)], fill=color, width=1)
    d.line([(x + dx / 5, y + dy / 4), (x + dx / 5, y + dy * 0.85)], fill=color, width=1)
    # Tiny diamond pip at the inner end
    px = x + dx * 0.85
    py = y + dy * 0.85
    d.line([(px, py - 2), (px + 2, py)], fill=color, width=1)
    d.line([(px + 2, py), (px, py + 2)], fill=color, width=1)
    d.line([(px, py + 2), (px - 2, py)], fill=color, width=1)
    d.line([(px - 2, py), (px, py - 2)], fill=color, width=1)


def _brass_pill(d, center, on, w=26, h=12):
    cx, cy = center
    rect = (cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2)
    # Outer brass cup
    d.rounded_rectangle(rect, radius=h / 2, fill=BRASS_PLATE_LIGHT,
                        outline=BRASS_FOIL_DIM, width=1)
    # Knurled inner pinstripes
    for i in range(int(rect[0]) + 4, int(rect[2]) - 4, 2):
        d.line([(i, cy - h / 2 + 2), (i, cy + h / 2 - 2)], fill=rgba(BRASS_FOIL_DIM, 60), width=1)
    # Lever knob (slides left/right)
    knob_x = rect[2] - h / 2 if on else rect[0] + h / 2
    d.ellipse([knob_x - h / 2 + 2, cy - h / 2 + 2,
               knob_x + h / 2 - 2, cy + h / 2 - 2],
              fill=BRASS_FOIL if on else (90, 70, 42),
              outline=BRASS_INK, width=1)
    # Engraved I/O over the lever bay
    if on:
        d.text((rect[0] + 3, cy - 5), "I", font=font_bold(8), fill=rgba(BRASS_FOIL_DIM, 220))
    else:
        d.text((rect[2] - 8, cy - 5), "O", font=font_bold(8), fill=rgba(BRASS_FOIL_DIM, 220))


def draw_brass_block(canvas, d, rect, header, effects):
    L, T, R, B = rect
    # Plate fill
    d.rectangle(rect, fill=BRASS_PLATE)
    _engraved_hatch(canvas, rect, color=(220, 180, 100, 16), step=3)
    # Double-line frame (outer + inner pinstripe)
    d.rectangle((L, T, R, B), outline=BRASS_FOIL, width=1)
    d.rectangle((L + 4, T + 4, R - 4, B - 4), outline=rgba(BRASS_FOIL_DIM, 220), width=1)
    # Filigree corners
    cs = 14
    _draw_filigree_corner(d, L + 6, T + 6, cs, False, False, BRASS_FOIL)
    _draw_filigree_corner(d, R - 6, T + 6, cs, True, False, BRASS_FOIL)
    _draw_filigree_corner(d, L + 6, B - 6, cs, False, True, BRASS_FOIL)
    _draw_filigree_corner(d, R - 6, B - 6, cs, True, True, BRASS_FOIL)
    # Rivets at the very corners
    _rivet(d, L + 11, T + 11)
    _rivet(d, R - 11, T + 11)
    _rivet(d, L + 11, B - 11)
    _rivet(d, R - 11, B - 11)

    # Header — engraved cartouche
    cart_l, cart_r = L + 26, R - 26
    cart_t, cart_b = T + 12, T + 32
    d.rectangle((cart_l, cart_t, cart_r, cart_b), fill=BRASS_INK, outline=BRASS_FOIL, width=1)
    # decorative pips on either side of header text
    d.line([(cart_l - 6, cart_t + 9), (cart_l - 2, cart_t + 9)], fill=BRASS_FOIL, width=1)
    d.line([(cart_r + 2, cart_t + 9), (cart_r + 6, cart_t + 9)], fill=BRASS_FOIL, width=1)
    fnt_hdr = font_bold(13)
    bbox = d.textbbox((0, 0), header, font=fnt_hdr)
    hw = bbox[2] - bbox[0]
    cx = (cart_l + cart_r) / 2
    text_left(d, (cx - (hw + 12) / 2, cart_t + 4), header, fnt_hdr, BRASS_FOIL)
    chev_x = cx - (hw + 12) / 2 + hw + 6
    chev_y = cart_t + 10
    d.line([(chev_x, chev_y - 3), (chev_x + 3, chev_y), (chev_x, chev_y + 3)], fill=BRASS_FOIL, width=1)

    # Slots
    inner_top = T + 42
    inner_bot = B - 12
    n = len(effects)
    slot_h = (inner_bot - inner_top) / n
    for i, (name, motif, active, _) in enumerate(effects):
        sy = inner_top + i * slot_h
        sr = (L + 12, sy + 4, R - 12, sy + slot_h - 4)
        _draw_brass_slot(canvas, d, sr, name, motif, active)


def _draw_brass_slot(canvas, d, rect, name, motif, active):
    L, T, R, B = rect
    # Plate
    d.rounded_rectangle(rect, radius=2, fill=BRASS_INK,
                        outline=BRASS_FOIL_DIM, width=1)
    # Engraved hatch on bypassed
    if active is False:
        _engraved_hatch(canvas, rect, color=(220, 180, 100, 26), step=3, tilt=-1)
    elif active is None:
        _engraved_hatch(canvas, rect, color=(220, 180, 100, 16), step=3)
    # Hit-zone divider — engraved double hairline
    toggle_zone_w = 36
    nav_right = R - toggle_zone_w
    d.line([(nav_right, T + 3), (nav_right, B - 3)], fill=rgba(BRASS_FOIL_DIM, 180), width=1)
    d.line([(nav_right + 1, T + 3), (nav_right + 1, B - 3)], fill=rgba(BRASS_FOIL_DIM, 80), width=1)

    # Motif (recolor: gold-tinted instead of teal)
    motif_rect = (L + 5, T + 3, L + 5 + (nav_right - L - 10) * 0.55, B - 3)
    # Custom palette monkey-patch is heavy; instead overlay a brass tint
    motif(d, motif_rect, active=bool(active))
    # Brass tint overlay on the motif area
    overlay = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    od.rectangle(motif_rect, fill=(200, 160, 90, 50 if active else 30))
    canvas.alpha_composite(overlay)

    # Label
    label_x = motif_rect[2] + 6
    label_y = (T + B) / 2 - 7
    text_left(d, (label_x, label_y), name, font_bold(12),
              BRASS_FOIL if active else BRASS_FOIL_DIM)

    # Toggle
    tcx = (nav_right + R) / 2
    tcy = (T + B) / 2
    if active is None:
        d.ellipse([tcx - 3, tcy - 3, tcx + 3, tcy + 3], outline=BRASS_FOIL_DIM, width=1)
    else:
        _brass_pill(d, (tcx, tcy), bool(active))


def variant_a_brass(canvas, d, layout, hero_rect, triptych_rect, pre_band, post_band):
    hL, hT, hR, hB = hero_rect
    block_w = 178
    block_h = 240
    by = (hT + hB) / 2 - block_h / 2
    left_block = (hL - block_w - 18, by, hL - 18, by + block_h)
    right_block = (hR + 18, by, hR + 18 + block_w, by + block_h)
    draw_brass_block(canvas, d, left_block, "PRE", PRE_EFFECTS)
    draw_brass_block(canvas, d, right_block, "POST", POST_EFFECTS[:2])


# ----------------------------------------------------------------- STYLE 2
# "Blueprint"  —  schematic / engineering drawing. Hairline-only frames,
# faint cyan engineer's grid, monospace coordinate tags, SPST-symbol toggle.

BP_BG = (8, 12, 18)
BP_GRID = (40, 90, 110, 60)
BP_INK = (140, 220, 230)
BP_INK_DIM = (90, 160, 180)
BP_INK_FAINT = (60, 110, 130)
BP_AMBER = (250, 200, 110)


def _bp_grid(canvas, rect, step=12, color=BP_GRID):
    L, T, R, B = rect
    layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    for x in range(int(L), int(R), step):
        ld.line([(x, T), (x, B)], fill=color, width=1)
    for y in range(int(T), int(B), step):
        ld.line([(L, y), (R, y)], fill=color, width=1)
    # 5-step major lines slightly stronger
    major = (color[0], color[1], color[2], min(255, color[3] + 50))
    for x in range(int(L), int(R), step * 5):
        ld.line([(x, T), (x, B)], fill=major, width=1)
    for y in range(int(T), int(B), step * 5):
        ld.line([(L, y), (R, y)], fill=major, width=1)
    mask = Image.new("L", canvas.size, 0)
    md = ImageDraw.Draw(mask)
    md.rectangle(rect, fill=255)
    canvas.paste(layer, (0, 0), mask)


def _bp_spst(d, center, on, length=22, gap=3):
    """Schematic SPST switch symbol. On = bar horizontal connecting both posts.
    Off = bar tilted up-right disconnecting from right post."""
    cx, cy = center
    L = cx - length / 2
    R = cx + length / 2
    pivot = (L + 3, cy)
    contact = (R - 3, cy)
    # the two terminals (small filled circles)
    for tx, ty in [pivot, contact]:
        d.ellipse([tx - 2, ty - 2, tx + 2, ty + 2], outline=BP_INK, width=1)
    # connecting wires going off frame (visual hint)
    d.line([(L - 3, cy), (pivot[0] - 2, cy)], fill=BP_INK, width=1)
    d.line([(contact[0] + 2, cy), (R + 3, cy)], fill=BP_INK, width=1)
    # the lever
    if on:
        d.line([(pivot[0] + 2, cy), (contact[0] - 2, cy)], fill=BP_AMBER, width=1)
        # closed-state indicator dot
        d.ellipse([cx - 2, cy - 2, cx + 2, cy + 2], fill=BP_AMBER)
    else:
        # tilted up-right (open)
        end_x = contact[0] - 2
        end_y = cy - 8
        d.line([(pivot[0] + 2, cy), (end_x, end_y)], fill=BP_INK_DIM, width=1)


def draw_blueprint_block(canvas, d, rect, header, effects, prefix="A"):
    L, T, R, B = rect
    # Solid block background slightly darker than chrome BG
    d.rectangle(rect, fill=BP_BG)
    _bp_grid(canvas, rect, step=10, color=(70, 130, 150, 38))
    # Hairline frame
    d.rectangle(rect, outline=BP_INK_DIM, width=1)
    # tiny tick marks at midpoints to emulate dimensioning
    d.line([((L + R) / 2 - 4, T - 1), ((L + R) / 2 + 4, T - 1)], fill=BP_INK_DIM, width=1)
    d.line([((L + R) / 2 - 4, B + 1), ((L + R) / 2 + 4, B + 1)], fill=BP_INK_DIM, width=1)
    d.line([(L - 1, (T + B) / 2 - 4), (L - 1, (T + B) / 2 + 4)], fill=BP_INK_DIM, width=1)
    d.line([(R + 1, (T + B) / 2 - 4), (R + 1, (T + B) / 2 + 4)], fill=BP_INK_DIM, width=1)

    # Header — schematic title block
    fnt_hdr = font_mono(11)
    text_left(d, (L + 8, T + 8), f"§ {header}", fnt_hdr, BP_INK)
    # chevron
    bbox = d.textbbox((0, 0), f"§ {header}", font=fnt_hdr)
    cx_after = L + 8 + (bbox[2] - bbox[0]) + 6
    d.line([(cx_after, T + 12 - 3), (cx_after + 4, T + 12), (cx_after, T + 12 + 3)], fill=BP_INK, width=1)
    # Drawing-revision tag at the top right
    text_left(d, (R - 36, T + 8), "REV.A", font_mono(9), BP_INK_FAINT)
    d.line([(L + 6, T + 24), (R - 6, T + 24)], fill=BP_INK_DIM, width=1)

    inner_top = T + 30
    inner_bot = B - 10
    n = len(effects)
    slot_h = (inner_bot - inner_top) / n
    for i, (name, motif, active, _) in enumerate(effects):
        sy = inner_top + i * slot_h
        sr = (L + 8, sy + 3, R - 8, sy + slot_h - 3)
        _draw_blueprint_slot(canvas, d, sr, name, motif, active, f"{prefix}.{i + 1}")


def _draw_blueprint_slot(canvas, d, rect, name, motif, active, coord_tag):
    L, T, R, B = rect
    # transparent fill (just the grid showing through), hairline rectangle
    if active:
        d.rectangle(rect, fill=(20, 35, 50, 200))
    d.rectangle(rect, outline=BP_INK if active else BP_INK_FAINT, width=1)

    # Coordinate tag in the top-left corner — tiny mono
    text_left(d, (L + 4, T + 2), coord_tag, font_mono(8), BP_INK_FAINT)

    if active is False:
        # X across the slot's motif area to indicate "open / disconnected"
        pass  # the tilted SPST already conveys this
    elif active is None:
        text_centered(d, ((L + R) / 2, (T + B) / 2), "// reserved", font_mono(9), BP_AMBER)
        return

    # Hit-zone divider
    toggle_zone_w = 38
    nav_right = R - toggle_zone_w
    # Dashed divider
    seg = 3
    y = T + 4
    while y < B - 4:
        d.line([(nav_right, y), (nav_right, y + seg)], fill=BP_INK_FAINT, width=1)
        y += seg + 2

    # Motif (smaller, line-only feel)
    motif_rect = (L + 8, T + 14, L + 8 + (nav_right - L - 14) * 0.50, B - 6)
    motif(d, motif_rect, active=bool(active))
    # Recolor overlay: drain to teal-cyan (multiply darker)
    overlay = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    od.rectangle(motif_rect, fill=(20, 80, 110, 70 if not active else 30))
    canvas.alpha_composite(overlay)

    # Label
    text_left(d, (motif_rect[2] + 6, (T + B) / 2 - 6), name, font_mono(10), BP_INK if active else BP_INK_DIM)

    # SPST symbol toggle
    _bp_spst(d, ((nav_right + R) / 2, (T + B) / 2), bool(active))


def variant_a_blueprint(canvas, d, layout, hero_rect, triptych_rect, pre_band, post_band):
    hL, hT, hR, hB = hero_rect
    block_w = 178
    block_h = 240
    by = (hT + hB) / 2 - block_h / 2
    left_block = (hL - block_w - 18, by, hL - 18, by + block_h)
    right_block = (hR + 18, by, hR + 18 + block_w, by + block_h)
    draw_blueprint_block(canvas, d, left_block, "PRE", PRE_EFFECTS, prefix="A")
    draw_blueprint_block(canvas, d, right_block, "POST", POST_EFFECTS[:2], prefix="B")


# ----------------------------------------------------------------- STYLE 3
# "Eurorack Module"  —  Anodized metal panels, hex screws top/bottom, patch
# jacks instead of LEDs, and miniature SPDT toggle switches.

EU_PANEL = (190, 195, 200)
EU_PANEL_SHADOW = (140, 144, 150)
EU_PANEL_DARK = (100, 102, 108)
EU_INK = (32, 32, 36)
EU_INK_DIM = (90, 90, 96)
EU_LED_ON = TEAL
EU_LED_OFF = (60, 60, 66)
EU_JACK = (40, 40, 44)
EU_JACK_RING = (220, 220, 224)


def _eu_screw(d, cx, cy, r=4):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=EU_PANEL_SHADOW, outline=EU_INK, width=1)
    d.ellipse([cx - r + 1, cy - r + 1, cx + r - 1, cy + r - 1], fill=EU_PANEL)
    # phillips cross
    d.line([(cx - r + 2, cy), (cx + r - 2, cy)], fill=EU_INK, width=1)
    d.line([(cx, cy - r + 2), (cx, cy + r - 2)], fill=EU_INK, width=1)


def _eu_jack(d, cx, cy, r=5, lit=False):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=EU_JACK_RING)
    d.ellipse([cx - r + 1, cy - r + 1, cx + r - 1, cy + r - 1], fill=EU_JACK)
    inner = r - 2.5
    d.ellipse([cx - inner, cy - inner, cx + inner, cy + inner],
              fill=EU_LED_ON if lit else EU_LED_OFF)


def _eu_spdt(d, center, on, length=20):
    """SPDT toggle: a chrome bezel with a black tip pointing UP (on) or DOWN (off)."""
    cx, cy = center
    bezel_w = 8
    bezel_h = length
    d.rounded_rectangle((cx - bezel_w / 2, cy - bezel_h / 2,
                         cx + bezel_w / 2, cy + bezel_h / 2),
                        radius=bezel_w / 2, fill=EU_PANEL_SHADOW, outline=EU_INK, width=1)
    # toggle tip (chrome cap) pointing up if on, down if off
    cap_h = 7
    if on:
        d.rounded_rectangle((cx - bezel_w / 2 - 1, cy - bezel_h / 2 - 1,
                             cx + bezel_w / 2 + 1, cy - bezel_h / 2 + cap_h),
                            radius=2, fill=EU_PANEL, outline=EU_INK, width=1)
    else:
        d.rounded_rectangle((cx - bezel_w / 2 - 1, cy + bezel_h / 2 - cap_h,
                             cx + bezel_w / 2 + 1, cy + bezel_h / 2 + 1),
                            radius=2, fill=EU_PANEL_DARK, outline=EU_INK, width=1)
    # tiny ON / OFF lithography
    text_left(d, (cx + bezel_w / 2 + 3, cy - bezel_h / 2 + 1),
              "ON", font_mono(7), EU_INK if on else EU_INK_DIM)
    text_left(d, (cx + bezel_w / 2 + 3, cy + bezel_h / 2 - 8),
              "OFF", font_mono(7), EU_INK if not on else EU_INK_DIM)


def draw_eurorack_block(canvas, d, rect, header, effects):
    L, T, R, B = rect
    # Brushed panel — base fill plus subtle vertical streaks
    d.rectangle(rect, fill=EU_PANEL)
    layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    rng = random.Random(7)
    for _ in range(160):
        x = rng.uniform(L + 4, R - 4)
        ld.line([(x, T + 4), (x, B - 4)], fill=(110, 114, 120, 30), width=1)
    # clip
    mask = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(mask).rectangle(rect, fill=255)
    canvas.paste(layer, (0, 0), mask)
    # Outer dark shadow line + inner bevel
    d.rectangle(rect, outline=EU_PANEL_DARK, width=1)
    d.rectangle((L + 2, T + 2, R - 2, B - 2), outline=(220, 224, 230), width=1)
    # Hex screws in 4 corners (canonical Eurorack)
    _eu_screw(d, L + 10, T + 10)
    _eu_screw(d, R - 10, T + 10)
    _eu_screw(d, L + 10, B - 10)
    _eu_screw(d, R - 10, B - 10)

    # Header — black silkscreen band
    band_t, band_b = T + 22, T + 40
    d.rectangle((L + 22, band_t, R - 22, band_b), fill=EU_INK)
    fnt_hdr = font_mono(11)
    bbox = d.textbbox((0, 0), header, font=fnt_hdr)
    hw = bbox[2] - bbox[0]
    cx = (L + R) / 2
    text_left(d, (cx - (hw + 12) / 2, band_t + 4), header, fnt_hdr, EU_PANEL)
    chev_x = cx - (hw + 12) / 2 + hw + 6
    chev_y = band_t + 9
    d.line([(chev_x, chev_y - 3), (chev_x + 3, chev_y), (chev_x, chev_y + 3)], fill=EU_PANEL, width=1)

    # HP-mark below the band
    text_left(d, (L + 14, band_b + 2), "12 HP", font_mono(7), EU_INK_DIM)

    # Slots
    inner_top = band_b + 14
    inner_bot = B - 18
    n = len(effects)
    slot_h = (inner_bot - inner_top) / n
    for i, (name, motif, active, _) in enumerate(effects):
        sy = inner_top + i * slot_h
        sr = (L + 14, sy + 3, R - 14, sy + slot_h - 3)
        _draw_eurorack_slot(canvas, d, sr, name, motif, active)


def _draw_eurorack_slot(canvas, d, rect, name, motif, active):
    L, T, R, B = rect
    # slot well (recessed darker rectangle)
    d.rounded_rectangle(rect, radius=3, fill=(170, 174, 180), outline=EU_PANEL_DARK, width=1)
    # screws on the divider (top + bottom of toggle bay) — tiny
    toggle_zone_w = 36
    nav_right = R - toggle_zone_w
    # Divider line + small pinhole holes
    d.line([(nav_right, T + 4), (nav_right, B - 4)], fill=EU_PANEL_DARK, width=1)
    d.ellipse([nav_right - 1.5, T + 8 - 1.5, nav_right + 1.5, T + 8 + 1.5], fill=EU_INK)
    d.ellipse([nav_right - 1.5, B - 8 - 1.5, nav_right + 1.5, B - 8 + 1.5], fill=EU_INK)

    # Motif region — engraved black silkscreen square
    motif_rect = (L + 5, T + 4, L + 5 + (nav_right - L - 10) * 0.55, B - 4)
    d.rounded_rectangle(motif_rect, radius=2, fill=EU_INK)
    motif(d, motif_rect, active=bool(active))
    # The motif drew teal/blue on default. Recolor: drain saturation.
    overlay = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ImageDraw.Draw(overlay).rectangle(motif_rect, fill=(0, 0, 0, 60 if active else 110))
    canvas.alpha_composite(overlay)

    # Label — silkscreen black on panel
    label_x = motif_rect[2] + 6
    label_y = (T + B) / 2 - 12
    text_left(d, (label_x, label_y), name, font_mono(10), EU_INK if active else EU_INK_DIM)
    # patch jack below label
    _eu_jack(d, label_x + 8, (T + B) / 2 + 6, r=5, lit=bool(active))

    # Toggle — SPDT switch in the toggle bay
    if active is None:
        text_centered(d, ((nav_right + R) / 2, (T + B) / 2 - 4), "FUTURE", font_mono(7), EU_INK_DIM)
        # empty mounting hole
        d.ellipse([(nav_right + R) / 2 - 4, (T + B) / 2 + 2,
                   (nav_right + R) / 2 + 4, (T + B) / 2 + 10], outline=EU_PANEL_DARK, width=1)
        return
    _eu_spdt(d, ((nav_right + R) / 2 - 6, (T + B) / 2), bool(active))


def variant_a_eurorack(canvas, d, layout, hero_rect, triptych_rect, pre_band, post_band):
    hL, hT, hR, hB = hero_rect
    block_w = 178
    block_h = 240
    by = (hT + hB) / 2 - block_h / 2
    left_block = (hL - block_w - 18, by, hL - 18, by + block_h)
    right_block = (hR + 18, by, hR + 18 + block_w, by + block_h)
    draw_eurorack_block(canvas, d, left_block, "PRE", PRE_EFFECTS)
    draw_eurorack_block(canvas, d, right_block, "POST", POST_EFFECTS[:2])


# ----------------------------------------------------------------- STYLE 4
# "Holographic"  —  Modern glassmorphic / synthwave. Translucent fills,
# generous teal glows, flat modern pill switches. Most contemporary read.

HOLO_GLASS = (16, 22, 32)
HOLO_GLASS_LIGHT = (28, 38, 56)
HOLO_TEAL = (94, 230, 220)
HOLO_TEAL_DEEP = (40, 130, 160)
HOLO_MAGENTA = (220, 110, 200)
HOLO_INK = (220, 240, 250)
HOLO_INK_DIM = (130, 165, 195)


def _holo_glow(canvas, rect, color, radius=18, intensity=80):
    """Soft outer glow rectangle."""
    L, T, R, B = rect
    g = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(g)
    pad_rect = (L - radius, T - radius, R + radius, B + radius)
    gd.rounded_rectangle(pad_rect, radius=radius * 1.4,
                         fill=(color[0], color[1], color[2], intensity))
    g = g.filter(ImageFilter.GaussianBlur(radius))
    canvas.alpha_composite(g)


def _holo_pill(d, center, on, w=26, h=12):
    cx, cy = center
    rect = (cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2)
    if on:
        d.rounded_rectangle(rect, radius=h / 2, fill=rgba(HOLO_TEAL, 220))
        knob_x = rect[2] - h / 2
        d.ellipse([knob_x - h / 2 + 1.5, cy - h / 2 + 1.5,
                   knob_x + h / 2 - 1.5, cy + h / 2 - 1.5],
                  fill=(8, 16, 24))
    else:
        d.rounded_rectangle(rect, radius=h / 2, fill=(28, 36, 48), outline=rgba(HOLO_INK_DIM, 140), width=1)
        knob_x = rect[0] + h / 2
        d.ellipse([knob_x - h / 2 + 1.5, cy - h / 2 + 1.5,
                   knob_x + h / 2 - 1.5, cy + h / 2 - 1.5],
                  fill=rgba(HOLO_INK_DIM, 200))


def draw_holo_block(canvas, d, rect, header, effects):
    L, T, R, B = rect
    # Outer soft glow (very subtle, magenta-teal duo)
    _holo_glow(canvas, rect, HOLO_TEAL_DEEP, radius=22, intensity=22)
    # Translucent glass plate
    plate = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    pd = ImageDraw.Draw(plate)
    pd.rounded_rectangle(rect, radius=10, fill=(*HOLO_GLASS, 200))
    canvas.alpha_composite(plate)
    # 1px gradient-y top highlight (manual gradient using a few lines)
    for i, alpha in enumerate([180, 120, 60, 30]):
        d.line([(L + 8 + i, T + 1 + i), (R - 8 - i, T + 1 + i)],
               fill=(255, 255, 255, alpha), width=1)
    # Outer hairline border with cyan tinge
    d.rounded_rectangle(rect, radius=10, outline=rgba(HOLO_TEAL, 140), width=1)

    # Header
    fnt_hdr = font_bold(13)
    bbox = d.textbbox((0, 0), header, font=fnt_hdr)
    hw = bbox[2] - bbox[0]
    cx = (L + R) / 2
    text_left(d, (cx - (hw + 12) / 2, T + 12), header, fnt_hdr, HOLO_TEAL)
    chev_x = cx - (hw + 12) / 2 + hw + 6
    chev_y = T + 19
    d.line([(chev_x, chev_y - 3), (chev_x + 3, chev_y), (chev_x, chev_y + 3)], fill=HOLO_TEAL, width=1)
    # Underline as a thin gradient bar
    bar_y = T + 32
    for i in range(int(R - L - 32)):
        a = int(180 * math.sin(i / max(1, R - L - 32) * math.pi))
        d.point((L + 16 + i, bar_y), fill=(94, 230, 220, max(40, a)))

    inner_top = T + 38
    inner_bot = B - 12
    n = len(effects)
    slot_h = (inner_bot - inner_top) / n
    for i, (name, motif, active, _) in enumerate(effects):
        sy = inner_top + i * slot_h
        sr = (L + 10, sy + 4, R - 10, sy + slot_h - 4)
        _draw_holo_slot(canvas, d, sr, name, motif, active)


def _draw_holo_slot(canvas, d, rect, name, motif, active):
    L, T, R, B = rect
    if active:
        _holo_glow(canvas, rect, HOLO_TEAL, radius=14, intensity=52)
    # Glass slot fill
    slot_layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    sld = ImageDraw.Draw(slot_layer)
    if active:
        sld.rounded_rectangle(rect, radius=6, fill=(36, 60, 76, 170))
    else:
        sld.rounded_rectangle(rect, radius=6, fill=(20, 28, 40, 150))
    canvas.alpha_composite(slot_layer)
    if active is False:
        # very subtle scan lines for bypass
        layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
        lld = ImageDraw.Draw(layer)
        for y in range(int(T + 4), int(B - 4), 3):
            lld.line([(L + 3, y), (R - 3, y)], fill=(60, 100, 140, 30), width=1)
        mask = Image.new("L", canvas.size, 0)
        ImageDraw.Draw(mask).rounded_rectangle(rect, radius=6, fill=255)
        canvas.paste(layer, (0, 0), mask)
    # 1px frame
    d.rounded_rectangle(rect, radius=6,
                        outline=rgba(HOLO_TEAL, 200) if active else rgba(HOLO_INK_DIM, 80),
                        width=1)

    # Hit-zone divider — subtle vertical gradient
    toggle_zone_w = 36
    nav_right = R - toggle_zone_w
    d.line([(nav_right, T + 5), (nav_right, B - 5)], fill=rgba(HOLO_INK_DIM, 90), width=1)

    # Motif (keep teal palette, add brightness when active)
    motif_rect = (L + 6, T + 4, L + 6 + (nav_right - L - 12) * 0.55, B - 4)
    motif(d, motif_rect, active=bool(active))

    # Label
    label_x = motif_rect[2] + 6
    label_y = (T + B) / 2 - 7
    text_left(d, (label_x, label_y), name, font_bold(12),
              HOLO_INK if active else HOLO_INK_DIM)

    # Toggle
    tcx = (nav_right + R) / 2
    tcy = (T + B) / 2
    if active is None:
        d.ellipse([tcx - 3, tcy - 3, tcx + 3, tcy + 3], outline=rgba(HOLO_MAGENTA, 200), width=1)
    else:
        _holo_pill(d, (tcx, tcy), bool(active))


def variant_a_holo(canvas, d, layout, hero_rect, triptych_rect, pre_band, post_band):
    hL, hT, hR, hB = hero_rect
    block_w = 178
    block_h = 240
    by = (hT + hB) / 2 - block_h / 2
    left_block = (hL - block_w - 18, by, hL - 18, by + block_h)
    right_block = (hR + 18, by, hR + 18 + block_w, by + block_h)
    draw_holo_block(canvas, d, left_block, "PRE", PRE_EFFECTS)
    draw_holo_block(canvas, d, right_block, "POST", POST_EFFECTS[:2])


# Mark these new variants as 'a-shaped' for layout — no banding
for _key in ["a_brass", "a_blueprint", "a_eurorack", "a_holo"]:
    VARIANT_PRE_HEIGHT[_key] = 0
    VARIANT_POST_HEIGHT[_key] = 0


# ===========================================================================
# Alts — strict VoLum-language refinements of variant A.
# No new colors, no new materials. Each varies one specific design LEVER.
#
# Shared constants:
BLOCK_W_ALT = 178
BLOCK_H_ALT = 240


# ----------------------------------------------------------------- ALT 1
# "Quiet"  —  remove per-slot frames; let the block panel contain the slots
# without internal subdivisions. Active state is signalled by a 3 px teal
# edge bar on the left of the slot. Bypass = ghosted motif and dim label,
# without the noisy diagonal hatch.

def _draw_quiet_block(canvas, d, rect, header, effects):
    L, T, R, B = rect
    # Block: same as original
    d.rectangle(rect, fill=HERO_BG, outline=rgba(FRAME, 70), width=1)
    cs = 10
    corner_accent(d, L + 4, T + 4, cs, False, False, rgba(TEAL_DIM, 200))
    corner_accent(d, R - 4, T + 4, cs, True, False, rgba(TEAL_DIM, 200))
    corner_accent(d, L + 4, B - 4, cs, False, True, rgba(TEAL_DIM, 200))
    corner_accent(d, R - 4, B - 4, cs, True, True, rgba(TEAL_DIM, 200))
    # Header — same chevron treatment
    fnt_hdr = font_bold(14)
    bbox = d.textbbox((0, 0), header, font=fnt_hdr)
    hw = bbox[2] - bbox[0]
    cx = (L + R) / 2
    text_left(d, (cx - (hw + 12) / 2, T + 9), header, fnt_hdr, GOLD_DIM)
    chev_x = cx - (hw + 12) / 2 + hw + 6
    chev_y = T + 16
    d.line([(chev_x, chev_y - 4), (chev_x + 4, chev_y), (chev_x, chev_y + 4)],
           fill=rgba(GOLD_DIM, 220), width=1)
    d.line([(L + 16, T + 28), (R - 16, T + 28)], fill=rgba(TEAL_DIM, 100), width=1)

    # Slots — frameless, separated by a single hairline.
    inner_top = T + 32
    inner_bot = B - 8
    n = len(effects)
    slot_h = (inner_bot - inner_top) / n
    for i, (name, motif, active, _) in enumerate(effects):
        sy_top = inner_top + i * slot_h
        sy_bot = sy_top + slot_h
        # hairline separator below this slot (skip the last)
        if i < n - 1:
            d.line([(L + 14, sy_bot), (R - 14, sy_bot)], fill=rgba(FRAME, 50), width=1)
        sr = (L + 8, sy_top + 2, R - 8, sy_bot - 2)
        _draw_quiet_slot(canvas, d, sr, name, motif, active)


def _draw_quiet_slot(canvas, d, rect, name, motif, active):
    L, T, R, B = rect
    # Active edge bar on the left
    if active:
        d.rectangle((L, T + 4, L + 3, B - 4), fill=TEAL)
    elif active is None:
        # ghost: a faint gold-dim dotted edge bar
        for y in range(int(T + 4), int(B - 4), 4):
            d.line([(L + 1, y), (L + 1, y + 2)], fill=rgba(GOLD_DIM, 120), width=1)

    # Hit-zone divider (very subtle — almost invisible)
    toggle_zone_w = 32
    nav_right = R - toggle_zone_w
    d.line([(nav_right, T + 8), (nav_right, B - 8)], fill=rgba(FRAME, 40), width=1)

    # Motif
    motif_inset_l = L + 12  # leave room for the edge bar
    motif_rect = (motif_inset_l, T + 4, motif_inset_l + (nav_right - motif_inset_l - 8) * 0.50, B - 4)
    motif(d, motif_rect, active=bool(active))

    # Label
    label_x = motif_rect[2] + 8
    label_y = (T + B) / 2 - 8
    text_left(d, (label_x, label_y), name, font_bold(13),
              CREAM if active else CREAM_DIM)

    # Toggle
    tcx = (nav_right + R) / 2
    tcy = (T + B) / 2
    if active is None:
        d.ellipse([tcx - 3, tcy - 3, tcx + 3, tcy + 3], outline=rgba(GOLD_DIM, 140), width=1)
    else:
        draw_mini_pill(canvas, d, (tcx, tcy), bool(active))


def variant_a_quiet(canvas, d, layout, hero_rect, triptych_rect, pre_band, post_band):
    hL, hT, hR, hB = hero_rect
    by = (hT + hB) / 2 - BLOCK_H_ALT / 2
    left_block = (hL - BLOCK_W_ALT - 18, by, hL - 18, by + BLOCK_H_ALT)
    right_block = (hR + 18, by, hR + 18 + BLOCK_W_ALT, by + BLOCK_H_ALT)
    _draw_quiet_block(canvas, d, left_block, "PRE", PRE_EFFECTS)
    _draw_quiet_block(canvas, d, right_block, "POST", POST_EFFECTS[:2])


# ----------------------------------------------------------------- ALT 2
# "Anchored"  —  the header is treated as a nameplate that echoes the
# Diezel Herbert Mk1 strip below the hero (same gold display face, same
# thin underline + tiny diamond pip). The block frame is corner-accents-only
# (matches the hero's frame language). Slots keep their per-slot frames.

def _draw_anchored_nameplate(d, rect, header):
    L, T, R, B = rect
    cx = (L + R) / 2
    cy = (T + B) / 2
    # Display-face label, slightly larger than original header
    fnt = font_disp(20)
    text_centered(d, (cx, cy - 4), header, fnt, GOLD)
    # underline + diamond pip echoing the amp-name divider
    line_w = (R - L) - 24
    d.line([(cx - line_w / 2, cy + 8), (cx + line_w / 2, cy + 8)],
           fill=rgba(TEAL_DIM, 200), width=1)
    pip = [(cx, cy + 4), (cx + 4, cy + 8), (cx, cy + 12), (cx - 4, cy + 8)]
    d.polygon(pip, outline=rgba(TEAL_DIM, 220))
    # navigation chevron, hung off the right end of the underline
    chev_x = cx + line_w / 2 + 4
    d.line([(chev_x, cy + 5), (chev_x + 4, cy + 8), (chev_x, cy + 11)],
           fill=rgba(GOLD_DIM, 220), width=1)


def _draw_anchored_block(canvas, d, rect, header, effects):
    L, T, R, B = rect
    # Block: HERO_BG fill + corner accents only (no rectangle outline).
    d.rectangle(rect, fill=HERO_BG)
    cs = 14
    corner_accent(d, L + 2, T + 2, cs, False, False, rgba(TEAL_DIM, 220))
    corner_accent(d, R - 2, T + 2, cs, True, False, rgba(TEAL_DIM, 220))
    corner_accent(d, L + 2, B - 2, cs, False, True, rgba(TEAL_DIM, 220))
    corner_accent(d, R - 2, B - 2, cs, True, True, rgba(TEAL_DIM, 220))

    # Nameplate header (~22 px tall)
    plate_rect = (L + 4, T + 6, R - 4, T + 36)
    _draw_anchored_nameplate(d, plate_rect, header)

    # Slots
    inner_top = T + 44
    inner_bot = B - 8
    n = len(effects)
    slot_h = (inner_bot - inner_top) / n
    for i, (name, motif, active, _) in enumerate(effects):
        sy = inner_top + i * slot_h
        sr = (L + 8, sy + 4, R - 8, sy + slot_h - 4)
        _draw_anchored_slot(canvas, d, sr, name, motif, active)


def _draw_anchored_slot(canvas, d, rect, name, motif, active):
    L, T, R, B = rect
    # Slot frame: same as original refined
    sub_bg = (20, 20, 32) if active else (10, 10, 16)
    d.rectangle(rect, fill=sub_bg)
    border_col = rgba(TEAL, 220) if active else rgba(FRAME, 60)
    d.rectangle(rect, outline=border_col, width=1)
    if active is False:
        diagonal_hatch(canvas, rect, color=(100, 180, 200, 24), step=10)
    elif active is None:
        diagonal_hatch(canvas, rect, color=(150, 130, 90, 22), step=12)

    toggle_zone_w = 34
    nav_right = R - toggle_zone_w
    d.line([(nav_right, T + 4), (nav_right, B - 4)], fill=rgba(FRAME, 70), width=1)

    motif_rect = (L + 6, T + 4, L + 6 + (nav_right - L - 12) * 0.55, B - 4)
    motif(d, motif_rect, active=bool(active))

    label_x = motif_rect[2] + 6
    label_y = (T + B) / 2 - 8
    text_left(d, (label_x, label_y), name, font_bold(13),
              CREAM if active else CREAM_DIM)

    tcx = (nav_right + R) / 2
    tcy = (T + B) / 2
    if active is None:
        d.ellipse([tcx - 3, tcy - 3, tcx + 3, tcy + 3], outline=rgba(GOLD_DIM, 140), width=1)
    else:
        draw_mini_pill(canvas, d, (tcx, tcy), bool(active))


def variant_a_anchored(canvas, d, layout, hero_rect, triptych_rect, pre_band, post_band):
    hL, hT, hR, hB = hero_rect
    by = (hT + hB) / 2 - BLOCK_H_ALT / 2
    left_block = (hL - BLOCK_W_ALT - 18, by, hL - 18, by + BLOCK_H_ALT)
    right_block = (hR + 18, by, hR + 18 + BLOCK_W_ALT, by + BLOCK_H_ALT)
    _draw_anchored_block(canvas, d, left_block, "PRE", PRE_EFFECTS)
    _draw_anchored_block(canvas, d, right_block, "POST", POST_EFFECTS[:2])


# ----------------------------------------------------------------- ALT 3
# "Hierarchical"  —  active slot expanded; bypassed slots compressed.
# Visual area maps directly to "what's currently doing something". Same
# materials, same toggle, only the slot heights change.

def _draw_hier_block(canvas, d, rect, header, effects):
    L, T, R, B = rect
    d.rectangle(rect, fill=HERO_BG, outline=rgba(FRAME, 70), width=1)
    cs = 10
    corner_accent(d, L + 4, T + 4, cs, False, False, rgba(TEAL_DIM, 200))
    corner_accent(d, R - 4, T + 4, cs, True, False, rgba(TEAL_DIM, 200))
    corner_accent(d, L + 4, B - 4, cs, False, True, rgba(TEAL_DIM, 200))
    corner_accent(d, R - 4, B - 4, cs, True, True, rgba(TEAL_DIM, 200))
    # Header
    fnt_hdr = font_bold(14)
    bbox = d.textbbox((0, 0), header, font=fnt_hdr)
    hw = bbox[2] - bbox[0]
    cx = (L + R) / 2
    text_left(d, (cx - (hw + 12) / 2, T + 9), header, fnt_hdr, GOLD_DIM)
    chev_x = cx - (hw + 12) / 2 + hw + 6
    chev_y = T + 16
    d.line([(chev_x, chev_y - 4), (chev_x + 4, chev_y), (chev_x, chev_y + 4)],
           fill=rgba(GOLD_DIM, 220), width=1)
    d.line([(L + 16, T + 28), (R - 16, T + 28)], fill=rgba(TEAL_DIM, 100), width=1)

    # Compute weighted heights: active=2.0, bypass=1.0, ghost=0.85
    weights = []
    for _, _, active, _ in effects:
        if active is True:
            weights.append(2.0)
        elif active is None:
            weights.append(0.85)
        else:
            weights.append(1.0)
    total = sum(weights)
    inner_top = T + 36
    inner_bot = B - 12
    avail = inner_bot - inner_top
    y = inner_top
    for (name, motif, active, _), w in zip(effects, weights):
        h = avail * (w / total)
        sr = (L + 8, y + 3, R - 8, y + h - 3)
        _draw_hier_slot(canvas, d, sr, name, motif, active)
        y += h


def _draw_hier_slot(canvas, d, rect, name, motif, active):
    L, T, R, B = rect
    sub_bg = (20, 20, 32) if active else (10, 10, 16)
    d.rectangle(rect, fill=sub_bg)
    border_col = rgba(TEAL, 220) if active else rgba(FRAME, 50)
    d.rectangle(rect, outline=border_col, width=1)
    if active is False:
        diagonal_hatch(canvas, rect, color=(100, 180, 200, 22), step=10)
    elif active is None:
        diagonal_hatch(canvas, rect, color=(150, 130, 90, 20), step=12)

    toggle_zone_w = 34
    nav_right = R - toggle_zone_w
    d.line([(nav_right, T + 4), (nav_right, B - 4)], fill=rgba(FRAME, 60), width=1)

    # When the slot is taller (active), give the motif more vertical room
    motif_rect = (L + 6, T + 4, L + 6 + (nav_right - L - 12) * 0.55, B - 4)
    motif(d, motif_rect, active=bool(active))

    label_x = motif_rect[2] + 6
    label_y = (T + B) / 2 - 8
    label_size = 14 if active else 12
    text_left(d, (label_x, label_y), name, font_bold(label_size),
              CREAM if active else CREAM_DIM)

    tcx = (nav_right + R) / 2
    tcy = (T + B) / 2
    if active is None:
        d.ellipse([tcx - 3, tcy - 3, tcx + 3, tcy + 3], outline=rgba(GOLD_DIM, 140), width=1)
    else:
        draw_mini_pill(canvas, d, (tcx, tcy), bool(active))


def variant_a_hierarchical(canvas, d, layout, hero_rect, triptych_rect, pre_band, post_band):
    hL, hT, hR, hB = hero_rect
    by = (hT + hB) / 2 - BLOCK_H_ALT / 2
    left_block = (hL - BLOCK_W_ALT - 18, by, hL - 18, by + BLOCK_H_ALT)
    right_block = (hR + 18, by, hR + 18 + BLOCK_W_ALT, by + BLOCK_H_ALT)
    _draw_hier_block(canvas, d, left_block, "PRE", PRE_EFFECTS)
    _draw_hier_block(canvas, d, right_block, "POST", POST_EFFECTS[:2])


# ----------------------------------------------------------------- ALT 4
# "Switchblade"  —  per-slot toggles unified into a single column flush
# right of the block. The slots themselves contain only motif + label,
# making the navigation hit zone larger and clearer. The toggle column
# reads as a coherent "bypass strip" — like a power-strip with named
# outlets running down the side.

def _draw_switch_block(canvas, d, rect, header, effects):
    L, T, R, B = rect
    bay_w = 26
    slots_R = R - bay_w
    slots_rect = (L, T, slots_R, B)
    bay_rect = (slots_R, T, R, B)

    # Slots panel
    d.rectangle(slots_rect, fill=HERO_BG, outline=rgba(FRAME, 70), width=1)
    cs = 10
    corner_accent(d, L + 4, T + 4, cs, False, False, rgba(TEAL_DIM, 200))
    corner_accent(d, slots_R - 4, T + 4, cs, True, False, rgba(TEAL_DIM, 200))
    corner_accent(d, L + 4, B - 4, cs, False, True, rgba(TEAL_DIM, 200))
    corner_accent(d, slots_R - 4, B - 4, cs, True, True, rgba(TEAL_DIM, 200))

    # Header on the slots panel
    fnt_hdr = font_bold(14)
    bbox = d.textbbox((0, 0), header, font=fnt_hdr)
    hw = bbox[2] - bbox[0]
    cx = (L + slots_R) / 2
    text_left(d, (cx - (hw + 12) / 2, T + 9), header, fnt_hdr, GOLD_DIM)
    chev_x = cx - (hw + 12) / 2 + hw + 6
    chev_y = T + 16
    d.line([(chev_x, chev_y - 4), (chev_x + 4, chev_y), (chev_x, chev_y + 4)],
           fill=rgba(GOLD_DIM, 220), width=1)
    d.line([(L + 16, T + 28), (slots_R - 16, T + 28)], fill=rgba(TEAL_DIM, 100), width=1)

    # Toggle bay panel (slightly darker, hairline left edge already from slots panel)
    d.rectangle(bay_rect, fill=(8, 8, 14), outline=rgba(FRAME, 70), width=1)
    # bay header label "BYP" rotated would be ideal; keep it simple — vertical letter stack
    bay_cx = (slots_R + R) / 2
    text_centered(d, (bay_cx, T + 14), "B", font_bold(9), GOLD_DIM)
    text_centered(d, (bay_cx, T + 22), "Y", font_bold(9), GOLD_DIM)
    text_centered(d, (bay_cx, T + 30), "P", font_bold(9), GOLD_DIM)

    # Slots — content-only (motif + label), no toggle inside
    inner_top = T + 36
    inner_bot = B - 12
    n = len(effects)
    slot_h = (inner_bot - inner_top) / n
    for i, (name, motif, active, _) in enumerate(effects):
        sy_top = inner_top + i * slot_h
        sy_bot = sy_top + slot_h
        sr = (L + 8, sy_top + 3, slots_R - 8, sy_bot - 3)
        _draw_switch_slot(canvas, d, sr, name, motif, active)
        # corresponding pill in the bay, vertically aligned with this slot
        bay_cy = (sy_top + sy_bot) / 2
        if active is None:
            d.ellipse([bay_cx - 3, bay_cy - 3, bay_cx + 3, bay_cy + 3],
                      outline=rgba(GOLD_DIM, 140), width=1)
        else:
            # vertical alignment uses the same horizontal pill (compact: 18x9)
            draw_mini_pill(canvas, d, (bay_cx, bay_cy), bool(active), w=18, h=9)


def _draw_switch_slot(canvas, d, rect, name, motif, active):
    L, T, R, B = rect
    sub_bg = (20, 20, 32) if active else (10, 10, 16)
    d.rectangle(rect, fill=sub_bg)
    border_col = rgba(TEAL, 220) if active else rgba(FRAME, 60)
    d.rectangle(rect, outline=border_col, width=1)
    if active is False:
        diagonal_hatch(canvas, rect, color=(100, 180, 200, 24), step=10)
    elif active is None:
        diagonal_hatch(canvas, rect, color=(150, 130, 90, 22), step=12)

    # No internal divider — full slot width is the navigation hit zone
    motif_rect = (L + 6, T + 4, L + 6 + (R - L - 12) * 0.45, B - 4)
    motif(d, motif_rect, active=bool(active))
    label_x = motif_rect[2] + 8
    label_y = (T + B) / 2 - 8
    text_left(d, (label_x, label_y), name, font_bold(13),
              CREAM if active else CREAM_DIM)


def variant_a_switchblade(canvas, d, layout, hero_rect, triptych_rect, pre_band, post_band):
    hL, hT, hR, hB = hero_rect
    by = (hT + hB) / 2 - BLOCK_H_ALT / 2
    left_block = (hL - BLOCK_W_ALT - 18, by, hL - 18, by + BLOCK_H_ALT)
    right_block = (hR + 18, by, hR + 18 + BLOCK_W_ALT, by + BLOCK_H_ALT)
    _draw_switch_block(canvas, d, left_block, "PRE", PRE_EFFECTS)
    _draw_switch_block(canvas, d, right_block, "POST", POST_EFFECTS[:2])


for _key in ["a_quiet", "a_anchored", "a_hierarchical", "a_switchblade"]:
    VARIANT_PRE_HEIGHT[_key] = 0
    VARIANT_POST_HEIGHT[_key] = 0


def render_mockup(variant_fn: Callable, variant_key: str, label: str) -> Image.Image:
    layout = Layout(canvas_w=CW, canvas_h=CH)
    img = Image.new("RGBA", (CW, CH), BG + (255,))
    d = ImageDraw.Draw(img)

    # base background
    d.rectangle([0, 0, CW, CH], fill=BG + (255,))

    draw_titlebar(img, d, layout)
    draw_sidebar(img, d, layout)
    draw_speaker_row(img, d, layout)

    # Main column geometry
    main_l = layout.sidebar_w + 20
    main_r = layout.canvas_w - 20
    main_cx = (main_l + main_r) / 2
    main_top = layout.titlebar_h + layout.speaker_row_h + 16

    # Per-variant vertical claims above/below hero
    pre_h = VARIANT_PRE_HEIGHT.get(variant_key, 0)
    post_h = VARIANT_POST_HEIGHT.get(variant_key, 0)

    # Hero stays a constant size; we shift its top down by pre_h, and consume post_h
    # from the gap between amp name and the knob row.
    hero_w = 800
    hero_h = 380 if (pre_h + post_h) == 0 else 320  # shrink slightly when banded variants need space
    hero_t = main_top + 10 + pre_h + (8 if pre_h else 0)
    hero_rect = (main_cx - hero_w / 2, hero_t, main_cx + hero_w / 2, hero_t + hero_h)

    pre_band = (main_l, main_top + 6, main_r, main_top + 6 + pre_h) if pre_h else None

    # Draw hero first (so PRE/POST overlay frames don't get clobbered)
    draw_hero_block(img, d, hero_rect)

    # Amp name strip directly below hero
    name_rect = (hero_rect[0], hero_rect[3] + 6, hero_rect[2], hero_rect[3] + 60)
    draw_amp_name_block(img, d, name_rect)

    post_band = (main_l, name_rect[3] + 6, main_r, name_rect[3] + 6 + post_h) if post_h else None

    # Knob row (shifts down by post_h)
    knob_top = name_rect[3] + 18 + (post_h + 8 if post_h else 0)
    knob_rect = (main_l, knob_top, main_r, knob_top + layout.knob_row_h)
    draw_knob_row(img, d, knob_rect)

    # Toggle row
    toggle_rect = (main_l, knob_rect[3] + 4, main_r, knob_rect[3] + 4 + layout.toggle_row_h)
    draw_toggle_row(img, d, toggle_rect)

    # Footer
    footer_rect = (main_l, layout.canvas_h - layout.footer_h, main_r, layout.canvas_h)
    draw_footer(img, d, footer_rect)

    # Variant insert
    triptych_rect = (main_l, main_top, main_r, hero_rect[3] + 60)
    variant_fn(img, d, layout, hero_rect, triptych_rect, pre_band, post_band)

    # Variant label badge bottom-left of canvas
    bl_x, bl_y = 16, layout.canvas_h - 28
    bbox = d.textbbox((0, 0), label, font=font_bold(20))
    pad = 8
    plate = (bl_x - 2, bl_y - 4, bl_x + (bbox[2] - bbox[0]) + pad * 2, bl_y + 28)
    d.rounded_rectangle(plate, radius=4, fill=(28, 28, 38), outline=rgba(GOLD_DIM, 180), width=1)
    text_left(d, (bl_x + pad, bl_y - 1), label, font_bold(20), GOLD)

    return img


def downsample(img: Image.Image, target_size) -> Image.Image:
    return img.resize(target_size, Image.LANCZOS)


def render_interaction_annotation():
    """Zoomed view of variant A's PRE block, annotated with click zones."""
    W_, H_ = 1200, 900
    img = Image.new("RGBA", (W_, H_), BG + (255,))
    d = ImageDraw.Draw(img)

    # Title
    text_left(d, (40, 28), "Variant A  ·  Interaction Model", font_disp(38), GOLD)
    text_left(d, (40, 76), "Two click targets per slot, plus the section header", font_mono(13), CREAM_DIM)

    # Render an enlarged PRE block centered-left
    block_w = 380
    block_h = 460
    bx = 90
    by = 130
    block_rect = (bx, by, bx + block_w, by + block_h)
    # We need scaled-up effect drawing — call draw_effect_block then overlay annotations
    # Temporarily monkey-patch font sizes by using a local effect set + redraw.
    # Easier: call the existing draw_effect_block (it scales by rect size for slots).
    draw_effect_block(img, d, block_rect, "PRE", PRE_EFFECTS)

    # Compute slot positions inside the block (must match draw_effect_block math)
    inner_top = by + 36
    inner_bot = by + block_h - 12
    n = len(PRE_EFFECTS)
    slot_h = (inner_bot - inner_top) / n
    slot_rects = []
    for i in range(n):
        sy = inner_top + i * slot_h
        slot_rects.append((bx + 8, sy + 4, bx + block_w - 8, sy + slot_h - 4))

    # Annotations to the right
    ax = bx + block_w + 80
    line_col = rgba(GOLD_DIM, 220)
    text_col = CREAM
    sub_col = CREAM_DIM

    def callout(target, label_anchor, title, body):
        tx, ty = target
        lx, ly = label_anchor
        # leader line: target -> elbow -> anchor
        elbow_x = (tx + lx) / 2
        d.line([(tx, ty), (elbow_x, ty)], fill=line_col, width=1)
        d.line([(elbow_x, ty), (elbow_x, ly)], fill=line_col, width=1)
        d.line([(elbow_x, ly), (lx, ly)], fill=line_col, width=1)
        # tick on target
        d.ellipse([tx - 3, ty - 3, tx + 3, ty + 3], outline=line_col, width=1)
        # label box
        text_left(d, (lx + 10, ly - 12), title, font_bold(15), text_col)
        text_left(d, (lx + 10, ly + 8), body, font_reg(12), sub_col)

    # 1. Header chevron — clicking PRE label/chevron opens the section
    hdr_target = (bx + block_w / 2 + 26, by + 16)
    callout(
        hdr_target,
        (ax, by + 8),
        "Click  PRE ›",
        "Open PRE section. Focus the active effect\n(or NAM 1 here, since it's on); fall back\nto first slot if nothing is active.",
    )

    # 2. Click on motif/label area (use slot 1 = NAM 1, the active one)
    s1 = slot_rects[1]
    nav_target = ((s1[0] + s1[2] - 34) / 2 + s1[0] / 2, (s1[1] + s1[3]) / 2)  # bit of a hack, use mid-left
    nav_target = (s1[0] + (s1[2] - s1[0] - 34) * 0.4, (s1[1] + s1[3]) / 2)
    callout(
        nav_target,
        (ax, by + 130),
        "Click  art / label",
        "Open PRE section with THIS effect\nfocused (e.g. NAM 1 capture browser).\nNavigation zone covers ~70% of the slot.",
    )

    # 3. Toggle pill on slot 0 (COMP, currently off)
    s0 = slot_rects[0]
    toggle_target = (s0[2] - 17, (s0[1] + s0[3]) / 2)
    callout(
        toggle_target,
        (ax, by + 250),
        "Click  toggle pill",
        "Bypass / un-bypass this effect inline.\nDoes NOT navigate. Same vocabulary as\nthe NOISE GATE / EQ pills below.",
    )

    # 4. Hairline divider between zones
    div_target = (s0[2] - 34, (s0[1] + s0[3]) / 2)
    callout(
        div_target,
        (ax, by + 370),
        "Hairline divider",
        "Separates navigation zone from toggle\nzone so the two hit targets read as\ndistinct without extra chrome.",
    )

    # State legend at the bottom
    ly = H_ - 220
    text_left(d, (40, ly), "State Vocabulary", font_disp(28), GOLD)
    legend_items = [
        ("active",   "teal frame  +  saturated motif  +  gold pill toggled right (LED dot embedded in knob)"),
        ("bypassed", "muted frame  +  ghosted motif  +  diagonal hatch overlay  +  cream-dim pill toggled left"),
        ("future",   "gold-dim frame  +  hatch  +  '+ slot' label  (POST-only, no toggle until populated)"),
    ]
    for i, (state, desc) in enumerate(legend_items):
        y = ly + 50 + i * 32
        # state pill
        text_left(d, (60, y - 8), state.upper(), font_bold(13), GOLD_DIM)
        text_left(d, (180, y - 7), desc, font_reg(13), CREAM)

    # Footer
    text_left(d, (40, H_ - 38), "VoLum  ·  Lattice Reverence  ·  collapsed PRE / POST hit-target model", font_mono(12), CREAM_DIM)
    return img


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    variants = [
        (variant_a, "a", "Variant A — Flanking-Compact (vertical slot list)", "variant_a_flanking_compact.png"),
        (variant_b, "b", "Variant B — Horizontal Chip Row Above AMP", "variant_b_horizontal_chips.png"),
        (variant_c, "c", "Variant C — Split Twin-Rows (PRE / POST)", "variant_c_split_twin_rows.png"),
        (variant_d, "d", "Variant D — Pedalboard Mini-Rack", "variant_d_pedal_minirack.png"),
    ]

    rendered = []
    for fn, key, label, fname in variants:
        print(f"Rendering {label} ...")
        img = render_mockup(fn, key, label)
        img.convert("RGB").save(OUT_DIR / fname, "PNG", optimize=True)
        rendered.append((label, fname, img))

    # Comparison sheet — 2x2 grid of small versions, with title and footer
    print("Rendering comparison sheet ...")
    cell_w = 1080
    cell_h = int(cell_w * CH / CW)
    margin = 36
    title_h = 90
    foot_h = 56
    sheet_w = cell_w * 2 + margin * 3
    sheet_h = title_h + cell_h * 2 + margin * 3 + foot_h
    sheet = Image.new("RGBA", (sheet_w, sheet_h), BG + (255,))
    sd = ImageDraw.Draw(sheet)
    text_left(sd, (margin, 24), "VoLum  ·  PRE / POST collapsed-block redesign", font_disp(54), GOLD)
    text_left(sd, (margin, 70), "Lattice Reverence  —  four variants for review", font_mono(16), CREAM_DIM)

    positions = [
        (margin, title_h + margin),
        (margin * 2 + cell_w, title_h + margin),
        (margin, title_h + margin * 2 + cell_h),
        (margin * 2 + cell_w, title_h + margin * 2 + cell_h),
    ]
    letters = ["A", "B", "C", "D"]
    blurbs = [
        "Flanking-Compact",
        "Horizontal Chip Row",
        "Split Twin-Rows",
        "Pedalboard Mini-Rack",
    ]
    for (label, fname, im), (x, y), letter, blurb in zip(rendered, positions, letters, blurbs):
        thumb = im.resize((cell_w, cell_h), Image.LANCZOS).convert("RGBA")
        sheet.paste(thumb, (x, y))
        sd.rectangle([x, y, x + cell_w, y + cell_h], outline=rgba(FRAME, 100), width=1)
        # corner letter badge
        badge = (x + 12, y + 12, x + 56, y + 56)
        sd.rounded_rectangle(badge, radius=4, fill=(28, 28, 38), outline=rgba(GOLD, 200), width=1)
        text_centered(sd, ((badge[0] + badge[2]) / 2, (badge[1] + badge[3]) / 2 - 1), letter, font_disp(36), GOLD)
        # caption
        text_left(sd, (x + 70, y + 22), blurb, font_bold(20), CREAM)
    text_left(
        sd,
        (margin, sheet_h - foot_h + 16),
        "Lattice Reverence  ·  near-black substrate, teal life, gold gravity, cream voice",
        font_mono(14),
        CREAM_DIM,
    )

    sheet.convert("RGB").save(OUT_DIR / "variants_comparison.png", "PNG", optimize=True)

    # Refined variant A also saved under a clearer name (since the chosen direction)
    print("Rendering variant_a_refined alias ...")
    refined = render_mockup(variant_a, "a", "Variant A — Refined (toggle pills + click zones)")
    refined.convert("RGB").save(OUT_DIR / "variant_a_refined.png", "PNG", optimize=True)

    print("Rendering interaction annotation ...")
    annot = render_interaction_annotation()
    annot.convert("RGB").save(OUT_DIR / "variant_a_interaction.png", "PNG", optimize=True)

    # ---- Style explorations of Variant A ----
    style_variants = [
        (variant_a_brass,     "a_brass",     "Style 1 — Engraved Brass",      "variant_a_style1_brass.png"),
        (variant_a_blueprint, "a_blueprint", "Style 2 — Blueprint",           "variant_a_style2_blueprint.png"),
        (variant_a_eurorack,  "a_eurorack",  "Style 3 — Eurorack Module",     "variant_a_style3_eurorack.png"),
        (variant_a_holo,      "a_holo",      "Style 4 — Holographic Glow",    "variant_a_style4_holographic.png"),
    ]

    style_rendered = []
    for fn, key, label, fname in style_variants:
        print(f"Rendering {label} ...")
        img = render_mockup(fn, key, label)
        img.convert("RGB").save(OUT_DIR / fname, "PNG", optimize=True)
        style_rendered.append((label, fname, img))

    # 2x2 sheet for the four styles
    print("Rendering styles comparison sheet ...")
    cell_w = 1080
    cell_h = int(cell_w * CH / CW)
    margin = 36
    title_h = 90
    foot_h = 56
    sheet_w = cell_w * 2 + margin * 3
    sheet_h = title_h + cell_h * 2 + margin * 3 + foot_h
    sheet = Image.new("RGBA", (sheet_w, sheet_h), BG + (255,))
    sd = ImageDraw.Draw(sheet)
    text_left(sd, (margin, 24), "VoLum  ·  Variant A — four visual styles", font_disp(54), GOLD)
    text_left(sd, (margin, 70),
              "Same skeleton, same hit zones — different visual language",
              font_mono(16), CREAM_DIM)

    positions = [
        (margin, title_h + margin),
        (margin * 2 + cell_w, title_h + margin),
        (margin, title_h + margin * 2 + cell_h),
        (margin * 2 + cell_w, title_h + margin * 2 + cell_h),
    ]
    badges = ["1", "2", "3", "4"]
    blurbs = ["Engraved Brass", "Blueprint", "Eurorack Module", "Holographic Glow"]
    for (label, fname, im), (x, y), badge_t, blurb in zip(style_rendered, positions, badges, blurbs):
        thumb = im.resize((cell_w, cell_h), Image.LANCZOS).convert("RGBA")
        sheet.paste(thumb, (x, y))
        sd.rectangle([x, y, x + cell_w, y + cell_h], outline=rgba(FRAME, 100), width=1)
        badge = (x + 12, y + 12, x + 56, y + 56)
        sd.rounded_rectangle(badge, radius=4, fill=(28, 28, 38), outline=rgba(GOLD, 200), width=1)
        text_centered(sd, ((badge[0] + badge[2]) / 2, (badge[1] + badge[3]) / 2 - 1), badge_t, font_disp(36), GOLD)
        text_left(sd, (x + 70, y + 22), blurb, font_bold(20), CREAM)
    text_left(sd, (margin, sheet_h - foot_h + 16),
              "Variant A skeleton  ·  flank · 178x240  ·  3 PRE + 2 POST  ·  nav-zone | toggle-zone",
              font_mono(14), CREAM_DIM)
    sheet.convert("RGB").save(OUT_DIR / "variant_a_styles_comparison.png", "PNG", optimize=True)

    # ---- Restrained refinements (within VoLum's existing language) ----
    alt_variants = [
        (variant_a_quiet,        "a_quiet",        "Alt 1 — Quiet (frameless slots, edge-bar active)",          "variant_a_alt1_quiet.png"),
        (variant_a_anchored,     "a_anchored",     "Alt 2 — Anchored (nameplate header, hero-corner frame)",     "variant_a_alt2_anchored.png"),
        (variant_a_hierarchical, "a_hierarchical", "Alt 3 — Hierarchical (active expanded, bypassed compressed)", "variant_a_alt3_hierarchical.png"),
        (variant_a_switchblade,  "a_switchblade",  "Alt 4 — Switchblade (unified bypass column on the right)",   "variant_a_alt4_switchblade.png"),
    ]
    alt_rendered = []
    for fn, key, label, fname in alt_variants:
        print(f"Rendering {label} ...")
        img = render_mockup(fn, key, label)
        img.convert("RGB").save(OUT_DIR / fname, "PNG", optimize=True)
        alt_rendered.append((label, fname, img))

    # 2x2 sheet for the alts
    print("Rendering alts comparison sheet ...")
    cell_w = 1080
    cell_h = int(cell_w * CH / CW)
    margin = 36
    title_h = 90
    foot_h = 56
    sheet_w = cell_w * 2 + margin * 3
    sheet_h = title_h + cell_h * 2 + margin * 3 + foot_h
    sheet = Image.new("RGBA", (sheet_w, sheet_h), BG + (255,))
    sd = ImageDraw.Draw(sheet)
    text_left(sd, (margin, 24), "VoLum  ·  Variant A — restrained refinements", font_disp(54), GOLD)
    text_left(sd, (margin, 70),
              "Same palette, same fonts, same dimensions — one design lever each",
              font_mono(16), CREAM_DIM)

    positions = [
        (margin, title_h + margin),
        (margin * 2 + cell_w, title_h + margin),
        (margin, title_h + margin * 2 + cell_h),
        (margin * 2 + cell_w, title_h + margin * 2 + cell_h),
    ]
    badges = ["1", "2", "3", "4"]
    blurbs = ["Quiet", "Anchored", "Hierarchical", "Switchblade"]
    for (label, fname, im), (x, y), badge_t, blurb in zip(alt_rendered, positions, badges, blurbs):
        thumb = im.resize((cell_w, cell_h), Image.LANCZOS).convert("RGBA")
        sheet.paste(thumb, (x, y))
        sd.rectangle([x, y, x + cell_w, y + cell_h], outline=rgba(FRAME, 100), width=1)
        badge = (x + 12, y + 12, x + 56, y + 56)
        sd.rounded_rectangle(badge, radius=4, fill=(28, 28, 38), outline=rgba(GOLD, 200), width=1)
        text_centered(sd, ((badge[0] + badge[2]) / 2, (badge[1] + badge[3]) / 2 - 1), badge_t, font_disp(36), GOLD)
        text_left(sd, (x + 70, y + 22), blurb, font_bold(20), CREAM)
    text_left(sd, (margin, sheet_h - foot_h + 16),
              "All variants stay strictly within the existing VoLum visual language.",
              font_mono(14), CREAM_DIM)
    sheet.convert("RGB").save(OUT_DIR / "variant_a_alts_comparison.png", "PNG", optimize=True)

    print("Done.")


if __name__ == "__main__":
    main()
