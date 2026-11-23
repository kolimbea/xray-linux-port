#include "matrix_inverse.h"
#include <cstdio>
#include <cmath>
#include <random>
int main(){ std::mt19937 g(7); std::uniform_real_distribution<float> d(-3,3); int fails=0;
 for(int t=0;t<1000;t++){ float a[16],b[16]; for(float& x:a) x=d(g);
  // perspective-like matrices included: zero last column except [11]
  if(t%2){ a[3]=a[7]=a[15]=0; a[11]=1; }
  if(!xr_matrix_inverse4x4(b,a)) continue;
  double e=0; for(int i=0;i<4;i++) for(int j=0;j<4;j++){ double s=0; for(int k=0;k<4;k++) s+=a[i*4+k]*b[k*4+j]; e=fmax(e,fabs(s-(i==j))); }
  if(e>1e-3){ double na=0,nb=0; for(int i=0;i<16;i++){na=fmax(na,fabs(a[i]));nb=fmax(nb,fabs(b[i]));} printf("t=%d err=%g |A|max=%g |A^-1|max=%g\n",t,e,na,nb); if(na*nb<1e4) fails++; } }
 float z[16]={0}, keep[16]={42}; bool ok=!xr_matrix_inverse4x4(keep,z) && keep[0]==42;
 printf("inverse fails=%d singular_ok=%d\n",fails,ok); return fails||!ok; }
