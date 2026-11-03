// Metroplex sequencer engine. This file has no VCV Rack code in it, so the
// test program in test/ can run it on its own.
//
// The engine knows about stages, play orders, skips, slides and scales.
// The Rack module (Metroplex.cpp) handles clocks, gate timing and slides,
// and asks the engine what to play on each clock pulse.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <cstdio>

namespace mpx {

static const int NUM_STAGES = 16;

// ---------------------------------------------------------------- scales

struct Scale {
	const char* name;
	const char* shortName;
	int count;          // notes per octave
	int notes[12];      // semitones above the root
};

static const Scale SCALES[] = {
	{"Chromatic", "Chrom", 12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
	{"Major", "Major", 7, {0, 2, 4, 5, 7, 9, 11}},
	{"Natural minor", "Minor", 7, {0, 2, 3, 5, 7, 8, 10}},
	{"Harmonic minor", "HarMin", 7, {0, 2, 3, 5, 7, 8, 11}},
	{"Melodic minor", "MelMin", 7, {0, 2, 3, 5, 7, 9, 11}},
	{"Dorian", "Dorian", 7, {0, 2, 3, 5, 7, 9, 10}},
	{"Phrygian", "Phryg", 7, {0, 1, 3, 5, 7, 8, 10}},
	{"Lydian", "Lydian", 7, {0, 2, 4, 6, 7, 9, 11}},
	{"Mixolydian", "Mixo", 7, {0, 2, 4, 5, 7, 9, 10}},
	{"Locrian", "Locr", 7, {0, 1, 3, 5, 6, 8, 10}},
	{"Major pentatonic", "MajPen", 5, {0, 2, 4, 7, 9}},
	{"Minor pentatonic", "MinPen", 5, {0, 3, 5, 7, 10}},
	{"Blues", "Blues", 6, {0, 3, 5, 6, 7, 10}},
	{"Whole tone", "Whole", 6, {0, 2, 4, 6, 8, 10}},
	{"Hirajoshi", "Hira", 5, {0, 2, 3, 7, 8}},
	{"Phrygian dominant", "PhrDom", 7, {0, 1, 4, 5, 7, 8, 10}},
	{"Octaves only", "Oct", 1, {0}},
	{"Fifths", "Fifths", 2, {0, 7}},
};
static const int NUM_SCALES = sizeof(SCALES) / sizeof(SCALES[0]);

static const char* const NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

inline int floorDiv(int a, int b) {
	int q = a / b;
	if ((a % b != 0) && ((a < 0) != (b < 0)))
		q--;
	return q;
}
inline int floorMod(int a, int b) {
	return a - floorDiv(a, b) * b;
}

// Scale degree (0 = root, 7 = next octave's root in a 7-note scale) to semitones.
inline int degreeToSemis(const Scale& s, int degree) {
	return floorDiv(degree, s.count) * 12 + s.notes[floorMod(degree, s.count)];
}

// Nearest scale note to a (fractional) number of semitones above the root.
// A tie goes to the lower note.
inline int quantizeSemis(const Scale& s, float semis) {
	int base = (int)std::floor(semis / 12.f) * 12;
	int best = 0;
	float bestDist = 1e9f;
	for (int oct = -1; oct <= 1; oct++) {
		for (int i = 0; i < s.count; i++) {
			int n = base + oct * 12 + s.notes[i];
			float d = std::fabs((float)n - semis);
			if (d < bestDist - 1e-4f) {
				bestDist = d;
				best = n;
			}
		}
	}
	return best;
}

// ---------------------------------------------------------------- settings

enum GateType { GATE_REST, GATE_SINGLE, GATE_MULTI, GATE_HOLD, NUM_GATE_TYPES };
static const char* const GATE_NAMES[NUM_GATE_TYPES] = {"Rest", "Single", "Multi", "Hold"};

enum Order { ORDER_FORWARD, ORDER_REVERSE, ORDER_PENDULUM, ORDER_PINGPONG,
	ORDER_CONVERGE, ORDER_DIVERGE, ORDER_RANDOM, ORDER_BROWNIAN, NUM_ORDERS };
static const char* const ORDER_NAMES[NUM_ORDERS] = {"Forward", "Reverse", "Pendulum", "Ping-pong",
	"Converge", "Diverge", "Random", "Brownian"};

enum SlideType { SLIDE_TEMPO, SLIDE_ANALOG, SLIDE_ACID, NUM_SLIDE_TYPES };
static const char* const SLIDE_NAMES[NUM_SLIDE_TYPES] = {"Tempo", "Analog", "Acid"};

struct Stage {
	float pitch = 0.f;  // slider, 0..1
	int pulses = 1;     // 1..8
	int gate = GATE_SINGLE;
	bool slide = false;
	bool skip = false;   // skipped stages are not played at all
	bool mutable_ = true;  // MUT button: this stage may be changed by Mutate
	int degreeOffset = 0;  // set by Mutate: scale degrees added to the slider's note
};

// Everything the engine needs for one pulse, after the CTRL knobs have had
// their say.
struct Settings {
	int length = 16;          // 1..16 stages
	int start = 0;            // first stage of the window, 0..15 (wraps past 16)
	int order = ORDER_FORWARD;
	int scale = 1;            // index into SCALES
	int root = 0;             // 0..11
	int range = 2;            // slider range in octaves, 1..4
	int octave = 0;           // -3..+3
	float preSemis = 0.f;     // added before quantizing (CTRL + TRANSPOSE input)
	int postSemis = 0;        // added after quantizing, chromatic
	int pulseOffset = 0;      // added to every stage's pulse count
	int slideType = SLIDE_TEMPO;
};

// What happens on one pulse.
struct PulseEvent {
	int stage = 0;
	int pulseIndex = 0;       // 0 = first pulse of the stage
	int stagePulses = 1;      // how many pulses this stage lasts
	bool newStage = false;
	bool gate = false;        // start a gate on this pulse
	bool holdGate = false;    // the gate lasts the whole stage
	bool slide = false;       // glide into this stage
	bool tie = false;         // Acid: the gate runs on into the next stage (legato)
	bool endOfCycle = false;  // first stage of a new cycle
};

// ---------------------------------------------------------------- engine

struct SeqEngine {
	int stage = -1;       // stage now playing, -1 = waiting for the first pulse
	int pulseInStage = 0; // pulses already played in this stage
	int stepCount = 0;    // stages played since reset (drives the orders)
	int walk = 0;         // Brownian position
	bool allSkipped = false;
	bool acidArmed = false;  // Acid: the stage just played had SLIDE on, so glide into the next
	uint32_t rng = 0x12345678u;

	void reset() {
		stage = -1;
		allSkipped = false;
		acidArmed = false;
		pulseInStage = 0;
		stepCount = 0;
		walk = 0;
	}

	uint32_t random() {
		// xorshift32
		rng ^= rng << 13;
		rng ^= rng >> 17;
		rng ^= rng << 5;
		return rng;
	}

	static int cycleLength(int order, int len) {
		switch (order) {
			case ORDER_PENDULUM: return std::max(1, 2 * len - 2);
			case ORDER_PINGPONG: return 2 * len;
			default: return len;
		}
	}

	// Stage for the t-th step of a cycle (orders that are not random).
	static int orderedStage(int order, int len, int t) {
		switch (order) {
			case ORDER_REVERSE: return len - 1 - t;
			case ORDER_PENDULUM: return (t < len) ? t : 2 * len - 2 - t;
			case ORDER_PINGPONG: return (t < len) ? t : 2 * len - 1 - t;
			case ORDER_CONVERGE: return (t % 2 == 0) ? t / 2 : len - 1 - t / 2;
			case ORDER_DIVERGE: {
				int u = len - 1 - t;
				return (u % 2 == 0) ? u / 2 : len - 1 - u / 2;
			}
			default: return t;
		}
	}

	int nextStage(const Settings& s, bool* endOfCycle) {
		int len = std::min(std::max(s.length, 1), NUM_STAGES);
		int cyc = cycleLength(s.order, len);
		int t = stepCount % cyc;
		*endOfCycle = (t == 0);
		stepCount++;
		if (s.order == ORDER_RANDOM)
			return physical(s, (int)(random() % (uint32_t)len));
		if (s.order == ORDER_BROWNIAN) {
			if (stepCount > 1) {
				int r = (int)(random() % 4u);  // back, stay, forward, forward
				walk += (r == 0) ? -1 : (r == 1) ? 0 : 1;
			}
			else {
				walk = 0;
			}
			walk = floorMod(walk, len);
			return physical(s, walk);
		}
		return physical(s, orderedStage(s.order, len, t));
	}

	// Position j in the window (0 = the start stage) to a stage number.
	static int physical(const Settings& s, int j) {
		return floorMod(s.start + j, NUM_STAGES);
	}

	// Is stage i inside the playing window (start .. start + length - 1)?
	static bool inWindow(const Settings& s, int i) {
		int len = std::min(std::max(s.length, 1), NUM_STAGES);
		return floorMod(i - s.start, NUM_STAGES) < len;
	}

	// Next stage that is not skipped. If every stage in the length is
	// skipped, *allSkipped is set and the sequence keeps moving in silence.
	int nextPlayable(const Stage* stages, const Settings& s, bool* endOfCycle, bool* allSkipped) {
		int len = std::min(std::max(s.length, 1), NUM_STAGES);
		*endOfCycle = false;
		*allSkipped = true;
		for (int i = 0; i < len; i++)
			if (!stages[physical(s, i)].skip)
				*allSkipped = false;
		if (*allSkipped)
			return nextStage(s, endOfCycle);
		int tries = 4 * cycleLength(s.order, len) + 64;
		for (int k = 0; k < tries; k++) {
			bool e = false;
			int c = nextStage(s, &e);
			*endOfCycle = *endOfCycle || e;
			if (!stages[c].skip)
				return c;
		}
		for (int i = 0; i < len; i++)
			if (!stages[physical(s, i)].skip)
				return physical(s, i);
		return physical(s, 0);
	}

	static int effectivePulses(const Stage& st, const Settings& s) {
		return std::min(std::max(st.pulses + s.pulseOffset, 1), 8);
	}

	// Called once per (divided) clock pulse.
	PulseEvent pulse(const Stage* stages, const Settings& s) {
		PulseEvent ev;
		bool advance = (stage < 0);
		if (!advance) {
			int pulses = effectivePulses(stages[stage], s);
			if (pulseInStage >= pulses)
				advance = true;
		}
		bool silent = false;
		if (advance) {
			stage = nextPlayable(stages, s, &ev.endOfCycle, &silent);
			pulseInStage = 0;
			ev.newStage = true;
			allSkipped = silent;
		}
		silent = allSkipped;
		const Stage& st = stages[stage];
		ev.stage = stage;
		ev.pulseIndex = pulseInStage;
		ev.stagePulses = effectivePulses(st, s);
		bool acid = (s.slideType == SLIDE_ACID);
		int gate = st.gate;
		if (acid) {
			// Acid: a SLIDE stage glides *out* into the next played stage, and
			// holds its gate across all its pulses, tied into the next note.
			if (ev.newStage) {
				ev.slide = acidArmed;
				acidArmed = st.slide && !silent;
			}
			if (st.slide) {
				gate = GATE_HOLD;
				ev.tie = true;
			}
		}
		else {
			ev.slide = ev.newStage && st.slide;
			acidArmed = false;
		}
		switch (gate) {
			case GATE_SINGLE: ev.gate = (pulseInStage == 0); break;
			case GATE_MULTI: ev.gate = true; break;
			case GATE_HOLD: ev.gate = (pulseInStage == 0); ev.holdGate = true; break;
			default: break;
		}
		if (silent) {
			ev.gate = false;
			ev.holdGate = false;
			ev.slide = false;
			ev.tie = false;
		}
		pulseInStage++;
		return ev;
	}

	// Semitones above the root (before octave and root).
	static int stageSemis(const Stage& st, const Settings& s) {
		const Scale& sc = SCALES[std::min(std::max(s.scale, 0), NUM_SCALES - 1)];
		int steps = std::max(1, s.range) * sc.count;
		int degree = (int)std::lround(std::min(std::max(st.pitch, 0.f), 1.f) * steps);
		degree += st.degreeOffset;
		int semis = degreeToSemis(sc, degree);
		if (s.preSemis != 0.f)
			semis = quantizeSemis(sc, semis + s.preSemis);
		return semis + s.postSemis;
	}

	// Pitch output in volts (1 V/oct, 0 V = C4).
	static float stageVolts(const Stage& st, const Settings& s) {
		int semis = stageSemis(st, s) + floorMod(s.root, 12);
		return semis / 12.f + s.octave;
	}

	// Will the next pulse begin a new cycle? (Snapshots switch here.)
	bool nextPulseStartsCycle(const Stage* stages, const Settings& s) const {
		if (stage < 0)
			return true;
		if (pulseInStage < effectivePulses(stages[stage], s))
			return false;
		int len = std::min(std::max(s.length, 1), NUM_STAGES);
		return stepCount % cycleLength(s.order, len) == 0;
	}
};

inline void noteName(float volts, char* out, int size) {
	int semis = (int)std::lround(volts * 12.f);
	int pc = floorMod(semis, 12);
	int oct = floorDiv(semis, 12) + 4;
	snprintf(out, size, "%s%d", NOTE_NAMES[pc], oct);
}

}  // namespace mpx
