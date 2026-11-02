#!/usr/bin/env python3
# Draws res/Metroplex.svg. VCV Rack can't draw SVG <text>, so every label is
# turned into outline paths with fontTools. Positions are in mm and must match
# src/Layout.hpp.
#   pip install fonttools
#   python3 res/make_panel.py
import os
import random
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 254.0, 128.5  # 50 HP

BG = "#1C1F24"
BG_DARK = "#15171B"
INK = "#E8E6E1"
SOFT = "#9AA3AD"
AMBER = "#F2A93B"
CYAN = "#5FD0E4"
OUTBOX = "#0F1114"

# --- keep in step with src/Layout.hpp
DISP_X, DISP_Y, DISP_W, DISP_H = 6.0, 12.0, 90.0, 28.0
PAGE_X = [106.0, 120.0, 134.0]
PAGE_Y = [21.0, 35.0]
ENC_X, ENC_Y = 156.0, 27.0
CTRL_X = [182.0, 206.0]
CTRL_Y, CTRL_BTN_Y = 24.0, 36.5
RUN_X, RUN_Y, CLK_LIGHT_Y = 232.0, 21.0, 33.0
STAGE_X0, STAGE_DX = 17.5, 14.6
NUM_Y, SLIDER_Y, PULSE_Y, GATE_Y, SLIDE_Y, ACCUM_Y = 47.0, 62.0, 81.0, 90.5, 98.5, 106.5
JACK_Y = 118.5
IN_X0, IN_DX = 12.0, 14.0
OUT_X = [202.0, 216.0, 230.0, 244.0]

fonts = {}


def text(s, x, y, size, font=FONT_MED, fill=INK, anchor="middle", spacing=0.04):
    """Return an SVG path for string s; (x, y) is the baseline point in mm."""
    if font not in fonts:
        f = TTFont(font)
        fonts[font] = (f, f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm)
    f, gs, cmap, upm = fonts[font]
    scale = size / upm
    track = spacing * upm
    names = [cmap[ord(c)] for c in s]
    width = sum(gs[n].width + track for n in names) - track
    if anchor == "middle":
        x0 = x - width * scale / 2
    elif anchor == "end":
        x0 = x - width * scale
    else:
        x0 = x
    pen = SVGPathPen(gs)
    cx = 0.0
    for n in names:
        tp = TransformPen(pen, (scale, 0, 0, -scale, x0 + cx * scale, y))
        gs[n].draw(tp)
        cx += gs[n].width + track
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'


parts = []
add = parts.append


