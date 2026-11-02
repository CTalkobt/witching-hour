#!/usr/bin/env python3
"""gen_art.py — every picture of Witching Hour, drawn here.

    python3 tools/gen_art.py <build dir> [--preview]

Writes, in the build directory:
  witchgfx1.bin  bank 4 ($40000): full-colour characters 0-1022
  witchgfx2.bin  bank 5 ($50000): the rest of the characters, the sprites,
                 the band maps, the screen pictures and the samples
  gfx.h, gfx.c   where everything is, the palettes, the bands
  preview_*.png  (with --preview) the screens and the sprites, to look at

The backgrounds are drawn as pictures (palette indices) and cut into 8 x 8
characters, each different one kept once. The sprites are drawn below as
text, a character a pixel.
"""
import math
import os
import random
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sprites as SP          # noqa: E402
import sounds                 # noqa: E402

GFX1 = 0x40000
GFX2 = 0x50000
GFX2_END = 0x5F000            # the screen is there
GFX3 = 0x12000                # bank 1 (BASIC's, free while the game runs): samples
GFX3_END = 0x1C000            # the music is there (WITCHGFX4)
CHAR_BASE = GFX1 // 64        # $1000
LCOLS, ROWS = 41, 25

# ---------------------------------------------------------------- palette


class Palette:
    def __init__(self):
        self.rgb = [(0, 0, 0)]
        self.where = {(0, 0, 0): 0}

    def __call__(self, c, new=False):
        if isinstance(c, str):
            c = tuple(int(c[i:i + 2], 16) for i in (1, 3, 5))
        if not new and c in self.where:
            return self.where[c]
        if len(self.rgb) >= 255:
            raise SystemExit("palette full")
        self.rgb.append(c)
        if c not in self.where:
            self.where[c] = len(self.rgb) - 1
        return len(self.rgb) - 1


P = Palette()


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def hexc(s):
    return tuple(int(s[i:i + 2], 16) for i in (1, 3, 5))


BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0

# the night sky, top to bottom (global y): near black, indigo, purple, the
# ember glow on the horizon behind the far hills
SKY_KEYS = [(0, "#040208"), (30, "#0a0620"), (60, "#1a0c3c"), (84, "#34124e"),
            (100, "#5a1848"), (112, "#8a2a3a")]
SKY_SHADES = 14
_sky_cols = []


def sky_shade(t):
    # t 0..1 along the keys
    y = t * SKY_KEYS[-1][0]
    for (y0, c0), (y1, c1) in zip(SKY_KEYS, SKY_KEYS[1:]):
        if y <= y1:
            return lerp(hexc(c0), hexc(c1), (y - y0) / (y1 - y0))
    return hexc(SKY_KEYS[-1][1])


def sky_index(k):
    while len(_sky_cols) <= k:
        _sky_cols.append(P(sky_shade(len(_sky_cols) / (SKY_SHADES - 1))))
    return _sky_cols[k]


def sky_pixel(x, y):
    """The sky's index at global (x, y): two shades dithered."""
    f = min(max(y / SKY_KEYS[-1][0], 0), 1) * (SKY_SHADES - 1)
    k = int(f)
    frac = f - k
    if k >= SKY_SHADES - 1:
        return sky_index(SKY_SHADES - 1)
    return sky_index(k + 1) if frac > BAYER[y & 3][x & 3] else sky_index(k)


def sky_fill(img, y0):
    h, w = img.shape
    for y in range(h):
        for x in range(8):
            img[y, x::8] = sky_pixel(x, y + y0)


# ---------------------------------------------------------------- colours

C = {}


def colours():
    C["far"] = P("#2a1c4c")
    C["far_rim"] = P("#4c3c7c")
    C["far_dark"] = P("#21163e")
    C["castle"] = P("#140c26")
    C["castle_rim"] = P("#3a2c62")
    C["mid"] = P("#170f2a")
    C["mid_rim"] = P("#2c2050")
    C["tree"] = P("#08050e")
    C["grass"] = P("#13241a")
    C["grass_hi"] = P("#24442a")
    C["grass_lo"] = P("#0c1810")
    C["soil"] = P("#0a0c0a")
    C["soil_hi"] = P("#18160f")
    C["bone"] = P("#8c8670")
    C["stone_d"] = P("#30303e")
    C["stone"] = P("#565668")
    C["stone_l"] = P("#80809a")
    C["stone_h"] = P("#a8a8c0")
    C["moss"] = P("#2c4a30")
    C["iron"] = P("#050508")
    C["pump_d"] = P("#7a2a04")
    C["pump"] = P("#c25a0a")
    C["pump_l"] = P("#f08a1c")
    C["stem"] = P("#2c5a18")
    C["moon"] = P("#f6eed2")
    C["moon_d"] = P("#dcd0aa")
    C["moon_c"] = P("#c4b690")
    C["halo1"] = P("#2a1e52")
    C["halo2"] = P("#3c2c6c")
    C["cloud"] = P("#3a2c5c")
    C["cloud_l"] = P("#5c4c84")
    C["white"] = P("#ffffff")
    # cycled entries (palette animation): stars, windows, lantern glow
    C["star"] = [P("#ffffff", True), P("#c8c8ff", True), P("#8888c0", True)]
    C["win"] = [P("#ffd040", True), P("#ff9a20", True)]
    C["glow"] = [P("#ffe060", True), P("#ffa020", True)]
    # their sources
    C["star_src"] = [P("#ffffff"), P("#b0b0f0"), P("#5c5c98"), P("#9090d0")]
    C["win_src"] = [P("#ffd040"), P("#ffb030"), P("#ff9a20"), P("#ffe070")]
    C["glow_src"] = [P("#ffe060"), P("#ffb028"), P("#ff8a10"), P("#fff090")]
    # the logo's ramp
    C["logo"] = [P(c) for c in ("#fff6a0", "#ffe050", "#ffb820", "#ff8a10", "#e05a08", "#a83004")]
    C["logo_sh"] = P("#3a0c4a")
    C["slime"] = P("#5adc3a")
    C["slime_d"] = P("#2a8a1a")
    # the font's ramp
    C["font"] = [P(c) for c in ("#ffffff", "#fff2b0", "#ffd860", "#ffb030", "#f08a18", "#d06a10", "#a04808")]
    C["font_sh"] = P("#2a0a3a")


