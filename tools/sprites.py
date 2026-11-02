"""sprites.py — the sprites of Witching Hour, drawn as text

A character a pixel; the letters are the sprite palette's 15 colours (every
hardware sprite shares them), '.' is transparent. Sprites are 16 pixels
wide, 21 high (or 48: tall). Wider things are several side by side: the
witch is two, the bosses three tall ones.
"""
import math
import random

PALETTE = [
    (0, 0, 0),          # . transparent
    (18, 10, 24),       # k outline
    (248, 248, 248),    # w white
    (74, 24, 104),      # P dark purple
    (138, 60, 192),     # p purple
    (208, 160, 240),    # l lilac
    (184, 74, 8),       # O dark orange
    (255, 140, 24),     # o orange
    (255, 224, 72),     # y yellow
    (30, 106, 36),      # G dark green
    (92, 200, 74),      # g green
    (192, 224, 255),    # c ghost blue
    (138, 138, 154),    # a grey
    (74, 74, 90),       # A dark grey
    (232, 40, 40),      # r red
    (122, 68, 32),      # b brown
]
KEY = {ch: i for i, ch in enumerate(".kwPplOoyGgcaArb")}

W = 16


def pad(rows, h=21, top=None):
    """Rows (any width up to 16) centred into 16 x h."""
    rows = [r.ljust(W, ".")[:W] if len(r) <= W else r for r in rows]
    if top is None:
        top = (h - len(rows)) // 2
    out = ["." * W] * top + rows
    out += ["." * W] * (h - len(out))
    return out[:h]


def split(rows, n, h):
    """A picture n * 16 wide into n sprites."""
    rows = [r.ljust(W * n, ".") for r in rows]
    rows += ["." * (W * n)] * (h - len(rows))
    return [[r[i * W:(i + 1) * W] for r in rows[:h]] for i in range(n)]


# ---------------------------------------------------------------- the witch (32 x 21)

WITCH_A = [
    "................................",
    "........kk......................",
    ".........kk.....................",
    "..........kPk...................",
    "..........kPPk..................",
    ".........kPPPPk.................",
    ".........kPPPPPk................",
    "........kPPPPPPk................",
    "........kllllllk................",
    "......kkkkkkkkkkkkkk............",
    ".......kooogggggk...............",
    "......kooogkgggggk..............",
    ".....kPoooggggggggk.............",
    "....kPPPokggggkkkk..............",
    "..kkPpppPkkggk..................",
    ".yokPpppppPPkgk.................",
    "yyoobbbbbbbbbbbbbbbbbbbbbbbbbbk.",
    "yoOOkPpppPPkbbk.................",
    ".yoOkkPPPkk.kkk.................",
    "..yo...kk.......................",
    "................................",
]
WITCH_B = [
    "................................",
    "................................",
    ".......kk.......................",
    "........kkPk....................",
    "..........kPPk..................",
    ".........kPPPPk.................",
    ".........kPPPPPk................",
    "........kPPPPPPk................",
    "........kllllllk................",
    "......kkkkkkkkkkkkkk............",
    ".......kooogggggk...............",
    "......kooogkgggggk..............",
    "....kPPoooggggggggk.............",
    "...kPPPPokggggkkkk..............",
    ".kkPppppPkkggk..................",
    "yokPppppppPkkgk.................",
    "yyyoObbbbbbbbbbbbbbbbbbbbbbbbbk.",
    ".yoOOkPpppPPkbbk................",
    "..yOkkkPPkk..kkk................",
    "...y............................",
    "................................",
]

# ---------------------------------------------------------------- the witch's spell (a star)

SHOT_A = pad([
    "..........y.....",
    ".........yyy....",
    "..l..p..lwwwy...",
    "plplppwwwwwwwyy.",
    "..l..p..lwwwy...",
    ".........yyy....",
    "..........y.....",
])
SHOT_B = pad([
    ".........y.y....",
    "..........y.....",
    ".l..p..llwwwy...",
    "lplpppwwwwwwwwy.",
    ".l..p..llwwwy...",
    "..........y.....",
    ".........y.y....",
])

# ---------------------------------------------------------------- a bat (three frames)

