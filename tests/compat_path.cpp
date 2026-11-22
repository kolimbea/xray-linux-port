#include "path_resolve.h"
#include <cstdio>
#include <string>
int fails=0;
void eq(const std::string& got, const std::string& want){ if(got!=want){ printf("FAIL got=[%s] want=[%s]\n",got.c_str(),want.c_str()); fails++; } }
int main(){ std::string r="@SCRATCH@/ci1";
 eq(xr_resolve_path("@SCRATCH@/ci1/gamedata/configs/system.ltx"), r+"/GameData/Configs/System.LTX");
 eq(xr_resolve_path("@SCRATCH@/ci1\\gamedata\\configs\\text\\ST.XML"), r+"/GameData/Configs/Text/st.xml");
 eq(xr_resolve_path("@SCRATCH@/ci1/GameData/Configs/System.LTX"), r+"/GameData/Configs/System.LTX");
 eq(xr_resolve_path("@SCRATCH@/ci1/gamedata/configs/new_file.ltx"), r+"/GameData/Configs/new_file.ltx");
 eq(xr_resolve_path("@SCRATCH@/ci1/gamedata/newdir/a/b.txt"), r+"/GameData/newdir/a/b.txt");
 eq(xr_resolve_path("@SCRATCH@/ci1/gamedata/configs/"), r+"/GameData/Configs/");
 eq(xr_resolve_path("@SCRATCH@/ci1//gamedata\\\\configs"), r+"/GameData/Configs");
 eq(xr_resolve_path("@SCRATCH@/ci1/gamedata/configs/../configs/system.ltx"), r+"/GameData/Configs/../Configs/System.LTX");
 FILE* f=fopen((r+"/GameData/Late.txt").c_str(),"w"); fclose(f); // created after the dir was cached
 eq(xr_resolve_path("@SCRATCH@/ci1/gamedata/late.TXT"), r+"/GameData/Late.txt");
 eq(xr_resolve_path(""), "");
 printf("path fails=%d\n",fails); return fails; }
