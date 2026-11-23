#include "win32_file.h"
#include <cstdio>
#include <unistd.h>
#include <sys/wait.h>
unsigned long GetLastError(); 
int main(){ HANDLE a=CreateMutex(nullptr,1,"Local\\xr-test-mutex"); int first=GetLastError()==0;
 pid_t p=fork(); if(p==0){ HANDLE b=CreateMutex(nullptr,1,"Local\\xr-test-mutex"); _exit(GetLastError()==ERROR_ALREADY_EXISTS && b ? 0 : 1);} int st; waitpid(p,&st,0);
 CloseHandle(a); pid_t q=fork(); if(q==0){ CreateMutex(nullptr,1,"Local\\xr-test-mutex"); _exit(GetLastError()==0?0:1);} int st2; waitpid(q,&st2,0);
 printf("first=%d second_sees_lock=%d after_close_free=%d\n",first,WEXITSTATUS(st)==0,WEXITSTATUS(st2)==0); return !(first&&WEXITSTATUS(st)==0&&WEXITSTATUS(st2)==0); }
