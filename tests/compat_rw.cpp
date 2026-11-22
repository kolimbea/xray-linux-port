#include "win32_file.h"
#include <cstdio>
#include <cstring>
int main(){ HANDLE f=CreateFile("@SCRATCH@/blob.bin",GENERIC_READ,0,0,OPEN_EXISTING,0,0); unsigned r=0; unsigned char a[8],b[8];
 int ok = SetFilePointer(f,0,0,FILE_BEGIN)==0 && ReadFile(f,a,8,&r,0) && r==8 && SetFilePointer(f,100,0,FILE_CURRENT)==108
   && ReadFile(f,b,8,&r,0) && SetFilePointer(f,0,0,FILE_END)==200000 && ReadFile(f,b,8,&r,0) && r==0
   && SetFilePointer(f,-10,0,FILE_BEGIN)==INVALID_SET_FILE_POINTER;
 FILE* fp=fopen("@SCRATCH@/blob.bin","rb"); unsigned char c[8]; fread(c,1,8,fp); fclose(fp); ok = ok && !memcmp(a,c,8);
 printf("rw ok=%d\n",ok); CloseHandle(f); return !ok; }
