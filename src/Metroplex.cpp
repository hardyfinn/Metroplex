// Metroplex: a 16-stage pulse-count sequencer with a menu display, four
// assignable CTRL knobs, Mutate and snapshots.
#include "plugin.hpp"
#include "Program.hpp"
#include "Mutate.hpp"
#include "Layout.hpp"

using namespace mpx;

static const int NUM_PAGE_BUTTONS = 6;  // HOME CLOCK SEQ SCALE SLIDE MUTATE
static const char* const PAGE_BUTTON_NAMES[NUM_PAGE_BUTTONS] = {"Home", "Clock", "Sequence", "Scale", "Slide", "Mutate"};

struct Metroplex : Module {
	enum ParamId {
		ENUMS(PITCH_PARAM, NUM_STAGES),
		ENUMS(PULSES_PARAM, NUM_STAGES),
		ENUMS(GATE_PARAM, NUM_STAGES),
		ENUMS(SLIDE_PARAM, NUM_STAGES),
		ENUMS(MUTABLE_PARAM, NUM_STAGES),  // the MUT row (was the accumulator row in 2.0)
		ENUMS(CTRL_PARAM, 2),
		ENUMS(CTRL_DEST_PARAM, 2),
		ENUMS(CTRL_PAGE_PARAM, 2),
		ENUMS(PAGE_PARAM, NUM_PAGE_BUTTONS),
		RUN_PARAM,
		ENUMS(FIELD_PARAM, NUM_FIELDS),
		ALL_PULSES_PARAM,
		ENUMS(SKIP_PARAM, NUM_STAGES),
		ENUMS(CTRL34_DEST_PARAM, 2),
		ENUMS(CTRL_ATT_PARAM, 2),
		ENUMS(CTRL34_PARAM, 2),
		ENUMS(CTRL34_PAGE_PARAM, 2),
		ENUMS(CTRL34_ATT_PARAM, 2),
		MUTATE_PARAM,
		NEW_SEED_PARAM,
		SAVE_PARAM,
		ENUMS(SNAP_PARAM, NUM_SNAPSHOTS),
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		RESET_INPUT,
		RUN_INPUT,
		TRANSPOSE_INPUT,
		SEED_INPUT,  // a trigger makes a new seed
		ENUMS(CTRL_CV_INPUT, 2),
		ENUMS(CTRL34_CV_INPUT, 2),
		SNAP_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		CLOCK_OUTPUT,
		EOC_OUTPUT,
		GATE_OUTPUT,
		PITCH_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(STAGE_LIGHT, NUM_STAGES),
		ENUMS(GATE_LIGHT, NUM_STAGES * 3),
		ENUMS(SLIDE_LIGHT, NUM_STAGES),
		ENUMS(MUTABLE_LIGHT, NUM_STAGES),
		ENUMS(SKIP_LIGHT, NUM_STAGES),
		ENUMS(CTRL_PAGE_LIGHT, NUM_CTRLS),
		ENUMS(PAGE_LIGHT, NUM_PAGE_BUTTONS),
		RUN_LIGHT,
		CLOCK_LIGHT,
		ENUMS(SNAP_LIGHT, NUM_SNAPSHOTS),
		SAVE_LIGHT,
		SEED_LIGHT,
		LIGHTS_LEN
	};

	SeqEngine seq;
	Stage baseStages[NUM_STAGES];  // as set on the panel
	Stage stages[NUM_STAGES];      // after Mutate: what plays
	Effective eff;
	MutateSettings mut;

	// Mutate seed and snapshots (saved with the patch)
	uint32_t seed = 1;
	Snapshot snapshots[NUM_SNAPSHOTS];
	int curSnap = 0;
	int pendingSnap = -1;
	int lastSnapCv = -1, lastCtrlSnap = -1;
	dsp::BooleanTrigger seedButton, saveButton, snapButton[NUM_SNAPSHOTS];
	bool saveArmed = false;  // SAVE pressed: the next snapshot button stores
	dsp::SchmittTrigger seedTrigger;
	float seedLight = 0.f;

	// display state (written by the UI and by the page buttons)
	int page = PAGE_HOME;
	int selected = 0;  // row on the current page

	// clock
	dsp::SchmittTrigger clockTrigger, resetTrigger, runTrigger;
	dsp::BooleanTrigger pageTrigger[NUM_PAGE_BUTTONS], ctrlPageTrigger[NUM_CTRLS];
	float phase = 0.f;              // internal clock
	float samplesSinceEdge = 0.f;   // external clock period measuring
	float edgePeriod = 0.125f;      // seconds between raw clock edges
	int divCount = 0;
	bool externalClock = false;
	float measuredBpm = 0.f;

	// output state
	int curStage = -1;
	int lastAllPulses = -1;  // -1 until the first process(), so loading a patch keeps its stage values
	bool havePrev = false;
	float pitchOut = 0.f;
	bool sliding = false;
	float slideRate = 0.f;   // volts per second
	float gateLeft = 0.f, gapLeft = 0.f;
	bool legato = false;     // Acid: the next gate continues the current one
	dsp::PulseGenerator eocPulse;
	float clockOutLeft = 0.f;
	float clockLight = 0.f;
	float clockBlink = 0.f;

	dsp::ClockDivider lightDivider;

