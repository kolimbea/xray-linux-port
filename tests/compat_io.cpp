#include "io_compat.h"
#include <cstdio>
#include <cstring>
int main(){ _finddata_t d; int all=0, ltx=0, sub=0;
 for (intptr_t h=_findfirst("@SCRATCH@/fs/*.*",&d), r=h; r!=-1; r=_findnext(h,&d)) { all++; if(d.attrib&_A_SUBDIR && !strcmp(d.name,"Sub")) sub=1; if(h==-1) break; }
 intptr_t h=_findfirst("@SCRATCH@/fs/*.ltx",&d); for (int r=(int)(h==-1?-1:0); r!=-1; r=_findnext(h,&d)) ltx++; _findclose(h);
 int fd; int e=_sopen_s(&fd,"@SCRATCH@/fs/c.txt",_O_RDONLY|_O_BINARY,_SH_DENYNO,_S_IREAD); long n=_filelength(fd);
 printf("all=%d ltx=%d sub=%d open=%d len=%ld miss=%ld\n",all,ltx,sub,e,n,(long)_findfirst("@SCRATCH@/nope/*.*",&d));
 return !(all==7 && ltx==2 && sub && e==0 && n==3); }
