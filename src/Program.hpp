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
	F_ACC_SCOPE, F_ACC_TRIG, F_ACC_MODE, F_ACC_POLAR, F_ACC_STEP, F_ACC_LIMIT,
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
	{"Accum scope", "", 0, 1, 0},
	{"Accum trigger", "", 0, 1, 0},
	{"Accum mode", "", 0, NUM_ACC_MODES - 1, 0},
	{"Accum polarity", "", 0, 1, 0},
	{"Accum step", "", -7, 7, 1},
	{"Accum limit", "", 1, 16, 7},
};

// Text for a field's value, as the display shows it.
inline std::string fieldText(int f, int v) {
	char buf[32];
	switch (f) {
		case F_TEMPO: snprintf(buf, sizeof buf, "%d BPM", v); break;
		case F_DIV: snprintf(buf, sizeof buf, "/%d", v); break;
		case F_GATELEN: snprintf(buf, sizeof buf, "%d%%", v); break;
		case F_LENGTH: snprintf(buf, sizeof buf, "%d", v); break;
		case F_ORDER: return ORDER_NAMES[v];
		case F_SCALE: return SCALES[v].name;
		case F_ROOT: return NOTE_NAMES[v];
		case F_RANGE: snprintf(buf, sizeof buf, "%d oct", v); break;
		case F_OCTAVE: snprintf(buf, sizeof buf, "%+d", v); break;
		case F_SLIDE_TYPE: return SLIDE_NAMES[v];
		case F_SLIDE_AMT: snprintf(buf, sizeof buf, "%d%%", v); break;
		case F_ACC_SCOPE: return ACC_SCOPE_NAMES[v];
		case F_ACC_TRIG: return ACC_TRIG_NAMES[v];
		case F_ACC_MODE: return ACC_MODE_NAMES[v];
		case F_ACC_POLAR: return ACC_POLAR_NAMES[v];
		case F_ACC_STEP: snprintf(buf, sizeof buf, "%+d deg", v); break;
		case F_ACC_LIMIT: snprintf(buf, sizeof buf, "%d deg", v); break;
		default: snprintf(buf, sizeof buf, "%d", v); break;
	}
	return buf;
}

// Fields that have names rather than numbers (shown as lists in menus).
inline bool fieldIsNamed(int f) {
	return f == F_ORDER || f == F_SCALE || f == F_ROOT || f == F_SLIDE_TYPE || f == F_ACC_SCOPE ||
		f == F_ACC_TRIG || f == F_ACC_MODE || f == F_ACC_POLAR;
}

// ---------------------------------------------------------------- pages

enum Page { PAGE_HOME, PAGE_CLOCK, PAGE_SEQ, PAGE_SCALE, PAGE_SLIDE, PAGE_ACCUM, PAGE_CTRL1, PAGE_CTRL2, NUM_PAGES };

struct PageInfo {
	const char* title;
	int count;
	int fields[6];
};

static const PageInfo PAGES[NUM_PAGES] = {
	{"HOME", 0, {}},
	{"CLOCK", 3, {F_TEMPO, F_DIV, F_GATELEN}},
	{"SEQUENCE", 2, {F_LENGTH, F_ORDER}},
	{"SCALE", 4, {F_SCALE, F_ROOT, F_RANGE, F_OCTAVE}},
	{"SLIDE", 2, {F_SLIDE_TYPE, F_SLIDE_AMT}},
	{"ACCUMULATOR", 6, {F_ACC_SCOPE, F_ACC_TRIG, F_ACC_MODE, F_ACC_POLAR, F_ACC_STEP, F_ACC_LIMIT}},
	{"CTRL 1", 0, {}},
	{"CTRL 2", 0, {}},
};

// ---------------------------------------------------------------- CTRL destinations

enum DestKind { DEST_KIND_NONE, DEST_KIND_DIRECT, DEST_KIND_OFFSET, DEST_KIND_DIRECTION };

enum Dest {
	DEST_NONE,
	// direct: the knob sets the menu value itself
	DEST_TEMPO, DEST_DIV, DEST_GATELEN, DEST_LENGTH, DEST_ORDER, DEST_SCALE, DEST_RANGE,
	DEST_SLIDE_AMT, DEST_ACC_MODE, DEST_ACC_STEP, DEST_ACC_LIMIT,
	// offset: noon = no change, left subtracts, right adds
	DEST_PITCH_PRE, DEST_PITCH_POST, DEST_OCTAVE, DEST_PULSES,
	// left = reverse, noon = freeze, right = forward
	DEST_ACC_DIR,
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
	{"Accum mode", "Picks Wrap, Pendulum or Hold", DEST_KIND_DIRECT, F_ACC_MODE, 0},
	{"Accum step", "Sets the accumulator step, -7 to +7 degrees", DEST_KIND_DIRECT, F_ACC_STEP, 0},
	{"Accum limit", "Sets the accumulator limit, 1 to 16 degrees", DEST_KIND_DIRECT, F_ACC_LIMIT, 0},
	{"Pitch pre", "Noon = none. Shifts every note up to 12 semitones, then puts it back in the scale", DEST_KIND_OFFSET, -1, 12},
	{"Pitch post", "Noon = none. Shifts every note up to 12 semitones after the scale (chromatic)", DEST_KIND_OFFSET, -1, 12},
	{"Octave", "Noon = none. Shifts up to 2 octaves", DEST_KIND_OFFSET, -1, 2},
	{"Pulse count", "Noon = none. Adds or removes up to 7 pulses on every stage", DEST_KIND_OFFSET, -1, 7},
	{"Accum direction", "Left = runs backwards, noon = frozen, right = runs forwards", DEST_KIND_DIRECTION, -1, 0},
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
	int owner[NUM_FIELDS];  // 0 = menu, 1 = CTRL 1, 2 = CTRL 2
	Settings eng;
};

// prog: the menu values. dest/amount: the two CTRL knobs. transposeVolts:
// the TRANSPOSE input (added before quantizing).
inline void computeEffective(const int* prog, const int* dest, const float* amount, float transposeVolts, Effective* out) {
	for (int f = 0; f < NUM_FIELDS; f++) {
		out->value[f] = prog[f];
		out->owner[f] = 0;
	}
	float preSemis = transposeVolts * 12.f;
	int postSemis = 0, octave = 0, pulses = 0, accDir = 1;
	// CTRL 2 is applied last, so it wins when both set the same thing.
	for (int k = 0; k < 2; k++) {
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
		else if (di.kind == DEST_KIND_DIRECTION) {
			accDir = (a < 0.45f) ? -1 : (a > 0.55f) ? 1 : 0;
		}
	}
	const int* v = out->value;
	Settings& s = out->eng;
	s.length = v[F_LENGTH];
	s.order = v[F_ORDER];
	s.scale = v[F_SCALE];
	s.root = v[F_ROOT];
	s.range = v[F_RANGE];
	s.octave = std::min(std::max(v[F_OCTAVE] + octave, -5), 5);
	s.preSemis = preSemis;
	s.postSemis = postSemis;
	s.pulseOffset = pulses;
	s.accScope = v[F_ACC_SCOPE];
	s.accTrigger = v[F_ACC_TRIG];
	s.accMode = v[F_ACC_MODE];
	s.accPolarity = v[F_ACC_POLAR];
	s.accStep = v[F_ACC_STEP];
	s.accLimit = v[F_ACC_LIMIT];
	s.accDirection = accDir;
}

}  // namespace mpx