BAT_UP = pad([
    "k.............k.",
    "kk...........kk.",
    "kPk.........kPk.",
    "kPPk..k.k..kPPk.",
    ".kPPk.kkk.kPPk..",
    ".kPPPkkkkkPPPk..",
    "..kPPkrkrkPPk...",
    "...kkPkkkPkk....",
    ".....kwkwk......",
    "......k.k.......",
])
BAT_MID = pad([
    ".....k...k......",
    ".....kkkkk......",
    "kk..kkrkrkk..kk.",
    "kPPkPkkkkkPkPPk.",
    ".kPPPPkwkPPPPk..",
    "..kkPPkkkPPkk...",
    "....kk.k.kk.....",
])
BAT_DOWN = pad([
    ".....k...k......",
    ".....kkkkk......",
    "....kkrkrkk.....",
    "...kPPkkkPPk....",
    "..kPPPkwkPPPk...",
    ".kPPPk.k.kPPPk..",
    ".kPPk.....kPPk..",
    "kPk.........kPk.",
    "kk...........kk.",
])

# ---------------------------------------------------------------- a ghost (two frames)

GHOST_TOP = [
    ".....kkkkkk.....",
    "....kwwwwwwkk...",
    "...kwwwwwwccck..",
    "..kwwkkwwkkccck.",
    "..kwkkkwkkkccck.",
    "..kwwkkwwkkwcck.",
    ".kwwwwwwwwwwcck.",
    ".kwwwwkkkwwwcck.",
    "kkwwwkkkkkwwccck",
    "kckwwwkkkwwwcckk",
    ".kwwwwwwwwwccck.",
    ".kwwwwwwwwwccck.",
    ".kcwwwwwwwwcccck",
]
GHOST_A = pad(GHOST_TOP + [
    ".kccwcwwccwccck.",
    ".kcck.kcck.kcck.",
    "..kk...kk...kk..",
])
GHOST_B = pad(GHOST_TOP + [
    ".kcwccwcccwccck.",
    "..kcck.kcck.kck.",
    "...kk...kk...k..",
])

# ---------------------------------------------------------------- a flying jack-o'-lantern

PUMP_A = pad([
    ".......kk.......",
    "......kGgk......",
    "...kkkkGkkkkk...",
    "..kOooOoooOook..",
    ".kOooOoooOoooOk.",
    ".kOkyyOoooyykOk.",
    "kOoyyyOoooyyyoOk",
    "kOoooooykooooOOk",
    "kOooooOyyoooooOk",
    "kOykoykyykoykyOk",
    "kOoyyyyyyyyyyoOk",
    ".kOoyykyykyyoOk.",
    ".kOOoooOooooOOk.",
    "..kkOOOOOOOOkk..",
    "....kkkkkkkk....",
])
PUMP_B = pad([
    ".......kk.......",
    "......kGgk......",
    "...kkkkGkkkkk...",
    "..kOooOoooOook..",
    ".kOooOoooOoooOk.",
    ".kOkwwOoooywkOk.",
    "kOoywyOoooywyoOk",
    "kOoooooykooooOOk",
    "kOooooOywoooooOk",
    "kOykoykwykoykyOk",
    "kOoywwwwwwwwyoOk",
    ".kOoyykyykyyoOk.",
    ".kOOoooOooooOOk.",
    "..kkOOOOOOOOkk..",
    "....kkkkkkkk....",
])

# ---------------------------------------------------------------- a flaming skull

SKULL_BODY = [
    "...kkkkkkkkkk...",
    "..kwwwwwwwwwak..",
    ".kwwwwwwwwwwwak.",
    ".kwwkkkwwkkkwak.",
    ".kwkkgkkwkgkkak.",
    ".kwkkkkwwkkkkak.",
    ".kwwkkwkkwkkwak.",
    "..kwwwwkkwwwak..",
    "..kkwwwwwwwakk..",
    "...kwkwkwkwak...",
    "...kawawawaak...",
    "....kkkkkkkk....",
]
SKULL_A = pad([
    "......g..g......",
    "....g.gG.gg.g...",
    "...gGgGggGgGg...",
] + SKULL_BODY)
SKULL_B = pad([
    ".....g...g..g...",
    "...g.gGg.gGg....",
    "...gGgGgGgGgg...",
] + SKULL_BODY)

# ---------------------------------------------------------------- bullets

