"""Roguelike sprites for RockBlasterRogue, made in code in the style of the existing ones
(dark outline, top-left light, few colors): rock kinds recolored from the rock sprites, ship
variants from the ship, bosses built like procgen.enemy/asteroid, 16x16 upgrade icons drawn by hand.
usage: python rogue.py  (writes /workspace/sprites/rogue/*.png)
The paths are the sprite workspace this was run in; its pack.py then builds atlas.png/json.
"""
import numpy as np
from PIL import Image
from procgen import outline, pad, even, shade, crop, asteroid, ROCK, OUT

S = '/workspace/sprites/'
D = S + 'rogue/'
HULL = [(41,49,70),(55,54,81),(85,91,114),(121,125,147),(162,163,182),(199,202,215),(235,236,243)]
PURP = [(48,22,64),(78,34,102),(118,52,148),(160,84,196),(200,130,230)]
RED = [(110,14,34),(176,24,44),(232,54,60),(255,128,110)]
METAL = [(30,36,50),(46,56,74),(66,80,102),(90,106,130),(118,136,160),(152,170,192),(192,206,224),(230,238,248)]
EXPL = [(44,24,28),(66,34,34),(90,46,40),(116,60,46),(144,78,54),(172,98,64),(200,124,80),(226,154,102)]
CRYS = [(18,40,46),(26,60,64),(34,84,82),(46,110,100),(64,138,116),(92,166,134),(130,196,158),(182,228,192)]

def load(name):
    return np.array(Image.open(S + name + '.png').convert('RGBA'))

def lum(img):
    return img[..., :3].astype(float) @ [0.3, 0.59, 0.11]

def body_mask(img):
    """Opaque pixels that aren't the outline."""
    a = img[..., 3] > 0
    o = np.all(img[..., :3] == OUT, axis=-1)
    return a & ~o

def recolor(img, pal, mask=None, gamma=1.0):
    """Maps the luminance of `mask` pixels (rank-normalized, so the shading survives) onto pal."""
    out = img.copy()
    m = body_mask(img) if mask is None else mask
    L = lum(img)[m]
    t = (L - L.min()) / max(L.max() - L.min(), 1)
    out[m, :3] = shade(pal, t ** gamma)
    return out

def put(img, y, x, c):
    if 0 <= y < img.shape[0] and 0 <= x < img.shape[1] and img[y, x, 3] > 0 and \
            not np.all(img[y, x, :3] == OUT):
        img[y, x, :3] = c

def crack(img, rng, y, x, angle, length, core, glow):
    """A jagged line from (y, x): `core` pixels with a `glow` fringe."""
    for _ in range(length):
        for dy, dx in ((0, 1), (1, 0), (0, -1), (-1, 0)):
            if not np.all(img[int(y) + dy, int(x) + dx, :3] == core) if 0 <= int(y) + dy < img.shape[0] and 0 <= int(x) + dx < img.shape[1] else False:
                put(img, int(y) + dy, int(x) + dx, glow)
        put(img, int(y), int(x), core)
        angle += rng.uniform(-0.6, 0.6)
        y += np.sin(angle)
        x += np.cos(angle)

# ---------------- rock kinds ----------------
def rock_metal(src, seed):
    img = recolor(src, METAL)
    m = body_mask(img)
    h, w = m.shape
    rng = np.random.default_rng(seed)
    # plate seams: one horizontal, one vertical, dark with rivets along them
    sy = int(h * rng.uniform(0.4, 0.55))
    sx = int(w * rng.uniform(0.45, 0.6))
    for x in range(w):
        if m[sy, x]:
            img[sy, x, :3] = METAL[1]
            if x % 5 == 2:
                put(img, sy - 1, x, METAL[7])
    for y in range(sy):
        if m[y, sx]:
            img[y, sx, :3] = METAL[1]
            if y % 5 == 2:
                put(img, y, sx + 1, METAL[7])
    for y in range(sy + 1, h):
        x2 = sx - w // 4
        if 0 <= x2 < w and m[y, x2]:
            img[y, x2, :3] = METAL[1]
    return img

