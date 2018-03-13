#include "AH.hpp"
#include "Core.hpp"
#include "UI.hpp"
#include "dsp/digital.hpp"

#include <iostream>

struct LFOGenerator {
	float phase = 0.0f;
	float pw = 0.5f;
	float freq = 1.0f;	

	void setFreq(float freq_to_set) {
		freq = freq_to_set;
	}
	
	void step(float dt) {
		float deltaPhase = fminf(freq * dt, 0.5f);
		phase += deltaPhase;
		if (phase >= 1.0f) {
			phase -= 1.0f;
		}
	}	

	float sqr() {
		float sqr = phase < pw ? 1.0f : -1.0f;
		return sqr;
	}
};


struct MetaSequencer : AHModule {

	enum ParamIds {
		RUN_PARAM,
		RESET_PARAM,
		TEMPO_PARAM,
		TIMESIGTOP_PARAM,
		TIMESIGBOTTOM_PARAM,
		RESET_SWITCH,
		RUN_SWITCH,    
		NUM_PARAMS
	};
	enum InputIds {
	    RESET_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(SEQOUT_OUT, 8),
		BEAT_OUT,
		EIGHTHS_OUT,
		SIXTEENTHS_OUT,
		BAR_OUT,
		RESET_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		RESET_LED,
		RUN_LED,
		NUM_LIGHTS
	};

	float sampleRate;
		
	MetaSequencer() : AHModule(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		sampleRate = engineGetSampleRate();
	}
	
	void onSampleRateChange() override { 
		sampleRate = engineGetSampleRate();
	}

	void step() override;
	
	SchmittTrigger eighths_trig;
	SchmittTrigger quarters_trig;
	SchmittTrigger bars_trig;
	SchmittTrigger run_button_trig;
	SchmittTrigger reset_btn_trig;
	
	LFOGenerator clock;
	
	const float lightLambda = 0.075f;
	float resetLight = 0.0f;
	
	bool running = true;

	int eighths_count = 0;
	int quarters_count = 0;
	int bars_count = 0;
	
	int tempo, time_sig_top, time_sig_bottom = 0;
	float frequency = 2.0f;
	int quarters_count_limit = 4;
	int eighths_count_limit = 2;
	int bars_count_limit = 16;

	int seqBeatCount = 0;
	int seqBarCount = 0;

};

void MetaSequencer::step() {
	
	AHModule::step();
	
	if (run_button_trig.process(params[RUN_SWITCH].value)){
		running = !running;
	}
	
	lights[RUN_LED].value = running ? 1.0f : 0.0f;

	tempo = std::round(params[TEMPO_PARAM].value);
	time_sig_top = std::round(params[TIMESIGTOP_PARAM].value);
	time_sig_bottom = std::round(params[TIMESIGBOTTOM_PARAM].value);
	time_sig_bottom = std::pow(2, time_sig_bottom + 1);

	frequency = tempo / 60.0f;
	
	if (reset_btn_trig.process(params[RESET_SWITCH].value)) {
		eighths_count = 0;
		quarters_count = 0;
		bars_count = 0;
		resetLight = 1.0;
		seqBeatCount = 0;
		seqBarCount = 0;
		outputs[RESET_OUTPUT].value = 10.0f;
	} else {
		outputs[RESET_OUTPUT].value = 0.0f;
	}

	resetLight -= resetLight / lightLambda / engineGetSampleRate();
	lights[RESET_LED].value = resetLight;

	if (!running) {
		eighths_count = 0;	
		quarters_count = 0;
		bars_count = 0; 
		outputs[BAR_OUT].value = 0.0;
		outputs[BEAT_OUT].value = 0.0;
		outputs[EIGHTHS_OUT].value = 0.0;
		outputs[SIXTEENTHS_OUT].value = 0.0;
	} else {
		if (time_sig_top == time_sig_bottom) {
			clock.setFreq(frequency * 4);
			quarters_count_limit = 4;
			eighths_count_limit = 2;
			bars_count_limit = 16;
		} else {
			if (time_sig_bottom == 4) {
				  quarters_count_limit = 4;
				  eighths_count_limit = 2;
				  bars_count_limit = time_sig_top * 4;
				  clock.setFreq(frequency * 4);
			}
			if (time_sig_bottom == 8) {
				quarters_count_limit = 4;
				eighths_count_limit = 2;
				bars_count_limit = time_sig_top * 2;
				clock.setFreq(frequency * 4);
				if ((time_sig_top % 3) == 0) {
					quarters_count_limit = 6;
					eighths_count_limit = 2;
					bars_count_limit = (time_sig_top/3) * 6;
					clock.setFreq(frequency * 6);
				}
			}	
		}
	}

	clock.step(delta);
	outputs[SIXTEENTHS_OUT].value = clamp(10.0f * clock.sqr(), 0.0f, 10.0f);

	if (eighths_trig.process(clock.sqr()) && eighths_count <= eighths_count_limit) {
		eighths_count++;
	}
	
	if (eighths_count >= eighths_count_limit) {
		eighths_count = 0;
	}

	if (eighths_count == 0) {
		outputs[EIGHTHS_OUT].value = 10.0f;
	} else {
		outputs[EIGHTHS_OUT].value = 0.0f;
	}
	
	if (quarters_trig.process(clock.sqr()) && quarters_count <= quarters_count_limit) {
		quarters_count++;
		seqBeatCount++;
	}

	if (quarters_count >= quarters_count_limit) {
		quarters_count = 0;    
	}
	
	if (quarters_count == 0) {
		outputs[BEAT_OUT].value = 10.0f;
	} else {
		outputs[BEAT_OUT].value = 0.0f;
	}
	
	bool doActions = false;
	if (bars_trig.process(clock.sqr()) && bars_count <= bars_count_limit) {
		bars_count++;
		seqBarCount++;
		doActions = true;
	}

	if (bars_count >= bars_count_limit) {
		bars_count = 0;
	}
	
	if (bars_count == 0) {
		outputs[BAR_OUT].value = 10.0f;
	} else {
		outputs[BAR_OUT].value = 0.0f; 
	}
	
	
	if (doActions) {
	// For each rule
	//  Is rule active
	//   Are the rule conditions met?
	//    If so, add the rule action to the action list, setting the action status as 'to be activated'
	
	// For each action in the current action list
	//  Has action expired?
	//   If so, remove action from the list
	//  Is the action 'active'
	//   Do action
	//  Is the action 'to be activated'
	//   Activate action and set as 'active'
	//   Do action
	}
	
}