	Metroplex() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int i = 0; i < NUM_STAGES; i++) {
			std::string n = string::f("Stage %d", i + 1);
			configParam<PitchQuantity>(PITCH_PARAM + i, 0.f, 1.f, 0.f, n + " pitch");
			configParam(PULSES_PARAM + i, 1.f, 8.f, 1.f, n + " pulse count")->snapEnabled = true;
			configSwitch(GATE_PARAM + i, 0.f, 3.f, 1.f, n + " gate type", {"Rest", "Single", "Multi", "Hold"});
			configSwitch(SLIDE_PARAM + i, 0.f, 1.f, 0.f, n + " slide", {"Off", "On"});
			configSwitch(MUTABLE_PARAM + i, 0.f, 1.f, 1.f, n + " can mutate", {"Locked", "Can mutate"});
			configSwitch(SKIP_PARAM + i, 0.f, 1.f, 0.f, n + " skip", {"Plays", "Skipped"});
		}
		std::vector<std::string> destNames;
		for (int d = 0; d < NUM_DESTS; d++)
			destNames.push_back(DESTS[d].name);
		const float knobDefault[NUM_CTRLS] = {0.5f, 0.25f, 0.5f, 0.5f};
		const int destDefault[NUM_CTRLS] = {DEST_GATELEN, DEST_SLIDE_AMT, DEST_NONE, DEST_NONE};
		for (int k = 0; k < NUM_CTRLS; k++) {
			auto* q = configParam<CtrlQuantity>(knobId(k), 0.f, 1.f, knobDefault[k], string::f("CTRL %d", k + 1));
			q->k = k;
			configSwitch(destParamId(k), 0.f, NUM_DESTS - 1, destDefault[k], string::f("CTRL %d destination", k + 1), destNames);
			configButton(pageButtonId(k), string::f("CTRL %d menu", k + 1));
			// attenuverter: noon = CV off, right = normal, left = inverted
			configParam(attId(k), -1.f, 1.f, 1.f, string::f("CTRL %d CV amount (noon = off, left = inverted)", k + 1), "%", 0.f, 100.f);
			configInput(cvId(k), string::f("CTRL %d CV (+-5 V moves the whole knob, scaled by CV amount)", k + 1));
		}
		for (int b = 0; b < NUM_PAGE_BUTTONS; b++)
			configButton(PAGE_PARAM + b, std::string(PAGE_BUTTON_NAMES[b]) + " page (press again for the next row)");
		configSwitch(RUN_PARAM, 0.f, 1.f, 1.f, "Run", {"Stopped", "Running"});
		for (int f = 0; f < NUM_FIELDS; f++) {
			const FieldInfo& fi = FIELDS[f];
			if (fieldIsNamed(f)) {
				std::vector<std::string> names;
				for (int v = fi.min; v <= fi.max; v++)
					names.push_back(fieldText(f, v));
				configSwitch(FIELD_PARAM + f, fi.min, fi.max, fi.def, fi.name, names);
			}
			else {
				configParam(FIELD_PARAM + f, fi.min, fi.max, fi.def, fi.name, fi.unit)->snapEnabled = true;
			}
		}
		configInput(CLOCK_INPUT, "Clock (unpatched = internal clock, 4 pulses per beat)");
		configInput(RESET_INPUT, "Reset");
		configInput(RUN_INPUT, "Run toggle");
		configInput(TRANSPOSE_INPUT, "Transpose (1 V/oct, before the scale)");
		configInput(SEED_INPUT, "New seed (trigger)");
		configInput(SNAP_INPUT, "Snapshot select (0-7 V picks 1-8)");
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.f, "Mutate (left = as programmed)", "%", 0.f, 100.f);
		configButton(NEW_SEED_PARAM, "New seed");
		configButton(SAVE_PARAM, "Save (press, then press a snapshot button)");
		for (int j = 0; j < NUM_SNAPSHOTS; j++)
			configButton(SNAP_PARAM + j, string::f("Snapshot %d (hold SAVE to store)", j + 1));
		configOutput(CLOCK_OUTPUT, "Clock (after division)");
		configOutput(EOC_OUTPUT, "End of cycle");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(PITCH_OUTPUT, "Pitch (1 V/oct)");
		configParam(ALL_PULSES_PARAM, 1.f, 8.f, 1.f, "All pulses (sets every stage's pulse count)")->snapEnabled = true;
		lightDivider.setDivision(64);
		int prog[NUM_FIELDS];
		for (int f = 0; f < NUM_FIELDS; f++)
			prog[f] = FIELDS[f].def;
		int dest[2] = {DEST_NONE, DEST_NONE};
		float amt[2] = {0.5f, 0.5f};
		computeEffective(prog, dest, amt, 0.f, &eff);
	}

	// ---- tooltips that show what the knob is doing

	struct PitchQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			Metroplex* m = dynamic_cast<Metroplex*>(module);
			if (!m)
				return ParamQuantity::getDisplayValueString();
			int i = paramId - PITCH_PARAM;
			char buf[16];
			noteName(SeqEngine::stageVolts(m->stages[i], m->eff.eng), buf, sizeof buf);
			return buf;
		}
	};

	struct CtrlQuantity : ParamQuantity {
		int k = 0;
		std::string getDisplayValueString() override {
			Metroplex* m = dynamic_cast<Metroplex*>(module);
			if (!m)
				return ParamQuantity::getDisplayValueString();
			return m->ctrlReadout(k);
		}
		std::string getLabel() override {
			Metroplex* m = dynamic_cast<Metroplex*>(module);
			if (!m)
				return ParamQuantity::getLabel();
			return string::f("CTRL %d: %s", k + 1, DESTS[m->ctrlDest(k)].name);
		}
	};

	// CTRL 1-2 and CTRL 3-4 live in separate id blocks (3-4 were added in 2.1)
	static int destParamId(int k) {
		return k < 2 ? CTRL_DEST_PARAM + k : CTRL34_DEST_PARAM + (k - 2);
	}
	static int knobId(int k) {
		return k < 2 ? CTRL_PARAM + k : CTRL34_PARAM + (k - 2);
	}
	static int pageButtonId(int k) {
		return k < 2 ? CTRL_PAGE_PARAM + k : CTRL34_PAGE_PARAM + (k - 2);
	}
	static int attId(int k) {
		return k < 2 ? CTRL_ATT_PARAM + k : CTRL34_ATT_PARAM + (k - 2);
	}
	static int cvId(int k) {
		return k < 2 ? CTRL_CV_INPUT + k : CTRL34_CV_INPUT + (k - 2);
	}

	int ctrlDest(int k) {
		return clamp((int)std::round(params[destParamId(k)].getValue()), 0, NUM_DESTS - 1);
	}

	float ctrlAmountNow(int k) {
		return ctrlAmount(params[knobId(k)].getValue(), inputs[cvId(k)].getVoltage() * params[attId(k)].getValue());
	}

	std::string ctrlReadout(int k) {
		int d = ctrlDest(k);
		const DestInfo& di = DESTS[d];
		float a = ctrlAmountNow(k);
		switch (di.kind) {
			case DEST_KIND_DIRECT: return fieldText(di.field, directValue(di.field, a));
			case DEST_KIND_OFFSET: {
				int v = offsetValue(di.range, a);
				const char* unit = (d == DEST_OCTAVE) ? " oct" : (d == DEST_PULSES) ? " pulses" : " semis";
				return string::f("%+d%s", v, unit);
			}
			case DEST_KIND_MUTATE: return string::f("%+d%%", (int)std::lround((a - 0.5f) * 200.f));
			case DEST_KIND_SNAPSHOT: return string::f("Snapshot %d", std::min((int)(a * 8.f), 7) + 1);
			default: return string::f("%d%%", (int)std::round(a * 100.f));
		}
	}

	// ---- menu editing (called from the UI)

	int pageRows() {
		if (isCtrlPage(page))
			return 1;
		return PAGES[page].count;
	}

	// The param that the selected row edits, or -1.
	int selectedParam() {
		if (isCtrlPage(page))
			return destParamId(page - PAGE_CTRL1);
		if (PAGES[page].count == 0)
			return -1;
		int f = PAGES[page].fields[clamp(selected, 0, PAGES[page].count - 1)];
		return FIELD_PARAM + f;
	}

	// Which CTRL knob (1 or 2) owns a field param right now, 0 if none.
	int paramOwner(int paramId) {
		int f = paramId - FIELD_PARAM;
		if (f < 0 || f >= NUM_FIELDS)
			return 0;
		return eff.owner[f];
	}

	void goToPage(int p) {
		if (p == page) {
			int rows = pageRows();
			selected = rows > 0 ? (selected + 1) % rows : 0;
		}
		else {
			page = p;
			selected = 0;
		}
	}

	void nextRow() {
		int rows = pageRows();
		if (rows > 0)
			selected = (selected + 1) % rows;
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		page = PAGE_HOME;
		selected = 0;
		seq.reset();
		curStage = -1;
		havePrev = false;
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "page", json_integer(page));
		json_object_set_new(root, "seed", json_integer(seed));
		json_object_set_new(root, "snapshot", json_integer(curSnap));
		json_t* arr = json_array();
		for (int j = 0; j < NUM_SNAPSHOTS; j++) {
			const Snapshot& sn = snapshots[j];
			json_t* o = json_object();
			json_object_set_new(o, "used", json_boolean(sn.used));
			if (sn.used) {
				json_object_set_new(o, "seed", json_integer(sn.seed));
				json_object_set_new(o, "start", json_integer(sn.start));
				json_object_set_new(o, "length", json_integer(sn.length));
				json_object_set_new(o, "order", json_integer(sn.order));
				json_t* st = json_array();
				for (int i = 0; i < NUM_STAGES; i++) {
					const Stage& g = sn.stages[i];
					json_t* a = json_array();
					json_array_append_new(a, json_real(g.pitch));
					json_array_append_new(a, json_integer(g.pulses));
					json_array_append_new(a, json_integer(g.gate));
					json_array_append_new(a, json_integer(g.slide));
					json_array_append_new(a, json_integer(g.mutable_));
					json_array_append_new(a, json_integer(g.skip));
					json_array_append_new(st, a);
				}
				json_object_set_new(o, "stages", st);
			}
			json_array_append_new(arr, o);
		}
		json_object_set_new(root, "snapshots", arr);
		return root;
	}

	void dataFromJson(json_t* root) override {
		json_t* p = json_object_get(root, "page");
		if (p)
			page = clamp((int)json_integer_value(p), 0, NUM_PAGES - 1);
		selected = 0;
		json_t* sd = json_object_get(root, "seed");
		if (sd)
			seed = (uint32_t)json_integer_value(sd);
		json_t* cs = json_object_get(root, "snapshot");
		if (cs)
			curSnap = clamp((int)json_integer_value(cs), 0, NUM_SNAPSHOTS - 1);
		json_t* arr = json_object_get(root, "snapshots");
		for (int j = 0; j < NUM_SNAPSHOTS; j++)
			snapshots[j] = Snapshot();
		if (arr) {
			for (int j = 0; j < NUM_SNAPSHOTS && j < (int)json_array_size(arr); j++) {
				json_t* o = json_array_get(arr, j);
				Snapshot& sn = snapshots[j];
				sn.used = json_is_true(json_object_get(o, "used"));
				if (!sn.used)
					continue;
				sn.seed = (uint32_t)json_integer_value(json_object_get(o, "seed"));
				sn.start = clamp((int)json_integer_value(json_object_get(o, "start")), 1, 16);
				sn.length = clamp((int)json_integer_value(json_object_get(o, "length")), 1, 16);
				sn.order = clamp((int)json_integer_value(json_object_get(o, "order")), 0, NUM_ORDERS - 1);
				json_t* st = json_object_get(o, "stages");
				for (int i = 0; i < NUM_STAGES && st && i < (int)json_array_size(st); i++) {
					json_t* a = json_array_get(st, i);
					Stage& g = sn.stages[i];
					g.pitch = clamp((float)json_number_value(json_array_get(a, 0)), 0.f, 1.f);
					g.pulses = clamp((int)json_integer_value(json_array_get(a, 1)), 1, 8);
					g.gate = clamp((int)json_integer_value(json_array_get(a, 2)), 0, 3);
					g.slide = json_integer_value(json_array_get(a, 3)) != 0;
					g.mutable_ = json_integer_value(json_array_get(a, 4)) != 0;
					g.skip = json_integer_value(json_array_get(a, 5)) != 0;
				}
			}
		}
		pendingSnap = -1;
	}

	// ---- snapshots (run on the audio thread)

	void saveSnapshot(int j) {
		Snapshot& sn = snapshots[j];
		sn.used = true;
		for (int i = 0; i < NUM_STAGES; i++)
			sn.stages[i] = baseStages[i];
		sn.start = (int)std::round(params[FIELD_PARAM + F_START].getValue());
		sn.length = (int)std::round(params[FIELD_PARAM + F_LENGTH].getValue());
		sn.order = (int)std::round(params[FIELD_PARAM + F_ORDER].getValue());
		sn.seed = seed;
		curSnap = j;
		pendingSnap = -1;
	}

	void recallSnapshot(int j) {
		pendingSnap = -1;
		curSnap = j;
		const Snapshot& sn = snapshots[j];
		if (!sn.used)
			return;  // an empty slot: carry on with what's there
		for (int i = 0; i < NUM_STAGES; i++) {
			const Stage& g = sn.stages[i];
			params[PITCH_PARAM + i].setValue(g.pitch);
			params[PULSES_PARAM + i].setValue((float)g.pulses);
			params[GATE_PARAM + i].setValue((float)g.gate);
			params[SLIDE_PARAM + i].setValue(g.slide ? 1.f : 0.f);
			params[MUTABLE_PARAM + i].setValue(g.mutable_ ? 1.f : 0.f);
			params[SKIP_PARAM + i].setValue(g.skip ? 1.f : 0.f);
		}
		params[FIELD_PARAM + F_START].setValue((float)sn.start);
		params[FIELD_PARAM + F_LENGTH].setValue((float)sn.length);
		params[FIELD_PARAM + F_ORDER].setValue((float)sn.order);
		seed = sn.seed;
		readStages();
	}

	void requestSnapshot(int j, bool running) {
		if (eff.value[F_SNAP_MODE] == 0 || !running)
			recallSnapshot(j);
		else
			pendingSnap = j;
	}

	void newSeed() {
		seed = nextSeed(seed, (uint32_t)random::u32());
		seedLight = 1.f;
	}

	// ---- audio thread

	void readStages() {
		mut.amount = clamp(params[MUTATE_PARAM].getValue() + eff.mutateOffset, 0.f, 1.f);
		mut.pitchRange = eff.value[F_MUT_PITCH];
		mut.rhythm = eff.value[F_MUT_RHYTHM] != 0;
		mut.slides = eff.value[F_MUT_SLIDES] != 0;
		mut.skips = eff.value[F_MUT_SKIPS] != 0;
		for (int i = 0; i < NUM_STAGES; i++) {
			Stage& s = baseStages[i];
			s.pitch = params[PITCH_PARAM + i].getValue();
			s.pulses = clamp((int)std::round(params[PULSES_PARAM + i].getValue()), 1, 8);
			s.gate = clamp((int)std::round(params[GATE_PARAM + i].getValue()), 0, 3);
			s.slide = params[SLIDE_PARAM + i].getValue() > 0.5f;
			s.mutable_ = params[MUTABLE_PARAM + i].getValue() > 0.5f;
			s.skip = params[SKIP_PARAM + i].getValue() > 0.5f;
			stages[i] = mutateStage(s, i, seed, mut);
		}
	}

	void startGate(float seconds) {
		bool tied = legato;
		legato = false;
		if (gateLeft > 0.f && gapLeft <= 0.f && !tied) {
			// already high: drop it for 1 ms so the next module sees a new gate
			gapLeft = 0.001f;
		}
		gateLeft = std::max(seconds, 0.002f);
	}

	void doReset() {
		seq.reset();
		divCount = 0;
		phase = 0.f;
		curStage = -1;
		havePrev = false;
		sliding = false;
		legato = false;
	}

	void playPulse(float pulsePeriod) {
		const Settings& s = eff.eng;
		PulseEvent ev = seq.pulse(stages, s);
		curStage = ev.stage;
		float target = SeqEngine::stageVolts(stages[curStage], s);
		float slideAmt = eff.value[F_SLIDE_AMT] / 100.f;
		if (ev.slide && havePrev && slideAmt > 0.f && target != pitchOut) {
			float dist = std::fabs(target - pitchOut);
			if (eff.value[F_SLIDE_TYPE] == SLIDE_TEMPO) {
				// the same time for every interval: a share of one pulse
				float t = std::max(slideAmt * pulsePeriod, 1e-4f);
				slideRate = dist / t;
			}
			else {
				// constant speed: 100% = 2 seconds per octave, so big jumps take longer
				slideRate = 1.f / std::max(slideAmt * 2.f, 1e-4f);
			}
			sliding = true;
		}
		else if (ev.newStage) {
			sliding = false;
		}
		float gl = eff.value[F_GATELEN] / 100.f;
		if (ev.gate) {
			float len = ev.holdGate ? (ev.stagePulses - 1 + gl) * pulsePeriod : gl * pulsePeriod;
			if (ev.tie)
				len = (ev.stagePulses + 0.5f) * pulsePeriod;  // runs past the next pulse
			startGate(len);
			legato = ev.tie;
		}
		if (ev.endOfCycle && havePrev)
			eocPulse.trigger(1e-3f);
		clockOutLeft = std::max(0.5f * pulsePeriod, 0.001f);
		clockLight = 1.f;
		havePrev = true;
	}

	void process(const ProcessArgs& args) override {
		// master PULSES knob: when it moves, every stage takes its value
		int allPulses = clamp((int)std::round(params[ALL_PULSES_PARAM].getValue()), 1, 8);
		if (lastAllPulses >= 0 && allPulses != lastAllPulses) {
			for (int i = 0; i < NUM_STAGES; i++)
				params[PULSES_PARAM + i].setValue((float)allPulses);
		}
		lastAllPulses = allPulses;

		// menu values plus CTRL knobs
		int prog[NUM_FIELDS];
		for (int f = 0; f < NUM_FIELDS; f++)
			prog[f] = clamp((int)std::round(params[FIELD_PARAM + f].getValue()), FIELDS[f].min, FIELDS[f].max);
		int dest[NUM_CTRLS];
		float amt[NUM_CTRLS];
		for (int k = 0; k < NUM_CTRLS; k++) {
			dest[k] = ctrlDest(k);
			amt[k] = ctrlAmountNow(k);
		}
		computeEffective(prog, dest, amt, inputs[TRANSPOSE_INPUT].getVoltage(), &eff, NUM_CTRLS);
		readStages();

		// page buttons
		for (int b = 0; b < NUM_PAGE_BUTTONS; b++)
			if (pageTrigger[b].process(params[PAGE_PARAM + b].getValue() > 0.f))
				goToPage(b);
		for (int k = 0; k < NUM_CTRLS; k++)
			if (ctrlPageTrigger[k].process(params[pageButtonId(k)].getValue() > 0.f))
				goToPage(PAGE_CTRL1 + k);

		// run
		if (runTrigger.process(inputs[RUN_INPUT].getVoltage(), 0.1f, 1.f))
			params[RUN_PARAM].setValue(params[RUN_PARAM].getValue() > 0.5f ? 0.f : 1.f);
		bool running = params[RUN_PARAM].getValue() > 0.5f;

		// resets
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f))
			doReset();

		// Mutate seed
		if (seedButton.process(params[NEW_SEED_PARAM].getValue() > 0.f))
			newSeed();
		if (seedTrigger.process(inputs[SEED_INPUT].getVoltage(), 0.1f, 1.f))
			newSeed();

		// snapshots: buttons (press SAVE, then a number to store), SNAP input, or a CTRL
		if (saveButton.process(params[SAVE_PARAM].getValue() > 0.f))
			saveArmed = !saveArmed;
		for (int j = 0; j < NUM_SNAPSHOTS; j++)
			if (snapButton[j].process(params[SNAP_PARAM + j].getValue() > 0.f)) {
				if (saveArmed || params[SAVE_PARAM].getValue() > 0.f) {
					saveSnapshot(j);
					saveArmed = false;
				}
				else
					requestSnapshot(j, running);
			}
		if (inputs[SNAP_INPUT].isConnected()) {
			int j = clamp((int)std::floor(inputs[SNAP_INPUT].getVoltage() + 0.02f), 0, NUM_SNAPSHOTS - 1);
			if (j != lastSnapCv && (lastSnapCv >= 0 || j != curSnap))
				requestSnapshot(j, running);
			lastSnapCv = j;
		}
		else {
			lastSnapCv = -1;
		}
		if (eff.snapshot >= 0) {
			if (eff.snapshot != lastCtrlSnap && (lastCtrlSnap >= 0 || eff.snapshot != curSnap))
				requestSnapshot(eff.snapshot, running);
			lastCtrlSnap = eff.snapshot;
		}
		else {
			lastCtrlSnap = -1;
		}

		// clock
		bool edge = false;
		externalClock = inputs[CLOCK_INPUT].isConnected();
		if (externalClock) {
			samplesSinceEdge += 1.f;
			if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f)) {
				float period = samplesSinceEdge * args.sampleTime;
				if (period > 0.002f && period < 4.f) {
					edgePeriod = period;
					measuredBpm = 60.f / (period * 4.f);
				}
				samplesSinceEdge = 0.f;
				edge = true;
			}
		}
		else {
			edgePeriod = 60.f / (eff.value[F_TEMPO] * 4.f);
			if (running) {
				phase += args.sampleTime / edgePeriod;
				if (phase >= 1.f) {
					phase -= 1.f;
					edge = true;
				}
			}
		}
		if (edge && running) {
			int div = std::max(1, eff.value[F_DIV]);
			if (divCount % div == 0) {
				// a waiting snapshot lands on the first stage of a new cycle
				if (pendingSnap >= 0 && seq.nextPulseStartsCycle(stages, eff.eng))
					recallSnapshot(pendingSnap);
				playPulse(edgePeriod * div);
			}
			divCount = (divCount + 1) % div;
		}

		// pitch: follow the stage live, gliding when a slide is on
		if (curStage >= 0) {
			float target = SeqEngine::stageVolts(stages[curStage], eff.eng);
			if (sliding) {
				float step = slideRate * args.sampleTime;
				if (std::fabs(target - pitchOut) <= step) {
					pitchOut = target;
					sliding = false;
				}
				else {
					pitchOut += (target > pitchOut) ? step : -step;
				}
			}
			else {
				pitchOut = target;
			}
		}

		// gates
		bool gate = false;
		if (gapLeft > 0.f) {
			gapLeft -= args.sampleTime;
		}
		else if (gateLeft > 0.f) {
			gate = true;
			gateLeft -= args.sampleTime;
		}
		bool clockHigh = clockOutLeft > 0.f;
		if (clockHigh)
			clockOutLeft -= args.sampleTime;

		outputs[PITCH_OUTPUT].setVoltage(pitchOut);
		outputs[GATE_OUTPUT].setVoltage(gate ? 10.f : 0.f);
		outputs[EOC_OUTPUT].setVoltage(eocPulse.process(args.sampleTime) ? 10.f : 0.f);
		outputs[CLOCK_OUTPUT].setVoltage(clockHigh ? 10.f : 0.f);

		if (lightDivider.process())
			updateLights(args.sampleTime * lightDivider.getDivision(), running);
	}

	void updateLights(float dt, bool running) {
		for (int i = 0; i < NUM_STAGES; i++) {
			float b = (i == curStage) ? 1.f : (SeqEngine::inWindow(eff.eng, i) ? 0.06f : 0.f);
			lights[STAGE_LIGHT + i].setBrightness(b);
			float r = 0.f, g = 0.f, bl = 0.f;
			switch (stages[i].gate) {
				case GATE_REST: r = 0.18f; break;
				case GATE_SINGLE: g = 1.f; break;
				case GATE_MULTI: r = 1.f; g = 0.75f; break;
				case GATE_HOLD: bl = 1.f; g = 0.35f; break;
			}
			lights[GATE_LIGHT + 3 * i + 0].setBrightness(r);
			lights[GATE_LIGHT + 3 * i + 1].setBrightness(g);
			lights[GATE_LIGHT + 3 * i + 2].setBrightness(bl);
			lights[SLIDE_LIGHT + i].setBrightness(stages[i].slide ? 1.f : 0.f);
			lights[MUTABLE_LIGHT + i].setBrightness(baseStages[i].mutable_ ? 1.f : 0.f);
			lights[SKIP_LIGHT + i].setBrightness(stages[i].skip ? 1.f : 0.f);
		}
		for (int b = 0; b < NUM_PAGE_BUTTONS; b++)
			lights[PAGE_LIGHT + b].setBrightness(page == b ? 1.f : 0.f);
		for (int k = 0; k < NUM_CTRLS; k++)
			lights[CTRL_PAGE_LIGHT + k].setBrightness(page == PAGE_CTRL1 + k ? 1.f : 0.f);
		lights[RUN_LIGHT].setBrightness(running ? 1.f : 0.f);
		lights[CLOCK_LIGHT].setBrightness(clockLight);
		clockLight = std::max(0.f, clockLight - dt * 12.f);
		// snapshot lights: bright = current, dim = has something saved, blinking = waiting
		clockBlink = std::fmod(clockBlink + dt, 0.25f);
		bool blink = clockBlink < 0.125f;
		for (int j = 0; j < NUM_SNAPSHOTS; j++) {
			float b = (j == curSnap) ? 1.f : snapshots[j].used ? 0.2f : 0.f;
			if (j == pendingSnap)
				b = blink ? 1.f : 0.2f;
			lights[SNAP_LIGHT + j].setBrightness(b);
		}
		lights[SAVE_LIGHT].setBrightness(saveArmed ? (blink ? 1.f : 0.3f) : 0.f);
		lights[SEED_LIGHT].setBrightness(seedLight);
		seedLight = std::max(0.f, seedLight - dt * 4.f);
	}
};

