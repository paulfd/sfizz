/* ------------------------------------------------------------
author: "Jean Pierre Cimalando"
license: "BSD-2-Clause"
name: "sfz_filters"
Code generated with Faust 2.15.11 (https://faust.grame.fr)
Compilation options: -inpl -double -ftz 0
------------------------------------------------------------ */

#ifndef  __faustLpf2pSv_H__
#define  __faustLpf2pSv_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <math.h>


#ifndef FAUSTCLASS 
#define FAUSTCLASS faustLpf2pSv
#endif
#ifdef __APPLE__ 
#define exp10f __exp10f
#define exp10 __exp10
#endif

class faustLpf2pSv : public sfzFilterDsp {
	
 public:
	
	int fSamplingFreq;
	double fConst0;
	double fConst1;
	double fConst2;
	double fConst3;
	FAUSTFLOAT fCutoff;
	double fRec3[2];
	FAUSTFLOAT fQ;
	double fRec4[2];
	double fRec5[2];
	double fRec1[2];
	double fRec2[2];
	
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
		fConst3 = (3.1415926535897931 / fConst0);
		
	}
	
	virtual void instanceResetUserInterface() {
		fCutoff = FAUSTFLOAT(440.0);
		fQ = FAUSTFLOAT(0.0);
		
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; (l0 < 2); l0 = (l0 + 1)) {
			fRec3[l0] = 0.0;
			
		}
		for (int l1 = 0; (l1 < 2); l1 = (l1 + 1)) {
			fRec4[l1] = 0.0;
			
		}
		for (int l2 = 0; (l2 < 2); l2 = (l2 + 1)) {
			fRec5[l2] = 0.0;
			
		}
		for (int l3 = 0; (l3 < 2); l3 = (l3 + 1)) {
			fRec1[l3] = 0.0;
			
		}
		for (int l4 = 0; (l4 < 2); l4 = (l4 + 1)) {
			fRec2[l4] = 0.0;
			
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
	
	virtual faustLpf2pSv* clone() {
		return new faustLpf2pSv();
	}
	
	virtual int getSampleRate() {
		return fSamplingFreq;
		
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		
	}
	
	virtual void compute(int count, FAUSTFLOAT** inputs, FAUSTFLOAT** outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		double fSlow0 = (fConst2 * std::tan((fConst3 * double(fCutoff))));
		double fSlow1 = (1.0 / std::pow(10.0, (0.050000000000000003 * double(fQ))));
		for (int i = 0; (i < count); i = (i + 1)) {
			double fTemp0 = double(input0[i]);
			fRec3[0] = (fSlow0 + (fConst1 * fRec3[1]));
			double fTemp1 = (fSlow1 + fRec3[0]);
			fRec4[0] = ((fConst1 * fRec4[1]) + (fConst2 / ((fRec3[0] * fTemp1) + 1.0)));
			fRec5[0] = ((fConst1 * fRec5[1]) + (fConst2 * fTemp1));
			double fTemp2 = (fTemp0 - (fRec1[1] + (fRec5[0] * fRec2[1])));
			double fTemp3 = ((fRec3[0] * fRec4[0]) * fTemp2);
			double fTemp4 = (fRec2[1] + (2.0 * fTemp3));
			double fRec0 = (fRec1[1] + (fRec3[0] * fTemp4));
			double fTemp5 = (fRec2[1] + fTemp3);
			fRec1[0] = (fRec1[1] + (2.0 * (fRec3[0] * fTemp5)));
			fRec2[0] = fTemp4;
			output0[i] = FAUSTFLOAT(fRec0);
			fRec3[1] = fRec3[0];
			fRec4[1] = fRec4[0];
			fRec5[1] = fRec5[0];
			fRec1[1] = fRec1[0];
			fRec2[1] = fRec2[0];
			
		}
		
	}

};

#endif
