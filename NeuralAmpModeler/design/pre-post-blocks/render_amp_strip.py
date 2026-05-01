"""Render mockup variants of the collapsed AMP strip (visible in PRE/POST view).

Produces five PNGs that all show the same triptych row (Quiet PRE block on
the left, AMP strip variant in the middle, Quiet POST block on the right)
at 2x scale, plus a comparison sheet stitching all five strips for quick
side-by-side review.

Run:
    python NeuralAmpModeler/design/pre-post-blocks/render_amp_strip.py
"""

from __future__ import annotations

import math
import random
from pathlib import Path
from typing import Callable, List, Tuple

from PIL import Image, ImageDraw, ImageFont

# ---------------------------------------------------------------------------
# Paths & resolution
# ---------------------------------------------------------------------------

ROOT = Path(__file__).resolve().parents[3]
FONT_DIR = ROOT / "NeuralAmpModeler" / "resources" / "fonts"
FONT_DIR_WIN = ROOT / "NeuralAmpModeler" / "resources" / "fonts-win"
OUT_DIR = Path(__file__).resolve().parent

# All native sizes match the actual app; we render at SCALE x for crisper PNGs.
SCALE = 3
TRIP_W_NAT = 290        # 100 + 10 + 70 + 10 + 100
TRIP_H_NAT = 196
PRE_W_NAT = 100
AMP_W_NAT = 70
POST_W_NAT = 100
GAP_NAT = 10

# ---------------------------------------------------------------------------
# Palette (matches VoLumColorHelpers.h; alpha tuned by call-site)
# ---------------------------------------------------------------------------

BG = (17, 17, 24)
HERO_BG = (12, 12, 18)
TEAL = (91, 196, 196)
TEAL_DIM = (75, 162, 162)
GOLD = (252, 222, 145)
GOLD_DIM = (235, 210, 145)
AMBER = (232, 168, 92)
CREAM = (237, 227, 208)
CREAM_DIM = (166, 149, 124)
TEXT_BRIGHT = (255, 248, 238)
FRAME = (200, 162, 78)
DIVIDER = (200, 162, 78)


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


def corner_accent(d, x, y, size, flip_h, flip_v, color, width=1):
    dx = -size if flip_h else size
    dy = -size if flip_v else size
    d.line([(x, y), (x + dx, y)], fill=color, width=width)
    d.line([(x, y), (x, y + dy)], fill=color, width=width)


def text_centered(d, xy, text, font, fill):
    bbox = d.textbbox((0, 0), text, font=font)
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]
    d.text((xy[0] - w / 2 - bbox[0], xy[1] - h / 2 - bbox[1]), text, font=font, fill=fill)


def text_left(d, xy, text, font, fill):
    d.text(xy, text, font=font, fill=fill)


def rotated_text_image(text, font, fill, angle=270):
    """Return a transparent RGBA image of `text` drawn and rotated."""
    # Measure first on a scratch image so we can build a tight rect.
    scratch = Image.new("RGBA", (10, 10), (0, 0, 0, 0))
    sd = ImageDraw.Draw(scratch)
    bbox = sd.textbbox((0, 0), text, font=font)
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]
    pad = 4
    img = Image.new("RGBA", (w + pad * 2, h + pad * 2), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.text((pad - bbox[0], pad - bbox[1]), text, font=font, fill=fill)
    return img.rotate(angle, resample=Image.BICUBIC, expand=True)


# ---------------------------------------------------------------------------
# Quiet PRE/POST block (context, mirrors what's already in the C++ build)
# ---------------------------------------------------------------------------


PRE_EFFECTS = [
    ("COMP", "comp", False),
    ("NAM 1", "lattice", True),
    ("NAM 2", "circles", False),
]
POST_EFFECTS = [
    ("DELAY", "delay", True),
    ("REVERB", "reverb", False),
]


def draw_motif_comp(d, rect, dim=False):
    L, T, R, B = rect
    cx, cy = (L + R) / 2, (T + B) / 2
    w, h = R - L, B - T
    a = 60 if dim else 220
    teal_a = rgba(TEAL, a)
    blue_a = rgba((145, 220, 245), int(a * 0.85))
    radii = [(w * 0.20, h * 0.10), (w * 0.30, h * 0.18), (w * 0.40, h * 0.26)]
    for i, (rx, ry) in enumerate(radii):
        col = teal_a if i % 2 == 0 else blue_a
        d.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], outline=col, width=1)
    pin = teal_a if not dim else rgba(TEAL_DIM, 90)
    for px, py in [(-0.30, 0.10), (-0.10, -0.10), (0.15, 0.10), (0.30, -0.05)]:
        d.ellipse(
            [cx + w * px - 1.5, cy + h * py - 1.5, cx + w * px + 1.5, cy + h * py + 1.5],
            fill=pin,
        )