// ---------------------------------------------------------------- display

static const NVGcolor AMBER = nvgRGB(0xff, 0xb3, 0x47);
static const NVGcolor AMBER_DIM = nvgRGB(0x6a, 0x48, 0x1c);
static const NVGcolor CYAN = nvgRGB(0x5f, 0xd0, 0xe4);
static const NVGcolor SCREEN = nvgRGB(0x0b, 0x0c, 0x0e);

// Shared drag/scroll editing for the encoder and the display.
struct EditWidget : OpaqueWidget {
	Metroplex* module = nullptr;
	float dragAccum = 0.f;
	float dragMoved = 0.f;
	Vec pressPos;
	int editParam = -1;
	float oldValue = 0.f;

	void beginEdit() {
		editParam = module ? module->selectedParam() : -1;
		if (editParam >= 0)
			oldValue = module->params[editParam].getValue();
	}

	void endEdit() {
		if (editParam < 0 || !module)
			return;
		float nv = module->params[editParam].getValue();
		if (nv != oldValue) {
			auto* h = new history::ParamChange;
			h->name = "change " + module->paramQuantities[editParam]->name;
			h->moduleId = module->id;
			h->paramId = editParam;
			h->oldValue = oldValue;
			h->newValue = nv;
			APP->history->push(h);
		}
		editParam = -1;
	}

