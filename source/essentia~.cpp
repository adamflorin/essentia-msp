/**
* essentia~.cpp: wrapper around the essentia library for
* Music Information Retrieval
* http://essentia.upf.edu/
*
* Copyright 2018 Adam Florin
*/

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

extern "C" {
	// External struct
	typedef struct _essentia {
		t_pxobject object;
	} t_essentia;

	// Method prototypes
	void *essentia_new(t_symbol *s, long argc, t_atom *argv);
	void essentia_free(t_essentia *x);
	void essentia_assist(t_essentia *x, void *b, long m, long a, char *s);
	void essentia_dsp64(
		t_essentia *x,
		t_object *dsp64,
		short *count,
		double samplerate,
		long maxvectorsize,
		long flags
	);
	void essentia_perform64(
		t_essentia *x,
		t_object *dsp64,
		double **ins,
		long numins,
		double **outs,
		long numouts,
		long sampleframes,
		long flags,
		void *userparam
	);

	// External class
	static t_class *essentia_class = NULL;

	/** Initialize external class */
	void ext_main(void *r) {
		t_class *c = class_new(
			"essentia~",
			(method)essentia_new,
			(method)essentia_free,
			(long)sizeof(t_essentia),
			0L,
			A_GIMME,
			0
		);

		class_addmethod(c, (method)essentia_dsp64, "dsp64", A_CANT, 0);
		class_addmethod(c, (method)essentia_assist, "assist", A_CANT, 0);

		class_dspinit(c);
		class_register(CLASS_BOX, c);
		essentia_class = c;
	}

	/** Initialize external instance */
	void *essentia_new(t_symbol *s, long argc, t_atom *argv) {
		t_essentia *x = (t_essentia *)object_alloc(essentia_class);

		if (!x) {
			return x;
		}

		// I/O
		dsp_setup((t_pxobject *)x, 1);
		outlet_new(x, "signal");

		return x;
	}

	/** Destroy external instance */
	void essentia_free(t_essentia *x) {
		dsp_free((t_pxobject *)x);
	}

	/** Configure user tooltip prompts */
	void essentia_assist(t_essentia *x, void *b, long m, long a, char *s) {
		if (m == ASSIST_INLET) {
			sprintf(s, "I am inlet %ld", a);
		} else {
			sprintf(s, "I am outlet %ld", a);
		}
	}

	/** Register perform method */
	void essentia_dsp64(
		t_essentia *x,
		t_object *dsp64,
		short *count,
		double samplerate,
		long maxvectorsize,
		long flags
	) {
		post("my sample rate is: %f", samplerate);

		object_method(dsp64, gensym("dsp_add64"), x, essentia_perform64, 0, NULL);
	}

	/** Perform DSP */
	void essentia_perform64(
		t_essentia *x,
		t_object *dsp64,
		double **ins,
		long numins,
		double **outs,
		long numouts,
		long sampleframes,
		long flags,
		void *userparam
	) {
		t_double *inL = ins[0];
		t_double *outL = outs[0];
		int n = sampleframes;

		while (n--) {
			*outL++ = *inL++;
		}
	}
}
