#!/usr/bin/env python3
# Draws res/Metroplex.svg. VCV Rack can't draw SVG <text>, so every label is
# turned into outline paths with fontTools. Positions are in mm and must match
# src/Layout.hpp.
#   pip install fonttools
#   python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 294.64, 128.5  # 58 HP

import sys

# Theme: "white" (default) writes Metroplex.svg, "dark" writes Metroplex-dark.svg.
#   python3 res/make_panel.py dark
THEME = sys.argv[1] if len(sys.argv) > 1 else "white"
THEMES = {
    "dark": dict(BG="#1C1F24", BG_DARK="#15171B", INK="#E8E6E1", SOFT="#9AA3AD", AMBER="#F2A93B",
                 CYAN="#5FD0E4", OUTBOX="#0F1114", OUT_TEXT="#F2A93B", BUILDING="#262A31", SHADE_OPACITY="0.55"),
    "white": dict(BG="#F2F0EB", BG_DARK="#E0DCD3", INK="#1E2126", SOFT="#6B737C", AMBER="#C2730F",
                  CYAN="#17879C", OUTBOX="#1E2126", OUT_TEXT="#F2A93B", BUILDING="#DAD6CD", SHADE_OPACITY="0.6"),
}
globals().update(THEMES[THEME])

# --- keep in step with src/Layout.hpp
DISP_X, DISP_Y, DISP_W, DISP_H = 6.0, 12.0, 64.0, 28.0
PAGE_X = [82.0, 98.0, 114.0]
PAGE_Y = [21.0, 35.0]
ENC_X, ENC_Y = 137.0, 27.0
CTRL_X = [162.0, 188.0, 214.0, 240.0]
CTRL_Y, CTRL_BTN_Y = 24.0, 36.5
CTRL_BTN_DX, CTRL_ATT_DX = -5.5, 5.5  # menu button left, CV attenuator right, under each knob
RUN_X, RUN_Y, CLK_LIGHT_Y = 262.0, 21.0, 32.0
ALL_PULSES_X, ALL_PULSES_Y = 280.0, 24.0
STAGE_X0, STAGE_DX = 20.5, 17.6
NUM_Y, SLIDER_Y, PULSE_Y, GATE_Y, SLIDE_Y, MUT_Y, SKIP_Y = 46.4, 61.5, 79.5, 87.0, 94.0, 101.0, 108.0
JACK_Y = 120.2
IN_X0, IN_DX = 12.0, 13.0  # 10 inputs
OUT_X = [240.0, 252.0, 264.0, 276.0]
MUTATE_X, SEED_X, SAVE_X, SNAP_X0, SNAP_DX = 145.0, 157.0, 168.0, 178.0, 7.2
JACK_LABEL_Y = 115.3  # just above the jacks, so each label sits with its own jack
FOOT_Y = 126.9

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
add(text("16-stage pulse sequencer", 50.0, 8.7, 2.6, FONT_MED, SOFT, anchor="start"))
add(text("Haer", W - 6.0, 8.8, 2.6, FONT_BOLD, INK, anchor="end", spacing=0.08))

# display window
add(f'<rect x="{DISP_X - 0.8}" y="{DISP_Y - 0.8}" width="{DISP_W + 1.6}" height="{DISP_H + 1.6}" rx="2" fill="#07080A"/>')
add(f'<rect x="{DISP_X}" y="{DISP_Y}" width="{DISP_W}" height="{DISP_H}" rx="1.2" fill="#0B0C0E"/>')

# page buttons
page_names = [["HOME", "CLOCK", "SEQ"], ["SCALE", "SLIDE", "MUTATE"]]
for r in range(2):
    for c in range(3):
        add(text(page_names[r][c], PAGE_X[c], PAGE_Y[r] - 4.4, 2.9, FONT_BOLD, INK, spacing=0.06))
add(text("press again = next row", PAGE_X[1], 41.6, 2.2, FONT_MED, SOFT))

# data encoder
add(text("DATA", ENC_X, 18.4, 3.2, FONT_BOLD, AMBER, spacing=0.1))
add(f'<circle cx="{ENC_X}" cy="{ENC_Y}" r="8.2" fill="none" stroke="{BG_DARK}" stroke-width="0.8"/>')
add(text("drag: change", ENC_X, 37.6, 2.1, FONT_MED, SOFT))
add(text("click: next row", ENC_X, 40.4, 2.1, FONT_MED, SOFT))