def rock_explosive(src, seed):
    img = recolor(src, EXPL)
    m = body_mask(img)
    L = lum(src)
    t = np.zeros_like(L)
    t[m] = (L[m] - L[m].min()) / max(L[m].max() - L[m].min(), 1)
    # the darkest spots (crater floors) glow like magma
    img[m & (t < 0.2), :3] = (226, 88, 34)
    img[m & (t < 0.1), :3] = (255, 190, 64)
    rng = np.random.default_rng(seed)
    h, w = m.shape
    for k in range(3 if w > 30 else 2 if w > 16 else 1):
        crack(img, rng, h / 2, w / 2, rng.uniform(0, 6.3) + k * 2.1, int(w * 0.4),
              (255, 214, 96), (232, 96, 36))
    return img

def rock_splitter(src, seed):
    img = recolor(src, CRYS)
    rng = np.random.default_rng(seed)
    h, w = img.shape[:2]
    # three fracture lines from the middle: it splits in three
    for k in range(3):
        crack(img, rng, h / 2, w / 2, -1.57 + k * 2.09, int(w * 0.42), (214, 255, 226), CRYS[5])
    return img

# ---------------- ships ----------------
def ship_variant(src, width_scale, hull_pal, glass_pal, stripes=False):
    img = src.copy()
    rgb = img[..., :3].astype(int)
    a = img[..., 3] > 0
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    sat = rgb.max(-1) - rgb.min(-1)
    outl = np.all(rgb == OUT, axis=-1)
    flame = a & (r > 180) & (b < 120)
    glass = a & (g > r + 25) & (b > r + 15) & ~outl
    hull = a & ~outl & ~flame & ~glass & (sat < 60)
    img = recolor(img, hull_pal, hull)
    img = recolor(img, glass_pal, glass)
    if stripes:
        for y in range(20, 64, 7):
            for yy in (y, y + 1):
                row = hull[yy]
                img[yy, row, :3] = (img[yy, row, :3] * 0.35).astype(np.uint8)
    # resample the left half to the new width and mirror it (keeps it symmetric)
    h, w = img.shape[:2]
    half = img[:, : w // 2]
    nw = max(4, int(round(w // 2 * width_scale)))
    cols = np.minimum((np.arange(nw) / width_scale).astype(int), w // 2 - 1)
    left = half[:, cols]
    out = np.concatenate([left, left[:, ::-1]], axis=1)
    return out

# ---------------- bosses ----------------
def big_rock(D, seed, craters=6):
    """procgen.asteroid for a big rock: fewer, smaller craters (they scale with D there)."""
    rng = np.random.default_rng(seed); R = D / 2 - 1.5; c = D / 2 - 0.5
    Y, X = np.mgrid[0:D, 0:D].astype(float); ang = np.arctan2(Y - c, X - c); r = np.hypot(X - c, Y - c)
    rad = np.ones_like(ang)
    for k in range(2, 7):
        rad += rng.uniform(0.02, 0.07) * np.cos(k * ang + rng.uniform(0, 6.3))
    rad *= R / rad.max()
    m = r <= rad
    nx = (X - c) / R; ny = (Y - c) / R; nz = np.sqrt(np.clip(1 - nx ** 2 - ny ** 2, 0, 1))
    L = 0.12 + 0.85 * np.clip(nz * 0.5 - nx * 0.4 - ny * 0.5 + 0.25, 0, 1)
    for _ in range(8):
        bx, by, br = rng.uniform(0, D), rng.uniform(0, D), rng.uniform(D * 0.08, D * 0.2)
        L += rng.uniform(-0.08, 0.08) * np.exp(-((X - bx) ** 2 + (Y - by) ** 2) / (br ** 2))
    L -= np.clip((r / rad - 0.85) * 1.2, 0, 1) * np.clip(nx + ny + 0.3, 0, 1) * 0.5
    for _ in range(craters):
        for _t in range(30):
            a_ = rng.uniform(0, 6.3); dd = rng.uniform(0.2, 0.75) * R; cr = rng.uniform(D * 0.04, D * 0.08)
            px, py = c + np.cos(a_) * dd, c + np.sin(a_) * dd
            if np.hypot(px - c, py - c) + cr < R * 0.9:
                break
        q = np.hypot(X - px, Y - py) / cr
        inn = q <= 1
        dirx = (X - px) / cr; diry = (Y - py) / cr
        L[inn] = np.minimum(L[inn], 0.3 + 0.15 * np.clip(dirx[inn] + diry[inn], -1, 1) + 0.12)
        L[(q > 1) & (q <= 1.0 + 1.6 / cr) & ((dirx + diry) < 0.2)] += 0.12
    col = shade(ROCK, L)
    img = np.zeros((D, D, 4), np.uint8); img[m, :3] = col[m]; img[m, 3] = 255
    return img

def boss_rock():
    img = big_rock(96, 42)
    h, w = img.shape[:2]
    m = body_mask(img)
    rng = np.random.default_rng(5)
    Y, X = np.mgrid[0:h, 0:w]
    L = lum(img)
    t = np.zeros_like(L)
    t[m] = (L[m] - L[m].min()) / (L[m].max() - L[m].min())
    # dark stone (the rock's own craters and shading), a little cooler than the small rocks
    img[m, :3] = shade([(30, 28, 38), (44, 42, 54), (60, 58, 70), (80, 76, 86), (102, 96, 104),
                        (126, 118, 122), (152, 142, 140), (178, 168, 160)], t[m])
    cx, cy = w / 2, h / 2
    R = np.hypot(X - cx, Y - cy)[m].max()
    # bolted armor plates: rotated rectangles, shaded with the rock's light but darker metal
    plates = [(-0.55, -0.35, 0.5), (0.35, -0.5, 1.2), (0.55, 0.2, -0.4), (-0.3, 0.5, 0.9),
              (-0.62, 0.15, -1.1)]
    for px, py, a in plates:
        ox, oy = cx + px * R, cy + py * R
        u = (X - ox) * np.cos(a) + (Y - oy) * np.sin(a)
        v = -(X - ox) * np.sin(a) + (Y - oy) * np.cos(a)
        plate = (np.abs(u) < R * 0.24) & (np.abs(v) < R * 0.14) & m
        img[plate, :3] = shade(METAL, np.clip(t[plate] * 0.6 + 0.08, 0, 0.66))
        edge = plate & ~(np.roll(plate, 1, 0) & np.roll(plate, -1, 0) & np.roll(plate, 1, 1) & np.roll(plate, -1, 1))
        img[edge, :3] = METAL[0]
        for su in (-1, 1):
            for sv in (-1, 1):
                ry = int(round(oy + np.sin(a) * su * R * 0.18 + np.cos(a) * sv * R * 0.08))
                rx = int(round(ox + np.cos(a) * su * R * 0.18 - np.sin(a) * sv * R * 0.08))
                if plate[ry, rx]:
                    put(img, ry, rx, METAL[6])
    # a glowing core showing through cracks
    rr = np.hypot(X - cx, Y - cy)
    core = (rr < 9) & m
    img[core, :3] = shade([(150, 26, 34), (220, 60, 44), (255, 130, 70), (255, 214, 130)],
                          np.clip(1 - rr[core] / 9, 0, 0.99))
    for k in range(4):
        crack(img, rng, cy, cx, k * np.pi / 2 + 0.4, 22, (255, 150, 70), (150, 36, 40))
    return even(outline(pad(crop(img))))


def mothership(W=196, H=92):
    img = np.zeros((H, W, 4), np.uint8)
    cx = (W - 1) / 2
    Y, X = np.mgrid[0:H, 0:W].astype(float)
    dx = (X - cx) / 96
    dy = (Y - 40) / 26
    disc = dx ** 2 + dy ** 2 <= 1
    belly = ((X - cx) / 66) ** 2 + ((Y - 56) / 24) ** 2 <= 1
    pods = np.zeros_like(disc)
    for s in (-1, 1):
        pods |= ((X - (cx + s * 74)) / 12) ** 2 + ((Y - 56) / 14) ** 2 <= 1
    m = disc | belly | pods
    nz = np.sqrt(np.clip(1 - dx ** 2 - dy ** 2, 0, 1))
    Lt = 0.1 + 0.72 * np.clip(nz * 0.55 - dx * 0.35 - dy * 0.4 + 0.05, 0, 1)
    col = shade(HULL, Lt)
    rim = disc & (dx ** 2 + dy ** 2 > 0.7)
    col[rim] = shade(PURP, 0.3 + 0.5 * (-dx[rim] * 0.3 - dy[rim] * 0.5 + 0.4))
    lower = (belly | pods) & ~disc
    col[lower] = shade(PURP, np.clip(0.15 + 0.5 * (1 - (Y[lower] - 44) / 36) - 0.2 * (X[lower] - cx) / 70, 0, 1))
    # three domes on top, the middle one big
    for ox, rx, ry, oy in ((0, 22, 15, 24), (-46, 11, 8, 30), (46, 11, 8, 30)):
        dd = ((X - cx - ox) / rx) ** 2 + ((Y - oy) / ry) ** 2
        dm = dd <= 1
        col[dm] = shade(RED, np.clip(0.95 - dd[dm] * 0.8 - ((X[dm] - cx - ox) / rx) * 0.2 - ((Y[dm] - oy) / ry) * 0.25, 0, 1))
        hy, hx = int(oy - ry * 0.45), int(cx + ox - rx * 0.4)
        col[hy, hx] = (255, 200, 190)
        col[hy, hx + 1] = (255, 200, 190)
    # hangar bays in the belly: dark openings with a red glow
    for ox in (-30, 0, 30):
        bay = (np.abs(X - cx - ox) <= 8) & (Y >= 58) & (Y <= 70)
        col[bay] = (36, 14, 40)
        col[bay & (Y >= 66)] = RED[1]
        col[bay & (Y >= 69)] = RED[2]
    # panel lines and rim lights
    for k in (-70, -58, -20, 20, 58, 70):
        xx = int(round(cx + k))
        for yy in range(16, 46):
            if disc[yy, xx] and not rim[yy, xx] and not np.all(col[yy, xx] == RED[0]):
                col[yy, xx] = HULL[1]
    for ang in np.linspace(-1.2, 1.2, 13):
        lx = int(round(cx + np.sin(ang) * 88))
        ly = int(round(40 + np.cos(ang) * 20))
        col[ly, lx] = RED[3]
        col[ly + 1, lx] = RED[1]
    img[m, :3] = col[m]
    img[m, 3] = 255
    img[:, W // 2:] = img[:, : W // 2][:, ::-1]
    return even(outline(pad(crop(img))))

def station(N=132):
    img = np.zeros((N, N, 4), np.uint8)
    c = (N - 1) / 2
    Y, X = np.mgrid[0:N, 0:N].astype(float)
    r = np.hypot(X - c, Y - c)
    ang = np.arctan2(Y - c, X - c)
    nx, ny = (X - c) / (N / 2), (Y - c) / (N / 2)
    light = np.clip(0.55 - nx * 0.35 - ny * 0.45, 0, 1)
    ring = (r >= 38) & (r <= 54)
    # the ring is a tube: lit across its width
    tube = np.clip(1 - np.abs(r - 46) / 8, 0, 1)
    arms = np.zeros_like(ring)
    for k in range(4):
        a = k * np.pi / 2 + np.pi / 4
        along = (X - c) * np.cos(a) + (Y - c) * np.sin(a)
        side = -(X - c) * np.sin(a) + (Y - c) * np.cos(a)
        arms |= (along > 20) & (along < 64) & (np.abs(side) < 8 - np.clip(along - 52, 0, 12) * 0.3)
    spokes = np.zeros_like(ring)
    for k in range(4):
        a = k * np.pi / 2
        side = -(X - c) * np.sin(a) + (Y - c) * np.cos(a)
        along = (X - c) * np.cos(a) + (Y - c) * np.sin(a)
        spokes |= (np.abs(side) < 3) & (along > 18) & (along < 40)
    hub = r <= 24
    m = ring | arms | spokes | hub
    col = np.zeros((N, N, 3), np.uint8)
    col[ring] = shade(METAL, np.clip(light[ring] * 0.6 + tube[ring] * 0.45, 0, 1))
    col[spokes] = shade(METAL, np.clip(light[spokes] * 0.7 + 0.1, 0, 1))
    col[arms] = shade(HULL, np.clip(light[arms] * 0.8 + 0.15, 0, 1))
    # gun barrels: dark, with a red muzzle
    for k in range(4):
        a = k * np.pi / 2 + np.pi / 4
        along = (X - c) * np.cos(a) + (Y - c) * np.sin(a)
        side = -(X - c) * np.sin(a) + (Y - c) * np.cos(a)
        barrel = arms & (np.abs(side) < 2.2) & (along > 30)
        col[barrel] = HULL[0]
        col[arms & (along > 59)] = RED[2]
    col[hub] = shade(HULL, np.clip(light[hub] * 0.7 + 0.25 * (1 - r[hub] / 24), 0, 1))
    core = r <= 13
    col[core] = shade(RED, np.clip(1 - r[core] / 13 + 0.1, 0, 1))
    col[(r <= 4)] = (255, 220, 190)
    # lights around the ring
    for k in range(16):
        a = k * np.pi / 8
        col[int(round(c + np.sin(a) * 46)), int(round(c + np.cos(a) * 46))] = RED[3] if k % 2 else (140, 230, 255)
    img[m, :3] = col[m]
    img[m, 3] = 255
    return even(outline(pad(crop(img))))

# ---------------- hand-drawn icons ----------------
PAL = {'o': OUT, 'w': (236, 242, 250), 'c': (90, 220, 255), 'C': (40, 120, 176),
       'y': (255, 214, 90), 'Y': (240, 130, 40), 'r': (226, 58, 60), 'R': (140, 24, 40),
       'g': (160, 170, 190), 'G': (92, 102, 124), 'p': (180, 120, 240), 'P': (98, 50, 140)}

def glyph(rows):
    h, w = len(rows), len(rows[0])
    img = np.zeros((h, w, 4), np.uint8)
    for y, row in enumerate(rows):
        assert len(row) == w, (rows, row)
        for x, ch in enumerate(row):
            if ch != '.':
                img[y, x, :3] = PAL[ch]
                img[y, x, 3] = 255
    return img

def icon(rows):
    """A 12x12 glyph on a 16x16 tile (dark, bevelled)."""
    tile = np.zeros((16, 16, 4), np.uint8)
    for y in range(16):
        for x in range(16):
            corner = (x in (0, 15)) and (y in (0, 15))
            if corner:
                continue
            border = x in (0, 15) or y in (0, 15)
            lit = x == 0 or y == 0
            tile[y, x, :3] = ((96, 116, 156) if lit else (54, 66, 96)) if border else (24, 30, 48)
            tile[y, x, 3] = 255
    g = glyph(rows)
    m = g[..., 3] > 0
    tile[2:14, 2:14][m] = g[m]
    return tile

ICONS = {
    'spread': ["............",
               ".y...yy...y.",
               ".yy..yy..yy.",
               "..y..yy..y..",
               "..yy.yy.yy..",
               "...y.yy.y...",
               "....wwww....",
               "...wwwwww...",
               "...wccccw...",
               "..wwccccww..",
               "..ww....ww..",
               "............"],
    'pierce': ["............",
               "....GGGG....",
               "...GggggG...",
               "..GggGgggG..",
               "wwwwwwwwwwy.",
               "wwwwwwwwwwyy",
               "..GgggggGG..",
               "..GggGggGG..",
               "...GgggGG...",
               "....GGGG....",
               "............",
               "............"],
    'rapid': ["......yyy...",
              ".....yyy....",
              "....yyy.....",
              "...yyy......",
              "..yyyyyyy...",
              "..YYYYyyy...",
              ".....yyy....",
              "....yyY.....",
              "...yyY......",
              "..yyY.......",
              "..yY........",
              "..Y........."],
    'shield': ["..cccccccc..",
               ".cwwwwwwwwc.",
               ".cwCCCCCCwc.",
               ".cwCCwwCCwc.",
               ".cwCCwwCCwc.",
               ".cwCCCCCCwc.",
               "..cwCCCCwc..",
               "..cwCCCCwc..",
               "...cwCCwc...",
               "....cwwc....",
               ".....cc.....",
               "............"],
    'thruster': ["....gggg....",
                 "...gwwwwg...",
                 "...gGGGGg...",
                 "...gGGGGg...",
                 "....gggg....",
                 "....yyyy....",
                 "...yywwyy...",
                 "...YyywyY...",
                 "....YyyY....",
                 "....YYYY....",
                 ".....YY.....",
                 ".....r......"],
    'hyperblast': [".....yy.....",
                   "..y..yy..y..",
                   "...y.ww.y...",
                   "....wwww....",
                   ".yywwccwwyy.",
                   "yyywcCCcwyyy",
                   "yyywcCCcwyyy",
                   ".yywwccwwyy.",
                   "....wwww....",
                   "...y.ww.y...",
                   "..y..yy..y..",
                   ".....yy....."],
    'homing': ["..........r.",
               ".........rw.",
               "........rww.",
               ".......gww..",
               "......gwg...",
               ".....gwg....",
               "....ggg.....",
               "...Yy.......",
               "..y.Y.......",
               ".y..........",
               "y...........",
               "............"],
    'reargun': [".....ww.....",
                "....wwww....",
                "...wwwwww...",
                ".....ww.....",
                ".....ww.....",
                "....gggg....",
                "....gGGg....",
                ".....yy.....",
                ".....yy.....",
                "...yyyyyy...",
                "....yyyy....",
                ".....yy....."],
    'longrange': [".....cc.....",
                  "...cccccc...",
                  "..cc.cc.cc..",
                  ".cc..cc..cc.",
                  ".c...ww...c.",
                  "cccccwwccccc",
                  "cccccwwccccc",
                  ".c...ww...c.",
                  ".cc..cc..cc.",
                  "..cc.cc.cc..",
                  "...cccccc...",
                  ".....cc....."],
    'magnet': ["..rrrrrrrr..",
               ".rrwwwwwwrr.",
               "rrw......Rrr",
               "rrr......RRr",
               "rrr......RRr",
               "rrr......RRr",
               "rrr......RRr",
               "rrr......RRr",
               "www......www",
               "wwg......wwg",
               "ggg......ggg",
               "............"],
}

SCRAP = ["..ooo..",
         ".oyyyo.",
         "oyywyYo",
         "oyw.wYo",
         "oyywYYo",
         ".oYYYo.",
         "..ooo.."]
MISSILE = ["..w..",
           ".wgw.",
           ".ggg.",
           ".gGg.",
           ".gGg.",
           ".grg.",
           ".gGg.",
           "ogggo",
           "og.go"]

def save(img, name):
    Image.fromarray(img).save(D + name + '.png')

if __name__ == '__main__':
    for size, src in (('large', 'asteroid_large_1'), ('medium', 'asteroid_medium_1'),
                      ('small', 'asteroid_small_1')):
        a = load(src)
        save(rock_metal(a, 3), f'rock_metal_{size}')
        save(rock_explosive(a, 4), f'rock_explosive_{size}')
        save(rock_splitter(a, 5), f'rock_splitter_{size}')
    ship = load('ship')
    save(ship_variant(ship, 1.3, [(38,40,26),(58,62,38),(84,88,54),(116,118,74),(150,148,98),(186,180,128),(222,214,168)],
                      [(90,50,10),(170,100,20),(240,170,60),(255,226,150)]), 'ship_bulwark')
    save(ship_variant(ship, 0.9, [(60,40,8),(104,72,12),(160,116,20),(210,164,34),(244,204,70),(255,232,140)],
                      [(90,16,24),(170,34,40),(232,70,64),(255,160,140)], stripes=True), 'ship_wasp')
    save(ship_variant(ship, 0.78, [(20,22,52),(32,36,84),(48,56,120),(70,84,160),(100,120,200),(140,162,234),(196,210,255)],
                      [(70,20,90),(140,40,160),(210,90,230),(250,180,255)]), 'ship_lancer')
    save(boss_rock(), 'boss_monolith')
    save(mothership(), 'boss_mothership')
    save(station(), 'boss_station')
    for name, rows in ICONS.items():
        save(icon(rows), 'upgrade_' + name)
    save(glyph(SCRAP), 'scrap')
    save(glyph(MISSILE), 'missile')
    print('ok')