	// Move the selected row by delta steps.
	void step(int delta) {
		if (!module)
			return;
		if (module->page == PAGE_HOME) {
			// on HOME, turning browses the pages
			module->page = (module->page + (delta > 0 ? 1 : NUM_PAGES - 1)) % NUM_PAGES;
			module->selected = 0;
			return;
		}
		int pid = module->selectedParam();
		if (pid < 0 || module->paramOwner(pid))
			return;  // a CTRL knob has this one
		ParamQuantity* q = module->paramQuantities[pid];
		float v = clamp(std::round(q->getValue()) + delta, q->getMinValue(), q->getMaxValue());
		q->setValue(v);
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS)
			pressPos = e.pos;
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			openMenu();
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT)
			e.consume(this);
	}

	void onDragStart(const DragStartEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		dragAccum = 0.f;
		dragMoved = 0.f;
		if (module && module->page != PAGE_HOME)
			beginEdit();
	}

	void onDragMove(const DragMoveEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		float dy = -e.mouseDelta.y;
		dragMoved += std::fabs(e.mouseDelta.x) + std::fabs(e.mouseDelta.y);
		dragAccum += dy;
		const float pixelsPerStep = 7.f;
		int mult = (APP->window->getMods() & GLFW_MOD_SHIFT) ? 10 : 1;
		while (dragAccum >= pixelsPerStep) {
			step(mult);
			dragAccum -= pixelsPerStep;
		}
		while (dragAccum <= -pixelsPerStep) {
			step(-mult);
			dragAccum += pixelsPerStep;
		}
	}

	void onDragEnd(const DragEndEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		endEdit();
		if (dragMoved < 2.f)
			click(e);
	}

	virtual void click(const DragEndEvent& e) {
		if (module)
			module->nextRow();
	}

	void onHoverScroll(const HoverScrollEvent& e) override {
		if (!module)
			return;
		int d = (e.scrollDelta.y > 0.f) ? 1 : (e.scrollDelta.y < 0.f) ? -1 : 0;
		if (d != 0) {
			beginEdit();
			step(d);
			endEdit();
		}
		e.consume(this);
	}

	// Right-click: every setting on this page, plus a page chooser.
	void openMenu() {
		if (!module)
			return;
		Metroplex* m = module;
		ui::Menu* menu = rack::createMenu();
		menu->addChild(createMenuLabel("Metroplex menus"));
		menu->addChild(createIndexSubmenuItem("Page",
			{"Home", "Clock", "Sequence", "Scale", "Slide", "Mutate", "CTRL 1", "CTRL 2", "CTRL 3", "CTRL 4"},
			[=]() { return m->page; },
			[=](int p) { m->page = p; m->selected = 0; }));
		appendPageMenu(menu, m, m->page);
	}

	static void appendFieldItem(ui::Menu* menu, Metroplex* m, int f) {
		int pid = Metroplex::FIELD_PARAM + f;
		ParamQuantity* q = m->paramQuantities[pid];
		if (fieldIsNamed(f)) {
			std::vector<std::string> names;
			for (int v = FIELDS[f].min; v <= FIELDS[f].max; v++)
				names.push_back(fieldText(f, v));
			int mn = FIELDS[f].min;
			menu->addChild(createIndexSubmenuItem(FIELDS[f].name, names,
				[=]() { return (int)std::round(q->getValue()) - mn; },
				[=](int i) { q->setValue(i + mn); }));
		}
		else {
			auto* s = new ui::Slider;
			s->quantity = q;
			s->box.size.x = 220.f;
			menu->addChild(s);
		}
	}

	static void appendPageMenu(ui::Menu* menu, Metroplex* m, int page) {
		if (isCtrlPage(page)) {
			int k = page - PAGE_CTRL1;
			appendDestMenu(menu, m, k);
			return;
		}
		for (int r = 0; r < PAGES[page].count; r++)
			appendFieldItem(menu, m, PAGES[page].fields[r]);
	}

	static void appendDestMenu(ui::Menu* menu, Metroplex* m, int k) {
		std::vector<std::string> names;
		for (int d = 0; d < NUM_DESTS; d++)
			names.push_back(DESTS[d].name);
		ParamQuantity* q = m->paramQuantities[Metroplex::destParamId(k)];
		menu->addChild(createIndexSubmenuItem(string::f("CTRL %d destination", k + 1), names,
			[=]() { return (int)std::round(q->getValue()); },
			[=](int d) { q->setValue(d); }));
	}
};

