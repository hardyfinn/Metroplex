// Panel positions in mm. res/make_panel.py uses the same numbers.
#pragma once

namespace layout {
static const float W = 294.64f;  // 58 HP
// display window
static const float DISP_X = 6.0f, DISP_Y = 12.0f, DISP_W = 64.0f, DISP_H = 28.0f;
// page buttons: two rows of three
static const float PAGE_X[3] = {82.0f, 98.0f, 114.0f};
static const float PAGE_Y[2] = {21.0f, 35.0f};
// data encoder
static const float ENC_X = 137.0f, ENC_Y = 27.0f, ENC_SIZE = 14.0f;
// CTRL knobs and their menu buttons
static const float CTRL_X[4] = {162.0f, 188.0f, 214.0f, 240.0f};
static const float CTRL_Y = 24.0f, CTRL_BTN_Y = 36.5f;
static const float CTRL_BTN_DX = -5.5f, CTRL_ATT_DX = 5.5f;  // menu button left, CV attenuator right
// run button and clock light
static const float RUN_X = 262.0f, RUN_Y = 21.0f, CLK_LIGHT_Y = 32.0f;
// master PULSES knob
static const float ALL_PULSES_X = 280.0f, ALL_PULSES_Y = 24.0f;
// stage columns
static const float STAGE_X0 = 20.5f, STAGE_DX = 17.6f;
static const float NUM_Y = 46.4f, SLIDER_Y = 61.5f, PULSE_Y = 79.5f, GATE_Y = 87.0f, SLIDE_Y = 94.0f, MUT_Y = 101.0f, SKIP_Y = 108.0f;
// jacks
static const float JACK_Y = 120.2f;
static const float IN_X0 = 12.0f, IN_DX = 13.0f;  // 10 inputs
static const float OUT_X[4] = {240.0f, 252.0f, 264.0f, 276.0f};
// Mutate knob, new-seed button, snapshot SAVE and 1-8 (all on the jack row)
static const float MUTATE_X = 145.0f, SEED_X = 157.0f, SAVE_X = 168.0f, SNAP_X0 = 178.0f, SNAP_DX = 7.2f;
}  // namespace layout
