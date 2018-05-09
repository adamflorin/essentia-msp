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

	const int VECTOR_SIZE = 1;
	const int DEFAULT_FRAME_SIZE = 4410;

	// External struct
	typedef struct _essentia {
		t_pxobject object;
		int frame_size;
		long buffer_offset;
		std::vector<essentia::Real> audio_buffer;
		essentia::streaming::VectorInput<essentia::Real> *vector_input;
		essentia::streaming::Algorithm* fc;
		essentia::streaming::Algorithm* pitch_yin;
		essentia::Pool pool;
		essentia::scheduler::Network *network;
		void *vector_outlet;
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
		x->vector_outlet = listout(x);

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
		x->vector_input = new essentia::streaming::VectorInput<essentia::Real>(&x->audio_buffer);

		// init factory
		auto & factory = essentia::streaming::AlgorithmFactory::instance();

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
		x->pitch_yin = factory.create("PitchYin");

		// build signal chain
		x->vector_input->output("data") >> x->fc->input("signal");
		x->fc->output("frame") >> x->pitch_yin->input("signal");
		x->pitch_yin->output("pitchConfidence") >> essentia::streaming::NOWHERE;
		x->pitch_yin->output("pitch") >> PC(x->pool, "my.pitch");

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

			// get values
			const std::map<std::string,std::vector<essentia::Real>> & pool_real_map = x->pool.getRealPool();
			std::vector<essentia::Real> pitch_vector = pool_real_map.at("my.pitch");

			// output
			t_atom vector_atoms[VECTOR_SIZE];
			atom_setfloat(vector_atoms, pitch_vector.at(0));
			outlet_list(x->vector_outlet, 0L, VECTOR_SIZE, vector_atoms);
		}
	}
}
