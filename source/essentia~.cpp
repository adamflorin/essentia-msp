/**
* essentia~.cpp: wrapper around the Essentia Music Information Retrieval library
* http://essentia.upf.edu/
*
* Copyright 2018 Adam Florin
*/

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

#include "essentia/algorithmfactory.h"
#include "essentia/streaming/algorithms/vectorinput.h"
#include "essentia/pool.h"
#include "essentia/streaming/algorithms/poolstorage.h"
#include "essentia/scheduler/network.h"

extern "C" {

	const int DEFAULT_NUM_MFCCS = 20;
	const int DEFAULT_FRAME_SIZE = 4410;

	// External struct
	typedef struct _essentia {
		t_pxobject object;
		int frame_size;
		long buffer_offset;
		std::vector<essentia::Real> audio_buffer;
		essentia::streaming::VectorInput<essentia::Real> *vector_input;
		essentia::streaming::Algorithm* fc;
		essentia::streaming::Algorithm* spec;
		essentia::streaming::Algorithm* mfcc;
		essentia::Pool pool;
		essentia::scheduler::Network *network;
		void *mfcc_outlet;
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
		x->mfcc_outlet = listout(x);

		// essentia
		x->frame_size = DEFAULT_FRAME_SIZE;
		essentia::init();

		return x;
	}

	/** Destroy external instance */
	void essentia_free(t_essentia *x) {
		x->network->clear();
		delete x->network;
		essentia::shutdown();

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
        object_post((t_object *)x, "Preparing DSP at frame size %d", x->frame_size);
        
		// init real audio buffer
		x->audio_buffer = std::vector<essentia::Real>(x->frame_size, 0);

		// init factory
		auto & factory = essentia::streaming::AlgorithmFactory::instance();

		x->vector_input = new essentia::streaming::VectorInput<essentia::Real>(&x->audio_buffer);

		// init algorithms
		x->fc = factory.create(
			"FrameCutter",
			"frameSize", x->frame_size,
			"hopSize", x->frame_size,
			"startFromZero", true,
			"validFrameThresholdRatio", 0,
			"lastFrameToEndOfFile", true,
			"silentFrames", "keep"
        );
		x->spec = factory.create("Spectrum");
		x->mfcc = factory.create(
			"MFCC",
			"numberCoefficients", DEFAULT_NUM_MFCCS,
			"sampleRate", samplerate
		);

		// build signal chain
		x->vector_input->output("data") >> x->fc->input("signal");
		x->fc->output("frame") >> x->spec->input("frame");
		x->spec->output("spectrum") >> x->mfcc->input("spectrum");
		x->mfcc->output("bands") >> essentia::streaming::NOWHERE;
		x->mfcc->output("mfcc") >> PC(x->pool, "my.mfcc");

		// init network
		x->network = new essentia::scheduler::Network(x->vector_input);
		x->network->runPrepare();

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
		// copy audio to buffer, increment offset, and set compute flag
		bool compute_frame = false;
		for (int i = 0; i < sampleframes; i++) {
			x->audio_buffer[x->buffer_offset + i] = ins[0][i];
		}
		x->buffer_offset += sampleframes;
		if (x->buffer_offset >= x->frame_size) {
			compute_frame = true;
			x->buffer_offset = 0;
		}

		// compute
		if (compute_frame) {
			// clean
			x->pool.clear();
			x->vector_input->reset();
			x->network->reset();

			// process
			x->network->run();

			// get mfccs
			auto pool_map = x->pool.getVectorRealPool();
			auto mfccs = pool_map.at("my.mfcc").at(0);

			// output
			t_atom mfcc_atoms[DEFAULT_NUM_MFCCS];
			for (int i = 0; i < DEFAULT_NUM_MFCCS; i++) {
				atom_setfloat(mfcc_atoms + i, mfccs.at(i));
			}
			outlet_list(x->mfcc_outlet, 0L, DEFAULT_NUM_MFCCS, mfcc_atoms);
		}
	}
}