struct Display : EditWidget {
	std::string fontPath;

	Display() {
		fontPath = asset::system("res/fonts/ShareTechMono-Regular.ttf");
	}

	void click(const DragEndEvent& e) override {
		// a click picks the row under the mouse
		if (!module)
			return;
		int rows = module->pageRows();
		if (module->page == PAGE_HOME || module->page >= PAGE_CTRL1 || rows == 0)
			return;
		float rowH = rowHeight(rows);
		int r = (int)std::floor((pressPos.y - ROWS_TOP) / rowH);
		if (r >= 0 && r < rows)
			module->selected = r;
	}

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3.f);
		nvgFillColor(args.vg, SCREEN);
		nvgFill(args.vg);
		OpaqueWidget::draw(args);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1)
			drawScreen(args);
		OpaqueWidget::drawLayer(args, layer);
	}

	void text(NVGcontext* vg, float x, float y, const std::string& s, NVGcolor c, float size, int align = NVG_ALIGN_LEFT) {
		nvgFontSize(vg, size);
		nvgFillColor(vg, c);
		nvgTextAlign(vg, align | NVG_ALIGN_BASELINE);
		nvgText(vg, x, y, s.c_str(), NULL);
	}

	void drawScreen(const DrawArgs& args) {
		std::shared_ptr<window::Font> font = APP->window->loadFont(fontPath);
		if (!font || font->handle < 0)
			return;
		NVGcontext* vg = args.vg;
		nvgFontFaceId(vg, font->handle);
		nvgTextLetterSpacing(vg, 0.f);
		float W = box.size.x, H = box.size.y;
		nvgScissor(vg, 0, 0, W, H);

		if (!module) {
			text(vg, W / 2, H / 2 + 4, "METROPLEX", AMBER, 14.f, NVG_ALIGN_CENTER);
			nvgResetScissor(vg);
			return;
		}
		Metroplex* m = module;
		const Effective& ef = m->eff;
		const float pad = 5.f;

		// header: page title on the left, clock on the right
		std::string title = PAGES[m->page].title;
		if (m->page == PAGE_HOME) {
			// HOME shows the snapshot and how much Mutate is in
			title = string::f("SNAP %d", m->curSnap + 1);
			if (m->pendingSnap >= 0)
				title += string::f(">%d", m->pendingSnap + 1);
			int mp = (int)std::lround(m->mut.amount * 100.f);
			if (mp > 0)
				title += string::f("  MUT %d%%", mp);
		}
		else if (m->page == PAGE_MUTATE) {
			title = string::f("MUTATE %d%%", (int)std::lround(m->mut.amount * 100.f));
		}
		text(vg, pad, 11.f, title, AMBER, 10.f);
		std::string clk;
		if (m->externalClock)
			clk = m->measuredBpm > 0.f ? string::f("EXT %.0f", m->measuredBpm) : "EXT";
		else
			clk = string::f("%d BPM", ef.value[F_TEMPO]);
		if (ef.value[F_DIV] > 1)
			clk += string::f(" /%d", ef.value[F_DIV]);
		bool running = m->params[Metroplex::RUN_PARAM].getValue() > 0.5f;
		text(vg, W - pad, 11.f, running ? clk : "STOPPED", running ? CYAN : AMBER_DIM, 9.f, NVG_ALIGN_RIGHT);
		nvgBeginPath(vg);
		nvgMoveTo(vg, pad, 14.f);
		nvgLineTo(vg, W - pad, 14.f);
		nvgStrokeColor(vg, AMBER_DIM);
		nvgStrokeWidth(vg, 0.8f);
		nvgStroke(vg);

		if (m->page == PAGE_HOME)
			drawHome(vg, m, W, H, pad);
		else if (isCtrlPage(m->page))
			drawCtrl(vg, m, m->page - PAGE_CTRL1, W, H, pad);
		else
			drawFields(vg, m, W, H, pad);
		nvgResetScissor(vg);
	}

	void drawHome(NVGcontext* vg, Metroplex* m, float W, float H, float pad) {
		const Effective& ef = m->eff;
		float top = 18.f, bottom = H - 14.f;
		float colW = (W - 2 * pad) / NUM_STAGES;
		int len = ef.eng.length;  // for the footer
		for (int i = 0; i < NUM_STAGES; i++) {
			const Stage& st = m->stages[i];
			float x = pad + i * colW;
			float semis = (float)SeqEngine::stageSemis(st, ef.eng);
			float span = 12.f * std::max(1, ef.eng.range);
			float h = clamp((semis + 1.f) / (span + 1.f), 0.05f, 1.f) * (bottom - top);
			bool cur = (i == m->curStage);
			bool inside = SeqEngine::inWindow(ef.eng, i);
			NVGcolor c = cur ? CYAN : AMBER;
			if (!inside || st.skip)
				c = nvgRGB(0x2a, 0x22, 0x18);
			nvgBeginPath(vg);
			nvgRect(vg, x + 1.f, bottom - h, colW - 2.f, h);
			if (st.gate == GATE_REST) {
				nvgStrokeColor(vg, c);
				nvgStrokeWidth(vg, 0.8f);
				nvgStroke(vg);
			}
			else {
				nvgFillColor(vg, c);
				nvgFill(vg);
			}
			// a cyan tick above the bar: Mutate has changed this stage
			const Stage& b = m->baseStages[i];
			bool changed = st.degreeOffset != 0 || st.pulses != b.pulses || st.gate != b.gate || st.slide != b.slide || st.skip != b.skip;
			if (changed) {
				nvgBeginPath(vg);
				nvgRect(vg, x + 1.f, bottom - h - 2.5f, colW - 2.f, 1.2f);
				nvgFillColor(vg, CYAN);
				nvgFill(vg);
			}
		}
		// footer
		std::string left = string::f("%s %s", NOTE_NAMES[ef.eng.root], SCALES[ef.eng.scale].shortName);
		std::string mid = string::f("%d-%d %s", ef.eng.start + 1, (ef.eng.start + len - 1) % NUM_STAGES + 1, ORDER_NAMES[ef.eng.order]);
		std::string right;
		if (m->curStage >= 0) {
			char nb[16];
			noteName(m->pitchOut, nb, sizeof nb);
			right = string::f("S%d %s", m->curStage + 1, nb);
		}
		text(vg, pad, H - 4.f, left, AMBER, 9.f);
		text(vg, W / 2, H - 4.f, mid, AMBER, 9.f, NVG_ALIGN_CENTER);
		text(vg, W - pad, H - 4.f, right, CYAN, 9.f, NVG_ALIGN_RIGHT);
	}

	static constexpr float ROWS_TOP = 17.f;

	float rowHeight(int rows) {
		return std::min(12.f, (box.size.y - ROWS_TOP - 2.f) / std::max(rows, 1));
	}

	void drawFields(NVGcontext* vg, Metroplex* m, float W, float H, float pad) {
		const PageInfo& pg = PAGES[m->page];
		int rows = pg.count;
		float top = ROWS_TOP;
		float rowH = rowHeight(rows);
		float fs = std::min(9.5f, rowH * 0.85f);
		for (int r = 0; r < rows; r++) {
			int f = pg.fields[r];
			float y = top + r * rowH;
			bool sel = (r == m->selected);
			int owner = m->eff.owner[f];
			if (sel) {
				nvgBeginPath(vg);
				nvgRect(vg, pad - 2.f, y, W - 2 * pad + 4.f, rowH);
				nvgFillColor(vg, AMBER);
				nvgFill(vg);
			}
			NVGcolor c = sel ? SCREEN : AMBER;
			text(vg, pad, y + rowH * 0.78f, FIELDS[f].name, c, fs);
			std::string v = fieldText(f, m->eff.value[f]);
			text(vg, W - pad - (owner ? 16.f : 0.f), y + rowH * 0.78f, v, sel ? SCREEN : (owner ? CYAN : AMBER), fs, NVG_ALIGN_RIGHT);
			if (owner) {
				// "C1" / "C2" badge: this row follows a CTRL knob
				float bx = W - pad - 13.f, by = y + 1.5f;
				nvgBeginPath(vg);
				nvgRoundedRect(vg, bx, by, 13.f, rowH - 3.f, 2.f);
				nvgFillColor(vg, CYAN);
				nvgFill(vg);
				text(vg, bx + 6.5f, y + rowH * 0.76f, string::f("C%d", owner), SCREEN, fs * 0.85f, NVG_ALIGN_CENTER);
			}
		}
		if (m->page == PAGE_MUTATE)
			text(vg, pad, H - 3.f, string::f("Seed %08X", (unsigned)m->seed), CYAN, 8.f);
	}

	void drawCtrl(NVGcontext* vg, Metroplex* m, int k, float W, float H, float pad) {
		int d = m->ctrlDest(k);
		const DestInfo& di = DESTS[d];
		// destination row (always selected)
		float y = 17.f, rowH = 12.f;
		nvgBeginPath(vg);
		nvgRect(vg, pad - 2.f, y, W - 2 * pad + 4.f, rowH);
		nvgFillColor(vg, AMBER);
		nvgFill(vg);
		text(vg, pad, y + 9.3f, "Destination", SCREEN, 9.5f);
		text(vg, W - pad, y + 9.3f, di.name, SCREEN, 9.5f, NVG_ALIGN_RIGHT);
		// help text
		nvgFontSize(vg, 7.5f);
		nvgFillColor(vg, AMBER_DIM);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		nvgTextBox(vg, pad, y + rowH + 3.f, W - 2 * pad, di.help, NULL);
		// gauge
		float a = m->ctrlAmountNow(k);
		float gy = H - 12.f, gw = W - 2 * pad - 60.f;
		nvgBeginPath(vg);
		nvgRect(vg, pad, gy, gw, 5.f);
		nvgStrokeColor(vg, AMBER_DIM);
		nvgStrokeWidth(vg, 0.8f);
		nvgStroke(vg);
		nvgBeginPath(vg);
		if (di.kind == DEST_KIND_OFFSET || di.kind == DEST_KIND_MUTATE) {
			float mid = pad + gw / 2;
			float x = pad + gw * a;
			nvgRect(vg, std::min(mid, x), gy, std::fabs(x - mid), 5.f);
		}
		else {
			nvgRect(vg, pad, gy, gw * a, 5.f);
		}
		nvgFillColor(vg, CYAN);
		nvgFill(vg);
		text(vg, W - pad, gy + 5.f, m->ctrlReadout(k), CYAN, 9.f, NVG_ALIGN_RIGHT);
	}
};

