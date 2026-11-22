#include "path_resolve.h"
#include <cstdio>
int main(){ auto a=xr_resolve_path("gamedata\\configs\\system.ltx"); auto b=xr_resolve_path("nothing/here.txt"); printf("[%s] [%s]\n",a.c_str(),b.c_str()); return !(a=="GameData/Configs/System.LTX" && b=="nothing/here.txt"); }