# CTRL knobs
for k in range(4):
    x = CTRL_X[k]
    add(text(f"CTRL {k + 1}", x, 15.6, 3.2, FONT_BOLD, CYAN, spacing=0.08))
    add(text("menu", x + CTRL_BTN_DX, 42.3, 2.2, FONT_MED, SOFT))
    add(text("CV amt", x + CTRL_ATT_DX, 42.3, 2.2, FONT_MED, SOFT))
    add(text("-", x + CTRL_ATT_DX - 4.4, CTRL_BTN_Y + 0.8, 2.2, FONT_BOLD, SOFT))
    add(text("+", x + CTRL_ATT_DX + 4.4, CTRL_BTN_Y + 0.8, 2.2, FONT_BOLD, SOFT))

# run and clock
add(text("RUN", RUN_X, 15.6, 3.2, FONT_BOLD, INK, spacing=0.08))
add(text("CLK", RUN_X, CLK_LIGHT_Y + 5.0, 2.9, FONT_BOLD, SOFT))

# master pulses knob
add(text("PULSES", ALL_PULSES_X, 15.6, 3.2, FONT_BOLD, INK, spacing=0.06))
add(text("all stages", ALL_PULSES_X, 32.2, 2.2, FONT_MED, SOFT))
for n, v in ((1, 0.0), (8, 1.0)):
    import math
    a = math.radians(-150 + 300 * v)
    add(text(str(n), ALL_PULSES_X + 5.9 * math.sin(a), ALL_PULSES_Y - 5.9 * math.cos(a) + 0.8, 2.4, FONT_MED, SOFT))

# divider
line(4.0, 43.6, W - 4.0, 43.6, AMBER, 0.25, 'opacity="0.5"')

# stage area: shade every other group of four
xs = [STAGE_X0 + STAGE_DX * i for i in range(16)]
for g in range(4):
    if g % 2 == 1:
        x0 = xs[g * 4] - STAGE_DX / 2
        add(f'<rect x="{x0:.2f}" y="44.4" width="{STAGE_DX * 4:.2f}" height="67.2" fill="{BG_DARK}" opacity="{SHADE_OPACITY}"/>')
for i, x in enumerate(xs):
    add(text(str(i + 1), x, NUM_Y + 1.1, 3.2, FONT_BOLD, AMBER if i % 4 == 0 else INK))

rows = [("PITCH", SLIDER_Y), ("PULSES", PULSE_Y), ("GATE", GATE_Y), ("SLIDE", SLIDE_Y), ("MUT", MUT_Y), ("SKIP", SKIP_Y)]
for name, y in rows:
    add(text(name, 2.2, y + 1.0, 2.9, FONT_BOLD, SOFT, anchor="start", spacing=0.04))
add(text("1-8", 2.2, PULSE_Y + 3.6, 2.2, FONT_MED, SOFT, anchor="start"))

# gate colour key, in the title strip
kx = 100.0
add(text("GATE", kx, 8.7, 2.6, FONT_BOLD, INK, anchor="start"))
key = [("#5A2020", "rest"), ("#3BD16F", "single"), ("#F2C23B", "multi"), ("#3B8BF2", "hold")]
cx = kx + 10.5
for col, lab in key:
    add(f'<circle cx="{cx:.2f}" cy="8.05" r="1.1" fill="{col}"/>')
    add(text(lab, cx + 1.8, 8.7, 2.4, FONT_MED, SOFT, anchor="start"))
    cx += 4.0 + len(lab) * 1.55 + 2.4

# divider between the stages and the jack row
line(4.0, 112.3, W - 4.0, 112.3, AMBER, 0.25, 'opacity="0.5"')

# input jacks: label right above each jack, notes underneath
for j, lab in enumerate(["CLOCK", "RESET", "RUN", "TRANSP", "SEED", "SNAP", "CTRL 1", "CTRL 2", "CTRL 3", "CTRL 4"]):
    x = IN_X0 + IN_DX * j
    add(text(lab, x, JACK_LABEL_Y, 2.9, FONT_BOLD, CYAN if lab.startswith("CTRL") else INK, spacing=0.04))
add(text("1 V/oct", IN_X0 + IN_DX * 3, FOOT_Y, 2.1, FONT_MED, SOFT))
add(text("trig", IN_X0 + IN_DX * 4, FOOT_Y, 2.1, FONT_MED, SOFT))
add(text("0-7 V", IN_X0 + IN_DX * 5, FOOT_Y, 2.1, FONT_MED, SOFT))
add(text("CTRL CV in", IN_X0 + IN_DX * 7.5, FOOT_Y, 2.1, FONT_MED, SOFT))