struct MetaSequencerWidget : ModuleWidget {
	MetaSequencerWidget(MetaSequencer *module);
};

MetaSequencerWidget::MetaSequencerWidget(MetaSequencer *module) : ModuleWidget(module) {
	
	UI ui;
		
	box.size = Vec(15*40, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/MetaSequencer.svg")));
		addChild(panel);
	}

	addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(Widget::create<ScrewSilver>(Vec(15, 365)));
	addChild(Widget::create<ScrewSilver>(Vec(box.size.x-30, 365)));	
	
	addParam(ParamWidget::create<AHKnobNoSnap>(ui.getPosition(UI::KNOB, 0, 0, true, false), module, MetaSequencer::TEMPO_PARAM, 40.0f, 250.0f, 120.0f));
	addParam(ParamWidget::create<AHKnobSnap>(ui.getPosition(UI::KNOB, 1, 0, true, false), module, MetaSequencer::TIMESIGTOP_PARAM,2.0f, 15.0f, 4.0f));
	addParam(ParamWidget::create<AHKnobSnap>(ui.getPosition(UI::KNOB, 2, 0, true, false), module, MetaSequencer::TIMESIGBOTTOM_PARAM,0.0f, 3.0f, 1.0f));
	addParam(ParamWidget::create<AHButton>(ui.getPosition(UI::BUTTON, 3, 0, true, false), module, MetaSequencer::RUN_SWITCH , 0.0f, 1.0f, 0.0f));
	addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(ui.getPosition(UI::LIGHT, 3, 0, true, false), module, MetaSequencer::RUN_LED));
	addParam(ParamWidget::create<AHButton>(ui.getPosition(UI::BUTTON, 4, 0, true, false), module, MetaSequencer::RESET_SWITCH , 0.0f, 1.0f, 0.0f));
	addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(ui.getPosition(UI::LIGHT, 4, 0, true, false), module, MetaSequencer::RESET_LED));
	
    addOutput(Port::create<PJ301MPort>(ui.getPosition(UI::PORT, 0, 1, true, false), Port::OUTPUT, module, MetaSequencer::BAR_OUT));
    addOutput(Port::create<PJ301MPort>(ui.getPosition(UI::PORT, 1, 1, true, false), Port::OUTPUT, module, MetaSequencer::BEAT_OUT));
    addOutput(Port::create<PJ301MPort>(ui.getPosition(UI::PORT, 2, 1, true, false), Port::OUTPUT, module, MetaSequencer::EIGHTHS_OUT));
    addOutput(Port::create<PJ301MPort>(ui.getPosition(UI::PORT, 3, 1, true, false), Port::OUTPUT, module, MetaSequencer::SIXTEENTHS_OUT));

	for (int i = 0; i < 8; i++) {
		addOutput(Port::create<PJ301MPort>(ui.getPosition(UI::PORT, i, 2, true, false), Port::OUTPUT, module, MetaSequencer::SEQOUT_OUT));
	}
	
	
	
	
}

Model *modelMetaSequencer = Model::create<MetaSequencer, MetaSequencerWidget>( "Amalgamated Harmonics", "MetaSequencer", "MetaSequencer", SEQUENCER_TAG);


