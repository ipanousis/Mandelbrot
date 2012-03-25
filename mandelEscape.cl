//
// mandelEscape kernel function
// for 1024x1024 square region
// upto 300 escape iterations
// and infinity beyond distance of 2 from origin
//
//
// Complex multiply
// z->x = z1.x * z2.x - z1.y * z2.y;
// z->y = z1.x * z2.y + z2.x * z1.y;
//

//#pragma OPENCL EXTENSION cl_khr_gl_sharing : enable

 __kernel void mandelEscape(__global uint  * mandelOut,
                              const  float   startX,
                              const  float   startY,
                              const  float   step)
{
  uint gid = get_global_id(0);

  float2 f0;
  float4 f1;
  f0.x = startX + step * (gid % 1024);
  f0.y = startY - step * (gid / 1024);
  f1.x = 0;
  f1.y = 0;

  uint mCount = 0;

  while(!((f1.x * f1.x + f1.y * f1.y >= 4) || mCount > 299)){
    f1.z = f1.x;
    f1.w = f1.y;
    f1.x = f1.z * f1.z - f1.w * f1.w + f0.x;
    f1.y = f1.z * f1.w + f1.z * f1.w + f0.y;

    mCount = mCount + 1;
  }

  mandelOut[gid] = mCount;
}