// The DATA encoder: drag up/down to change the selected row, click for the next row.
struct Encoder : EditWidget {
	float angle = 0.f;

	void onDragMove(const DragMoveEvent& e) override {
		angle += -e.mouseDelta.y * 0.04f;
		EditWidget::onDragMove(e);
	}

	void onHoverScroll(const HoverScrollEvent& e) override {
		angle += (e.scrollDelta.y > 0.f ? 0.3f : -0.3f);
		EditWidget::onHoverScroll(e);
	}

	void draw(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		float r = box.size.x / 2;
		Vec c(r, r);
		// knurled skirt
		nvgBeginPath(vg);
		nvgCircle(vg, c.x, c.y, r);
		nvgFillColor(vg, nvgRGB(0x14, 0x15, 0x18));
		nvgFill(vg);
		for (int i = 0; i < 24; i++) {
			float t = angle + i * (2 * M_PI / 24);
			nvgBeginPath(vg);
			nvgMoveTo(vg, c.x + std::cos(t) * r * 0.84f, c.y + std::sin(t) * r * 0.84f);
			nvgLineTo(vg, c.x + std::cos(t) * r * 0.98f, c.y + std::sin(t) * r * 0.98f);
			nvgStrokeColor(vg, nvgRGB(0x3a, 0x3d, 0x44));
			nvgStrokeWidth(vg, 1.f);
			nvgStroke(vg);
		}
		// cap
		nvgBeginPath(vg);
		nvgCircle(vg, c.x, c.y, r * 0.72f);
		NVGpaint p = nvgLinearGradient(vg, 0, 0, 0, 2 * r, nvgRGB(0x4a, 0x4e, 0x56), nvgRGB(0x22, 0x24, 0x28));
		nvgFillPaint(vg, p);
		nvgFill(vg);
		// dimple
		nvgBeginPath(vg);
		nvgCircle(vg, c.x + std::cos(angle - M_PI / 2) * r * 0.48f, c.y + std::sin(angle - M_PI / 2) * r * 0.48f, r * 0.08f);
		nvgFillColor(vg, AMBER);
		nvgFill(vg);
		OpaqueWidget::draw(args);
	}
};

