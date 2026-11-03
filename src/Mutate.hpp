// Mutate and snapshots. No VCV Rack code here, so the tests can check it.
//
// Mutate is repeatable: a seed fixes, for every stage, *what* each change
// would be and *how far* the MUTATE knob has to turn before it happens. So
// turning the knob up changes more and more stages, and turning it back down
// undoes them in reverse order until the pattern is exactly as programmed.
// The same seed and knob position always give the same result.
#pragma once
#include "Engine.hpp"

namespace mpx {

struct MutateSettings {
	float amount = 0.f;   // 0 = as programmed, 1 = every change that is switched on
	int pitchRange = 3;   // scale degrees up or down, 0 = pitch is left alone
	bool rhythm = true;   // pulse counts and gate types
	bool slides = false;  // slide on/off
	bool skips = false;   // skip on/off
};

// A well-mixed 32-bit hash of (seed, stage, what).
inline uint32_t mutHash(uint32_t seed, uint32_t stage, uint32_t what) {
	uint32_t x = seed ^ (stage * 0x9E3779B1u) ^ (what * 0x85EBCA77u);
	x ^= x >> 16;
	x *= 0x7FEB352Du;
	x ^= x >> 15;
	x *= 0x846CA68Bu;
	x ^= x >> 16;
	return x;
}

inline float mutHash01(uint32_t seed, uint32_t stage, uint32_t what) {
	return (mutHash(seed, stage, what) >> 8) * (1.f / 16777216.f);
}

enum MutWhat { MW_PITCH_AT, MW_PITCH_BY, MW_PULSES_AT, MW_PULSES_TO, MW_GATE_AT, MW_GATE_TO, MW_SLIDE_AT, MW_SKIP_AT };

// Knob position at which a change kicks in. Skips come later than the rest,
// so a little Mutate never empties the pattern.
inline float mutThreshold(uint32_t seed, int i, int what) {
	float h = mutHash01(seed, i, what);
	if (what == MW_SKIP_AT)
		return 0.35f + 0.65f * h;
	return 0.02f + 0.98f * h;
}

inline Stage mutateStage(const Stage& base, int i, uint32_t seed, const MutateSettings& m) {
	Stage st = base;
	st.degreeOffset = 0;
	if (!base.mutable_ || m.amount <= 0.f)
		return st;
	float a = m.amount;
	if (m.pitchRange > 0 && a > mutThreshold(seed, i, MW_PITCH_AT)) {
		float h = mutHash01(seed, i, MW_PITCH_BY);
		int d = (int)std::lround((h - 0.5f) * 2.f * m.pitchRange);
		if (d == 0)
			d = (h < 0.5f) ? -1 : 1;
		st.degreeOffset = d;
	}
	if (m.rhythm) {
		if (a > mutThreshold(seed, i, MW_PULSES_AT)) {
			float h = mutHash01(seed, i, MW_PULSES_TO);
			int p = 1 + (int)(h * h * 8.f);  // mostly short stages
			if (p == base.pulses)
				p = (p % 8) + 1;
			st.pulses = std::min(std::max(p, 1), 8);
		}
		if (a > mutThreshold(seed, i, MW_GATE_AT)) {
			float h = mutHash01(seed, i, MW_GATE_TO);
			// Single 35%, Multi 25%, Hold 20%, Rest 20%
			int g = (h < 0.35f) ? GATE_SINGLE : (h < 0.6f) ? GATE_MULTI : (h < 0.8f) ? GATE_HOLD : GATE_REST;
			if (g == base.gate)
				g = (g + 1) % NUM_GATE_TYPES;
			st.gate = g;
		}
	}
	if (m.slides && a > mutThreshold(seed, i, MW_SLIDE_AT))
		st.slide = !base.slide;
	if (m.skips && a > mutThreshold(seed, i, MW_SKIP_AT))
		st.skip = !base.skip;
	return st;
}

// A new seed from an old one (for the NEW SEED button and input).
inline uint32_t nextSeed(uint32_t seed, uint32_t salt) {
	uint32_t s = mutHash(seed, salt, 0x51EDu);
	return s ? s : 1u;
}

// ---------------------------------------------------------------- snapshots

static const int NUM_SNAPSHOTS = 8;

struct Snapshot {
	bool used = false;
	Stage stages[NUM_STAGES];
	int start = 1, length = 16, order = 0;  // menu values (start is 1-based)
	uint32_t seed = 1;
};

}  // namespace mpx
