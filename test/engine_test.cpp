// Tests for the Metroplex engine. Builds without VCV Rack:
//   c++ -std=c++11 -O2 -o test/engine_test test/engine_test.cpp && test/engine_test
#include "../src/Program.hpp"
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

static std::vector<int> accRun(int mode, int polar, int step, int limit, int n) {
	Settings s;
	s.accMode = mode;
	s.accPolarity = polar;
	s.accStep = step;
	s.accLimit = limit;
	Accumulator a;
	std::vector<int> out;
	for (int i = 0; i < n; i++) {
		a.advance(s);
		out.push_back(a.value);
	}
	return out;
}

static void testAccumulator() {
	CHECK((accRun(ACC_WRAP, ACC_UNIPOLAR, 1, 3, 6) == std::vector<int>{1, 2, 3, 0, 1, 2}));
	CHECK((accRun(ACC_WRAP, ACC_UNIPOLAR, 2, 3, 4) == std::vector<int>{2, 0, 2, 0}));
	CHECK((accRun(ACC_WRAP, ACC_UNIPOLAR, -1, 2, 4) == std::vector<int>{-1, -2, 0, -1}));
	CHECK((accRun(ACC_WRAP, ACC_BIPOLAR, 1, 2, 6) == std::vector<int>{1, 2, -2, -1, 0, 1}));
	CHECK((accRun(ACC_PENDULUM, ACC_UNIPOLAR, 1, 2, 7) == std::vector<int>{1, 2, 1, 0, 1, 2, 1}));
	CHECK((accRun(ACC_PENDULUM, ACC_BIPOLAR, 2, 3, 6) == std::vector<int>{2, 2, 0, -2, -2, 0}));
	CHECK((accRun(ACC_HOLD, ACC_UNIPOLAR, 3, 7, 4) == std::vector<int>{3, 6, 7, 7}));
	// frozen
	Settings s;
	s.accDirection = 0;
	Accumulator a;
	a.advance(s);
	CHECK(a.value == 0);
	// reverse direction
	s.accDirection = -1;
	s.accPolarity = ACC_BIPOLAR;
	a.advance(s);
	CHECK(a.value == -1);
	// large steps stay in range
	for (int m = 0; m < NUM_ACC_MODES; m++)
		for (int p = 0; p < 2; p++)
			for (int st = -7; st <= 7; st++)
				for (int lim = 1; lim <= 16; lim++)
					for (int x : accRun(m, p, st, lim, 50))
						CHECK(x >= -lim && x <= lim);
}

static void testAccumulateInSequence() {
	SeqEngine e;
	Stage st[NUM_STAGES];
	Settings s;
	s.length = 2;
	s.scale = 1;  // major
	s.range = 1;
	st[0].accum = true;
	st[0].pitch = 0.f;
	// pass 1 plays the programmed note, then accumulates
	PulseEvent ev = e.pulse(st, s);
	CHECK(e.stageSemis(st[0], 0, s) == 0);
	e.accumulate(ev, st, s);
	CHECK(e.stageSemis(st[0], 0, s) == 2);  // one degree up in major = D
	ev = e.pulse(st, s);
	e.accumulate(ev, st, s);  // stage 1 has accum off
	CHECK(e.stageSemis(st[1], 1, s) == 0);
	// pulse trigger on a 3-pulse stage moves three times per pass
	SeqEngine e2;
	s.accTrigger = ACC_ON_PULSE;
	st[0].pulses = 3;
	for (int i = 0; i < 3; i++) {
		ev = e2.pulse(st, s);
		e2.accumulate(ev, st, s);
	}
	CHECK(e2.stageAcc[0].value == 3);
	// track scope shares one counter across stages
	SeqEngine e3;
	s.accScope = ACC_TRACK;
	s.accTrigger = ACC_ON_STAGE;
	st[0].pulses = 1;
	st[1].accum = true;
	for (int i = 0; i < 4; i++) {
		ev = e3.pulse(st, s);
		e3.accumulate(ev, st, s);
	}
	CHECK(e3.trackAcc.value == 4);
	CHECK(e3.accOffset(5, s) == 4);
	e3.reset();
	CHECK(e3.trackAcc.value == 0 && e3.stage == -1);
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
	CHECK(e.stageSemis(st, 0, s) == 24);
	s.root = 9;  // A
	s.octave = -1;
	CHECK(std::fabs(e.stageVolts(st, 0, s) - (33.f / 12.f - 1.f)) < 1e-5f);
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
	// direction dead zone
	dest[0] = DEST_ACC_DIR; dest[1] = DEST_NONE;
	amt[0] = 0.5f;
	computeEffective(prog, dest, amt, 0.f, &ef);
	CHECK(ef.eng.accDirection == 0);
	amt[0] = 0.2f;
	computeEffective(prog, dest, amt, 0.f, &ef);
	CHECK(ef.eng.accDirection == -1);
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

int main() {
	testOrders();
	testPulsesAndGates();
	testAccumulator();
	testAccumulateInSequence();
	testScales();
	testCtrl();
	if (failures) {
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("all engine tests passed\n");
	return 0;
}