# ---------------------------------------------------------------- drawing


def canvas(w, h, fill=0):
    return np.full((h, w), fill, dtype=np.uint8)


def poly(img, pts, c, wrap=True):
    h, w = img.shape
    m = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(m)
    d.polygon(pts, fill=1)
    if wrap:
        d.polygon([(x - w, y) for x, y in pts], fill=1)
        d.polygon([(x + w, y) for x, y in pts], fill=1)
    img[np.array(m, dtype=bool)] = c


def rect(img, x0, y0, x1, y1, c):
    """Inclusive, wrapping in x."""
    h, w = img.shape
    for x in range(x0, x1 + 1):
        xx = x % w
        img[max(y0, 0):min(y1 + 1, h), xx] = c


def put(img, x, y, c):
    h, w = img.shape
    if 0 <= y < h:
        img[y, x % w] = c


def line(img, x0, y0, x1, y1, c, width=1):
    n = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
    for i in range(n + 1):
        t = i / n
        x = x0 + (x1 - x0) * t
        y = y0 + (y1 - y0) * t
        for dx in range(width):
            for dy in range(width):
                put(img, int(round(x + dx - width / 2)), int(round(y + dy - width / 2)), c)


def ellipse(img, cx, cy, rx, ry, c):
    h, w = img.shape
    for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            if ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2 <= 1:
                put(img, x, y, c)


def ridge(w, base, parts, seed):
    """A periodic height line: y for each x."""
    rnd = random.Random(seed)
    ys = np.full(w, float(base))
    for per, amp in parts:
        ph = rnd.random() * math.tau
        ys += amp * np.sin(np.arange(w) * math.tau * per / w + ph)
    return ys


def fill_below(img, ys, c, rim=None, rim2=None):
    h, w = img.shape
    for x in range(w):
        top = int(round(ys[x]))
        img[max(top, 0):, x] = c
        if rim is not None and 0 <= top < h:
            img[top, x] = rim
            # moonlight from the upper right: slopes facing right a little brighter
            if rim2 is not None and ys[(x + 1) % w] > ys[x] + 0.3 and top + 1 < h:
                img[top + 1, x] = rim2