// CTRL knobs: right-click to pick a destination.
struct CtrlKnob : RoundLargeBlackKnob {
	int k = 0;
	void appendContextMenu(ui::Menu* menu) override {
		Metroplex* m = dynamic_cast<Metroplex*>(module);
		if (!m)
			return;
		menu->addChild(new ui::MenuSeparator);
		EditWidget::appendDestMenu(menu, m, k);
		menu->addChild(createMenuItem("Centre knob (noon)", "", [=]() {
			m->paramQuantities[Metroplex::knobId(k)]->setValue(0.5f);
		}));
	}
};

// The gate button cycles Rest > Single > Multi > Hold and lights up in colour.
struct GateButton : VCVLightButton<MediumSimpleLight<RedGreenBlueLight>> {
	GateButton() {
		momentary = false;
		latch = true;  // looks the same whatever the value; the light shows the type
	}
};

using Toggle = VCVLightLatch<MediumSimpleLight<WhiteLight>>;

// A wider pitch slider, easier to grab. Sizes match res/make_panel.py.
struct WideSlider : app::SvgSlider {
	WideSlider() {
		setBackgroundSvg(Svg::load(asset::plugin(pluginInstance, "res/Slider.svg")));
		setHandleSvg(Svg::load(asset::plugin(pluginInstance, "res/SliderHandle.svg")));
		Vec bg = mm2px(Vec(9.6f, 26.0f));
		float hh = mm2px(4.6f);
		setHandlePosCentered(Vec(bg.x / 2, bg.y - hh / 2), Vec(bg.x / 2, hh / 2));
	}
};

