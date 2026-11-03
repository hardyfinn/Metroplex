// Panel positions in mm. res/make_panel.py uses the same numbers.
#pragma once

namespace layout {
static const float W = 254.0f;  // 50 HP
// display window
static const float DISP_X = 6.0f, DISP_Y = 12.0f, DISP_W = 64.0f, DISP_H = 28.0f;
// page buttons: two rows of three
static const float PAGE_X[3] = {80.0f, 94.0f, 108.0f};
static const float PAGE_Y[2] = {21.0f, 35.0f};
// data encoder
static const float ENC_X = 127.0f, ENC_Y = 27.0f, ENC_SIZE = 14.0f;
// CTRL knobs and their menu buttons
static const float CTRL_X[4] = {149.0f, 171.0f, 193.0f, 215.0f};
static const float CTRL_Y = 24.0f, CTRL_BTN_Y = 36.5f;
static const float CTRL_BTN_DX = -5.5f, CTRL_ATT_DX = 5.5f;  // menu button left, CV attenuator right
// run button and clock light
static const float RUN_X = 232.0f, RUN_Y = 21.0f, CLK_LIGHT_Y = 33.0f;
// master PULSES knob
static const float ALL_PULSES_X = 245.5f, ALL_PULSES_Y = 24.0f;
// stage columns
static const float STAGE_X0 = 17.5f, STAGE_DX = 14.6f;
static const float NUM_Y = 46.4f, SLIDER_Y = 61.5f, PULSE_Y = 79.5f, GATE_Y = 87.0f, SLIDE_Y = 94.0f, MUT_Y = 101.0f, SKIP_Y = 108.0f;
// jacks
static const float JACK_Y = 120.2f;
static const float IN_X0 = 12.0f, IN_DX = 10.8f;  // 10 inputs
static const float OUT_X[4] = {203.0f, 214.5f, 226.0f, 237.5f};
// Mutate knob, new-seed button, snapshot SAVE and 1-8 (all on the jack row)
static const float MUTATE_X = 120.0f, SEED_X = 130.0f, SAVE_X = 137.5f, SNAP_X0 = 145.0f, SNAP_DX = 6.8f;
}  // namespace layout
