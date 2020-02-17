/* ------------------------------------------------------------
author: "Jean Pierre Cimalando"
license: "BSD-2-Clause"
name: "sfz_filters"
Code generated with Faust 2.15.11 (https://faust.grame.fr)
Compilation options: -inpl -double -ftz 0
------------------------------------------------------------ */

#ifndef  __faustHpf1p_H__
#define  __faustHpf1p_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <math.h>


#ifndef FAUSTCLASS 
#define FAUSTCLASS faustHpf1p
#endif
#ifdef __APPLE__ 
#define exp10f __exp10f
#define exp10 __exp10
#endif

class faustHpf1p : public sfzFilterDsp {
	
 public:
	
	int fSamplingFreq;
	double fConst0;
	double fConst1;
	double fConst2;
	double fConst3;
	FAUSTFLOAT fCutoff;
	double fRec1[2];
	double fRec0[2];
	
 public:
	
	void metadata(Meta* m) { 
	}

	virtual int getNumInputs() {
		return 1;
		
	}
	virtual int getNumOutputs() {
		return 1;
		
	}
	virtual int getInputRate(int channel) {
		int rate;
		switch (channel) {
			case 0: {
				rate = 1;
				break;
			}
			default: {
				rate = -1;
				break;
			}
			
		}
		return rate;
		
	}
	virtual int getOutputRate(int channel) {
		int rate;
		switch (channel) {
			case 0: {
				rate = 1;
				break;
			}
			default: {
				rate = -1;
				break;
			}
			
		}
		return rate;
		
	}
	
	static void classInit(int samplingFreq) {
		
	}
	
	virtual void instanceConstants(int samplingFreq) {
		fSamplingFreq = samplingFreq;
		fConst0 = std::min<double>(192000.0, std::max<double>(1.0, double(fSamplingFreq)));
		fConst1 = std::exp((0.0 - (1000.0 / fConst0)));
		fConst2 = (1.0 - fConst1);
		fConst3 = (1.0 / fConst0);
		
	}
	
	virtual void instanceResetUserInterface() {
		fCutoff = FAUSTFLOAT(440.0);
		
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; (l0 < 2); l0 = (l0 + 1)) {
			fRec1[l0] = 0.0;
			
		}
		for (int l1 = 0; (l1 < 2); l1 = (l1 + 1)) {
			fRec0[l1] = 0.0;
			
		}
		
	}
	
	virtual void init(int samplingFreq) {
		classInit(samplingFreq);
		instanceInit(samplingFreq);
	}
	
	virtual void instanceInit(int samplingFreq) {
		instanceConstants(samplingFreq);
		instanceResetUserInterface();
		instanceClear();
	}
	
	virtual faustHpf1p* clone() {
		return new faustHpf1p();
	}
	
	virtual int getSampleRate() {
		return fSamplingFreq;
		
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		
	}
	
	virtual void compute(int count, FAUSTFLOAT** inputs, FAUSTFLOAT** outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		double fSlow0 = (fConst2 * std::exp((fConst3 * (0.0 - (6.2831853071795862 * double(fCutoff))))));
		for (int i = 0; (i < count); i = (i + 1)) {
			double fTemp0 = double(input0[i]);
			fRec1[0] = (fSlow0 + (fConst1 * fRec1[1]));
			fRec0[0] = ((fRec1[0] * fRec0[1]) + fTemp0);
			double fTemp1 = (fRec1[0] + 1.0);
			output0[i] = FAUSTFLOAT(((0.5 * (fRec0[0] * fTemp1)) + (fRec0[1] * (0.0 - (0.5 * fTemp1)))));
			fRec1[1] = fRec1[0];
			fRec0[1] = fRec0[0];
			
		}
		
	}

};

#endif