// The playing-stage light: a bar across the handle.
struct HandleLight : RectangleLight<YellowLight> {
	HandleLight() {
		box.size = mm2px(Vec(7.2f, 0.9f));
	}
};

using PitchSlider = LightSlider<WideSlider, HandleLight>;

struct MetroplexWidget : ModuleWidget {
	MetroplexWidget(Metroplex* module) {
		using namespace layout;
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Metroplex.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		Display* disp = createWidget<Display>(mm2px(Vec(DISP_X, DISP_Y)));
		disp->box.size = mm2px(Vec(DISP_W, DISP_H));
		disp->module = module;
		addChild(disp);

		Encoder* enc = createWidget<Encoder>(mm2px(Vec(ENC_X - ENC_SIZE / 2, ENC_Y - ENC_SIZE / 2)));
		enc->box.size = mm2px(Vec(ENC_SIZE, ENC_SIZE));
		enc->module = module;
		addChild(enc);

		for (int b = 0; b < NUM_PAGE_BUTTONS; b++) {
			Vec p = mm2px(Vec(PAGE_X[b % 3], PAGE_Y[b / 3]));
			addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<WhiteLight>>>(p, module,
				Metroplex::PAGE_PARAM + b, Metroplex::PAGE_LIGHT + b));
		}
		for (int k = 0; k < NUM_CTRLS; k++) {
			CtrlKnob* knob = createParamCentered<CtrlKnob>(mm2px(Vec(CTRL_X[k], CTRL_Y)), module, Metroplex::knobId(k));
			knob->k = k;
			addParam(knob);
			addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(CTRL_X[k] + CTRL_BTN_DX, CTRL_BTN_Y)),
				module, Metroplex::pageButtonId(k), Metroplex::CTRL_PAGE_LIGHT + k));
			addParam(createParamCentered<Trimpot>(mm2px(Vec(CTRL_X[k] + CTRL_ATT_DX, CTRL_BTN_Y)), module, Metroplex::attId(k)));
		}
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(ALL_PULSES_X, ALL_PULSES_Y)), module, Metroplex::ALL_PULSES_PARAM));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(mm2px(Vec(RUN_X, RUN_Y)), module,
			Metroplex::RUN_PARAM, Metroplex::RUN_LIGHT));
		addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(RUN_X, CLK_LIGHT_Y)), module, Metroplex::CLOCK_LIGHT));

		for (int i = 0; i < NUM_STAGES; i++) {
			float x = STAGE_X0 + STAGE_DX * i;
			addParam(createLightParamCentered<PitchSlider>(mm2px(Vec(x, SLIDER_Y)), module,
				Metroplex::PITCH_PARAM + i, Metroplex::STAGE_LIGHT + i));
			addParam(createParamCentered<Trimpot>(mm2px(Vec(x, PULSE_Y)), module, Metroplex::PULSES_PARAM + i));
			addParam(createLightParamCentered<GateButton>(mm2px(Vec(x, GATE_Y)), module,
				Metroplex::GATE_PARAM + i, Metroplex::GATE_LIGHT + 3 * i));
			addParam(createLightParamCentered<Toggle>(mm2px(Vec(x, SLIDE_Y)), module,
				Metroplex::SLIDE_PARAM + i, Metroplex::SLIDE_LIGHT + i));
			addParam(createLightParamCentered<Toggle>(mm2px(Vec(x, MUT_Y)), module,
				Metroplex::MUTABLE_PARAM + i, Metroplex::MUTABLE_LIGHT + i));
			addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(mm2px(Vec(x, SKIP_Y)), module,
				Metroplex::SKIP_PARAM + i, Metroplex::SKIP_LIGHT + i));
		}

		int ins[10] = {Metroplex::CLOCK_INPUT, Metroplex::RESET_INPUT, Metroplex::RUN_INPUT, Metroplex::TRANSPOSE_INPUT,
			Metroplex::SEED_INPUT, Metroplex::SNAP_INPUT, Metroplex::CTRL_CV_INPUT + 0, Metroplex::CTRL_CV_INPUT + 1,
			Metroplex::CTRL34_CV_INPUT + 0, Metroplex::CTRL34_CV_INPUT + 1};
		for (int j = 0; j < 10; j++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IN_X0 + IN_DX * j, JACK_Y)), module, ins[j]));
		// Mutate and snapshots
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(MUTATE_X, JACK_Y)), module, Metroplex::MUTATE_PARAM));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<YellowLight>>>(mm2px(Vec(SEED_X, JACK_Y)), module,
			Metroplex::NEW_SEED_PARAM, Metroplex::SEED_LIGHT));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<RedLight>>>(mm2px(Vec(SAVE_X, JACK_Y)), module,
			Metroplex::SAVE_PARAM, Metroplex::SAVE_LIGHT));
		for (int j = 0; j < NUM_SNAPSHOTS; j++)
			addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(SNAP_X0 + SNAP_DX * j, JACK_Y)),
				module, Metroplex::SNAP_PARAM + j, Metroplex::SNAP_LIGHT + j));
		int outs[4] = {Metroplex::CLOCK_OUTPUT, Metroplex::EOC_OUTPUT, Metroplex::GATE_OUTPUT, Metroplex::PITCH_OUTPUT};
		for (int j = 0; j < 4; j++)
			addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(OUT_X[j], JACK_Y)), module, outs[j]));
	}

	void appendContextMenu(Menu* menu) override {
		Metroplex* m = dynamic_cast<Metroplex*>(module);
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Settings"));
		const char* names[] = {"Clock", "Sequence", "Scale", "Slide", "Mutate"};
		for (int p = PAGE_CLOCK; p <= PAGE_MUTATE; p++) {
			int page = p;
			menu->addChild(createSubmenuItem(names[p - PAGE_CLOCK], "", [=](Menu* sub) {
				EditWidget::appendPageMenu(sub, m, page);
			}));
		}
		for (int k = 0; k < NUM_CTRLS; k++)
			EditWidget::appendDestMenu(menu, m, k);
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("New Mutate seed", "", [=]() { m->newSeed(); }));
		menu->addChild(createMenuItem("Clear all snapshots", "", [=]() {
			for (int j = 0; j < NUM_SNAPSHOTS; j++)
				m->snapshots[j] = Snapshot();
		}));
		menu->addChild(createMenuItem("Randomise pitches", "", [=]() {
			for (int i = 0; i < NUM_STAGES; i++)
				m->paramQuantities[Metroplex::PITCH_PARAM + i]->setValue(random::uniform());
		}));
	}
};

Model* modelMetroplex = createModel<Metroplex, MetroplexWidget>("Metroplex");