def tree(img, x, y, length, ang, depth, rnd, c):
    if depth == 0 or length < 2:
        return
    x2 = x + math.cos(ang) * length
    y2 = y - math.sin(ang) * length
    line(img, x, y, x2, y2, c, width=max(1, depth // 2))
    n = 2 if depth > 1 else 1
    for i in range(n):
        a = ang + (rnd.uniform(0.3, 0.8) if i == 0 else -rnd.uniform(0.3, 0.8))
        tree(img, x2, y2, length * rnd.uniform(0.6, 0.78), a, depth - 1, rnd, c)
    if rnd.random() < 0.3:
        tree(img, x2, y2, length * 0.5, ang + rnd.uniform(-0.2, 0.2), depth - 2, rnd, c)


# ---------------------------------------------------------------- the sky


def draw_moon(img, cx, cy, r, oy=0):
    h, w = img.shape
    for y in range(h):
        for x in range(w):
            d = math.hypot(x + 0.5 - cx, y + oy + 0.5 - cy)
            if d <= r:
                img[y, x] = C["moon"]
            elif d <= r + 3 and BAYER[y & 3][x & 3] < 0.75 - (d - r) / 5:
                img[y, x] = C["halo2"]
            elif d <= r + 8 and BAYER[y & 3][x & 3] < 0.45 - (d - r - 3) / 14:
                img[y, x] = C["halo1"]
    rnd = random.Random(7)
    # shading on the lower left, craters
    for y in range(h):
        for x in range(w):
            d = math.hypot(x + 0.5 - cx, y + oy + 0.5 - cy)
            if d <= r and (x - cx) * 0.7 - (y + oy - cy) * 0.7 < -r * 0.55 and BAYER[y & 3][x & 3] < 0.6:
                img[y, x] = C["moon_d"]
    for i in range(int(r * 0.6)):
        a = rnd.random() * math.tau
        rr = rnd.random() ** 0.7 * r * 0.8
        x, y = cx + math.cos(a) * rr, cy + math.sin(a) * rr
        s = rnd.uniform(1, r / 6)
        for yy in range(int(y - s), int(y + s) + 1):
            for xx in range(int(x - s), int(x + s) + 1):
                if math.hypot(xx - x, yy - y) <= s and 0 <= yy - oy < h and 0 <= xx < w:
                    if img[yy - oy, xx] in (C["moon"], C["moon_d"]):
                        img[yy - oy, xx] = C["moon_c"] if math.hypot(xx - x + 1, yy - y + 1) > s * 0.5 else C["moon_d"]


def draw_stars(img, n, seed, avoid=None):
    rnd = random.Random(seed)
    h, w = img.shape
    for i in range(n):
        x, y = rnd.randrange(w), rnd.randrange(h)
        if avoid and avoid(x, y):
            continue
        k = rnd.random()
        if k < 0.25:
            img[y, x] = C["star"][i % 3]
        elif k < 0.32 and 1 <= x < w - 1 and 1 <= y < h - 1:
            c = C["star"][i % 3]
            img[y, x] = c
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                img[y + dy, x + dx] = C["star_src"][2]
        else:
            img[y, x] = C["star_src"][2 if k < 0.8 else 3]


def draw_cloud(img, x0, y0, length, seed):
    rnd = random.Random(seed)
    h, w = img.shape
    for x in range(length):
        th = 2 + 2 * math.sin(x / length * math.pi) + rnd.random()
        for y in range(int(th)):
            yy = y0 + y - int(th) // 2
            if 0 <= yy < h and BAYER[yy & 3][(x0 + x) & 3] < 0.55:
                img[yy, (x0 + x) % w] = C["cloud_l"] if y == 0 else C["cloud"]


def game_sky():
    """Rows 0-7 of the game: the HUD row (black) and the still sky."""
    img = canvas(LCOLS * 8, 64)
    sky_fill(img, 0)
    img[0:8, :] = 0
    mx, my, mr = 250, 34, 19
    draw_stars(img[8:], 260, 3, avoid=lambda x, y: math.hypot(x - mx, y + 8 - my) < mr + 10)
    draw_moon(img, mx, my, mr)
    draw_cloud(img, 210, 40, 70, 1)
    draw_cloud(img, 40, 20, 50, 2)
    draw_cloud(img, 120, 52, 40, 3)
    return img


# ---------------------------------------------------------------- the bands


def far_band():
    W, H, Y0 = 640, 48, 64
    img = canvas(W, H)
    sky_fill(img, Y0)
    # a distant range behind, darker, then the near range
    ys = ridge(W, 30, [(1, 6), (3, 4), (7, 2.5), (13, 1.2)], 11)
    fill_below(img, ys - 6, C["far_dark"])
    ys2 = ridge(W, 34, [(2, 6), (5, 3), (9, 2), (17, 1)], 12)
    # a crag for the castle at x 200
    for x in range(W):
        d = min(abs(x - 200), W - abs(x - 200))
        if d < 50:
            ys2[x] -= 14 * (1 - d / 50) ** 1.5
    fill_below(img, ys2, C["far"], C["far_rim"], C["far_rim"])
    img[H - 8:, :] = C["far"]
    # the castle: towers with pointed roofs, a keep, lit windows
    base = int(ys2[200]) + 3
    def tower(x, w, top, roof=True):
        rect(img, x, top, x + w - 1, base, C["castle"])
        rect(img, x + w - 1, top, x + w - 1, base, C["castle_rim"])
        if roof:
            poly(img, [(x - 1, top), (x + w // 2 - 0.5, top - w - 3), (x + w, top)], C["castle"])
            put(img, x + w // 2, top - w - 4, C["castle"])
        else:
            for k in range(0, w, 2):
                put(img, x + k, top - 1, C["castle"])
    tower(184, 32, base - 14, roof=False)           # the walls
    tower(190, 8, base - 26)
    tower(202, 10, base - 32)                       # the keep
    tower(214, 7, base - 24)
    tower(180, 5, base - 18)
    tower(222, 5, base - 19)
    for (x, y) in ((193, base - 20), (205, base - 26), (206, base - 18), (216, base - 18),
                   (196, base - 9), (210, base - 9), (186, base - 8)):
        img[y, x] = C["win"][0]
        img[y + 1, x] = C["win"][1]
    # a few lit windows of a village far off
    for x in (420, 432, 437, 560):
        y = int(ys2[x]) + 4
        img[y, x] = C["win"][(x // 3) & 1]
    return img, Y0


def mid_band():
    W, H, Y0 = 768, 40, 112
    img = canvas(W, H, C["far"])
    ys = ridge(W, 16, [(2, 4), (3, 3), (8, 1.5), (19, 0.8)], 21)
    # the church's hill
    for x in range(W):
        d = min(abs(x - 470), W - abs(x - 470))
        if d < 60:
            ys[x] -= 5 * (1 - d / 60)
    fill_below(img, ys, C["mid"], C["mid_rim"], C["mid_rim"])
    img[H - 8:, :] = C["mid"]
    rnd = random.Random(5)
    # dead trees
    for x in (40, 150, 230, 330, 610, 700):
        x += rnd.randrange(-10, 10)
        y = int(ys[x % W]) + 2
        tree(img, x, y, rnd.uniform(6, 9), math.pi / 2 + rnd.uniform(-0.15, 0.15), 5, rnd, C["tree"])
    # the church: nave, steeple, cross, a lit window; a yew beside it
    gx = 470
    gy = int(ys[gx]) + 3
    rect(img, gx - 18, gy - 10, gx + 10, gy, C["tree"])
    poly(img, [(gx - 20, gy - 10), (gx - 4, gy - 17), (gx + 12, gy - 10)], C["tree"])
    rect(img, gx + 10, gy - 20, gx + 18, gy, C["tree"])
    poly(img, [(gx + 9, gy - 20), (gx + 14, gy - 37), (gx + 19, gy - 20)], C["tree"])
    rect(img, gx + 14, gy - 40, gx + 14, gy - 36, C["tree"])
    rect(img, gx + 12, gy - 39, gx + 16, gy - 39, C["tree"])
    for wx in (gx - 14, gx - 8, gx - 2, gx + 4):
        rect(img, wx, gy - 7, wx + 1, gy - 4, C["win"][(wx // 6) & 1])
    rect(img, gx + 13, gy - 16, gx + 15, gy - 13, C["win"][0])
    ellipse(img, gx - 30, gy - 8, 6, 9, C["tree"])
    # a windmill on a far rise, sails crossed
    wx = 300
    wy = int(ys[wx]) + 2
    poly(img, [(wx - 4, wy), (wx - 2, wy - 12), (wx + 2, wy - 12), (wx + 4, wy)], C["tree"])
    line(img, wx - 9, wy - 21, wx + 9, wy - 3, C["tree"])
    line(img, wx - 9, wy - 3, wx + 9, wy - 21, C["tree"])
    return img, Y0


def fore_band():
    W, H, Y0 = 1024, 48, 152
    img = canvas(W, H, C["mid"])
    ys = ridge(W, 16, [(3, 2.5), (7, 1.5), (16, 0.8)], 31)
    fill_below(img, ys, C["grass"])
    rnd = random.Random(9)
    # grass blades on the top edge
    for x in range(W):
        top = int(round(ys[x]))
        if rnd.random() < 0.6:
            hgt = rnd.randrange(1, 4)
            for k in range(hgt):
                put(img, x, top - 1 - k, C["grass_hi"] if k < hgt - 1 else C["grass"])
        put(img, x, top, C["grass_hi"])
    # the soil: a band of darker earth, roots, a buried bone or skull
    for x in range(W):
        top = int(round(ys[x]))
        for y in range(top + 6, H):
            img[y, x] = C["grass_lo"] if y < top + 10 and BAYER[y & 3][x & 3] < (y - top - 6) / 4 else C["grass_lo"]
        for y in range(max(top + 14, 38), H):
            img[y, x] = C["soil"] if BAYER[y & 3][x & 3] < (y - 34) / 10 else C["grass_lo"]
    for i in range(18):
        x = rnd.randrange(W)
        y = rnd.randrange(38, 46)
        if rnd.random() < 0.3:
            # a little skull
            rect(img, x, y, x + 3, y + 2, C["bone"])
            put(img, x + 1, y + 1, C["soil"])
            put(img, x + 3, y + 1, C["soil"])
            rect(img, x + 1, y + 3, x + 2, y + 3, C["bone"])
        else:
            line(img, x, y, x + rnd.randrange(3, 7), y + rnd.choice((-1, 0, 1)), C["bone"])
            put(img, x - 1, y, C["bone"])
    for i in range(30):
        x = rnd.randrange(W)
        y = rnd.randrange(30, 46)
        line(img, x, y, x + rnd.randrange(-6, 6), y + rnd.randrange(2, 5), C["soil_hi"])

    def headstone(x, kind):
        g = int(round(ys[(x + 4) % W]))
        if kind == 0:   # rounded slab
            hgt, w = rnd.randrange(12, 18), rnd.choice((9, 10, 11))
            top = g - hgt
            rect(img, x, top + 3, x + w - 1, g + 1, C["stone"])
            ellipse(img, x + w / 2, top + 4, w / 2, 4, C["stone"])
            rect(img, x, top + 3, x, g + 1, C["stone_l"])
            for yy in range(top, g + 2):
                for xx in range(x, x + w):
                    if img[yy, xx % W] == C["stone"] and (xx == x or img[yy, (xx - 1) % W] != C["stone"] and img[yy, (xx - 1) % W] != C["stone_l"]):
                        img[yy, xx % W] = C["stone_l"]
            rect(img, x + w - 2, top + 3, x + w - 1, g + 1, C["stone_d"])
            # R.I.P.
            ry = top + 6
            for dx, dy in ((2, 0), (2, 1), (2, 2), (3, 0), (4, 1), (3, 1), (4, 2)):
                put(img, x + dx, ry + dy, C["stone_d"])
            rect(img, x + 2, ry + 4, x + w - 4, ry + 4, C["stone_d"])
            rect(img, x + 2, ry + 6, x + w - 5, ry + 6, C["stone_d"])
            if rnd.random() < 0.5:
                rect(img, x, g - 2, x + 2, g + 1, C["moss"])
        elif kind == 1:  # a cross
            hgt = rnd.randrange(14, 20)
            top = g - hgt
            rect(img, x + 3, top, x + 5, g + 1, C["stone"])
            rect(img, x, top + 3, x + 8, top + 5, C["stone"])
            rect(img, x + 3, top, x + 3, g + 1, C["stone_l"])
            rect(img, x, top + 3, x + 8, top + 3, C["stone_l"])
            rect(img, x + 5, top + 6, x + 5, g + 1, C["stone_d"])
        elif kind == 2:  # a leaning slab, cracked
            hgt = rnd.randrange(10, 14)
            for k in range(hgt):
                xo = k // 4
                rect(img, x + xo, g - k, x + xo + 7, g - k, C["stone_d"] if k > hgt - 3 else C["stone"])
                put(img, x + xo, g - k, C["stone_l"])
            line(img, x + 3, g - hgt + 2, x + 5, g - 3, C["stone_d"])
        elif kind == 3:  # an obelisk
            hgt = rnd.randrange(22, 27)
            top = g - hgt
            poly(img, [(x + 1, g + 1), (x + 2, top + 3), (x + 4, top), (x + 6, top + 3), (x + 7, g + 1)], C["stone"])
            line(img, x + 2, top + 3, x + 1, g + 1, C["stone_l"])
            line(img, x + 4, top, x + 2, top + 3, C["stone_h"])
            line(img, x + 6, top + 3, x + 7, g + 1, C["stone_d"])
            rect(img, x - 1, g - 1, x + 9, g + 1, C["stone_d"])
            rect(img, x - 1, g - 1, x + 9, g - 1, C["stone_l"])

    def fence(x0, n):
        for i in range(n):
            x = x0 + i * 4
            g = int(round(ys[x % W]))
            rect(img, x, g - 13, x, g + 1, C["iron"])
            put(img, x, g - 14, C["iron"])
            put(img, x - 1, g - 12, C["iron"])
            put(img, x + 1, g - 12, C["iron"])
        for i in range(n * 4 - 3):
            x = x0 + i
            g = int(round(ys[x % W]))
            put(img, x, g - 10, C["iron"])
            put(img, x, g - 4, C["iron"])

    def lantern(x):
        g = int(round(ys[x % W]))
        ellipse(img, x + 4, g - 3, 5, 4, C["pump"])
        for yy in range(g - 7, g + 1):
            for xx in range(x - 1, x + 10):
                if img[yy, xx % W] == C["pump"]:
                    if xx < x + 1 or (xx - x) % 3 == 0:
                        img[yy, xx % W] = C["pump_d"]
                    elif yy < g - 5:
                        img[yy, xx % W] = C["pump_l"]
        rect(img, x + 4, g - 9, x + 4, g - 7, C["stem"])
        put(img, x + 5, g - 9, C["stem"])
        # the face glows
        put(img, x + 2, g - 4, C["glow"][0]); put(img, x + 6, g - 4, C["glow"][0])
        put(img, x + 2, g - 5, C["glow"][1]); put(img, x + 6, g - 5, C["glow"][1])
        rect(img, x + 2, g - 2, x + 6, g - 2, C["glow"][1])
        put(img, x + 3, g - 1, C["glow"][0]); put(img, x + 5, g - 1, C["glow"][0])

    x = 6
    k = 0
    while x < W - 30:
        r = rnd.random()
        if r < 0.12:
            n = rnd.randrange(5, 10)
            fence(x, n)
            x += n * 4 + rnd.randrange(8, 20)
        elif r < 0.24:
            lantern(x)
            x += 12 + rnd.randrange(6, 20)
        elif r < 0.30:
            # a dead tree on the hill
            g = int(round(ys[x % W]))
            tree(img, x, g, rnd.uniform(8, 10), math.pi / 2 + rnd.uniform(-0.2, 0.2), 6, rnd, C["tree"])
            x += 30
        else:
            kind = rnd.choice((0, 0, 0, 1, 1, 2, 3))
            headstone(x, kind)
            x += rnd.randrange(16, 34)
        k += 1
    return img, Y0


# ---------------------------------------------------------------- the font

FONT = {
    'A': "01110 10001 10001 11111 10001 10001 10001", 'B': "11110 10001 10001 11110 10001 10001 11110",
    'C': "01110 10001 10000 10000 10000 10001 01110", 'D': "11100 10010 10001 10001 10001 10010 11100",
    'E': "11111 10000 10000 11110 10000 10000 11111", 'F': "11111 10000 10000 11110 10000 10000 10000",
    'G': "01110 10001 10000 10111 10001 10001 01111", 'H': "10001 10001 10001 11111 10001 10001 10001",
    'I': "01110 00100 00100 00100 00100 00100 01110", 'J': "00111 00010 00010 00010 00010 10010 01100",
    'K': "10001 10010 10100 11000 10100 10010 10001", 'L': "10000 10000 10000 10000 10000 10000 11111",
    'M': "10001 11011 10101 10101 10001 10001 10001", 'N': "10001 10001 11001 10101 10011 10001 10001",
    'O': "01110 10001 10001 10001 10001 10001 01110", 'P': "11110 10001 10001 11110 10000 10000 10000",
    'Q': "01110 10001 10001 10001 10101 10010 01101", 'R': "11110 10001 10001 11110 10100 10010 10001",
    'S': "01111 10000 10000 01110 00001 00001 11110", 'T': "11111 00100 00100 00100 00100 00100 00100",
    'U': "10001 10001 10001 10001 10001 10001 01110", 'V': "10001 10001 10001 10001 10001 01010 00100",
    'W': "10001 10001 10001 10101 10101 10101 01010", 'X': "10001 10001 01010 00100 01010 10001 10001",
    'Y': "10001 10001 10001 01010 00100 00100 00100", 'Z': "11111 00001 00010 00100 01000 10000 11111",
    '0': "01110 10001 10011 10101 11001 10001 01110", '1': "00100 01100 00100 00100 00100 00100 01110",
    '2': "01110 10001 00001 00010 00100 01000 11111", '3': "11111 00010 00100 00010 00001 10001 01110",
    '4': "00010 00110 01010 10010 11111 00010 00010", '5': "11111 10000 11110 00001 00001 10001 01110",
    '6': "00110 01000 10000 11110 10001 10001 01110", '7': "11111 00001 00010 00100 01000 01000 01000",
    '8': "01110 10001 10001 01110 10001 10001 01110", '9': "01110 10001 10001 01111 00001 00010 01100",
    '!': "00100 00100 00100 00100 00100 00000 00100", '"': "01010 01010 01010 00000 00000 00000 00000",
    "'": "00100 00100 01000 00000 00000 00000 00000", '(': "00010 00100 01000 01000 01000 00100 00010",
    ')': "01000 00100 00010 00010 00010 00100 01000", ',': "00000 00000 00000 00000 01100 00100 01000",
    '-': "00000 00000 00000 11111 00000 00000 00000", '.': "00000 00000 00000 00000 00000 01100 01100",
    '/': "00001 00010 00010 00100 01000 01000 10000", ':': "00000 01100 01100 00000 01100 01100 00000",
    '?': "01110 10001 00001 00010 00100 00000 00100", '*': "00000 00100 10101 01110 10101 00100 00000",
    '&': "01100 10010 10100 01000 10101 10010 01101", '=': "00000 00000 11111 00000 11111 00000 00000",
    '+': "00000 00100 00100 11111 00100 00100 00000", '@': "01110 10001 10111 10101 10111 10000 01110",
    # a little pumpkin and a heart for the HUD ('[' and ']')
    '[': "00100 01110 10101 11111 10101 11011 01110", ']': "01010 11111 11111 11111 01110 00100 00000",
}


def glyph_bits(ch):
    rows = FONT[ch].split()
    return [[r[i] == '1' for i in range(5)] for r in rows]


def font_cell(ch, bg):
    """An 8 x 8 cell: the glyph with its ramp and a shadow; bg: 8 x 8 background."""
    cell = bg.copy()
    if ch == ' ':
        return cell
    bits = glyph_bits(ch)
    for y in range(7):
        for x in range(5):
            if bits[y][x] and y + 1 < 8 and x + 2 < 8:
                cell[y + 1, x + 2] = C["font_sh"]
    ramp = C["font"]
    if ch == '[':
        ramp = [C["pump_l"], C["pump_l"], C["pump"], C["pump"], C["pump"], C["pump_d"], C["pump_d"]]
    if ch == ']':
        ramp = [P("#ff6060"), P("#ff4040"), P("#e02828"), P("#c01818"), P("#a01010"), P("#801010"), P("#801010")]
    for y in range(7):
        for x in range(5):
            if bits[y][x]:
                cell[y, x + 1] = ramp[y]
    return cell


# ---------------------------------------------------------------- the logo


def draw_logo(img, text, x0, y0, s):
    """Big letters: the font scaled s times, a ramp, an outline and drips."""
    h, w = img.shape
    mask = np.zeros((h, w), dtype=bool)
    x = x0
    for ch in text:
        if ch != ' ':
            bits = glyph_bits(ch)
            for gy in range(7):
                for gx in range(5):
                    if bits[gy][gx]:
                        mask[y0 + gy * s:y0 + (gy + 1) * s, x + gx * s:x + (gx + 1) * s] = True
        x += 6 * s
    # shadow (down right), outline, the fill's ramp, a bevel
    sh = np.zeros_like(mask)
    sh[3:, 3:] = mask[:-3, :-3]
    out = np.zeros_like(mask)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            out |= np.roll(np.roll(mask, dy, 0), dx, 1)
    img[sh & ~out] = C["logo_sh"]
    img[out & ~mask] = P("#100410")
    ramp = C["logo"]
    for y in range(h):
        for xx in range(w):
            if mask[y, xx]:
                t = (y - y0) / (7 * s)
                k = min(int(t * len(ramp) + BAYER[y & 3][xx & 3] * 0.9), len(ramp) - 1)
                img[y, xx] = ramp[k]
                if not mask[y - 1, xx] or not mask[y, xx - 1]:
                    img[y, xx] = ramp[0]
    # slime drips from the letters' lower edges
    rnd = random.Random(13)
    for xx in range(w):
        for y in range(h - 1):
            if mask[y, xx] and not mask[y + 1, xx] and rnd.random() < 0.08:
                n = rnd.randrange(3, 9)
                for k in range(n):
                    if y + 1 + k < h:
                        img[y + 1 + k, xx] = C["slime"] if k < n - 1 else C["slime_d"]
                        if k < n - 2 and xx + 1 < w and rnd.random() < 0.5:
                            img[y + 1 + k, xx + 1] = C["slime_d"]


def title_picture(far, mid):
    img = canvas(LCOLS * 8, 200)
    for y in range(200):
        for x in range(8):
            img[y, x::8] = sky_pixel(x, int(y * 0.8))
    mx, my, mr = 152, 46, 34
    draw_stars(img, 300, 4, avoid=lambda x, y: math.hypot(x - mx, y - my) < mr + 12 or y > 96)
    draw_moon(img, mx, my, mr)
    draw_cloud(img, 40, 70, 90, 5)
    draw_cloud(img, 190, 78, 80, 6)
    draw_logo(img, "WITCHING", 152 - (8 * 24 - 4) // 2, 10, 4)
    draw_logo(img, "HOUR", 152 - (4 * 24 - 4) // 2, 46, 4)
    # the far hills and castle along the bottom of the sky, then black
    fy = 80
    for y in range(48):
        for x in range(LCOLS * 8):
            c = far[y, (x + 120) % far.shape[1]]
            if c in _sky_cols:
                if img[fy + y, x] not in _sky_cols and img[fy + y, x] != 0:
                    continue          # the logo's drips, the moon: in front of the sky
                c = sky_pixel(x, int((fy + y) * 0.8))
            img[fy + y, x] = c
    img[fy + 40:fy + 44, :] = C["far"]
    img[fy + 44:fy + 48, :] = C["mid"]
    img[fy + 48:, :] = 0
    # a row of headstones in silhouette at the very bottom
    rnd = random.Random(2)
    x = 2
    while x < LCOLS * 8 - 10:
        hgt = rnd.randrange(6, 12)
        if rnd.random() < 0.3:
            rect(img, x + 3, 199 - hgt, x + 4, 199, C["mid"])
            rect(img, x + 1, 199 - hgt + 2, x + 6, 199 - hgt + 3, C["mid"])
        else:
            rect(img, x, 199 - hgt + 2, x + 6, 199, C["mid"])
            ellipse(img, x + 3.5, 199 - hgt + 3, 3.5, 2.5, C["mid"])
        x += rnd.randrange(14, 30)
    return img


# ---------------------------------------------------------------- the characters

class Chars:
    def __init__(self):
        self.cells = []
        self.where = {}

    def __call__(self, cell):
        b = cell.astype(np.uint8).tobytes()
        if b not in self.where:
            # char 1023 (the last of bank 4) is left out: WITCHGFX1 stays under 64 KB
            if len(self.cells) == 1023:
                self.cells.append(bytes(64))
            self.where[b] = len(self.cells)
            self.cells.append(b)
        return CHAR_BASE + self.where[b]


CH = Chars()


def cut(img):
    h, w = img.shape
    return [[CH(img[r * 8:(r + 1) * 8, c * 8:(c + 1) * 8]) for c in range(w // 8)] for r in range(h // 8)]


# ---------------------------------------------------------------- sprites

SPAL = SP.PALETTE      # 16 (r, g, b); 0 transparent


def sprite_bytes(rows, height):
    """16 x height pixels (palette letters) -> 8 bytes a row, the left pixel in the high nibble... """
    out = bytearray()
    for y in range(height):
        r = rows[y] if y < len(rows) else "." * 16
        for x in range(0, 16, 2):
            a = SP.KEY[r[x]]
            b = SP.KEY[r[x + 1]]
            out.append(a << 4 | b)
    return bytes(out)


# ---------------------------------------------------------------- output


def main():
    build = sys.argv[1]
    preview = "--preview" in sys.argv
    os.makedirs(build, exist_ok=True)
    colours()
    for k in range(SKY_SHADES):
        sky_index(k)

    sky = game_sky()
    far, fy0 = far_band()
    mid, my0 = mid_band()
    fore, ry0 = fore_band()
    title = title_picture(far, mid)

    # the font: on black (the HUD and the title), and on the sky's rows 3 and 5
    order = [chr(c) for c in range(32, 96)]

    # glyphs must be consecutive: start each font at the end of the list
    def fresh_font(bg):
        start = len(CH.cells)
        if start <= 1023 < start + len(order):
            while len(CH.cells) < 1024:
                CH.cells.append(bytes(64))
            start = 1024
        for ch in order:
            cell = font_cell(ch if ch in FONT else ' ', bg)
            CH.cells.append(cell.astype(np.uint8).tobytes())
        return CHAR_BASE + start
    font_black = fresh_font(np.zeros((8, 8), dtype=np.uint8))
    font_sky = {}
    for r in (3, 5):
        bg = canvas(8, 8)
        sky_fill(bg, r * 8)
        font_sky[r] = fresh_font(bg)

    # the objects: 8 characters each, in order (column A's strip, column B's)
    obj_at = {}
    spr_idx = [0] + [P(c) for c in SPAL[1:]]
    for name, rows in SP.objects():
        start = len(CH.cells)
        if start <= 1023 < start + 8:
            while len(CH.cells) < 1024:
                CH.cells.append(bytes(64))
            start = 1024
        obj_at[name] = CHAR_BASE + start
        for col in range(2):
            px = np.zeros((32, 8), dtype=np.uint8)
            for y in range(16):
                for x in range(8):
                    px[8 + y, x] = spr_idx[SP.KEY[rows[y][col * 8 + x]]]
            for k in range(4):
                CH.cells.append(px[k * 8:(k + 1) * 8].tobytes())

    sky_map = cut(sky)                     # 8 rows x 41
    bands = []
    for name, img, y0, speed in (("far", far, fy0, 0x40), ("mid", mid, my0, 0x80), ("fore", fore, ry0, 0x100)):
        m = cut(img)
        period = img.shape[1] // 8
        rows = [r + r[:LCOLS] for r in m]
        bands.append((name, y0 // 8, len(m), period, speed, rows))
    title_map = cut(title)

    # the game's picture: HUD row black, the sky, the bands at 0
    game_map = [list(r) for r in sky_map]
    for name, row0, nrows, period, speed, rows in bands:
        for r in rows:
            game_map.append(r[:LCOLS])
    assert len(game_map) == ROWS, len(game_map)
    blank = CH(np.zeros((8, 8), dtype=np.uint8))
    for c in range(LCOLS):
        game_map[0][c] = blank

    nchars = len(CH.cells)
    print("characters: %d (%d KB)" % (nchars, nchars * 64 // 1024))

    # ---- WITCHGFX2: the rest of the characters, sprites, maps, pictures, samples
    gfx1 = b"".join(CH.cells[:1023])
    rest = b"".join(CH.cells[1024:]) if nchars > 1024 else b""
    g2 = bytearray(rest)
    def align(n):
        while len(g2) % n:
            g2.append(0)
    syms = {}
    # sprites
    align(64)
    img_ptr = {}
    for name, rows, tall in SP.all_images():
        align(64)
        img_ptr[name] = (GFX2 + len(g2)) // 64
        g2 += sprite_bytes(rows, 48 if tall else 21)
    # maps (16-bit cells)
    def cells(m):
        return b"".join(c.to_bytes(2, "little") for row in m for c in row)
    band_at = []
    for name, row0, nrows, period, speed, rows in bands:
        align(2)
        band_at.append(GFX2 + len(g2))
        g2 += cells(rows)
    align(2)
    syms["SCR_GAME"] = GFX2 + len(g2)
    g2 += cells(game_map)
    syms["SCR_TITLE"] = GFX2 + len(g2)
    g2 += cells(title_map)
    # samples (8-bit unsigned)
    g3 = bytearray()
    snd = sounds.make()
    for name, data, rate in snd:
        syms["SND_" + name] = GFX3 + len(g3)
        syms["SND_%s_LEN" % name] = len(data)
        syms["SND_%s_RATE" % name] = int(rate * (1 << 24) / 40500000)
        g3 += data
    end = GFX2 + len(g2)
    print("WITCHGFX1 %d bytes, WITCHGFX2 %d bytes (to $%05X, free %d), WITCHGFX3 %d (free %d)" % (
        len(gfx1), len(g2), end, GFX2_END - end, len(g3), GFX3_END - GFX3 - len(g3)))
    too_big = end > GFX2_END or GFX3 + len(g3) > GFX3_END
    with open(os.path.join(build, "witchgfx3.bin"), "wb") as f:
        f.write(bytes([GFX3 & 0xFF, (GFX3 >> 8) & 0xFF]) + g3)

    with open(os.path.join(build, "witchgfx1.bin"), "wb") as f:
        f.write(bytes([0x00, 0x00]) + gfx1)
    with open(os.path.join(build, "witchgfx2.bin"), "wb") as f:
        f.write(bytes([0x00, 0x00]) + g2)

    # ---- gfx.h / gfx.c
    h = ["/* gfx.h — made by tools/gen_art.py: do not edit */", "#pragma once", ""]
    h.append("#define NBANDS %d" % len(bands))
    h.append("#define FONT_CHAR 0x%04X        /* ' ' of the font on black */" % font_black)
    for r, v in font_sky.items():
        h.append("#define FONT_SKY%d 0x%04X       /* the font on the sky's row %d */" % (r, v, r))
    h.append("#define BLANK_CHAR 0x%04X" % blank)
    for k, v in syms.items():
        h.append("#define %s 0x%XUL" % (k, v) if not k.endswith(("_LEN", "_RATE")) else "#define %s %dUL" % (k, v))
    for k, v in obj_at.items():
        h.append("#define OBJ_%s 0x%04X" % (k.upper(), v))
    for k, v in img_ptr.items():
        h.append("#define IMG_%s 0x%04X" % (k.upper(), v))
    h.append("#define CYCLE_N %d" % 7)
    h += ["extern const unsigned char band_row[NBANDS], band_rows[NBANDS];",
          "extern const unsigned int band_period[NBANDS], band_stride[NBANDS], band_speed[NBANDS];",
          "extern const unsigned long band_map[NBANDS];",
          "extern const unsigned char pal_r[256], pal_g[256], pal_b[256];",
          "extern const unsigned char spal_r[16], spal_g[16], spal_b[16];",
          "extern const unsigned char cycle_entry[CYCLE_N], cycle_src[CYCLE_N * 4];", ""]
    with open(os.path.join(build, "gfx.h"), "w") as f:
        f.write("\n".join(h))

    def arr(t, name, vals, fmt="%d"):
        return "const %s %s[%d] = {%s};" % (t, name, len(vals), ", ".join(fmt % v for v in vals))
    rgb = P.rgb + [(0, 0, 0)] * (256 - len(P.rgb))
    cyc = C["star"] + C["win"] + C["glow"]
    src = []
    for i, e in enumerate(cyc):
        s = C["star_src"] if i < 3 else (C["win_src"] if i < 5 else C["glow_src"])
        src += s
    c = ['/* gfx.c — made by tools/gen_art.py: do not edit */', '#include "gfx.h"',
         arr("unsigned char", "band_row", [b[1] for b in bands]),
         arr("unsigned char", "band_rows", [b[2] for b in bands]),
         arr("unsigned int", "band_period", [b[3] for b in bands]),
         arr("unsigned int", "band_stride", [(b[3] + LCOLS) * 2 for b in bands]),
         arr("unsigned int", "band_speed", [b[4] for b in bands]),
         arr("unsigned long", "band_map", band_at, "0x%XUL"),
         arr("unsigned char", "pal_r", [x[0] for x in rgb]),
         arr("unsigned char", "pal_g", [x[1] for x in rgb]),
         arr("unsigned char", "pal_b", [x[2] for x in rgb]),
         arr("unsigned char", "spal_r", [x[0] for x in SPAL]),
         arr("unsigned char", "spal_g", [x[1] for x in SPAL]),
         arr("unsigned char", "spal_b", [x[2] for x in SPAL]),
         arr("unsigned char", "cycle_entry", cyc),
         arr("unsigned char", "cycle_src", src), ""]
    with open(os.path.join(build, "gfx.c"), "w") as f:
        f.write("\n".join(c))
    print("palette: %d colours" % len(P.rgb))

    if preview:
        pal = []
        for x in rgb:
            pal += list(x)
        def save(a, name, scale=2):
            im = Image.fromarray(a, "P")
            im.putpalette(pal)
            im = im.convert("RGB").resize((a.shape[1] * scale, a.shape[0] * scale), Image.NEAREST)
            im.save(os.path.join(build, name))
        scr = np.concatenate([sky, far[:, :LCOLS * 8], mid[:, :LCOLS * 8], fore[:, :LCOLS * 8]])
        save(scr[:, :320], "preview_game.png")
        save(title[:, :320], "preview_title.png")
        save(fore, "preview_fore.png", 1)
        SP.preview(os.path.join(build, "preview_sprites.png"))
    if too_big:
        raise SystemExit("too big")


if __name__ == "__main__":
    main()
