#include "complex.h"

point_d* complex_multiply(point_d* z,point_d z1,point_d z2){
   z->x = z1.x * z2.x - z1.y * z2.y;
   z->y = z1.x * z2.y + z2.x * z1.y;
   return z;
}
void complex_intpow_r(point_d* z,point_d* zorg,int power){
	if(power==1){
		return;
	}
	complex_intpow_r(complex_multiply(z,*z,*zorg),zorg,power-1);
}
void complex_intpow(point_d* z,point_d* zorg,int power){
   if (power==0){
      z->x = 1;
      z->y = 0;
   }
   else if(power > 1){
		*zorg = *z;
		complex_intpow_r(z,zorg,power);
   }
}
double point_dist(point_d* z1,point_d* z2){
   return sqrt(pow(z2->y-z1->y,2)+pow(z2->x-z1->x,2));
}
double point_abs(point_d* z){
   return sqrt(pow(z->x,2)+pow(z->y,2));
}

