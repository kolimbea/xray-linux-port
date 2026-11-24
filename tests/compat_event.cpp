#include "win32_file.h"
#include <cstdio>
#include <thread>
#include <atomic>
int main(){ int ok=1;
 HANDLE m=CreateEvent(nullptr,1,0,nullptr); ok&= WaitForSingleObject(m,10)==WAIT_TIMEOUT;
 std::thread t([&]{ SetEvent(m); }); ok&= WaitForSingleObject(m,INFINITE)==WAIT_OBJECT_0; t.join();
 ok&= WaitForSingleObject(m,0)==WAIT_OBJECT_0; // manual: stays signaled
 ResetEvent(m); ok&= WaitForSingleObject(m,5)==WAIT_TIMEOUT;
 HANDLE a=CreateEvent(nullptr,0,1,nullptr); ok&= WaitForSingleObject(a,0)==WAIT_OBJECT_0 && WaitForSingleObject(a,5)==WAIT_TIMEOUT; // auto-reset
 ok&= CloseHandle(m) && CloseHandle(a);
 printf("events ok=%d\n",ok); return !ok; }