# mutate and snapshots
add(text("MUTATE", MUTATE_X, JACK_LABEL_Y - 0.6, 2.9, FONT_BOLD, CYAN, spacing=0.04))
add(text("amount", MUTATE_X, FOOT_Y, 2.1, FONT_MED, SOFT))
add(text("SEED", SEED_X, JACK_LABEL_Y, 2.9, FONT_BOLD, INK, spacing=0.04))
add(text("new", SEED_X, FOOT_Y, 2.1, FONT_MED, SOFT))
add(text("SAVE", SAVE_X, JACK_LABEL_Y, 2.9, FONT_BOLD, INK, spacing=0.04))
for j in range(8):
    add(text(str(j + 1), SNAP_X0 + SNAP_DX * j, JACK_LABEL_Y, 2.9, FONT_BOLD, AMBER if j == 0 else INK))
add(text("SNAPSHOTS", SNAP_X0 + SNAP_DX * 3.5, FOOT_Y, 2.1, FONT_MED, SOFT, spacing=0.08))

# outputs
add(f'<rect x="{OUT_X[0] - 6.4}" y="113.0" width="{OUT_X[3] - OUT_X[0] + 12.8}" height="14.8" rx="1.2" fill="{OUTBOX}"/>')
for x, lab in zip(OUT_X, ["CLK", "EOC", "GATE", "PITCH"]):
    add(text(lab, x, JACK_LABEL_Y, 2.9, FONT_BOLD, OUT_TEXT))
add(text("OUT", OUT_X[0] + (OUT_X[3] - OUT_X[0]) / 2, FOOT_Y, 2.1, FONT_MED, SOFT, spacing=0.1))

# --- wide pitch slider parts (keep sizes in step with WideSlider in src/Metroplex.cpp)
SL_W, SL_H = 9.6, 26.0
HD_W, HD_H = 8.4, 4.6
ticks = []
for i in range(9):
    y = 1.0 + HD_H / 2 - 1.0 + i * (SL_H - HD_H) / 8
    long = i % 4 == 0
    ln = 1.8 if long else 1.1
    ticks.append(f'<rect x="{0.2:.2f}" y="{y - 0.12:.2f}" width="{ln}" height="0.24" fill="{SOFT}"/>')
    ticks.append(f'<rect x="{SL_W - 0.2 - ln:.2f}" y="{y - 0.12:.2f}" width="{ln}" height="0.24" fill="{SOFT}"/>')
slider_svg = (
    f'<svg xmlns="http://www.w3.org/2000/svg" width="{SL_W}mm" height="{SL_H}mm" viewBox="0 0 {SL_W} {SL_H}">\n'
    f'<rect x="{SL_W / 2 - 0.9}" y="0.6" width="1.8" height="{SL_H - 1.2}" rx="0.9" fill="#0E0F12"/>\n'
    + "\n".join(ticks) + "\n</svg>\n"
)
handle_svg = (
    f'<svg xmlns="http://www.w3.org/2000/svg" width="{HD_W}mm" height="{HD_H}mm" viewBox="0 0 {HD_W} {HD_H}">\n'
    f'<rect x="0" y="0" width="{HD_W}" height="{HD_H}" rx="0.9" fill="#26292E"/>\n'
    f'<rect x="0.35" y="0.3" width="{HD_W - 0.7}" height="1.2" rx="0.5" fill="#3E424A"/>\n'
    f'<rect x="0.6" y="{HD_H / 2 - 0.3}" width="{HD_W - 1.2}" height="0.6" fill="#E8E6E1"/>\n'
    "</svg>\n"
)
if THEME == "white":
    with open(os.path.join(HERE, "Slider.svg"), "w") as fh:
        fh.write(slider_svg)
    with open(os.path.join(HERE, "SliderHandle.svg"), "w") as fh:
        fh.write(handle_svg)

svg = (
    f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" '
    f'viewBox="0 0 {W} {H}">\n' + "\n".join(parts) + "\n</svg>\n"
)
name = "Metroplex.svg" if THEME == "white" else f"Metroplex-{THEME}.svg"
with open(os.path.join(HERE, name), "w") as fh:
    fh.write(svg)
print("wrote " + name)
