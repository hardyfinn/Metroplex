// Tests for the Metroplex engine. Builds without VCV Rack:
//   c++ -std=c++11 -O2 -o test/engine_test test/engine_test.cpp && test/engine_test
#include "../src/Program.hpp"
#include "../src/Mutate.hpp"
#include <cstdio>
#include <vector>

using namespace mpx;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

static std::vector<int> playOrder(int order, int len, int steps) {
	SeqEngine e;
	Stage st[NUM_STAGES];
	Settings s;
	s.order = order;
	s.length = len;
	std::vector<int> out;
	for (int i = 0; i < steps; i++)
		out.push_back(e.pulse(st, s).stage);
	return out;
}

static void testOrders() {
	CHECK((playOrder(ORDER_FORWARD, 4, 6) == std::vector<int>{0, 1, 2, 3, 0, 1}));
	CHECK((playOrder(ORDER_REVERSE, 4, 5) == std::vector<int>{3, 2, 1, 0, 3}));
	CHECK((playOrder(ORDER_PENDULUM, 4, 8) == std::vector<int>{0, 1, 2, 3, 2, 1, 0, 1}));
	CHECK((playOrder(ORDER_PINGPONG, 3, 7) == std::vector<int>{0, 1, 2, 2, 1, 0, 0}));
	CHECK((playOrder(ORDER_CONVERGE, 6, 6) == std::vector<int>{0, 5, 1, 4, 2, 3}));
	CHECK((playOrder(ORDER_DIVERGE, 6, 6) == std::vector<int>{3, 2, 4, 1, 5, 0}));
	CHECK((playOrder(ORDER_PENDULUM, 1, 3) == std::vector<int>{0, 0, 0}));
	for (int o = 0; o < NUM_ORDERS; o++) {
		for (int len = 1; len <= 16; len++) {
			std::vector<int> v = playOrder(o, len, 200);
			for (int x : v)
				CHECK(x >= 0 && x < len);
		}
	}
	// Converge and diverge visit every stage once per cycle
	for (int len = 1; len <= 16; len++) {
		for (int o : {ORDER_CONVERGE, ORDER_DIVERGE}) {
			std::vector<int> v = playOrder(o, len, len);
			std::vector<int> seen(len, 0);
			for (int x : v) seen[x]++;
			for (int c : seen) CHECK(c == 1);
		}
	}
	// 16 stages forward
	std::vector<int> v = playOrder(ORDER_FORWARD, 16, 17);
	CHECK(v[15] == 15 && v[16] == 0);
}

static void testPulsesAndGates() {
	SeqEngine e;
	Stage st[NUM_STAGES];
	Settings s;
	s.length = 3;
	st[0].pulses = 3; st[0].gate = GATE_SINGLE;
	st[1].pulses = 2; st[1].gate = GATE_MULTI;
	st[2].pulses = 2; st[2].gate = GATE_HOLD;
	int stages[] = {0, 0, 0, 1, 1, 2, 2, 0};
	bool gates[] = {1, 0, 0, 1, 1, 1, 0, 1};
	for (int i = 0; i < 8; i++) {
		PulseEvent ev = e.pulse(st, s);
		CHECK(ev.stage == stages[i]);
		CHECK(ev.gate == gates[i]);
		if (i == 5) CHECK(ev.holdGate && ev.stagePulses == 2);
		if (i == 7) CHECK(ev.endOfCycle);
		if (i == 3) CHECK(!ev.endOfCycle && ev.newStage);
	}
	// rest stage
	SeqEngine e2;
	Stage r[NUM_STAGES];
	r[0].gate = GATE_REST;
	CHECK(!e2.pulse(r, s).gate);
	// pulse offset clamps to 1..8
	s.pulseOffset = 20;
	CHECK(SeqEngine::effectivePulses(st[0], s) == 8);
	s.pulseOffset = -20;
	CHECK(SeqEngine::effectivePulses(st[0], s) == 1);
}

