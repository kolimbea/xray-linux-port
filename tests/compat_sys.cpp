#include "win32_system.h"
#include <cstdio>
#include <cstring>
unsigned _controlfp(unsigned, unsigned) { return 0; }
int main(){ char b[512]; unsigned n=sizeof(b); int ok=1;
 ok &= strstr(GetCommandLine(), "\"two words\"") && strstr(GetCommandLine(), "-nointro");
 ok &= GetModuleFileName(GetModuleHandle("x"), b, sizeof(b)) > 0 && strstr(b, "t_sys");
 ok &= GetCurrentDirectory(sizeof(b), b) > 0 && b[strlen(b)-1]=='/';
 n=sizeof(b); ok &= GetUserName(b,&n) && n==strlen(b)+1;
 n=sizeof(b); ok &= GetComputerName(b,&n) && n==strlen(b);
 printf("cmd=[%s] ok=%d\n", GetCommandLine(), ok); return !ok; }