def line(x1, y1, x2, y2, color=BG_DARK, w=0.4, extra=""):
    add(f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" stroke="{color}" stroke-width="{w}" {extra}/>')


add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')

# title strip
add(text("METROPLEX", 6.0, 9.0, 4.2, FONT_BOLD, AMBER, anchor="start", spacing=0.14))
add(text("16-stage pulse sequencer", 50.0, 8.7, 2.1, FONT_MED, SOFT, anchor="start"))
add(text("Haer", W - 6.0, 8.8, 2.6, FONT_BOLD, INK, anchor="end", spacing=0.08))

# display window
add(f'<rect x="{DISP_X - 0.8}" y="{DISP_Y - 0.8}" width="{DISP_W + 1.6}" height="{DISP_H + 1.6}" rx="2" fill="#07080A"/>')
add(f'<rect x="{DISP_X}" y="{DISP_Y}" width="{DISP_W}" height="{DISP_H}" rx="1.2" fill="#0B0C0E"/>')

# page buttons
page_names = [["HOME", "CLOCK", "SEQ"], ["SCALE", "SLIDE", "ACCUM"]]
for r in range(2):
    for c in range(3):
        add(text(page_names[r][c], PAGE_X[c], PAGE_Y[r] - 4.4, 2.0, FONT_BOLD, INK, spacing=0.06))
add(text("press again = next row", PAGE_X[1], 41.0, 1.5, FONT_MED, SOFT))

# data encoder
add(text("DATA", ENC_X, 18.2, 2.3, FONT_BOLD, AMBER, spacing=0.1))
add(f'<circle cx="{ENC_X}" cy="{ENC_Y}" r="8.2" fill="none" stroke="{BG_DARK}" stroke-width="0.8"/>')
add(text("drag / scroll: change", ENC_X, 38.0, 1.5, FONT_MED, SOFT))
add(text("click: next row", ENC_X, 40.1, 1.5, FONT_MED, SOFT))

# CTRL knobs
for k in range(2):
    x = CTRL_X[k]
    add(text(f"CTRL {k + 1}", x, 15.6, 2.3, FONT_BOLD, CYAN, spacing=0.08))
    add(text("menu", x + 5.6, CTRL_BTN_Y + 0.55, 1.5, FONT_MED, SOFT, anchor="start"))
add(text("right-click a knob to assign", (CTRL_X[0] + CTRL_X[1]) / 2, 41.0, 1.5, FONT_MED, SOFT))

# run and clock
add(text("RUN", RUN_X, 15.6, 2.3, FONT_BOLD, INK, spacing=0.08))
add(text("CLK", RUN_X, CLK_LIGHT_Y + 4.6, 1.8, FONT_BOLD, SOFT))

# divider
line(4.0, 43.6, W - 4.0, 43.6, AMBER, 0.25, 'opacity="0.5"')

# stage area: shade every other group of four
xs = [STAGE_X0 + STAGE_DX * i for i in range(16)]
for g in range(4):
    if g % 2 == 1:
        x0 = xs[g * 4] - STAGE_DX / 2
        add(f'<rect x="{x0:.2f}" y="44.4" width="{STAGE_DX * 4:.2f}" height="65.6" fill="{BG_DARK}" opacity="0.55"/>')
for i, x in enumerate(xs):
    add(text(str(i + 1), x, NUM_Y + 0.8, 2.3, FONT_BOLD, AMBER if i % 4 == 0 else INK))

rows = [("PITCH", SLIDER_Y), ("PULSES", PULSE_Y), ("GATE", GATE_Y), ("SLIDE", SLIDE_Y), ("ACCUM", ACCUM_Y)]
for name, y in rows:
    add(text(name, 2.2, y + 0.65, 1.75, FONT_BOLD, SOFT, anchor="start", spacing=0.04))
add(text("1-8", 2.2, PULSE_Y + 2.9, 1.5, FONT_MED, SOFT, anchor="start"))

# jacks
for j, lab in enumerate(["CLOCK", "RESET", "RUN", "TRANSP", "ACC RST", "CTRL 1", "CTRL 2"]):
    x = IN_X0 + IN_DX * j
    add(text(lab, x, 112.5, 1.9, FONT_BOLD, CYAN if lab.startswith("CTRL") else INK, spacing=0.04))
add(text("1 V/oct", IN_X0 + IN_DX * 3, 124.9, 1.45, FONT_MED, SOFT))
add(text("CV in", IN_X0 + IN_DX * 5.5, 124.9, 1.45, FONT_MED, SOFT))

add(f'<rect x="195.0" y="110.0" width="56.0" height="16.6" rx="1.2" fill="{OUTBOX}"/>')
for x, lab in zip(OUT_X, ["CLK", "EOC", "GATE", "PITCH"]):
    add(text(lab, x, 112.5, 1.9, FONT_BOLD, AMBER))

# gate colour key
kx = 106.0
add(text("GATE", kx, 113.4, 1.8, FONT_BOLD, INK, anchor="start"))
key = [("#5A2020", "rest"), ("#3BD16F", "single"), ("#F2C23B", "multi"), ("#3B8BF2", "hold")]
cx = kx + 9.0
for col, lab in key:
    add(f'<circle cx="{cx:.2f}" cy="112.75" r="1.1" fill="{col}"/>')
    add(text(lab, cx + 1.8, 113.4, 1.6, FONT_MED, SOFT, anchor="start"))
    cx += 4.0 + len(lab) * 1.05 + 2.4

# a small skyline
rnd = random.Random(7)
x = 104.0
while x < 191.0:
    w = rnd.uniform(3.0, 7.0)
    w = min(w, 191.0 - x)
    h = rnd.uniform(3.0, 9.5)
    y = 126.6 - h
    add(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w - 0.5:.2f}" height="{h:.2f}" fill="#262A31"/>')
    wy = y + 1.0
    while wy < 125.5:
        wx = x + 0.7
        while wx < x + w - 1.3:
            if rnd.random() < 0.28:
                add(f'<rect x="{wx:.2f}" y="{wy:.2f}" width="0.6" height="0.8" fill="{AMBER}" opacity="0.8"/>')
            wx += 1.3
        wy += 1.6
    x += w

svg = (
    f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" '
    f'viewBox="0 0 {W} {H}">\n' + "\n".join(parts) + "\n</svg>\n"
)
with open(os.path.join(HERE, "Metroplex.svg"), "w") as fh:
    fh.write(svg)
print("wrote Metroplex.svg")