def draw_motif_lattice(d, rect, dim=False):
    L, T, R, B = rect
    cx, cy = (L + R) / 2, (T + B) / 2
    w, h = R - L, B - T
    a = 70 if dim else 220
    teal_a = rgba(TEAL, a)
    blue_a = rgba((145, 220, 245), int(a * 0.7))
    cell = min(w * 0.14, h * 0.19)
    cols, rows = 5, 3
    grid_w = cell * cols * 1.16
    grid_h = cell * rows * 1.10
    left = cx - grid_w / 2
    top = cy - grid_h / 2
    for y in range(rows):
        for x in range(cols):
            px = left + x * cell * 1.16
            py = top + y * cell * 1.10
            col = blue_a if (x + y) % 2 else teal_a
            d.rectangle([px, py, px + cell, py + cell], outline=col, width=1)


def draw_motif_circles(d, rect, dim=False):
    L, T, R, B = rect
    cx, cy = (L + R) / 2, (T + B) / 2
    w, h = R - L, B - T
    a = 70 if dim else 220
    cxA = cx - w * 0.13
    cxB = cx + w * 0.13
    for i in range(4):
        rad = h * (0.10 + i * 0.07)
        d.ellipse(
            [cxA - rad, cy - rad, cxA + rad, cy + rad],
            outline=rgba(TEAL, int(a * (0.4 + i * 0.15))),
            width=1,
        )
        d.ellipse(
            [cxB - rad, cy - rad, cxB + rad, cy + rad],
            outline=rgba((145, 220, 245), int(a * (0.35 + i * 0.13))),
            width=1,
        )


def draw_motif_delay(d, rect, dim=False):
    L, T, R, B = rect
    cx, cy = (L + R) / 2, (T + B) / 2
    w, h = R - L, B - T
    base = 60 if dim else 220
    taps = 4
    tap_w = w / taps
    for t in range(taps):
        decay = 1 - t / taps * 0.7
        amp = h * 0.30 * decay
        a = int(base * decay)
        base_x = L + t * tap_w
        prev = (base_x, cy)
        for j in range(1, 25):
            t1 = j / 24
            x = base_x + t1 * tap_w
            env = math.sin(t1 * math.pi)
            y = cy + math.sin(t1 * math.pi * 2 * 3) * amp * env
            d.line([prev, (x, y)], fill=rgba(TEAL, a), width=1)
            prev = (x, y)