static void testScales() {
	const Scale& maj = SCALES[1];
	CHECK(degreeToSemis(maj, 0) == 0);
	CHECK(degreeToSemis(maj, 7) == 12);
	CHECK(degreeToSemis(maj, -1) == -1);
	CHECK(degreeToSemis(maj, 9) == 16);
	CHECK(quantizeSemis(maj, 1.f) == 0);    // C# -> C (tie goes down)
	CHECK(quantizeSemis(maj, 1.6f) == 2);
	CHECK(quantizeSemis(maj, 6.f) == 5);    // F# -> F
	CHECK(quantizeSemis(maj, -0.6f) == -1); // B below
	CHECK(quantizeSemis(maj, 12.4f) == 12);
	for (int i = 0; i < NUM_SCALES; i++) {
		CHECK(SCALES[i].count >= 1 && SCALES[i].count <= 12);
		CHECK(SCALES[i].notes[0] == 0);
		for (int n = 1; n < SCALES[i].count; n++)
			CHECK(SCALES[i].notes[n] > SCALES[i].notes[n - 1] && SCALES[i].notes[n] < 12);
	}
	// slider top = range octaves above root
	SeqEngine e;
	Stage st;
	Settings s;
	s.range = 2;
	st.pitch = 1.f;
	CHECK(SeqEngine::stageSemis(st, s) == 24);
	s.root = 9;  // A
	s.octave = -1;
	CHECK(std::fabs(SeqEngine::stageVolts(st, s) - (33.f / 12.f - 1.f)) < 1e-5f);
	char buf[8];
	noteName(0.f, buf, sizeof buf);
	CHECK(std::string(buf) == "C4");
	noteName(-1.f / 12.f, buf, sizeof buf);
	CHECK(std::string(buf) == "B3");
}

static void testCtrl() {
	int prog[NUM_FIELDS];
	for (int f = 0; f < NUM_FIELDS; f++) prog[f] = FIELDS[f].def;
	Effective ef;
	int dest[2] = {DEST_LENGTH, DEST_PITCH_POST};
	float amt[2] = {0.f, 1.f};
	computeEffective(prog, dest, amt, 0.f, &ef);
	CHECK(ef.eng.length == 1 && ef.owner[F_LENGTH] == 1);
	CHECK(ef.eng.postSemis == 12);
	amt[0] = 1.f; amt[1] = 0.5f;
	computeEffective(prog, dest, amt, 0.f, &ef);
	CHECK(ef.eng.length == 16 && ef.eng.postSemis == 0);
	// both on the same direct destination: CTRL 2 wins
	dest[0] = DEST_TEMPO; dest[1] = DEST_TEMPO;
	amt[0] = 0.f; amt[1] = 1.f;
	computeEffective(prog, dest, amt, 0.f, &ef);
	CHECK(ef.value[F_TEMPO] == 300 && ef.owner[F_TEMPO] == 2);
	// offsets add
	dest[0] = DEST_PULSES; dest[1] = DEST_PULSES;
	amt[0] = 1.f; amt[1] = 1.f;
	computeEffective(prog, dest, amt, 0.f, &ef);
	CHECK(ef.eng.pulseOffset == 14);
	// mutate amount offset and snapshot select
	dest[0] = DEST_MUT_AMOUNT; dest[1] = DEST_SNAPSHOT;
	amt[0] = 0.75f; amt[1] = 1.f;
	computeEffective(prog, dest, amt, 0.f, &ef);
	CHECK(std::fabs(ef.mutateOffset - 0.5f) < 1e-5f && ef.snapshot == 7);
	amt[1] = 0.f;
	computeEffective(prog, dest, amt, 0.f, &ef);
	CHECK(ef.snapshot == 0);
	// transpose input goes in before the scale
	dest[0] = DEST_NONE;
	computeEffective(prog, dest, amt, 1.f / 12.f, &ef);
	CHECK(std::fabs(ef.eng.preSemis - 1.f) < 1e-5f);
	CHECK(ctrlAmount(0.5f, 5.f) == 1.f && ctrlAmount(0.5f, -10.f) == 0.f);
	// every direct destination's value is in its field range
	for (int d = 0; d < NUM_DESTS; d++) {
		if (DESTS[d].kind != DEST_KIND_DIRECT) continue;
		for (float a = 0.f; a <= 1.f; a += 0.01f) {
			int v = directValue(DESTS[d].field, a);
			CHECK(v >= FIELDS[DESTS[d].field].min && v <= FIELDS[DESTS[d].field].max);
		}
	}
	for (int f = 0; f < NUM_FIELDS; f++)
		for (int v = FIELDS[f].min; v <= FIELDS[f].max; v++)
			CHECK(!fieldText(f, v).empty());
}

