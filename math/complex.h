#include <math.h>

#ifndef POINT_D
#define POINT_D
typedef struct point_d_tag{
	double x;
	double y;
} point_d ;
#endif
#ifndef POINT_I
#define POINT_I
typedef struct point_i_tag{
	int x;
	int y;
} point_i ;
#endif

point_d* complex_multiply(point_d*,point_d,point_d);
void complex_intpow_r(point_d*,point_d*,int);
void complex_intpow(point_d*,point_d*,int);
double point_dist(point_d*,point_d*);
double point_abs(point_d*);
