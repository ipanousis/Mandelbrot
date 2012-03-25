#include "mandel.h"

int MANDEL_POW_COMMON;
 
mandelbrot_section_params* new_mandelbrot_section_params(mandelarray* marray, 
                                                         point_d* startPoint, 
                                                         point_i* startIndex, 
                                                         point_i* dimensions, 
                                                         double step, int mandelpow){

  mandelbrot_section_params* ptr = (mandelbrot_section_params*)malloc(sizeof(mandelbrot_section_params));
  (*ptr).data = marray;
  (*ptr).startPoint.x = 0+startPoint->x;
  (*ptr).startPoint.y = 0+startPoint->y;
  (*ptr).startIndex.x = 0+startIndex->x;
  (*ptr).startIndex.y = 0+startIndex->y;
  (*ptr).dimensions.x = 0+dimensions->x;
  (*ptr).dimensions.y = 0+dimensions->y;
  (*ptr).step = step;
  (*ptr).mandelpow = mandelpow;

  return ptr;
}

mandel_state* new_mandel_state(){
	mandel_state* state = (mandel_state*)malloc(sizeof(mandel_state));
	state->MANDEL_INTPOW_ORG = (point_d*)malloc(sizeof(point_d));
	state->MANDEL_Z = (point_d*)malloc(sizeof(point_d));
	return state;
}

int mandel_escape(mandel_state* state){

   complex_intpow(state->MANDEL_Z,state->MANDEL_INTPOW_ORG,MANDEL_POW_COMMON);

   state->MANDEL_Z->x += state->MANDEL_C.x;
   state->MANDEL_Z->y += state->MANDEL_C.y;

   if (pow(state->MANDEL_Z->x,2) + pow(state->MANDEL_Z->y,2) > 4 || state->MANDEL_CUR_DEPTH > 99)
      return state->MANDEL_CUR_DEPTH;

   state->MANDEL_CUR_DEPTH++;

   return mandel_escape(state);
}

int initMandelbrotGPU(mandelbrot_section_params * mandel, ocl_constructs * mandelCl){

  cl_int error;

  mandelCl->groupSize = 128; // largest wavefront possible seems to perform best

  size_t groups = (mandel->dimensions.x * mandel->dimensions.y -1) / mandelCl->groupSize + 1;

  mandelCl->workerSize = groups * mandelCl->groupSize;

  // Prepare/Reuse platform, device, context, command queue
  cl_bool recreateBuffers = 0;

  error = buildCachedConstructs(mandelCl, &recreateBuffers);

  if(error){
    fprintf(stderr, "mandel.c::mandelbrotGPU buildCachedConstructs returned %d, cannot continue\n",error);
    return 1;
  }

  // Build/Reuse OpenCL program
  error = buildCachedProgram(mandelCl, "mandelEscape.cl", NULL);

  if(mandelCl->program == NULL){
    fprintf(stderr, "mandel.c::mandelbrotGPU buildCachedProgram returned NULL, cannot continue\n");
    return 1;
  }

  // Create buffer

  mandelCl->buffers = (cl_mem*) malloc(sizeof(cl_mem) * 1);
  mandelCl->buffers[0] = clCreateBuffer(mandelCl->context, CL_MEM_READ_WRITE,
                                        mandelCl->workerSize * sizeof(cl_uint),
                                        (void*)NULL, &error);

  if(error){
    fprintf(stderr, "mandel.c::mandelbrotGPU clCreateBuffer failed: %d\n",error);
    return 1;
  }

  // Prepare the kernel
  mandelCl->kernel = clCreateKernel(mandelCl->program,"mandelEscape",&error);

  if(error){
    fprintf(stderr, "mandel.c::mandelbrotGPU clCreateKernel failed: %d\n",error);
    return 1;
  }

  clSetKernelArg(mandelCl->kernel, 0, sizeof(mandelCl->buffers[0]), (void*)&(mandelCl->buffers[0]));

  return error;
}