static void testSkip() {
	Stage st[NUM_STAGES];
	Settings s;
	s.length = 5;
	st[1].skip = true;
	st[3].skip = true;
	SeqEngine e;
	std::vector<int> v;
	bool eoc[6];
	for (int i = 0; i < 6; i++) {
		PulseEvent ev = e.pulse(st, s);
		v.push_back(ev.stage);
		eoc[i] = ev.endOfCycle;
	}
	CHECK((v == std::vector<int>{0, 2, 4, 0, 2, 4}));
	CHECK(eoc[0] && !eoc[1] && eoc[3]);
	// skipping the first stage still marks the new cycle
	st[0].skip = true;
	SeqEngine e2;
	CHECK(e2.pulse(st, s).stage == 2);
	CHECK(e2.pulse(st, s).stage == 4);
	PulseEvent ev = e2.pulse(st, s);
	CHECK(ev.stage == 2 && ev.endOfCycle);
	// every order only plays unskipped stages
	for (int o = 0; o < NUM_ORDERS; o++) {
		s.order = o;
		SeqEngine e3;
		for (int i = 0; i < 200; i++) {
			int x = e3.pulse(st, s).stage;
			CHECK(x == 2 || x == 4);
		}
	}
	// all skipped: keeps running with no gates
	for (int i = 0; i < NUM_STAGES; i++) st[i].skip = true;
	s.order = ORDER_FORWARD;
	SeqEngine e4;
	for (int i = 0; i < 10; i++) {
		PulseEvent p = e4.pulse(st, s);
		CHECK(!p.gate && p.stage >= 0 && p.stage < 5);
	}
}

static void testAcid() {
	Stage st[NUM_STAGES];
	Settings s;
	s.length = 3;
	s.slideType = SLIDE_ACID;
	st[0].slide = true;
	st[0].pulses = 2;
	st[0].gate = GATE_REST;   // Acid turns a slide stage into a tied Hold
	SeqEngine e;
	PulseEvent a = e.pulse(st, s);
	CHECK(a.stage == 0 && a.gate && a.holdGate && a.tie && !a.slide);
	PulseEvent b = e.pulse(st, s);
	CHECK(b.stage == 0 && !b.gate);
	PulseEvent c = e.pulse(st, s);
	CHECK(c.stage == 1 && c.slide && !c.tie);  // glides out of stage 1 into stage 2
	PulseEvent d = e.pulse(st, s);
	CHECK(d.stage == 2 && !d.slide);
	// Tempo mode: slides into the marked stage instead
	s.slideType = SLIDE_TEMPO;
	SeqEngine e2;
	PulseEvent x = e2.pulse(st, s);
	CHECK(x.slide && !x.tie && !x.gate);
}

static void testStart() {
	Stage st[NUM_STAGES];
	Settings s;
	s.length = 4;
	s.start = 14;
	SeqEngine e;
	std::vector<int> v;
	for (int i = 0; i < 5; i++) v.push_back(e.pulse(st, s).stage);
	CHECK((v == std::vector<int>{14, 15, 0, 1, 14}));
	CHECK(SeqEngine::inWindow(s, 15) && SeqEngine::inWindow(s, 1) && !SeqEngine::inWindow(s, 2) && !SeqEngine::inWindow(s, 13));
	s.order = ORDER_REVERSE;
	SeqEngine e2;
	CHECK(e2.pulse(st, s).stage == 1);
	// start stage set from the menu field (1-based)
	int prog[NUM_FIELDS];
	for (int f = 0; f < NUM_FIELDS; f++) prog[f] = FIELDS[f].def;
	prog[F_START] = 5;
	int dest[4] = {DEST_NONE, DEST_NONE, DEST_NONE, DEST_START};
	float amt[4] = {0.5f, 0.5f, 0.5f, 1.f};
	Effective ef;
	computeEffective(prog, dest, amt, 0.f, &ef, 2);
	CHECK(ef.eng.start == 4);
	computeEffective(prog, dest, amt, 0.f, &ef, 4);
	CHECK(ef.eng.start == 15 && ef.owner[F_START] == 4);
}