FIRE_A = pad([
    "......ooo.......",
    ".....oyyyo......",
    "....oywwwyo.....",
    "....oywwwyo.....",
    ".....oyyyo......",
    "......ooo.......",
])
FIRE_B = pad([
    "......rOr.......",
    ".....Ooooo......",
    "....Ooyyyor.....",
    "....roywyoO.....",
    ".....ooyoo......",
    "......OrO.......",
])
TOXIC_A = pad([
    "......GGG.......",
    ".....Gggg G.....".replace(" ", "."),
    "....Ggcwcg......",
    "....Ggwwwg......",
    ".....Gggg.......",
    "......GG........",
])
TOXIC_B = pad([
    ".....G..........",
    "......GGG.......",
    ".....Gggcg......",
    "....Ggwwcg......",
    ".....ggcgG......",
    "......GG........",
])
SEED_A = pad([
    "......kk........",
    ".....kwyk.......",
    ".....kyyk.......",
    "......kk........",
])

# ---------------------------------------------------------------- pickups

CANDY = pad([
    "......kk........",
    ".....kwwk.......",
    ".....kwwk.......",
    "....kwwwwk......",
    "....kyyyyk......",
    "...kyyyyyyk.....",
    "...kyyyyyyk.....",
    "..kooooooook....",
    "..koooooooOk....",
    ".koooooooooOk...",
    ".kkkkkkkkkkkk...",
])
POTION = pad([
    "......kkkk......",
    "......kbbk......",
    ".......kk.......",
    "......kcck......",
    "......kcck......",
    ".....kcwcck.....",
    "....kplpppPk....",
    "...kpwlpppPPk...",
    "...kplppppPPk...",
    "...kppppppPPk...",
    "....kPPPPPPk....",
    ".....kkkkkk.....",
])
CAT = pad([
    "..k.........k...",
    "..kk.......kk...",
    "..kAk.....kAk...",
    "..kAAkkkkkAAk...",
    "..kAAAAAAAAAk...",
    ".kAAyyAAAyyAAk..",
    ".kAAykAAAkyAAk..",
    ".kAAAAAlAAAAAk..",
    "kwkAAAAAAAAAkwk.",
    ".kwkAAkAkAAkwk..",
    "...kkAAAAAkk....",
    ".....kkkkk......",
])

# ---------------------------------------------------------------- the bosses (48 x 48)


def _in_tri(x, y, a, b, c):
    def side(p, q, r):
        return (r[0] - q[0]) * (p[1] - q[1]) - (r[1] - q[1]) * (p[0] - q[0])
    d1, d2, d3 = side((x, y), a, b), side((x, y), b, c), side((x, y), c, a)
    neg = d1 < 0 or d2 < 0 or d3 < 0
    pos = d1 > 0 or d2 > 0 or d3 > 0
    return not (neg and pos)


def boss_pumpkin(frame):
    """The Pumpkin King: a great lantern with a crown, its grin open or shut."""
    g = [["."] * 48 for _ in range(48)]
    cx, cy = 24, 28
    for y in range(48):
        for x in range(48):
            dx, dy = (x + 0.5 - cx) / 22.5, (y + 0.5 - cy) / 18.5
            if dx * dx + dy * dy <= 1:
                rib = any(abs(dx - r) < 0.045 for r in (-0.68, -0.3, 0.3, 0.68))
                shade = "o"
                if rib or dx < -0.8 or dy > 0.82:
                    shade = "O"
                elif dx > 0.15 and dx < 0.45 and dy < -0.25:
                    shade = "y"                     # the moon's light
                g[y][x] = shade
    out = [row[:] for row in g]
    for y in range(48):
        for x in range(48):
            if g[y][x] == ".":
                for ddx, ddy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    xx, yy = x + ddx, y + ddy
                    if 0 <= xx < 48 and 0 <= yy < 48 and g[yy][xx] != ".":
                        out[y][x] = "k"
    g = out
    for y in range(7, 11):
        for x in range(22, 27):
            g[y][x] = "G" if x < 24 else "g"
    crown = [
        "...y.....y.....y...",
        "..yry...yry...yry..",
        "..yyy...yyy...yyy..",
        "..yyyy.yyyyy.yyyy..",
        "..yyyyyyyyyyyyyyy..",
        "..yOyryOyOyryOyOy..",
        "..yyyyyyyyyyyyyyy..",
    ]
    for j, row in enumerate(crown):
        for i, ch in enumerate(row):
            if ch != ".":
                g[2 + j][14 + i] = ch
    hot = "w" if frame else "y"
    def fill_tri(a, b, c):
        for y in range(48):
            for x in range(48):
                if _in_tri(x + 0.5, y + 0.5, a, b, c):
                    near = not (_in_tri(x + 1.5, y + 0.5, a, b, c) and _in_tri(x - 0.5, y + 0.5, a, b, c)
                                and _in_tri(x + 0.5, y + 1.5, a, b, c) and _in_tri(x + 0.5, y - 0.5, a, b, c))
                    g[y][x] = "O" if near else (hot if y > min(a[1], b[1], c[1]) + 3 else "y")
    # angry eyes, slanting in, a nose
    fill_tri((10, 26), (21, 26), (19, 17))
    fill_tri((27, 26), (38, 26), (29, 17))
    fill_tri((21, 31), (27, 31), (24, 27))
    # the grin: a crescent with teeth
    top, depth = 33, (9 if frame else 6)
    for x in range(10, 39):
        w = 1 - ((x - 24) / 14.5) ** 2
        if w <= 0:
            continue
        y0 = top - int(3 * w)
        y1 = top + int(depth * w) + 1
        for y in range(y0, y1 + 1):
            g[y][x] = "O" if y in (y0, y1) else hot if y > y0 + 1 else "y"
        if x % 5 == 1:                  # teeth from above
            g[y0 + 1][x] = g[y0 + 2][x] = "o"
        if x % 5 == 3 and y1 - 2 > y0 + 2:  # and below
            g[y1 - 1][x] = g[y1 - 2][x] = "o"
    return ["".join(r) for r in g]