// mandelbrot section on the GPU using OpenCL
int mandelbrotGPU(mandelbrot_section_params * mandel, ocl_constructs * mandelCl){

  cl_int error;

  float startX = (float)(mandel->startPoint.x);
  float startY = (float)(mandel->startPoint.y);
  float step = (float)(mandel->step);

  clSetKernelArg(mandelCl->kernel, 1, sizeof(float), (void*)&startX);
  clSetKernelArg(mandelCl->kernel, 2, sizeof(float), (void*)&startY);
  clSetKernelArg(mandelCl->kernel, 3, sizeof(float), (void*)&step);

  // Run
  error = clEnqueueNDRangeKernel(mandelCl->queue, mandelCl->kernel, CL_TRUE, NULL,
                                 &(mandelCl->workerSize), &(mandelCl->groupSize),
                                 0, NULL, NULL);

  if(error){
    fprintf(stderr, "mandel.c::mandelbrotGPU clEnqueueNDRangeKernel failed: %d\n",error);
    return 1;
  }

  // Get kernel output
  mandel->data->mandelarray_i = (int*)clEnqueueMapBuffer(mandelCl->queue, mandelCl->buffers[0], 
                                      CL_TRUE, CL_MAP_READ, 
                                      0, mandel->dimensions.x * mandel->dimensions.y * sizeof(cl_uint),
                                      0, NULL, NULL, &error);

  clFinish(mandelCl->queue);

  if(error){
    fprintf(stderr, "mandel.c::mandelbrotGPU clEnqueueMapBuffer failed: %d\n",error);
    return error;
  }

  return error;
}
/*

mandelbrot function will calculate for complex points in the rectangle:

[row,col,height,width] = [startPoint.y,startPoint.x,dimensions.y,dimensions.x]

with step being the resolution and mandelpow the power index of the mandelbrot formula

*/
void mandelbrot(mandelbrot_section_params* mandel, void *(*mandel_function) (void *)){

  // save current MANDEL_POW value so it can be restored before return
  int last_mandel_pow = MANDEL_POW_COMMON;
  MANDEL_POW_COMMON = mandel->mandelpow;
	point_i this_startIndex;
	point_i this_dimensions;
	
	// give each thread these many points 1024*1024*4
	int threadpoints = 250000;
	int threadnum;
	int threadsum = (mandel->dimensions.x * mandel->dimensions.y) / threadpoints;
	if((threadsum * threadpoints) < (mandel->dimensions.x * mandel->dimensions.y)){
		threadsum++;
	}
	int threadrows = threadpoints / mandel->dimensions.x;
	if(threadsum * threadrows * mandel->dimensions.x > mandel->dimensions.x * mandel->dimensions.y){
		threadrows--;
	}

	this_startIndex.x = 0;
	this_startIndex.y = 0;
	this_dimensions.x = mandel->dimensions.x;
	this_dimensions.y = threadrows;

	//printf("Starting %d threads with %d rows each..\n", threadsum, threadrows);

	pthread_t** threads = (pthread_t**)malloc(sizeof(pthread_t*) * threadsum);

	mandelbrot_section_params* section_params;

	for(threadnum = 0; threadnum < threadsum; threadnum++){
		//printf("Thread %d (rows=%d-%d)..\n", threadnum, this_startIndex.y, this_startIndex.y + this_dimensions.y-1);

		threads[threadnum] = (pthread_t*)malloc(sizeof(pthread_t));

		section_params = new_mandelbrot_section_params(mandel->data, 
                                                   &(mandel->startPoint), 
                                                   &this_startIndex, 
                                                   &this_dimensions, 
                                                   mandel->step, 
                                                   mandel->mandelpow);
	
		if(threadnum < threadsum-1){
			pthread_create(threads[threadnum], NULL, mandel_function, (void*)section_params);
		}			
		else{
			section_params->dimensions.y = mandel->dimensions.y - section_params->startIndex.y;
			if(section_params->dimensions.y > 0){
				pthread_create(threads[threadnum], NULL, mandel_function, (void*)section_params);
			}
			else{
				threadsum--;
			}
		}
		this_startIndex.y += threadrows;
	}

	for(threadnum = 0; threadnum < threadsum; threadnum++){
		pthread_join(*(threads[threadnum]),NULL);
	}

   // restore previous MANDEL_POW value
   MANDEL_POW_COMMON = last_mandel_pow;
}
