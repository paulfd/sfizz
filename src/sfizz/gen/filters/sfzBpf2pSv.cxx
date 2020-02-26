/* ------------------------------------------------------------
author: "Jean Pierre Cimalando"
license: "BSD-2-Clause"
name: "sfz_filters"
Code generated with Faust 2.20.2 (https://faust.grame.fr)
Compilation options: -lang cpp -inpl -double -ftz 0
------------------------------------------------------------ */

#ifndef  __faustBpf2pSv_H__
#define  __faustBpf2pSv_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif

#include <algorithm>
#include <cmath>
#include <math.h>


#ifndef FAUSTCLASS
#define FAUSTCLASS faustBpf2pSv
#endif

#ifdef __APPLE__
#define exp10f __exp10f
#define exp10 __exp10
#endif

class faustBpf2pSv : public sfzFilterDsp {

 public:

	int fSampleRate;
	double fConst0;
	double fConst1;
	double fConst2;
	FAUSTFLOAT fCutoff;
	double fRec3[2];
	FAUSTFLOAT fQ;
	double fRec4[2];
	double fRec5[2];
	double fRec1[2];
	double fRec2[2];
	double fRec6[2];

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
		switch ((channel)) {
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
		switch ((channel)) {
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

	static void classInit(int sample_rate) {
	}

	virtual void instanceConstants(int sample_rate) {
		fSampleRate = sample_rate;
		fConst0 = std::min<double>(192000.0, std::max<double>(1.0, double(fSampleRate)));
		fConst1 = std::exp((0.0 - (1000.0 / fConst0)));
		fConst2 = (3.1415926535897931 / fConst0);
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
		for (int l5 = 0; (l5 < 2); l5 = (l5 + 1)) {
			fRec6[l5] = 0.0;
		}
	}

	virtual void init(int sample_rate) {
		classInit(sample_rate);
		instanceInit(sample_rate);
	}
	virtual void instanceInit(int sample_rate) {
		instanceConstants(sample_rate);
		instanceResetUserInterface();
		instanceClear();
	}

	virtual faustBpf2pSv* clone() {
		return new faustBpf2pSv();
	}

	virtual int getSampleRate() {
		return fSampleRate;
	}

	virtual void buildUserInterface(UI* ui_interface) {
	}

	virtual void compute(int count, FAUSTFLOAT** inputs, FAUSTFLOAT** outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* output0 = outputs[0];
		double fSlow0 = (fSmoothEnable ? fConst1 : 0.0);
		double fSlow1 = (1.0 - fSlow0);
		double fSlow2 = (std::tan((fConst2 * double(fCutoff))) * fSlow1);
		double fSlow3 = (1.0 / std::pow(10.0, (0.050000000000000003 * double(fQ))));
		double fSlow4 = (fSlow3 * fSlow1);
		for (int i = 0; (i < count); i = (i + 1)) {
			double fTemp0 = double(input0[i]);
			fRec3[0] = ((fSlow0 * fRec3[1]) + fSlow2);
			double fTemp1 = (fSlow3 + fRec3[0]);
			fRec4[0] = ((fSlow0 * fRec4[1]) + (fSlow1 / ((fRec3[0] * fTemp1) + 1.0)));
			fRec5[0] = ((fSlow0 * fRec5[1]) + (fSlow1 * fTemp1));
			double fTemp2 = (fTemp0 - (fRec1[1] + (fRec5[0] * fRec2[1])));
			double fTemp3 = ((fRec3[0] * fRec4[0]) * fTemp2);
			double fTemp4 = (fRec2[1] + fTemp3);
			double fRec0 = fTemp4;
			fRec1[0] = (fRec1[1] + (2.0 * (fRec3[0] * fTemp4)));
			double fTemp5 = (fRec2[1] + (2.0 * fTemp3));
			fRec2[0] = fTemp5;
			fRec6[0] = ((fSlow0 * fRec6[1]) + fSlow4);
			output0[i] = FAUSTFLOAT((fRec0 * fRec6[0]));
			fRec3[1] = fRec3[0];
			fRec4[1] = fRec4[0];
			fRec5[1] = fRec5[0];
			fRec1[1] = fRec1[0];
			fRec2[1] = fRec2[0];
			fRec6[1] = fRec6[0];
		}
	}

};

#endif