def boss_skull(frame):
    """The Bone Lord: a great horned skull, its jaw dropping."""
    g = [["."] * 48 for _ in range(48)]
    cx, cy = 24, 20
    for y in range(48):
        for x in range(48):
            dx, dy = (x + 0.5 - cx) / 18, (y + 0.5 - cy) / 16
            if dx * dx + dy * dy <= 1:
                g[y][x] = "a" if dx > 0.55 or dy > 0.6 else "w"
    jaw = 6 if frame else 0
    for y in range(31, 44):
        for x in range(13, 35):
            yy = y - jaw
            if yy >= 31 and y < 42 and abs(x - 24) < 11 - (y - 31 - jaw) * 0.4:
                g[y][x] = "a" if x > 30 else "w"
    # cheekbones and the gap between the jaws
    for y in range(30, 30 + jaw + 2):
        for x in range(15, 33):
            if g[y][x] != ".":
                g[y][x] = "k" if 16 < x < 32 else g[y][x]
    for y in range(30, 30 + jaw + 1):
        for x in range(18, 30):
            g[y][x] = "r" if frame and (x + y) % 3 else "k"
    # teeth
    for x in range(16, 32, 2):
        g[30][x] = "w"
        g[30 + jaw + 1][x + 1] = "w"
    # horns
    for t in range(16):
        for s in (-1, 1):
            x = int(24 + s * (14 + t * 0.6))
            y = int(10 - t * 0.6 + (t / 6) ** 2)
            for w in range(max(1, 3 - t // 6)):
                if 0 <= y + w < 48 and 0 <= x < 48:
                    g[y + w][x] = "A" if t > 10 else "a"
    # eyes: deep sockets with green fire, a nose hole
    for (ex, ey) in ((17, 19), (31, 19)):
        for y in range(48):
            for x in range(48):
                if ((x - ex) / 5) ** 2 + ((y - ey) / 4.5) ** 2 <= 1:
                    g[y][x] = "k"
        g[ey][ex] = "g"
        g[ey - 1][ex] = "g" if frame else "G"
        g[ey][ex + (1 if ex < 24 else -1)] = "G"
    for y in range(25, 29):
        for x in range(23 - (y - 25) // 2, 26 + (y - 25) // 2):
            g[y][x] = "k"
    # cracks
    for (x, y) in ((28, 6), (29, 7), (29, 8), (30, 9), (14, 26), (13, 27)):
        g[y][x] = "A"
    # outline
    out = [row[:] for row in g]
    for y in range(48):
        for x in range(48):
            if g[y][x] == ".":
                for ddx, ddy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    xx, yy = x + ddx, y + ddy
                    if 0 <= xx < 48 and 0 <= yy < 48 and g[yy][xx] != ".":
                        out[y][x] = "k"
    return ["".join(r) for r in out]


def boss_count(frame):
    """The Vampire Count: pale, red-eyed, his cape spread like a bat's wings
    (raised, then lowered: frame 1, his mouth open)."""
    g = [["."] * 48 for _ in range(48)]
    lift = 0.75 if frame == 0 else 0.25
    for x in range(48):
        dx = abs(x + 0.5 - 24)
        if dx <= 8:
            top, bot = 21, 47
        else:
            d = dx - 8
            top = 21 - d * lift
            bot = 46 - d * 0.55 - 5 * abs(math.sin(math.pi * d / 5.4))
        for y in range(48):
            if top <= y <= bot:
                g[y][x] = "P"
    # the wings' ribs, from the shoulders out to the scallops' points
    for side in (-1, 1):
        for d0 in (5.4, 10.8):
            for t in range(40):
                f = t / 40
                d = d0 * f
                x = int(24 + side * (8 + d))
                y = int(22 + (46 - (8 + d0) * 0.0 - d0 * 0.55 - 22) * f - d * lift * (1 - f))
                if 0 <= x < 48 and 0 <= y < 48 and g[y][x] == "P":
                    g[y][x] = "k"
    # the cape's red lining, down the front; the black suit, the shirt
    for y in range(22, 48):
        for x in range(48):
            dx = abs(x + 0.5 - 24)
            if 6 <= dx <= 8.5:
                g[y][x] = "r"
            elif dx < 6:
                g[y][x] = "k"
                if y < 33 and dx < 5 - (y - 22) * 0.45:
                    g[y][x] = "w"
    g[27][24] = g[27][23] = "y"
    g[28][24] = g[28][23] = "O"
    # the high collar behind his head
    for y in range(6, 22):
        for side in (-1, 1):
            w = (y - 6) * 0.45
            for x in range(48):
                ax = (x + 0.5 - 24) * side
                if 7 <= ax <= 8 + w and ax < 15:
                    g[y][x] = "r" if ax < 7 + w else "P"
    # the head: pale, the hair's widow's peak, red eyes, fangs
    for y in range(48):
        for x in range(48):
            dx, dy = (x + 0.5 - 24) / 7, (y + 0.5 - 13) / 8.5
            if dx * dx + dy * dy <= 1:
                g[y][x] = "l" if dx > 0.6 else "c"
                peak = 7 + max(0, 3.5 - abs(x + 0.5 - 24) * 0.9)
                if y < peak:
                    g[y][x] = "k"
    for x in (20, 21, 27, 28):
        g[11][x] = "k"
    g[10][19] = g[10][29] = "k"
    for ex in (21, 27):
        g[13][ex] = "r"
        g[13][ex - 1 if ex < 24 else ex + 1] = "r"
        g[12][ex] = "y" if frame else "r"
    g[16][24] = "l"
    if frame:
        for x in range(21, 28):
            g[18][x] = "k"
            g[19][x] = "r"
        for x in range(22, 27):
            g[20][x] = "k"
        for x in (22, 26):
            g[19][x] = "w"
            g[20][x] = "w"
    else:
        for x in range(21, 28):
            g[19][x] = "k"
        g[20][22] = g[20][26] = "w"
    # outline
    out = [row[:] for row in g]
    for y in range(48):
        for x in range(48):
            if g[y][x] == ".":
                for ddx, ddy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    xx, yy = x + ddx, y + ddy
                    if 0 <= xx < 48 and 0 <= yy < 48 and g[yy][xx] != ".":
                        out[y][x] = "k"
    return ["".join(r) for r in out]


# ---------------------------------------------------------------- explosions


def burst(frame):
    """A magic burst: a ring of sparks, growing and fading."""
    rnd = random.Random(40 + frame)
    g = [["."] * 16 for _ in range(21)]
    cx, cy = 7.5, 10
    r = 2 + frame * 2.2
    cols = ["w", "l", "p", "P"][min(frame, 3):] + ["P"]
    for y in range(21):
        for x in range(16):
            d = math.hypot(x - cx, y - cy)
            if frame < 2 and d < r - 1:
                g[y][x] = "w" if d < r - 2.5 else "y"
            elif abs(d - r) < 1.0 and rnd.random() < 0.85 - frame * 0.15:
                g[y][x] = cols[0]
            elif abs(d - r + 1.5) < 0.8 and rnd.random() < 0.5 - frame * 0.1:
                g[y][x] = cols[1] if len(cols) > 1 else "P"
    for i in range(10):
        a = rnd.random() * math.tau
        d = r + rnd.uniform(0.5, 2.5)
        x, y = int(cx + math.cos(a) * d), int(cy + math.sin(a) * d)
        if 0 <= x < 16 and 0 <= y < 21:
            g[y][x] = rnd.choice("ygl" if frame < 3 else "pP")
    return ["".join(r) for r in g]


def fireburst(frame):
    """A fiery burst: what an enemy leaves."""
    rnd = random.Random(80 + frame)
    g = [["."] * 16 for _ in range(21)]
    cx, cy = 7.5, 10
    r = 2.5 + frame * 1.8
    for y in range(21):
        for x in range(16):
            d = math.hypot(x - cx, (y - cy) * 1.1) + rnd.uniform(-1.2, 1.2)
            if d < r:
                t = d / r
                if frame >= 3:
                    g[y][x] = "A" if rnd.random() < 0.5 else ("a" if rnd.random() < 0.3 else ".")
                elif t < 0.35 - frame * 0.1:
                    g[y][x] = "w"
                elif t < 0.6:
                    g[y][x] = "y"
                elif t < 0.85:
                    g[y][x] = "o"
                else:
                    g[y][x] = "O" if rnd.random() < 0.7 else "r"
    return ["".join(r) for r in g]


def crop16(rows):
    """A 16 x 21 picture to 16 x 16: its rows with pixels, centred."""
    filled = [i for i, r in enumerate(rows) if r.strip(".")]
    if not filled:
        return ["." * 16] * 16
    top, bot = filled[0], filled[-1]
    if bot - top >= 16:                     # (a burst: its middle)
        start = (top + bot + 1) // 2 - 8
    else:
        start = max(0, min(top - (16 - (bot - top + 1)) // 2, len(rows) - 16))
    return rows[start:start + 16]


def objects():
    """(name, 16 rows of 16) for every object (drawn by the RRB)."""
    simple = [("shot_a", SHOT_A), ("shot_b", SHOT_B), ("bat_0", BAT_UP), ("bat_1", BAT_MID),
              ("bat_2", BAT_DOWN), ("ghost_a", GHOST_A), ("ghost_b", GHOST_B),
              ("pump_a", PUMP_A), ("pump_b", PUMP_B), ("skull_a", SKULL_A), ("skull_b", SKULL_B),
              ("fire_a", FIRE_A), ("fire_b", FIRE_B), ("toxic_a", TOXIC_A), ("toxic_b", TOXIC_B),
              ("seed", SEED_A), ("candy", CANDY), ("potion", POTION), ("cat", CAT)]
    out = [(n, crop16(p)) for n, p in simple]
    for i in range(4):
        out.append(("burst_%d" % i, crop16(burst(i)[2:19])))
    for i in range(4):
        out.append(("fburst_%d" % i, crop16(fireburst(i)[2:19])))
    for name, rows in out:
        assert len(rows) == 16 and all(len(r) == 16 for r in rows), name
    return out


def all_images():
    """(name, rows, tall) for every hardware sprite image."""
    out = []
    for name, pic in (("witch_a", WITCH_A), ("witch_b", WITCH_B)):
        a, b = split(pic, 2, 21)
        out.append((name + "0", a, False))
        out.append((name + "1", b, False))
    for f in range(2):
        for i, part in enumerate(split(boss_pumpkin(f), 3, 48)):
            out.append(("king_%d%d" % (f, i), part, True))
    for f in range(2):
        for i, part in enumerate(split(boss_skull(f), 3, 48)):
            out.append(("lord_%d%d" % (f, i), part, True))
    for f in range(2):
        for i, part in enumerate(split(boss_count(f), 3, 48)):
            out.append(("count_%d%d" % (f, i), part, True))
    for name, rows, tall in out:
        assert len(rows) == (48 if tall else 21), name
        for r in rows:
            assert len(r) == 16, (name, r)
            for ch in r:
                assert ch in KEY, (name, ch)
    return out


def preview(path):
    from PIL import Image
    imgs = all_images() + [(n, r, False) for n, r in objects()]
    cols = 12
    cw, chh = 18, 50
    sheet = Image.new("RGB", (cols * cw * 4, ((len(imgs) + cols - 1) // cols) * chh * 4), (40, 30, 60))
    for n, (name, rows, tall) in enumerate(imgs):
        ox, oy = (n % cols) * cw, (n // cols) * chh
        for y, r in enumerate(rows):
            for x, ch in enumerate(r):
                if ch != ".":
                    c = PALETTE[KEY[ch]]
                    for a in range(4):
                        for b in range(4):
                            sheet.putpixel(((ox + x) * 4 + a, (oy + y) * 4 + b), c)
    sheet.save(path)