static bool sameStage(const Stage& a, const Stage& b) {
	return a.pitch == b.pitch && a.pulses == b.pulses && a.gate == b.gate && a.slide == b.slide &&
		a.skip == b.skip && a.degreeOffset == b.degreeOffset;
}

static void testMutate() {
	Stage base[NUM_STAGES];
	for (int i = 0; i < NUM_STAGES; i++) {
		base[i].pitch = i / 16.f;
		base[i].pulses = 1 + i % 3;
		base[i].gate = GATE_SINGLE;
	}
	MutateSettings m;
	m.slides = true;
	m.skips = true;
	// amount 0 = exactly as programmed
	for (int i = 0; i < NUM_STAGES; i++)
		CHECK(sameStage(mutateStage(base[i], i, 1234, m), base[i]));
	// more amount = never fewer changed stages, and the same seed gives the same result
	int prev = 0;
	for (float a = 0.f; a <= 1.0001f; a += 0.05f) {
		m.amount = a;
		int changed = 0;
		for (int i = 0; i < NUM_STAGES; i++) {
			Stage x = mutateStage(base[i], i, 1234, m);
			Stage y = mutateStage(base[i], i, 1234, m);
			CHECK(sameStage(x, y));
			CHECK(x.pulses >= 1 && x.pulses <= 8 && x.gate >= 0 && x.gate < NUM_GATE_TYPES);
			CHECK(std::abs(x.degreeOffset) <= m.pitchRange);
			if (!sameStage(x, base[i])) changed++;
		}
		CHECK(changed >= prev);
		prev = changed;
	}
	CHECK(prev == NUM_STAGES);  // fully mutated: every stage changed
	// back to 0 = back to the original
	m.amount = 0.f;
	for (int i = 0; i < NUM_STAGES; i++)
		CHECK(sameStage(mutateStage(base[i], i, 1234, m), base[i]));
	// locked stages never change
	m.amount = 1.f;
	Stage locked = base[3];
	locked.mutable_ = false;
	CHECK(sameStage(mutateStage(locked, 3, 1234, m), locked));
	// a different seed gives a different pattern
	int diff = 0;
	for (int i = 0; i < NUM_STAGES; i++)
		if (!sameStage(mutateStage(base[i], i, 1234, m), mutateStage(base[i], i, 98765, m))) diff++;
	CHECK(diff > 4);
	// pitch off, rhythm off: nothing changes
	m.pitchRange = 0;
	m.rhythm = m.slides = m.skips = false;
	for (int i = 0; i < NUM_STAGES; i++)
		CHECK(sameStage(mutateStage(base[i], i, 1234, m), base[i]));
	// pitch offset reaches the note
	Settings s;
	Stage st;
	st.pitch = 0.f;
	st.degreeOffset = 2;
	CHECK(SeqEngine::stageSemis(st, s) == 4);  // major: two degrees up = E
	CHECK(nextSeed(5, 1) != 5 && nextSeed(5, 1) == nextSeed(5, 1));
}

static void testSnapshotBoundary() {
	Stage st[NUM_STAGES];
	Settings s;
	s.length = 3;
	st[1].pulses = 2;
	SeqEngine e;
	CHECK(e.nextPulseStartsCycle(st, s));
	e.pulse(st, s);                        // stage 1
	CHECK(!e.nextPulseStartsCycle(st, s));
	e.pulse(st, s);                        // stage 2, pulse 1
	CHECK(!e.nextPulseStartsCycle(st, s));
	e.pulse(st, s);                        // stage 2, pulse 2
	CHECK(!e.nextPulseStartsCycle(st, s));
	e.pulse(st, s);                        // stage 3
	CHECK(e.nextPulseStartsCycle(st, s));
}

int main() {
	testMutate();
	testSnapshotBoundary();
	testStart();
	testAcid();
	testSkip();
	testOrders();
	testPulsesAndGates();
	testScales();
	testCtrl();
	if (failures) {
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("all engine tests passed\n");
	return 0;
}
