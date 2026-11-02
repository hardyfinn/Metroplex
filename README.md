# Metroplex

A 16-stage pulse-count sequencer for VCV Rack 2, by Haer. Each stage has a pitch
slider and a pulse count, and the stages share one sequence. It has
accumulators, slides, scale quantising, a menu display, and two CTRL knobs that
you can point at almost any setting.

The way it works follows the Intellijel Metropolix, but Metroplex is an
independent module written from scratch. It is not made by or connected with
Intellijel, and it shares no code with the Quo Vadis¿ plugin.

![panel](panel-preview.png)

## Install (Apple Silicon Mac)

1. Download `Metroplex-2.0.0-mac-arm64.vcvplugin` from the
   [latest release](https://github.com/hardyfinn/metroplex/releases/tag/latest).
2. In Rack, choose **Help → Open user folder**, then close Rack.
3. Put the file, unopened, into the `plugins-mac-arm64` folder.
4. Start Rack and search for **Metroplex**.

## First patch

1. Add **Metroplex**, a VCO, an envelope (ADSR) and a VCA. If you have the
   disting NT as your audio interface, use your usual Audio module out.
2. **PITCH** → VCO V/Oct. **GATE** → ADSR gate. VCO → VCA in, ADSR → VCA CV,
   VCA → audio out.
3. Leave **CLOCK** unpatched. Metroplex runs on its own clock at 120 BPM
   (4 pulses per beat, so each pulse is a 16th note).
4. Move a few sliders. Set stage 4's **PULSES** knob to 3 and its **GATE** to
   multi (yellow): it now plays three 16ths in a row. Set stage 8 to hold
   (blue) with 4 pulses for a long note.

To sync with Live, patch a 16th-note clock (CV Tools through the disting NT)
into **CLOCK** and Live's transport reset into **RESET**.

## Stage controls (one column per stage)

| Control | What it does |
|---|---|
| **PITCH** slider | The stage's note. The slider covers the **Slider range** (1 to 4 octaves) and always lands on a note in the chosen scale. Hover to see the note name. |
| **PULSES** (1-8) | How many clock pulses the stage lasts. |
| **GATE** button | Click to cycle the gate type. Dark red = **Rest** (no gate). Green = **Single** (one gate on the first pulse). Yellow = **Multi** (a gate on every pulse). Blue = **Hold** (one long gate across all the stage's pulses). |
| **SLIDE** button | Glide into this stage's note. The **Slide** page sets how. |
| **ACCUM** button | Lets this stage's accumulator move its note each time it plays. |

The slider's light shows the stage that is playing. Stages past the **Length**
are shown dimmer.

## The display and its menus

Six page buttons choose what the display shows: **HOME**, **CLOCK**, **SEQ**,
**SCALE**, **SLIDE** and **ACCUM**. Each CTRL knob has its own **menu**
button too.

- **Change the highlighted row**: drag the **DATA** encoder up or down, or use
  the scroll wheel over it. Hold Shift for steps of 10 (handy for tempo).
- **Move to the next row**: click the encoder, or press the page button again.
  You can also click a row on the display, and drag on the display itself.
- **On HOME**, turning the encoder flips through the pages.
- **Right-click the display** for a menu of everything on that page, with
  named lists and sliders. The module's own right-click menu has every page.

A cyan **C1** or **C2** badge on a row means that CTRL knob is setting it right
now. The row shows the knob's value and you can't change it from the menu until
you unassign the knob.

| Page | Rows |
|---|---|
| **HOME** | 16 bars, one per stage, showing each note's height. Cyan = playing now. An outline means a rest. A cyan tick means the stage's accumulator is on. Along the bottom: key and scale, length and play order, and the current stage, note and accumulator offset. |
| **CLOCK** | **Tempo** (20-300 BPM, internal clock only), **Clock div** (/1 to /16, also divides an external clock), **Gate length** (1-99% of a pulse) |
| **SEQ** | **Length** (1-16 stages), **Play order**: Forward, Reverse, Pendulum (no repeats at the ends), Ping-pong (repeats the end stages), Converge (outside in: 1, 16, 2, 15...), Diverge (inside out), Random, Brownian (a random walk that mostly moves forward) |
| **SCALE** | **Scale** (18 of them, from Chromatic to Fifths), **Root**, **Slider range** (1-4 octaves), **Octave** (-3 to +3) |
| **SLIDE** | **Slide type** and **Slide amount** (see below) |
| **ACCUM** | **Scope**, **Trigger**, **Mode**, **Polarity**, **Step**, **Limit** (see below) |
| **CTRL 1 / 2** | The knob's **Destination**, a line of help about it, and a gauge showing where the knob (plus its CV) sits and what it is doing |

## Accumulators

An accumulator shifts a stage's note by scale degrees every time that stage
plays, so the line climbs, falls or bounces over many passes. This is the
main way to get a pattern that changes by itself without being random. The
first pass always plays the note you set, and the moves start after that.

| Setting | Choices |
|---|---|
| **Scope** | **Stage**: each stage with ACCUM on has its own counter. **Track**: one shared counter moves the whole sequence. Every stage with ACCUM on pushes it along, and every stage is shifted by it. |
| **Trigger** | **Stage**: one move each time the stage starts. **Pulse**: one move on every pulse, so a 4-pulse multi stage plays a little run. |
| **Mode** | **Wrap**: past the limit it jumps back to the start. **Pendulum**: bounces back and forth between the ends. **Hold**: stops at the limit. |
| **Polarity** | **Unipolar**: counts from 0 up to the limit (or down, if the step is negative). **Bipolar**: runs from -limit to +limit. |
| **Step** | How many scale degrees per move, -7 to +7. In a 7-note scale, 7 is an octave and 2 is a third. |
| **Limit** | How far it can go, 1 to 16 degrees. |

**RESET** puts the accumulators back to zero along with the sequence. **ACC
RST** resets only the accumulators, so you can patch a slow clock or an EOC
there to restart the climb every few bars.

## Slides

Turn on **SLIDE** on a stage to glide into its note from the one before.

- **Tempo**: the glide takes the same time whatever the interval. **Slide
  amount** is that time as a share of one pulse (100% = a full pulse).
- **Analog**: the glide moves at a fixed speed, so big jumps take longer, like
  a slew limiter. 100% = 2 seconds per octave, 25% = half a second per octave.

## CTRL knobs

Each CTRL knob can be pointed at a setting. Choose it with the knob's **menu**
button and the DATA encoder, or right-click the knob. Their **CTRL 1** and
**CTRL 2** CV inputs add to the knob: +-5 V moves it across its whole range.
New modules start with CTRL 1 on **Gate length** and CTRL 2 on **Slide
amount**.

There are three kinds of destination:

- **Direct**: the knob *is* the setting. Its row on the menu shows a C1/C2
  badge. Tempo, Clock div, Gate length, Length, Play order, Scale, Slider
  range, Slide amount, Accum mode, Accum step and Accum limit.
- **Offset**: noon does nothing, left subtracts, right adds. **Pitch pre**
  (+-12 semitones, then back into the scale, so it moves the melody *through*
  the scale), **Pitch post** (+-12 semitones after the scale, a plain
  transpose), **Octave** (+-2), **Pulse count** (+-7 pulses on every stage).
- **Accum direction**: left = run backwards, noon = freeze where it is, right
  = run forwards.

If both knobs set the same direct setting, CTRL 2 wins. Two offsets on the same
destination add together.

## Jacks

| Jack | |
|---|---|
| **CLOCK** in | One pulse per rising edge. Unpatched = internal clock. |
| **RESET** in | Back to the first stage on the next clock, and accumulators to zero. |
| **RUN** in | A trigger toggles run/stop (the **RUN** button does the same). |
| **TRANSP** in | 1 V/oct transpose, added *before* the scale, so the notes stay in key. |
| **ACC RST** in | Resets the accumulators only. |
| **CTRL 1 / CTRL 2** in | CV for the two CTRL knobs. |
| **CLK** out | The clock after division. |
| **EOC** out | A trigger each time the sequence starts a new cycle. |
| **GATE** out | 10 V gates. |
| **PITCH** out | 1 V/oct, 0 V = C4. |

## Ideas to try

1. **Self-mutating line.** Scope Stage, Trigger Stage, Mode Pendulum, Step +2,
   Limit 6. Turn ACCUM on for stages 3, 7, 11 and 15 only. The rest of the
   riff stays put while those four notes drift up and back over many bars.
   Put CTRL 1 on **Accum direction** and freeze it at noon when you find a
   version you like.
2. **Arrangement from two knobs.** CTRL 1 on **Length**, CTRL 2 on **Pulse
   count**. Start at length 4 for the intro, open to 16 for the main section,
   then add pulses for a slower, stretched breakdown. Record the knob moves as
   automation in Live (through CV Tools into the CTRL CV jacks) to play the
   arrangement back.
3. **Acid-style bass.** Slide type Tempo, amount 60%. Gates mostly Single, with
   one Hold stage of 2 pulses and slides on two or three stages. Pitch pre on
   CTRL 2 shifts the riff through the scale for the next section.
4. **Converging chords.** Play order Converge on 8 stages, sent to Just Friends
   or Three Sisters. The outside-in order gives a call-and-response shape from
   a plain riff.

## Build it yourself

GitHub Actions builds the Mac Apple Silicon plugin on every push to `main` and
puts it on the [latest release](https://github.com/hardyfinn/metroplex/releases/tag/latest).

```sh
c++ -std=c++11 -O2 -o test/engine_test test/engine_test.cpp && test/engine_test   # engine tests
python3 res/make_panel.py                                                         # redraw the panel (needs fonttools)
make dist RACK_DIR=/path/to/Rack-SDK                                              # build the plugin
```

The sequencer engine (`src/Engine.hpp`) and the menus and CTRL routing
(`src/Program.hpp`) have no VCV code in them, so the tests run on their own.

Licence: GPL-3.0-or-later.
