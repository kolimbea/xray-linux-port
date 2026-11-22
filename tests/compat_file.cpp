#include "win32_file.h"
#include <cstdio>
#include <cstring>
int main(){ SYSTEM_INFO si; GetSystemInfo(&si);
 HANDLE f=CreateFile("@SCRATCH@/blob.bin",GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0); if(f==INVALID_HANDLE_VALUE) return 1;
 unsigned sz=GetFileSize(f,0); HANDLE m=CreateFileMapping(f,0,PAGE_READONLY,0,0,0); if(!m) return 2;
 unsigned char* all=(unsigned char*)MapViewOfFile(m,FILE_MAP_READ,0,0,0);
 unsigned char* w=(unsigned char*)MapViewOfFile(m,FILE_MAP_READ,0,si.dwAllocationGranularity,1000);
 FILE* fp=fopen("@SCRATCH@/blob.bin","rb"); unsigned char buf[1000]; fseek(fp,65536,0); fread(buf,1,1000,fp); fclose(fp);
 int ok = all && w && sz==200000 && !memcmp(w,buf,1000) && !memcmp(all+65536,buf,1000);
 int dir = CreateFile("@SCRATCH@",GENERIC_READ,0,0,OPEN_EXISTING,0,0)==INVALID_HANDLE_VALUE;
 int miss = CreateFile("@SCRATCH@/nope",GENERIC_READ,0,0,OPEN_EXISTING,0,0)==INVALID_HANDLE_VALUE;
 ok = ok && UnmapViewOfFile(w) && UnmapViewOfFile(all) && CloseHandle(m) && CloseHandle(f) && dir && miss;
 printf("size=%u gran=%u ok=%d\n",sz,si.dwAllocationGranularity,ok); return !ok; }
