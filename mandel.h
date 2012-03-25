#include "math/complex.h"
#include "ocl/cachedProgram.h"
#include "ocl/cachedConstructs.h"
#include <CL/cl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#ifndef MANDEL_ARRAY
#define MANDEL_ARRAY
typedef struct mandel_array_tag{
	double** mandelarray_d;
	int* mandelarray_i;
} mandelarray;
#endif
#ifndef MANDEL_STATE
#define MANDEL_STATE
typedef struct mandel_state_tag{
	int MANDEL_CUR_DEPTH;
	point_d MANDEL_C;
	point_d* MANDEL_Z;
	point_d* MANDEL_INTPOW_ORG;
} mandel_state ;
#endif
#ifndef SECTION_PARAMS
#define SECTION_PARAMS
typedef struct mandelbrot_section_params_tag{
	mandelarray* data;
	point_d startPoint;
	point_i startIndex;
	point_i dimensions;
	double step;
	int mandelpow;
} mandelbrot_section_params;
#endif

mandelbrot_section_params* new_mandelbrot_section_params(mandelarray*,point_d*,point_i*,point_i*,double,int);

mandel_state* new_mandel_state();

int mandel_escape(mandel_state*);

void mandelbrot(mandelbrot_section_params*, void *(*mandel_function) (void *));

int initMandelbrotGPU(mandelbrot_section_params*, ocl_constructs*);

int mandelbrotGPU(mandelbrot_section_params*, ocl_constructs*);

