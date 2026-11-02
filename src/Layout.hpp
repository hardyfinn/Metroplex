// Panel positions in mm. res/make_panel.py uses the same numbers.
#pragma once

namespace layout {
static const float W = 254.0f;  // 50 HP
// display window
static const float DISP_X = 6.0f, DISP_Y = 12.0f, DISP_W = 90.0f, DISP_H = 28.0f;
// page buttons: two rows of three
static const float PAGE_X[3] = {106.0f, 120.0f, 134.0f};
static const float PAGE_Y[2] = {21.0f, 35.0f};
// data encoder
static const float ENC_X = 156.0f, ENC_Y = 27.0f, ENC_SIZE = 14.0f;
// CTRL knobs and their menu buttons
static const float CTRL_X[2] = {182.0f, 206.0f};
static const float CTRL_Y = 24.0f, CTRL_BTN_Y = 36.5f;
// run button and clock light
static const float RUN_X = 232.0f, RUN_Y = 21.0f, CLK_LIGHT_Y = 33.0f;
// stage columns
static const float STAGE_X0 = 17.5f, STAGE_DX = 14.6f;
static const float NUM_Y = 47.0f, SLIDER_Y = 62.0f, PULSE_Y = 81.0f, GATE_Y = 90.5f, SLIDE_Y = 98.5f, ACCUM_Y = 106.5f;
// jacks
static const float JACK_Y = 118.5f;
static const float IN_X0 = 12.0f, IN_DX = 14.0f;  // 7 inputs
static const float OUT_X[4] = {202.0f, 216.0f, 230.0f, 244.0f};
}  // namespace layout
