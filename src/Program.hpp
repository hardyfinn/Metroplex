// The menu settings (fields), the menu pages, and the CTRL knob destinations.
// No VCV Rack code here either, so the tests can check it.
#pragma once
#include "Engine.hpp"
#include <string>

namespace mpx {

// ---------------------------------------------------------------- fields

enum Field {
	F_TEMPO, F_DIV, F_GATELEN,
	F_LENGTH, F_ORDER,
	F_SCALE, F_ROOT, F_RANGE, F_OCTAVE,
	F_SLIDE_TYPE, F_SLIDE_AMT,
	F_MUT_PITCH, F_MUT_RHYTHM, F_MUT_SLIDES, F_MUT_SKIPS,
	F_SNAP_MODE,
	F_START,
	NUM_FIELDS
};

struct FieldInfo {
	const char* name;   // shown in the menu and in Rack's tooltip
	const char* unit;
	int min, max, def;
};

static const FieldInfo FIELDS[NUM_FIELDS] = {
	{"Tempo", " BPM", 20, 300, 120},
	{"Clock div", "", 1, 16, 1},
	{"Gate length", "%", 1, 99, 50},
	{"Length", " stages", 1, 16, 16},
	{"Play order", "", 0, NUM_ORDERS - 1, 0},
	{"Scale", "", 0, NUM_SCALES - 1, 1},
	{"Root", "", 0, 11, 0},
	{"Slider range", " oct", 1, 4, 2},
	{"Octave", "", -3, 3, 0},
	{"Slide type", "", 0, NUM_SLIDE_TYPES - 1, 0},
	{"Slide amount", "%", 0, 100, 25},
	{"Mutate pitch", " deg", 0, 12, 3},
	{"Mutate rhythm", "", 0, 1, 1},
	{"Mutate slides", "", 0, 1, 0},
	{"Mutate skips", "", 0, 1, 0},
	{"Snapshot switch", "", 0, 1, 1},
	{"Start stage", "", 1, 16, 1},
};

// Text for a field's value, as the display shows it.
inline std::string fieldText(int f, int v) {
	char buf[32];
	switch (f) {
		case F_TEMPO: snprintf(buf, sizeof buf, "%d BPM", v); break;
		case F_DIV: snprintf(buf, sizeof buf, "/%d", v); break;
		case F_GATELEN: snprintf(buf, sizeof buf, "%d%%", v); break;
		case F_LENGTH: snprintf(buf, sizeof buf, "%d", v); break;
		case F_START: snprintf(buf, sizeof buf, "%d", v); break;
		case F_ORDER: return ORDER_NAMES[v];
		case F_SCALE: return SCALES[v].name;
		case F_ROOT: return NOTE_NAMES[v];
		case F_RANGE: snprintf(buf, sizeof buf, "%d oct", v); break;
		case F_OCTAVE: snprintf(buf, sizeof buf, "%+d", v); break;
		case F_SLIDE_TYPE: return SLIDE_NAMES[v];
		case F_SLIDE_AMT: snprintf(buf, sizeof buf, "%d%%", v); break;
		case F_MUT_PITCH:
			if (v == 0) return "Off";
			snprintf(buf, sizeof buf, "+-%d deg", v);
			break;
		case F_MUT_RHYTHM:
		case F_MUT_SLIDES:
		case F_MUT_SKIPS: return v ? "On" : "Off";
		case F_SNAP_MODE: return v ? "End of cycle" : "Now";
		default: snprintf(buf, sizeof buf, "%d", v); break;
	}
	return buf;
}

// Fields that have names rather than numbers (shown as lists in menus).
inline bool fieldIsNamed(int f) {
	return f == F_ORDER || f == F_SCALE || f == F_ROOT || f == F_SLIDE_TYPE || f == F_MUT_RHYTHM ||
		f == F_MUT_SLIDES || f == F_MUT_SKIPS || f == F_SNAP_MODE;
}

// ---------------------------------------------------------------- pages

enum Page { PAGE_HOME, PAGE_CLOCK, PAGE_SEQ, PAGE_SCALE, PAGE_SLIDE, PAGE_MUTATE, PAGE_CTRL1, PAGE_CTRL2, PAGE_CTRL3, PAGE_CTRL4, NUM_PAGES };

static const int NUM_CTRLS = 4;  // CTRL 1-2 are knobs (plus CV), CTRL 3-4 are CV inputs

inline bool isCtrlPage(int p) {
	return p >= PAGE_CTRL1 && p < PAGE_CTRL1 + NUM_CTRLS;
}

struct PageInfo {
	const char* title;
	int count;
	int fields[6];
};

static const PageInfo PAGES[NUM_PAGES] = {
	{"HOME", 0, {}},
	{"CLOCK", 3, {F_TEMPO, F_DIV, F_GATELEN}},
	{"SEQUENCE", 4, {F_START, F_LENGTH, F_ORDER, F_SNAP_MODE}},
	{"SCALE", 4, {F_SCALE, F_ROOT, F_RANGE, F_OCTAVE}},
	{"SLIDE", 2, {F_SLIDE_TYPE, F_SLIDE_AMT}},
	{"MUTATE", 4, {F_MUT_PITCH, F_MUT_RHYTHM, F_MUT_SLIDES, F_MUT_SKIPS}},
	{"CTRL 1", 0, {}},
	{"CTRL 2", 0, {}},
	{"CTRL 3", 0, {}},
	{"CTRL 4", 0, {}},
};

// ---------------------------------------------------------------- CTRL destinations

enum DestKind { DEST_KIND_NONE, DEST_KIND_DIRECT, DEST_KIND_OFFSET, DEST_KIND_MUTATE, DEST_KIND_SNAPSHOT };

enum Dest {
	DEST_NONE,
	// direct: the knob sets the menu value itself
	DEST_TEMPO, DEST_DIV, DEST_GATELEN, DEST_LENGTH, DEST_ORDER, DEST_SCALE, DEST_RANGE,
	DEST_SLIDE_AMT, DEST_MUT_PITCH, DEST_MUT_RHYTHM, DEST_MUT_AMOUNT,
	// offset: noon = no change, left subtracts, right adds
	DEST_PITCH_PRE, DEST_PITCH_POST, DEST_OCTAVE, DEST_PULSES,
	DEST_SNAPSHOT,
	DEST_START,
	NUM_DESTS
};

struct DestInfo {
	const char* name;
	const char* help;
	int kind;
	int field;   // for direct destinations: the field it takes over
	int range;   // for offsets: the amount at full left/right
};

static const DestInfo DESTS[NUM_DESTS] = {
	{"None", "Not assigned", DEST_KIND_NONE, -1, 0},
	{"Tempo", "Sets the internal tempo, 20 to 300 BPM", DEST_KIND_DIRECT, F_TEMPO, 0},
	{"Clock div", "Sets the clock division, /1 to /16", DEST_KIND_DIRECT, F_DIV, 0},
	{"Gate length", "Sets gate length, 1 to 99%", DEST_KIND_DIRECT, F_GATELEN, 0},
	{"Length", "Sets how many stages play, 1 to 16", DEST_KIND_DIRECT, F_LENGTH, 0},
	{"Play order", "Picks the play order", DEST_KIND_DIRECT, F_ORDER, 0},
	{"Scale", "Picks the scale", DEST_KIND_DIRECT, F_SCALE, 0},
	{"Slider range", "Sets the slider range, 1 to 4 octaves", DEST_KIND_DIRECT, F_RANGE, 0},
	{"Slide amount", "Sets the slide amount, 0 to 100%", DEST_KIND_DIRECT, F_SLIDE_AMT, 0},
	{"Mutate pitch", "Sets how far Mutate moves notes, off to 12 scale degrees", DEST_KIND_DIRECT, F_MUT_PITCH, 0},
	{"Mutate rhythm", "Left half = Off, right half = On", DEST_KIND_DIRECT, F_MUT_RHYTHM, 0},
	{"Mutate amount", "Noon = none. Adds to or takes from the MUTATE knob", DEST_KIND_MUTATE, -1, 0},
	{"Pitch pre", "Noon = none. Shifts every note up to 12 semitones, then puts it back in the scale", DEST_KIND_OFFSET, -1, 12},
	{"Pitch post", "Noon = none. Shifts every note up to 12 semitones after the scale (chromatic)", DEST_KIND_OFFSET, -1, 12},
	{"Octave", "Noon = none. Shifts up to 2 octaves", DEST_KIND_OFFSET, -1, 2},
	{"Pulse count", "Noon = none. Adds or removes up to 7 pulses on every stage", DEST_KIND_OFFSET, -1, 7},
	{"Snapshot", "Picks snapshot 1 to 8 across the knob's travel", DEST_KIND_SNAPSHOT, -1, 0},
	{"Start stage", "Sets the stage the sequence starts on, 1 to 16", DEST_KIND_DIRECT, F_START, 0},
};

// Knob position plus CV (+-5 V moves the whole range), as 0..1.
inline float ctrlAmount(float knob, float cv) {
	return std::min(std::max(knob + cv / 10.f, 0.f), 1.f);
}

inline int directValue(int field, float amount) {
	const FieldInfo& fi = FIELDS[field];
	return fi.min + (int)std::lround(amount * (fi.max - fi.min));
}

inline int offsetValue(int range, float amount) {
	return (int)std::lround((amount - 0.5f) * 2.f * range);
}

// Everything the module needs after the CTRL knobs are applied.
struct Effective {
	int value[NUM_FIELDS];
	int owner[NUM_FIELDS];  // 0 = menu, 1-4 = that CTRL
	Settings eng;
	float mutateOffset = 0.f;  // added to the MUTATE knob (-1..+1)
	int snapshot = -1;         // a CTRL picks this snapshot (0..7), -1 = none
};

// prog: the menu values. dest/amount: the CTRLs (count of them; a dest of
// DEST_NONE does nothing). transposeVolts: the TRANSPOSE input (added before
// quantizing).
inline void computeEffective(const int* prog, const int* dest, const float* amount, float transposeVolts, Effective* out, int count = 2) {
	for (int f = 0; f < NUM_FIELDS; f++) {
		out->value[f] = prog[f];
		out->owner[f] = 0;
	}
	float preSemis = transposeVolts * 12.f;
	int postSemis = 0, octave = 0, pulses = 0;
	out->mutateOffset = 0.f;
	out->snapshot = -1;
	// Higher-numbered CTRLs are applied later, so they win when two set the same thing.
	for (int k = 0; k < count; k++) {
		int d = dest[k];
		if (d <= DEST_NONE || d >= NUM_DESTS)
			continue;
		const DestInfo& di = DESTS[d];
		float a = amount[k];
		if (di.kind == DEST_KIND_DIRECT) {
			out->value[di.field] = directValue(di.field, a);
			out->owner[di.field] = k + 1;
		}
		else if (di.kind == DEST_KIND_OFFSET) {
			int v = offsetValue(di.range, a);
			if (d == DEST_PITCH_PRE) preSemis += v;
			else if (d == DEST_PITCH_POST) postSemis += v;
			else if (d == DEST_OCTAVE) octave += v;
			else if (d == DEST_PULSES) pulses += v;
		}
		else if (di.kind == DEST_KIND_MUTATE) {
			out->mutateOffset += (a - 0.5f) * 2.f;
		}
		else if (di.kind == DEST_KIND_SNAPSHOT) {
			out->snapshot = std::min((int)(a * 8.f), 7);
		}
	}
	const int* v = out->value;
	Settings& s = out->eng;
	s.length = v[F_LENGTH];
	s.start = v[F_START] - 1;
	s.order = v[F_ORDER];
	s.scale = v[F_SCALE];
	s.root = v[F_ROOT];
	s.range = v[F_RANGE];
	s.octave = std::min(std::max(v[F_OCTAVE] + octave, -5), 5);
	s.preSemis = preSemis;
	s.postSemis = postSemis;
	s.pulseOffset = pulses;
	s.slideType = v[F_SLIDE_TYPE];
}

}  // namespace mpx