def draw_motif_reverb(d, rect, dim=False):
    """Tiny Diezel-style branching dust, scaled to rect."""
    L, T, R, B = rect
    cx, cy = (L + R) / 2, (T + B) / 2
    w, h = R - L, B - T
    rng = random.Random(54321)
    pts = []
    for f_pct in range(5, 95, 14):
        pts.append((L + w * f_pct / 100, B - 2))
    for f_pct in range(10, 90, 18):
        pts.append((L + w * f_pct / 100, cy + h * 0.10))
    for f_pct in range(15, 85, 22):
        pts.append((L + w * f_pct / 100, cy - h * 0.10))
    base_count = max(220, int(min(w, h) ** 2 * 0.55))
    for i in range(base_count):
        idx = rng.randrange(len(pts))
        ang = rng.random() * 2 * math.pi
        ln = max(0.6, min(w, h) * 0.05) * (0.6 + rng.random() * 0.7)
        nx = pts[idx][0] + ln * math.cos(ang)
        ny = pts[idx][1] + ln * math.sin(ang)
        if L <= nx <= R and T <= ny <= B:
            a = 40 if dim else (140 if i < base_count // 8 else 75)
            d.line([pts[idx], (nx, ny)], fill=(120, 210, 220, a), width=1)
            pts.append((nx, ny))


MOTIFS = {
    "comp": draw_motif_comp,
    "lattice": draw_motif_lattice,
    "circles": draw_motif_circles,
    "delay": draw_motif_delay,
    "reverb": draw_motif_reverb,
}


def draw_mini_pill(d, center, on, w=22, h=14):
    cx, cy = center
    L = cx - w / 2
    T = cy - h / 2
    R = cx + w / 2
    B = cy + h / 2
    d.rounded_rectangle([L, T, R, B], radius=h / 2, fill=(8, 10, 14))
    track = rgba(GOLD, 90) if on else rgba(FRAME, 80)
    d.rounded_rectangle([L, T, R, B], radius=h / 2, outline=track, width=1)
    knob_r = h / 2 - 1.5
    knob_cx = R - h / 2 if on else L + h / 2
    knob_col = GOLD if on else CREAM_DIM
    d.ellipse([knob_cx - knob_r, cy - knob_r, knob_cx + knob_r, cy + knob_r], fill=knob_col)
    if on:
        d.ellipse([knob_cx - 1.4, cy - 1.4, knob_cx + 1.4, cy + 1.4], fill=TEAL)


def draw_quiet_block(canvas, d, rect, header, effects):
    L, T, R, B = rect
    d.rectangle(rect, fill=HERO_BG, outline=rgba(FRAME, 80), width=1)
    cs = 6
    corner_accent(d, L + 3, T + 3, cs, False, False, rgba(TEAL_DIM, 220))
    corner_accent(d, R - 3, T + 3, cs, True, False, rgba(TEAL_DIM, 220))
    corner_accent(d, L + 3, B - 3, cs, False, True, rgba(TEAL_DIM, 220))
    corner_accent(d, R - 3, B - 3, cs, True, True, rgba(TEAL_DIM, 220))

    # Header w/ chevron + hairline underline
    fnt_hdr = font_bold(int(11 * SCALE))  # mimic 10 pt @ scaled
    cx = (L + R) / 2
    text_centered(d, (cx - 6, T + 12 * SCALE), header, fnt_hdr, GOLD_DIM)
    chev_x = cx + (len(header) * 4 * SCALE) - 4
    chev_y = T + 12 * SCALE
    d.line([(chev_x, chev_y - 3), (chev_x + 3, chev_y)], fill=rgba(GOLD_DIM, 220), width=1)
    d.line([(chev_x + 3, chev_y), (chev_x, chev_y + 3)], fill=rgba(GOLD_DIM, 220), width=1)
    underline_y = T + 22 * SCALE
    d.line([(L + 6, underline_y), (R - 6, underline_y)], fill=rgba(TEAL_DIM, 130), width=1)

    # Slots
    inner_top = underlay_y = underline_y + 2
    inner_bot = B - 4
    n = len(effects)
    slot_h = (inner_bot - inner_top) / n
    for i, (name, motif_key, active) in enumerate(effects):
        sy_top = inner_top + i * slot_h
        sy_bot = sy_top + slot_h
        if i < n - 1:
            d.line([(L + 6, sy_bot), (R - 6, sy_bot)], fill=rgba(FRAME, 60), width=1)
        sr = (L + 2, sy_top + 1, R - 2, sy_bot - 1)
        _draw_quiet_slot(canvas, d, sr, name, MOTIFS[motif_key], active)


def _draw_quiet_slot(canvas, d, rect, name, motif_fn, active):
    L, T, R, B = rect
    if active:
        d.rectangle((L + 1, T + 2, L + 4, B - 2), fill=TEAL)
    motif_size = min(20 * SCALE, (B - T) - 6)
    motif_l = L + 6 * SCALE
    motif_t = (T + B) / 2 - motif_size / 2
    motif_rect = (motif_l, motif_t, motif_l + motif_size, motif_t + motif_size)
    motif_fn(d, motif_rect, dim=not active)

    pill_w = 22 * SCALE
    pill_h = 14 * SCALE
    tcx = R - pill_w / 2 - 6 * SCALE
    tcy = (T + B) / 2
    text_left(
        d,
        (motif_rect[2] + 4 * SCALE, tcy - 6 * SCALE),
        name,
        font_bold(int(9 * SCALE)),
        CREAM if active else CREAM_DIM,
    )
    draw_mini_pill(d, (tcx, tcy), bool(active), w=pill_w, h=pill_h)


# ---------------------------------------------------------------------------
# AMP strip variants  -  each draws into the given strip rect.
# ---------------------------------------------------------------------------


def _draw_amp_motif_dla(d, rect, dim=False):
    """Simplified Diezel-style branching dust for the amp strip context."""
    L, T, R, B = rect
    cx, cy = (L + R) / 2, (T + B) / 2
    w, h = R - L, B - T
    rng = random.Random(2026)
    pts = [(cx, B - 2), (cx, T + 2), (L + 4, cy), (R - 4, cy)]
    base = max(400, int(min(w, h) * 30))
    for i in range(base):
        idx = rng.randrange(len(pts))
        ang = rng.random() * 2 * math.pi
        ln = 1.4 + rng.random() * 2.0
        nx = pts[idx][0] + ln * math.cos(ang)
        ny = pts[idx][1] + ln * math.sin(ang)
        if L + 1 <= nx <= R - 1 and T + 1 <= ny <= B - 1:
            if i < base // 30:
                col = (120, 210, 220, 30 if dim else 150)
                tk = 2
            elif i < base // 6:
                col = (100, 180, 200, 25 if dim else 90)
                tk = 1
            else:
                col = (80, 150, 170, 18 if dim else 50)
                tk = 1
            d.line([pts[idx], (nx, ny)], fill=col, width=tk)
            pts.append((nx, ny))


def _quiet_frame(d, rect):
    L, T, R, B = rect
    d.rectangle(rect, fill=HERO_BG, outline=rgba(FRAME, 80), width=1)
    cs = 6
    corner_accent(d, L + 3, T + 3, cs, False, False, rgba(TEAL_DIM, 220))
    corner_accent(d, R - 3, T + 3, cs, True, False, rgba(TEAL_DIM, 220))
    corner_accent(d, L + 3, B - 3, cs, False, True, rgba(TEAL_DIM, 220))
    corner_accent(d, R - 3, B - 3, cs, True, True, rgba(TEAL_DIM, 220))


def _amp_header(d, rect, color=GOLD_DIM, with_chev=True, label="AMP"):
    L, T, R, B = rect
    cx = (L + R) / 2
    fnt = font_bold(int(10 * SCALE))
    text_centered(d, (cx - 4 * SCALE, T + 12 * SCALE), label, fnt, color)
    if with_chev:
        chev_x = cx + (len(label) * 3 * SCALE) - 1
        chev_y = T + 12 * SCALE
        # Down-chevron (back to AMP from below)
        d.line([(chev_x - 2, chev_y - 2), (chev_x, chev_y + 1), (chev_x + 2, chev_y - 2)],
               fill=rgba(color, 220), width=1)
    underline_y = T + 22 * SCALE
    d.line([(L + 6, underline_y), (R - 6, underline_y)], fill=rgba(TEAL_DIM, 130), width=1)
    return underline_y


def _spine_text(canvas, rect, text, font, fill):
    """Paste a 90-deg rotated single-line label centered in rect."""
    img = rotated_text_image(text, font, fill, angle=90)
    L, T, R, B = rect
    iw, ih = img.size
    px = int((L + R) / 2 - iw / 2)
    py = int((T + B) / 2 - ih / 2)
    canvas.alpha_composite(img, dest=(px, py))


def amp_quiet1_spine(canvas, d, rect, amp_name):
    """Quiet sibling: small motif at top, rotated spine + AMP> header (no on/off cue)."""
    _quiet_frame(d, rect)
    L, T, R, B = rect
    underline_y = _amp_header(d, rect)

    # Tiny motif just under the header (block is 140 tall - tight budget)
    msz = 22 * SCALE
    mcx = (L + R) / 2
    motif_rect = (mcx - msz / 2, underline_y + 4, mcx + msz / 2, underline_y + 4 + msz)
    _draw_amp_motif_dla(d, motif_rect)

    # Spine fills the rest of the block down to a small bottom margin.
    spine_top = motif_rect[3] + 6
    spine_bot = B - 6
    spine_rect = (L + 4, spine_top, R - 4, spine_bot)
    _spine_text(canvas, spine_rect, amp_name, font_bold(int(10 * SCALE)), rgba(CREAM, 240))


def amp_quiet2_minimal(canvas, d, rect, amp_name):
    """Quiet sibling, no motif, all-spine, max restraint."""
    _quiet_frame(d, rect)
    L, T, R, B = rect
    underline_y = _amp_header(d, rect)

    # Single rotated spine using the display face for elegance, fills inner area.
    spine_top = underline_y + 6
    spine_bot = B - 6
    spine_rect = (L + 4, spine_top, R - 4, spine_bot)
    _spine_text(canvas, spine_rect, amp_name, font_disp(int(13 * SCALE)), rgba(CREAM, 235))


def amp_loud1_marquee(canvas, d, rect, amp_name):
    """Loud anchor: gold corner accents, motif top, big rotated spine; no badge.

    Trimmed to fit in the 140-tall block - no separate AMP badge underline at
    the bottom (was overflowing); the header carries the 'AMP' label.
    """
    L, T, R, B = rect
    # Outer gold-dim frame (warmer than the quiet teal frame)
    d.rectangle(rect, fill=HERO_BG, outline=rgba(GOLD_DIM, 180), width=1)
    cs = 8
    corner_accent(d, L + 3, T + 3, cs, False, False, rgba(GOLD, 220), width=2)
    corner_accent(d, R - 3, T + 3, cs, True, False, rgba(GOLD, 220), width=2)
    corner_accent(d, L + 3, B - 3, cs, False, True, rgba(GOLD, 220), width=2)
    corner_accent(d, R - 3, B - 3, cs, True, True, rgba(GOLD, 220), width=2)

    # Header with gold tint and chevron, gold underline (matches the corner colour).
    cx = (L + R) / 2
    fnt = font_bold(int(10 * SCALE))
    text_centered(d, (cx - 4, T + 12 * SCALE), "AMP", fnt, GOLD)
    chev_x = cx + 12
    chev_y = T + 12 * SCALE
    d.line([(chev_x - 2, chev_y - 2), (chev_x, chev_y + 1), (chev_x + 2, chev_y - 2)],
           fill=rgba(GOLD, 220), width=1)
    underline_y = T + 22 * SCALE
    d.line([(L + 8, underline_y), (R - 8, underline_y)], fill=rgba(GOLD_DIM, 200), width=1)

    # Motif region just under the header (more prominent than quiet1).
    motif_top = underline_y + 6
    msz = 30 * SCALE
    mcx = (L + R) / 2
    motif_rect = (mcx - msz / 2, motif_top, mcx + msz / 2, motif_top + msz)
    _draw_amp_motif_dla(d, motif_rect)

    # Rotated spine fills the remaining vertical space.
    spine_top = motif_rect[3] + 4
    spine_bot = B - 6
    spine_rect = (L + 4, spine_top, R - 4, spine_bot)
    _spine_text(canvas, spine_rect, amp_name, font_mono(int(8 * SCALE)), rgba(GOLD, 240))


def amp_loud2_plate(canvas, d, rect, amp_name):
    """Vintage nameplate: warm gradient, brass rivets, display-face spine, no motif."""
    L, T, R, B = rect
    d.rectangle(rect, fill=(20, 18, 22))
    # Subtle warm gradient overlay (slightly darker toward the bottom)
    for y in range(int(T), int(B)):
        t = (y - T) / max(1, (B - T))
        a = int(36 - t * 30)
        if a > 0:
            d.line([(L + 1, y), (R - 1, y)], fill=(120, 90, 50, a), width=1)
    d.rectangle(rect, outline=rgba(GOLD_DIM, 220), width=1)
    d.rectangle((L + 4, T + 4, R - 4, B - 4), outline=rgba(GOLD, 90), width=1)

    # Brass rivets at the four interior corners
    for cx, cy in [(L + 10, T + 10), (R - 10, T + 10), (L + 10, B - 10), (R - 10, B - 10)]:
        d.ellipse([cx - 3, cy - 3, cx + 3, cy + 3], fill=GOLD, outline=(80, 60, 30))
        d.ellipse([cx - 1.2, cy - 1.2, cx + 1.2, cy + 1.2], fill=(255, 240, 200))

    # Spine in display face fills the inner plate, keeping clear of rivets.
    spine_rect = (L + 16, T + 16, R - 16, B - 16)
    _spine_text(canvas, spine_rect, amp_name, font_disp(int(15 * SCALE)), rgba(GOLD, 240))


def amp_loud3_channel(canvas, d, rect, amp_name):
    """Channel-strip energy: AMP> header + motif + spine + tiny model index.

    No LED indicator (the amp sim is always 'on'); the header carries the
    section identity and the model index `04 / 15` is the only extra info.
    """
    L, T, R, B = rect
    _quiet_frame(d, rect)
    underline_y = _amp_header(d, rect)

    # Small motif beneath header
    msz = 22 * SCALE
    cx = (L + R) / 2
    motif_rect = (cx - msz / 2, underline_y + 4, cx + msz / 2, underline_y + 4 + msz)
    _draw_amp_motif_dla(d, motif_rect)

    # Spine fills middle, leaving room for the model index at the bottom.
    spine_top = motif_rect[3] + 4
    spine_bot = B - 22 * SCALE
    spine_rect = (L + 4, spine_top, R - 4, spine_bot)
    _spine_text(canvas, spine_rect, amp_name, font_bold(int(9 * SCALE)), rgba(CREAM, 245))

    # Bottom: tiny model index, hairline above it.
    d.line([(L + 12, B - 18 * SCALE), (R - 12, B - 18 * SCALE)],
           fill=rgba(FRAME, 70), width=1)
    text_centered(d, (cx, B - 11 * SCALE), "04 / 15", font_mono(int(6 * SCALE)), CREAM_DIM)


VARIANTS = [
    ("quiet1_spine",  "Quiet  -  Spine + Motif",     amp_quiet1_spine),
    ("quiet2_minimal","Quiet  -  Minimal Spine",     amp_quiet2_minimal),
    ("loud1_marquee", "Loud  -  Marquee",            amp_loud1_marquee),
    ("loud2_plate",   "Loud  -  Vintage Plate",      amp_loud2_plate),
    ("loud3_channel", "Loud  -  Channel Strip",      amp_loud3_channel),
]


# ---------------------------------------------------------------------------
# Composer: triptych row at SCALE x with PRE block, AMP variant, POST block
# ---------------------------------------------------------------------------


def compose_triptych(variant_fn: Callable, amp_name: str) -> Image.Image:
    margin = 40
    title_h = 80
    foot_h = 32
    inner_w = TRIP_W_NAT * SCALE
    inner_h = TRIP_H_NAT * SCALE
    total_w = inner_w + margin * 2
    total_h = title_h + inner_h + foot_h + margin
    img = Image.new("RGBA", (total_w, total_h), BG + (255,))
    d = ImageDraw.Draw(img)

    # Title
    text_left(d, (margin, 24), "AMP strip variant", font_disp(int(34)), GOLD)
    text_left(d, (margin, 60), f"PRE-expanded view  -  amp: {amp_name}", font_mono(13), CREAM_DIM)

    # Triptych row geometry (in scaled px)
    tx = margin
    ty = title_h
    pre_rect = (tx, ty, tx + PRE_W_NAT * SCALE, ty + inner_h)
    amp_rect = (
        pre_rect[2] + GAP_NAT * SCALE,
        ty,
        pre_rect[2] + GAP_NAT * SCALE + AMP_W_NAT * SCALE,
        ty + inner_h,
    )
    post_rect = (
        amp_rect[2] + GAP_NAT * SCALE,
        ty,
        amp_rect[2] + GAP_NAT * SCALE + POST_W_NAT * SCALE,
        ty + inner_h,
    )

    # Vertically center 100x140 quiet block inside the strip rects (matches Draw())
    def center_block(rect, block_h_native=140, block_w_native=None):
        L, T, R, B = rect
        if block_w_native is None:
            bw = R - L
        else:
            bw = block_w_native * SCALE
        bh = block_h_native * SCALE
        cx = (L + R) / 2
        cy = (T + B) / 2
        return (cx - bw / 2, cy - bh / 2, cx + bw / 2, cy + bh / 2)

    pre_block = center_block(pre_rect)
    post_block = center_block(post_rect)
    # AMP strip block matches the PRE/POST block height so the row reads as a row.
    amp_block = center_block(amp_rect, block_h_native=140, block_w_native=AMP_W_NAT)

    draw_quiet_block(img, d, pre_block, "PRE", PRE_EFFECTS)
    variant_fn(img, d, amp_block, amp_name)
    draw_quiet_block(img, d, post_block, "POST", POST_EFFECTS)

    # Footer
    text_left(
        d,
        (margin, total_h - foot_h + 8),
        "Lattice Reverence  -  collapsed AMP strip exploration  -  rendered at 3x for inspection",
        font_mono(11),
        CREAM_DIM,
    )

    return img


# ---------------------------------------------------------------------------
# Comparison sheet
# ---------------------------------------------------------------------------


def compose_comparison(strip_imgs: List[Tuple[str, Image.Image]]) -> Image.Image:
    margin = 40
    cell_pad = 24
    label_h = 40
    cell_w = strip_imgs[0][1].size[0]
    cell_h = strip_imgs[0][1].size[1]
    cols = 3
    rows = (len(strip_imgs) + cols - 1) // cols
    title_h = 90
    foot_h = 50
    sheet_w = cell_w * cols + cell_pad * (cols + 1)
    sheet_h = title_h + (cell_h + label_h + cell_pad) * rows + foot_h + cell_pad
    sheet = Image.new("RGBA", (sheet_w, sheet_h), BG + (255,))
    d = ImageDraw.Draw(sheet)
    text_left(d, (cell_pad, 24), "VoLum  -  AMP strip redesign", font_disp(48), GOLD)
    text_left(d, (cell_pad, 70), "Five variants for review  -  PRE-expanded triptych row", font_mono(14), CREAM_DIM)

    for i, (label, im) in enumerate(strip_imgs):
        col = i % cols
        row = i // cols
        x = cell_pad + col * (cell_w + cell_pad)
        y = title_h + row * (cell_h + label_h + cell_pad)
        sheet.paste(im, (x, y), im)
        d.rectangle(
            [x, y + cell_h, x + cell_w, y + cell_h + label_h],
            fill=(28, 28, 38),
            outline=rgba(GOLD_DIM, 180),
            width=1,
        )
        text_centered(d, (x + cell_w / 2, y + cell_h + label_h / 2), label, font_bold(20), CREAM)

    text_left(
        d,
        (cell_pad, sheet_h - foot_h + 16),
        "Two quiet siblings + three loud anchors  -  spine treatment for the amp name throughout",
        font_mono(13),
        CREAM_DIM,
    )
    return sheet


# ---------------------------------------------------------------------------
# Entry
# ---------------------------------------------------------------------------


def main():
    AMP_NAME = "Diezel Herbert Mk1"
    rendered = []
    for key, label, fn in VARIANTS:
        print(f"Rendering amp_strip_{key} ...")
        img = compose_triptych(fn, AMP_NAME)
        out = OUT_DIR / f"amp_strip_{key}.png"
        img.convert("RGB").save(out, "PNG", optimize=True)
        rendered.append((label, img))

    print("Rendering comparison sheet ...")
    sheet = compose_comparison(rendered)
    sheet.convert("RGB").save(OUT_DIR / "amp_strip_variants.png", "PNG", optimize=True)
    print("Done.")


if __name__ == "__main__":
    main()

